"""KiCad 10 board-object generator."""

from __future__ import annotations

from pathlib import Path

import pcbnew

from .geometry import (
    BoardConfig,
    LayoutGeometry,
    Point,
    Segment,
    build_layout,
    format_report,
)


def _iu(value_mm: float) -> int:
    return pcbnew.FromMM(value_mm)


def _point(point: Point) -> pcbnew.VECTOR2I:
    return pcbnew.VECTOR2I(_iu(point.x), _iu(point.y))


def _add_shape(board: pcbnew.BOARD, start: Point, end: Point, layer: int, width: float) -> None:
    shape = pcbnew.PCB_SHAPE(board)
    shape.SetShape(pcbnew.SHAPE_T_SEGMENT)
    shape.SetStart(_point(start))
    shape.SetEnd(_point(end))
    shape.SetLayer(layer)
    shape.SetWidth(_iu(width))
    board.Add(shape)


def _add_text(
    board: pcbnew.BOARD,
    text: str,
    position: Point,
    size: float = 1.0,
    thickness: float = 0.15,
    layer: int = pcbnew.F_SilkS,
) -> None:
    item = pcbnew.PCB_TEXT(board)
    item.SetText(text)
    item.SetPosition(_point(position))
    item.SetLayer(layer)
    item.SetTextSize(pcbnew.VECTOR2I(_iu(size), _iu(size)))
    item.SetTextThickness(_iu(thickness))
    board.Add(item)


def _add_track(
    board: pcbnew.BOARD,
    segment: Segment,
    width: float,
    net: pcbnew.NETINFO_ITEM,
    layer: int,
) -> None:
    track = pcbnew.PCB_TRACK(board)
    track.SetStart(_point(segment.start))
    track.SetEnd(_point(segment.end))
    track.SetWidth(_iu(width))
    track.SetLayer(layer)
    track.SetNet(net)
    board.Add(track)


def _add_via(
    board: pcbnew.BOARD,
    position: Point,
    diameter: float,
    drill: float,
    net: pcbnew.NETINFO_ITEM,
) -> None:
    """Add a through via joining the F.Cu active stroke to a B.Cu return."""

    via = pcbnew.PCB_VIA(board)
    via.SetPosition(_point(position))
    via.SetWidth(_iu(diameter))
    via.SetDrill(_iu(drill))
    via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    via.SetNet(net)
    board.Add(via)


def _hide_default_fields(footprint: pcbnew.FOOTPRINT) -> None:
    footprint.Reference().SetVisible(False)
    footprint.Value().SetVisible(False)


def _surface_pad_layers(copper_layer: int, mask_layer: int) -> pcbnew.LSET:
    layers = pcbnew.LSET()
    layers.AddLayer(copper_layer)
    layers.AddLayer(mask_layer)
    return layers


def _add_mounting_hole(board: pcbnew.BOARD, position: Point, index: int, diameter: float) -> None:
    footprint = pcbnew.FOOTPRINT(board)
    footprint.SetReference(f"H{index}")
    footprint.SetValue(f"MountingHole_{diameter:.1f}mm")
    footprint.SetPosition(_point(position))
    footprint.SetLayer(pcbnew.F_Cu)
    _hide_default_fields(footprint)

    pad = pcbnew.PAD(footprint)
    pad.SetNumber("")
    pad.SetAttribute(pcbnew.PAD_ATTRIB_NPTH)
    pad.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
    pad.SetSize(pcbnew.VECTOR2I(_iu(diameter), _iu(diameter)))
    pad.SetDrillSize(pcbnew.VECTOR2I(_iu(diameter), _iu(diameter)))
    pad.SetLayerSet(pad.UnplatedHoleMask())
    pad.SetPosition(_point(position))
    footprint.Add(pad)

    # A 5 mm circle on silk makes the mechanical keep-out obvious to an editor.
    circle = pcbnew.PCB_SHAPE(footprint)
    circle.SetShape(pcbnew.SHAPE_T_CIRCLE)
    circle.SetCenter(_point(position))
    circle.SetEnd(_point(Point(position.x + 2.5, position.y)))
    circle.SetLayer(pcbnew.F_SilkS)
    circle.SetWidth(_iu(0.2))
    footprint.Add(circle)
    board.Add(footprint)


def _add_header(
    board: pcbnew.BOARD,
    pins,
    reference: str,
    orientation: str,
    nets: dict[str, pcbnew.NETINFO_ITEM],
    config: BoardConfig,
) -> None:
    footprint = pcbnew.FOOTPRINT(board)
    footprint.SetReference(reference)
    footprint.SetValue("Conn_01x04_Pin_2.54mm")
    centre = Point(
        sum(pin.position.x for pin in pins) / len(pins),
        sum(pin.position.y for pin in pins) / len(pins),
    )
    footprint.SetPosition(_point(centre))
    footprint.SetLayer(pcbnew.F_Cu)
    _hide_default_fields(footprint)

    group_shift = 3.0 * config.header_pitch + config.terminal_group_gap
    lane_pitch = config.terminal_connection_width + config.trace_spacing
    # Route the high-current terminal fanout toward the nearest board edge.
    # The 4-card outline includes the clearance needed by four 1.8 mm lanes.
    lane_offset = (
        1.0
        + config.trace_spacing
        + config.terminal_connection_width / 2.0
    )
    for pin_index, pin in enumerate(pins):
        pad = pcbnew.PAD(footprint)
        pad.SetNumber(pin.number)
        pad.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
        pad.SetShape(pcbnew.PAD_SHAPE_RECT if pin.number == "1" else pcbnew.PAD_SHAPE_CIRCLE)
        pad.SetSize(pcbnew.VECTOR2I(_iu(2.0), _iu(2.0)))
        pad.SetDrillSize(pcbnew.VECTOR2I(_iu(1.0), _iu(1.0)))
        pad.SetLayerSet(pad.PTHMask())
        pad.SetPosition(_point(pin.position))
        pad.SetNet(nets[pin.net_name])
        footprint.Add(pad)

        if config.surface_terminal_lands:
            mirrored_pin = pins[len(pins) - 1 - pin_index]
            if orientation == "vertical":
                front_position = Point(
                    pin.position.x,
                    mirrored_pin.position.y - group_shift,
                )
                back_position = Point(
                    pin.position.x,
                    mirrored_pin.position.y + group_shift,
                )
                pad_size = pcbnew.VECTOR2I(
                    _iu(config.terminal_land_across),
                    _iu(config.terminal_land_along),
                )
            else:
                front_position = Point(
                    mirrored_pin.position.x - group_shift,
                    pin.position.y,
                )
                back_position = Point(
                    mirrored_pin.position.x + group_shift,
                    pin.position.y,
                )
                pad_size = pcbnew.VECTOR2I(
                    _iu(config.terminal_land_along),
                    _iu(config.terminal_land_across),
                )

            for position, copper_layer, mask_layer, depth in (
                (front_position, pcbnew.F_Cu, pcbnew.F_Mask, pin_index),
                (
                    back_position,
                    pcbnew.B_Cu,
                    pcbnew.B_Mask,
                    len(pins) - 1 - pin_index,
                ),
            ):
                surface_pad = pcbnew.PAD(footprint)
                surface_pad.SetNumber(pin.number)
                surface_pad.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
                surface_pad.SetShape(pcbnew.PAD_SHAPE_RECT)
                surface_pad.SetSize(pad_size)
                surface_pad.SetLayerSet(
                    _surface_pad_layers(copper_layer, mask_layer)
                )
                surface_pad.SetPosition(_point(position))
                surface_pad.SetNet(nets[pin.net_name])
                footprint.Add(surface_pad)
                if orientation == "vertical":
                    lane = pin.position.x + lane_offset + depth * lane_pitch
                    connection_points = [
                        pin.position,
                        Point(lane, pin.position.y),
                        Point(lane, position.y),
                        position,
                    ]
                else:
                    lane = pin.position.y + lane_offset + depth * lane_pitch
                    connection_points = [
                        pin.position,
                        Point(pin.position.x, lane),
                        Point(position.x, lane),
                        position,
                    ]
                for start, end in zip(
                    connection_points,
                    connection_points[1:],
                ):
                    _add_track(
                        board,
                        Segment(start, end),
                        config.terminal_connection_width,
                        nets[pin.net_name],
                        copper_layer,
                    )

    # Courtyard for a standard 1x4 vertical pin header body.
    if orientation == "vertical":
        left, right = centre.x - 1.6, centre.x + 1.6
        top, bottom = pins[0].position.y - 1.6, pins[-1].position.y + 1.6
    else:
        left, right = pins[0].position.x - 1.6, pins[-1].position.x + 1.6
        top, bottom = centre.y - 1.6, centre.y + 1.6
    for start, end in [
        (Point(left, top), Point(right, top)),
        (Point(right, top), Point(right, bottom)),
        (Point(right, bottom), Point(left, bottom)),
        (Point(left, bottom), Point(left, top)),
    ]:
        line = pcbnew.PCB_SHAPE(footprint)
        line.SetShape(pcbnew.SHAPE_T_SEGMENT)
        line.SetStart(_point(start))
        line.SetEnd(_point(end))
        line.SetLayer(pcbnew.F_CrtYd)
        line.SetWidth(_iu(0.05))
        footprint.Add(line)

    board.Add(footprint)

    # Labels are placed to the left so they remain inside the outline.
    for pin in pins:
        if orientation == "vertical":
            label_position = Point(pin.position.x - 3.2, pin.position.y)
        else:
            label_position = Point(pin.position.x, pin.position.y - 3.2)
        _add_text(board, pin.label, label_position, size=0.9)


def clear_board(board: pcbnew.BOARD) -> None:
    """Remove existing user items before generating a complete replacement."""

    zones = board.GetZones() if hasattr(board, "GetZones") else board.Zones()
    collections = [board.GetTracks(), board.GetFootprints(), board.GetDrawings(), zones]
    for collection in collections:
        for item in list(collection):
            board.Remove(item)


def populate_board(board: pcbnew.BOARD, config: BoardConfig, clear_existing: bool = True) -> LayoutGeometry:
    layout = build_layout(config)
    if clear_existing:
        clear_board(board)

    settings = board.GetDesignSettings()
    settings.SetBoardThickness(_iu(config.board_thickness))
    settings.m_MinClearance = _iu(config.trace_spacing)

    nets: dict[str, pcbnew.NETINFO_ITEM] = {}
    for name in ("COIL_A", "COIL_B", "COIL_C", "COIL_D"):
        net = board.FindNet(name)
        if net is None:
            net = pcbnew.NETINFO_ITEM(board, name)
            board.Add(net)
        nets[name] = net

    # Board outline on Edge.Cuts and a second, inset outline on front silk.
    corners = [
        Point(0, 0),
        Point(config.board_width, 0),
        Point(config.board_width, config.board_height),
        Point(0, config.board_height),
    ]
    for start, end in zip(corners, corners[1:] + corners[:1]):
        _add_shape(board, start, end, pcbnew.Edge_Cuts, 0.05)
    silk = [
        Point(0.5, 0.5),
        Point(config.board_width - 0.5, 0.5),
        Point(config.board_width - 0.5, config.board_height - 0.5),
        Point(0.5, config.board_height - 0.5),
    ]
    for start, end in zip(silk, silk[1:] + silk[:1]):
        _add_shape(board, start, end, pcbnew.F_SilkS, 0.2)

    # Both phases' active straight conductors alternate inside this F.Cu area.
    tl, br = layout.motion_top_left, layout.motion_bottom_right
    motion = [tl, Point(br.x, tl.y), br, Point(tl.x, br.y)]
    for start, end in zip(motion, motion[1:] + motion[:1]):
        _add_shape(board, start, end, pcbnew.F_SilkS, 0.15)
    _add_text(
        board,
        f"XY / A+B F.Cu / C+D B.Cu / "
        f"{config.motion_width:g} x {config.motion_height:g} mm",
        Point(config.board_width / 2, config.board_height - 1.8),
        0.9,
    )

    inset = config.mounting_hole_inset
    hole_positions = [
        Point(inset, inset),
        Point(config.board_width - inset, inset),
        Point(inset, config.board_height - inset),
        Point(config.board_width - inset, config.board_height - inset),
    ]
    for index, position in enumerate(hole_positions, 1):
        _add_mounting_hole(board, position, index, config.mounting_hole_diameter)

    _add_header(board, layout.header_pins[:4], "J1", "vertical", nets, config)
    _add_header(board, layout.header_pins[4:], "J2", "horizontal", nets, config)
    copper_layers = {"F.Cu": pcbnew.F_Cu, "B.Cu": pcbnew.B_Cu}
    for phase in layout.phases:
        for layered in phase.segments:
            _add_track(
                board,
                layered.segment,
                config.trace_width,
                nets[phase.net_name],
                copper_layers[layered.copper_layer],
            )
        for position in phase.via_points:
            _add_via(
                board,
                position,
                config.via_diameter,
                config.via_drill,
                nets[phase.net_name],
            )
    return layout


def generate_board_file(output: str | Path, config: BoardConfig | None = None) -> LayoutGeometry:
    config = config or BoardConfig()
    board = pcbnew.BOARD()
    layout = populate_board(board, config, clear_existing=False)
    output_path = Path(output).expanduser().resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    if not pcbnew.SaveBoard(str(output_path), board):
        raise OSError(f"KiCad could not save board: {output_path}")
    print(format_report(layout))
    print(f"  Saved: {output_path}")
    return layout
