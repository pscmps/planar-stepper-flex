"""KiCad Action Plugin entry point and parameter dialog."""

from __future__ import annotations

import pcbnew
import wx

from .generator import populate_board
from .geometry import BoardConfig, format_report


class ParameterDialog(wx.Dialog):
    FIELDS = [
        ("coil_pitch", "Pitch per phase (mm)"),
        ("trace_width", "Trace width (mm)"),
        ("trace_spacing", "Trace spacing (mm)"),
        ("phase_offset", "B/D phase offset (mm)"),
        ("motion_width", "Active straight area X (mm)"),
        ("motion_height", "Active straight area Y (mm)"),
    ]

    def __init__(self, parent: wx.Window | None, defaults: BoardConfig):
        super().__init__(parent, title="XY planar stepper board", size=(450, 390))
        panel = wx.Panel(self)
        root = wx.BoxSizer(wx.VERTICAL)
        note = wx.StaticText(
            panel,
            label=(
                "Generates a complete 90 x 90 mm board and replaces current board items.\n"
                "A/B run vertically on F.Cu; C/D run horizontally on B.Cu."
            ),
        )
        root.Add(note, 0, wx.ALL | wx.EXPAND, 12)
        grid = wx.FlexGridSizer(rows=len(self.FIELDS), cols=2, vgap=8, hgap=12)
        grid.AddGrowableCol(1, 1)
        self.controls: dict[str, wx.TextCtrl] = {}
        for attr, label in self.FIELDS:
            grid.Add(wx.StaticText(panel, label=label), 0, wx.ALIGN_CENTER_VERTICAL)
            control = wx.TextCtrl(panel, value=str(getattr(defaults, attr)))
            self.controls[attr] = control
            grid.Add(control, 1, wx.EXPAND)
        root.Add(grid, 1, wx.LEFT | wx.RIGHT | wx.EXPAND, 12)
        root.Add(self.CreateSeparatedButtonSizer(wx.OK | wx.CANCEL), 0, wx.ALL | wx.EXPAND, 12)
        panel.SetSizer(root)

    def config(self) -> BoardConfig:
        values = {name: float(control.GetValue()) for name, control in self.controls.items()}
        config = BoardConfig(**values)
        config.validate()
        return config


class OneAxisStepperAction(pcbnew.ActionPlugin):
    def defaults(self) -> None:
        self.name = "Generate XY planar stepper board"
        self.category = "Board generators"
        self.description = "Generate perpendicular A/B and C/D planar-stepper coils on two layers"
        self.show_toolbar_button = True

    def Run(self) -> None:  # KiCad requires this exact method name.
        dialog = ParameterDialog(None, BoardConfig())
        try:
            if dialog.ShowModal() != wx.ID_OK:
                return
            try:
                config = dialog.config()
            except ValueError as error:
                wx.MessageBox(str(error), "Invalid parameters", wx.OK | wx.ICON_ERROR)
                return

            board = pcbnew.GetBoard()
            if board is None:
                wx.MessageBox("No board is open.", "XY planar stepper", wx.OK | wx.ICON_ERROR)
                return
            layout = populate_board(board, config, clear_existing=True)
            print(format_report(layout))
            pcbnew.Refresh()
            wx.MessageBox(format_report(layout), "Generation complete", wx.OK | wx.ICON_INFORMATION)
        finally:
            dialog.Destroy()
