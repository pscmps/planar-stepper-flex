#!/usr/bin/env python3
"""Generate a compact four-module AE-DRV8835-S carrier board."""

from __future__ import annotations

import argparse
import importlib.util
import json
import sys
from pathlib import Path

import pcbnew


ROOT = Path(__file__).resolve().parents[2]
BOARD_DIR = Path(__file__).resolve().parent
COMMON_PATH = ROOT / "hardware" / "pico2w-drv8835-controller" / "generate_drv8835_board.py"
SPEC = importlib.util.spec_from_file_location("drv8835_board_common", COMMON_PATH)
if SPEC is None or SPEC.loader is None:
    raise ImportError(f"Could not load {COMMON_PATH}")
common = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = common
SPEC.loader.exec_module(common)

BOARD_W = 48.0
BOARD_H = 60.0
CORNER_R = 3.0
MODULE_POSITIONS = {
    "U1": (11.5, 22.5),
    "U2": (11.5, 41.0),
    "U3": (36.5, 22.5),
    "U4": (36.5, 41.0),
}


def add_board_outline(board: pcbnew.BOARD) -> None:
    r = CORNER_R
    offset = r * (1.0 - 2.0**-0.5)
    for start, end in [
        ((r, 0), (BOARD_W - r, 0)),
        ((BOARD_W, r), (BOARD_W, BOARD_H - r)),
        ((BOARD_W - r, BOARD_H), (r, BOARD_H)),
        ((0, BOARD_H - r), (0, r)),
    ]:
        common.line(board, start, end, pcbnew.Edge_Cuts, 0.05)
    for start, middle, end in [
        ((BOARD_W - r, 0), (BOARD_W - offset, offset), (BOARD_W, r)),
        ((BOARD_W, BOARD_H - r), (BOARD_W - offset, BOARD_H - offset), (BOARD_W - r, BOARD_H)),
        ((r, BOARD_H), (offset, BOARD_H - offset), (0, BOARD_H - r)),
        ((0, r), (offset, offset), (r, 0)),
    ]:
        arc = pcbnew.PCB_SHAPE(board)
        arc.SetShape(pcbnew.SHAPE_T_ARC)
        arc.SetArcGeometry(common.vec(*start), common.vec(*middle), common.vec(*end))
        arc.SetLayer(pcbnew.Edge_Cuts)
        arc.SetWidth(common.iu(0.05))
        board.Add(arc)


def write_project(output: Path) -> None:
    template = ROOT / "hardware" / "pico2w-drv8835-controller" / "pico2w_drv8835_control.kicad_pro"
    project = json.loads(template.read_text(encoding="utf-8"))
    project.setdefault("meta", {})["filename"] = output.with_suffix(".kicad_pro").name
    output.with_suffix(".kicad_pro").write_text(
        json.dumps(project, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )


def add_locked_track(board: pcbnew.BOARD, target_net, points, width: float, layer) -> None:
    for start, end in zip(points, points[1:]):
        item = pcbnew.PCB_TRACK(board)
        item.SetStart(common.vec(*start))
        item.SetEnd(common.vec(*end))
        item.SetWidth(common.iu(width))
        item.SetLayer(layer)
        item.SetNet(target_net)
        item.SetLocked(True)
        board.Add(item)


def generate(output: Path) -> None:
    board = pcbnew.BOARD()
    settings = board.GetDesignSettings()
    settings.SetBoardThickness(common.iu(1.6))
    settings.m_MinClearance = common.iu(0.25)
    net_names = [
        "GND", "VCC", "VM_X", "VM_Y",
        "A_IN1", "A_IN2", "B_IN1", "B_IN2",
        "C_IN1", "C_IN2", "D_IN1", "D_IN2",
        "A+", "A-", "B+", "B-", "C+", "C-", "D+", "D-",
    ]
    nets = {name: common.net(board, name) for name in net_names}
    add_board_outline(board)

    module_a = common.add_drv8835_module(board, "U1", "A", MODULE_POSITIONS["U1"], "VM_X", "A_IN1", "A_IN2", "A+", "A-", nets, logic_supply="VCC")
    common.add_drv8835_module(board, "U2", "B", MODULE_POSITIONS["U2"], "VM_X", "B_IN1", "B_IN2", "B+", "B-", nets, logic_supply="VCC")
    common.add_drv8835_module(board, "U3", "C", MODULE_POSITIONS["U3"], "VM_Y", "C_IN1", "C_IN2", "C+", "C-", nets, flipped=True, logic_supply="VCC")
    module_d = common.add_drv8835_module(board, "U4", "D", MODULE_POSITIONS["U4"], "VM_Y", "D_IN1", "D_IN2", "D+", "D-", nets, flipped=True, logic_supply="VCC")

    input_x = [17.65 + 2.54 * index for index in range(6)]
    input_positions = [(x, 4.5) for x in input_x] + [(x, 7.04) for x in input_x]
    input_names = [
        "VCC", "GND", "A_IN1", "A_IN2", "B_IN1", "B_IN2",
        "VCC", "GND", "C_IN1", "C_IN2", "D_IN1", "D_IN2",
    ]
    common.add_header(board, "J1", "MCU INPUT X/Y 2x6", input_positions, input_names, nets)

    common.add_header(board, "J2", "X POWER", [(4.5, 4.5), (7.04, 4.5), (4.5, 9.58), (7.04, 9.58)], ["VM_X", "VM_X", "GND", "GND"], nets, pad_size=3.0, drill=1.2, draw_outline=False)
    common.add_header(board, "J3", "Y POWER", [(40.96, 4.5), (43.5, 4.5), (40.96, 9.58), (43.5, 9.58)], ["VM_Y", "VM_Y", "GND", "GND"], nets, pad_size=3.0, drill=1.2, draw_outline=False)
    x_output = common.add_header(board, "J4", "X OUTPUT", [(2.5, 28.5 + 2.54 * index) for index in range(4)], ["A+", "A-", "B+", "B-"], nets)
    y_output = common.add_header(board, "J5", "Y OUTPUT", [(45.5, 28.5 + 2.54 * index) for index in range(4)], ["C+", "C-", "D+", "D-"], nets)

    # The alternating module pads A+/A-/A+/A- leave a narrow 1.5 mm escape.
    # Fix A- on B.Cu; FreeRouting handles every remaining connection.
    a_minus_3 = common.pos(module_a, "3")
    a_minus_5 = common.pos(module_a, "5")
    a_minus_out = common.pos(x_output, "2")
    add_locked_track(
        board, nets["A-"],
        [a_minus_3, (5.3, a_minus_3[1]), (5.3, a_minus_5[1]), a_minus_5],
        1.5, pcbnew.B_Cu,
    )
    add_locked_track(
        board, nets["A-"],
        [(5.3, a_minus_5[1]), (4.8, a_minus_out[1]), a_minus_out],
        1.5, pcbnew.B_Cu,
    )

    # Route D+ beneath U4 so the 3.2 mm mounting hole can remain at (44, 44).
    d_plus_2 = common.pos(module_d, "2")
    d_plus_4 = common.pos(module_d, "4")
    d_plus_out = common.pos(y_output, "3")
    add_locked_track(
        board, nets["D+"],
        [d_plus_4, (37.9, d_plus_4[1]), (37.9, d_plus_2[1]), d_plus_2],
        1.5, pcbnew.B_Cu,
    )
    add_locked_track(
        board, nets["D+"],
        [d_plus_4, (42.4, d_plus_4[1]), (42.4, 34.0), d_plus_out],
        1.5, pcbnew.B_Cu,
    )

    common.add_capacitor(board, "C1", "470uF 16V", (6.5, 52.0), "VM_X", nets, radial=True)
    common.add_capacitor(board, "C2", "470uF 16V", (36.5, 52.0), "VM_Y", nets, radial=True)
    common.add_capacitor(board, "C3", "0.1uF", (6.5, 56.5), "VM_X", nets)
    common.add_capacitor(board, "C4", "0.1uF", (39.0, 56.5), "VM_Y", nets)
    common.add_solder_jumper(board, "SJ1", (24.0, 52.0), nets)
    common.add_solder_jumper(board, "SJ2", (24.0, 57.0), nets)

    resistor_positions = {
        "A_IN1": (19.0, 20.5), "A_IN2": (19.0, 24.5),
        "B_IN1": (19.0, 39.0), "B_IN2": (19.0, 43.0),
        "C_IN1": (29.0, 20.5), "C_IN2": (29.0, 24.5),
        "D_IN1": (29.0, 39.0), "D_IN2": (29.0, 43.0),
    }
    for index, (signal, position) in enumerate(resistor_positions.items(), 1):
        common.add_resistor_0805(board, f"R{index}", signal, position, nets, mirrored=position[0] > BOARD_W / 2)

    mounting_holes = {
        "H1": (3.0, 18.0),
        "H2": (45.0, 18.0),
        "H3": (3.0, 44.0),
        "H4": (45.0, 44.0),
    }
    for reference, position in mounting_holes.items():
        common.add_mounting_hole(board, reference, position)

    common.text(board, "X: V G A1 A2 B1 B2", (24.0, 2.0), 0.8)
    common.text(board, "Y: V G C1 C2 D1 D2", (24.0, 10.8), 0.8)
    common.text(board, "X: A+ A- B+ B-", (4.8, 32.0), 0.8, angle=90.0)
    common.text(board, "Y: C+ C- D+ D-", (43.2, 32.0), 0.8, angle=90.0)
    common.text(board, "X VM/G", (6.0, 12.4), 0.8)
    common.text(board, "Y VM/G", (42.0, 12.4), 0.8)

    output.parent.mkdir(parents=True, exist_ok=True)
    if not pcbnew.SaveBoard(str(output), board):
        raise OSError(f"Could not save {output}")
    write_project(output)

    manifest = {
        "board_mm": [BOARD_W, BOARD_H],
        "corner_radius_mm": CORNER_R,
        "layers": 2,
        "copper_oz": 1,
        "phase_trace_width_mm": 1.5,
        "axis_supply_width_mm": 2.0,
        "module": "Akizuki AE-DRV8835-S, 2.54 mm pitch, 7.62 mm row spacing",
        "input": {
            "connector": "J1 2x6 P2.54mm",
            "top_x": ["VCC", "GND", "A_IN1", "A_IN2", "B_IN1", "B_IN2"],
            "bottom_y": ["VCC", "GND", "C_IN1", "C_IN2", "D_IN1", "D_IN2"],
        },
        "connectors": {
            "J2": ["VM_X", "VM_X", "GND", "GND"],
            "J3": ["VM_Y", "VM_Y", "GND", "GND"],
            "J4": ["A+", "A-", "B+", "B-"],
            "J5": ["C+", "C-", "D+", "D-"],
        },
        "power_connector_geometry": {
            "pitch_within_row_mm": 2.54,
            "vm_to_gnd_row_gap_mm": 5.08,
            "pad_diameter_mm": 3.0,
            "plated_drill_mm": 1.2,
        },
        "solder_jumper_geometry": {
            "pad_size_mm": [5.0, 5.0],
            "vm_x_to_vm_y_gap_mm": 0.5,
            "parallel_jumpers": 2,
        },
        "mounting_holes": {
            "diameter_mm": 3.2,
            "plated": False,
            "positions_mm": mounting_holes,
        },
        "routing": "Final copper is generated by the shared autorouter and FreeRouting.",
    }
    output.with_name("routing-manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=Path,
        default=BOARD_DIR / "drv8835_driver_module.kicad_pcb",
    )
    args = parser.parse_args()
    generate(args.output.resolve())
    print(args.output)


if __name__ == "__main__":
    main()
