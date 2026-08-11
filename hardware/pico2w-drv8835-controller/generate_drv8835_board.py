#!/usr/bin/env python3
"""Generate the Pico 2 W + four AE-DRV8835-S two-layer control board."""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path

import pcbnew


BOARD_DIR = Path(__file__).resolve().parent
BOARD_W = 80.0
BOARD_H = 85.0
CORNER_R = 5.0
PICO_POS = (40.0, 26.5)
MODULE_POSITIONS = {
    "U2": (15.0, 29.0),
    "U3": (15.0, 56.0),
    "U4": (65.0, 29.0),
    "U5": (65.0, 56.0),
}


def iu(value: float) -> int:
    return pcbnew.FromMM(value)


def vec(x: float, y: float) -> pcbnew.VECTOR2I:
    return pcbnew.VECTOR2I(iu(x), iu(y))


def pos(item: pcbnew.FOOTPRINT, pad_number: str) -> tuple[float, float]:
    for pad in item.Pads():
        if pad.GetNumber() == pad_number:
            point = pad.GetPosition()
            return pcbnew.ToMM(point.x), pcbnew.ToMM(point.y)
    raise KeyError(f"{item.GetReference()} pad {pad_number} not found")


def net(board: pcbnew.BOARD, name: str) -> pcbnew.NETINFO_ITEM:
    found = board.FindNet(name)
    if found is None:
        found = pcbnew.NETINFO_ITEM(board, name)
        board.Add(found)
    return found


def line(parent, start, end, layer, width=0.2):
    shape = pcbnew.PCB_SHAPE(parent)
    shape.SetShape(pcbnew.SHAPE_T_SEGMENT)
    shape.SetStart(vec(*start))
    shape.SetEnd(vec(*end))
    shape.SetLayer(layer)
    shape.SetWidth(iu(width))
    parent.Add(shape)


def text(board, value, position, size=1.0, layer=pcbnew.F_SilkS, angle=0.0):
    item = pcbnew.PCB_TEXT(board)
    item.SetText(value)
    item.SetPosition(vec(*position))
    item.SetLayer(layer)
    item.SetTextSize(vec(size, size))
    item.SetTextThickness(iu(0.15))
    item.SetTextAngleDegrees(angle)
    board.Add(item)


def track(board, target_net, points, width, layer):
    for start, end in zip(points, points[1:]):
        if start == end:
            continue
        item = pcbnew.PCB_TRACK(board)
        item.SetStart(vec(*start))
        item.SetEnd(vec(*end))
        item.SetWidth(iu(width))
        item.SetLayer(layer)
        item.SetNet(target_net)
        board.Add(item)


def via(board, target_net, position, diameter=1.2, drill=0.6):
    item = pcbnew.PCB_VIA(board)
    item.SetPosition(vec(*position))
    item.SetWidth(iu(diameter))
    item.SetDrill(iu(drill))
    item.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    item.SetNet(target_net)
    board.Add(item)


def add_board_outline(board):
    r = CORNER_R
    offset = r * (1.0 - 2.0**-0.5)
    for start, end in [
        ((r, 0), (BOARD_W - r, 0)),
        ((BOARD_W, r), (BOARD_W, BOARD_H - r)),
        ((BOARD_W - r, BOARD_H), (r, BOARD_H)),
        ((0, BOARD_H - r), (0, r)),
    ]:
        line(board, start, end, pcbnew.Edge_Cuts, 0.05)
    for start, middle, end in [
        ((BOARD_W - r, 0), (BOARD_W - offset, offset), (BOARD_W, r)),
        ((BOARD_W, BOARD_H - r), (BOARD_W - offset, BOARD_H - offset), (BOARD_W - r, BOARD_H)),
        ((r, BOARD_H), (offset, BOARD_H - offset), (0, BOARD_H - r)),
        ((0, r), (offset, offset), (r, 0)),
    ]:
        arc = pcbnew.PCB_SHAPE(board)
        arc.SetShape(pcbnew.SHAPE_T_ARC)
        arc.SetArcGeometry(vec(*start), vec(*middle), vec(*end))
        arc.SetLayer(pcbnew.Edge_Cuts)
        arc.SetWidth(iu(0.05))
        board.Add(arc)


def add_mounting_hole(board, reference, position):
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(reference)
    fp.SetValue("MountingHole_3.2mm")
    fp.SetPosition(vec(*position))
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    pad = pcbnew.PAD(fp)
    pad.SetNumber("")
    pad.SetAttribute(pcbnew.PAD_ATTRIB_NPTH)
    pad.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
    pad.SetSize(vec(3.2, 3.2))
    pad.SetDrillSize(vec(3.2, 3.2))
    pad.SetLayerSet(pad.UnplatedHoleMask())
    pad.SetPosition(vec(*position))
    fp.Add(pad)
    board.Add(fp)


def add_header(board, reference, value, positions, names, nets, pad_size=2.2, drill=1.0, draw_outline=True):
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(reference)
    fp.SetValue(value)
    fp.SetPosition(vec(*positions[0]))
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    for index, (point, name) in enumerate(zip(positions, names), 1):
        pad = pcbnew.PAD(fp)
        pad.SetNumber(str(index))
        pad.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
        pad.SetShape(pcbnew.PAD_SHAPE_RECT if index == 1 else pcbnew.PAD_SHAPE_CIRCLE)
        pad.SetSize(vec(pad_size, pad_size))
        pad.SetDrillSize(vec(drill, drill))
        pad.SetLayerSet(pad.PTHMask())
        pad.SetPosition(vec(*point))
        pad.SetNet(nets[name])
        fp.Add(pad)
    if draw_outline:
        xs = [p[0] for p in positions]
        ys = [p[1] for p in positions]
        left, right = min(xs) - 1.4, max(xs) + 1.4
        top, bottom = min(ys) - 1.4, max(ys) + 1.4
        for start, end in [((left, top), (right, top)), ((right, top), (right, bottom)),
                           ((right, bottom), (left, bottom)), ((left, bottom), (left, top))]:
            line(fp, start, end, pcbnew.F_SilkS)
    board.Add(fp)
    return fp


def add_drv8835_module(board, reference, phase, center, supply, in1, in2, out_p, out_n, nets, flipped=False, logic_supply="+3V3"):
    """Akizuki AE-DRV8835-S: 2.54 mm pitch, 7.62 mm row spacing, 2x6 pins."""
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(reference)
    fp.SetValue(f"AE-DRV8835-S PHASE {phase}")
    fp.SetPosition(vec(*center))
    fp.Reference().SetVisible(True)
    fp.Reference().SetTextSize(vec(0.9, 0.9))
    fp.Reference().SetPosition(vec(center[0], center[1] - 8.5))
    fp.Value().SetVisible(False)
    left_x, right_x = center[0] - 3.81, center[0] + 3.81
    rows = [center[1] - 6.35 + 2.54 * index for index in range(6)]
    if not flipped:
        pin_data = [
            ("1", (left_x, rows[0]), supply), ("2", (left_x, rows[1]), out_p),
            ("3", (left_x, rows[2]), out_n), ("4", (left_x, rows[3]), out_p),
            ("5", (left_x, rows[4]), out_n), ("6", (left_x, rows[5]), "GND"),
            ("12", (right_x, rows[0]), logic_supply), ("11", (right_x, rows[1]), "GND"),
            ("10", (right_x, rows[2]), in1), ("9", (right_x, rows[3]), in2),
            ("8", (right_x, rows[4]), in1), ("7", (right_x, rows[5]), in2),
        ]
    else:
        pin_data = [
            ("1", (right_x, rows[5]), supply), ("2", (right_x, rows[4]), out_p),
            ("3", (right_x, rows[3]), out_n), ("4", (right_x, rows[2]), out_p),
            ("5", (right_x, rows[1]), out_n), ("6", (right_x, rows[0]), "GND"),
            ("12", (left_x, rows[5]), logic_supply), ("11", (left_x, rows[4]), "GND"),
            ("10", (left_x, rows[3]), in1), ("9", (left_x, rows[2]), in2),
            ("8", (left_x, rows[1]), in1), ("7", (left_x, rows[0]), in2),
        ]
    for number, point, name in pin_data:
        pad = pcbnew.PAD(fp)
        pad.SetNumber(number)
        pad.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
        pad.SetShape(pcbnew.PAD_SHAPE_RECT if number == "1" else pcbnew.PAD_SHAPE_CIRCLE)
        pad.SetSize(vec(2.0, 2.0))
        pad.SetDrillSize(vec(0.9, 0.9))
        pad.SetLayerSet(pad.PTHMask())
        pad.SetPosition(vec(*point))
        pad.SetNet(nets[name])
        fp.Add(pad)
    for start, end in [
        ((center[0] - 5.0, center[1] - 7.5), (center[0] + 5.0, center[1] - 7.5)),
        ((center[0] + 5.0, center[1] - 7.5), (center[0] + 5.0, center[1] + 7.5)),
        ((center[0] + 5.0, center[1] + 7.5), (center[0] - 5.0, center[1] + 7.5)),
        ((center[0] - 5.0, center[1] + 7.5), (center[0] - 5.0, center[1] - 7.5)),
    ]:
        line(fp, start, end, pcbnew.F_SilkS)
    board.Add(fp)
    text(board, phase, center, 1.4)
    return fp


def add_resistor_0805(board, reference, signal, position, nets, mirrored=False):
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(reference)
    fp.SetValue("10k PULLDOWN")
    fp.SetPosition(vec(*position))
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    signal_x = position[0] + (1.0 if mirrored else -1.0)
    ground_x = position[0] - (1.0 if mirrored else -1.0)
    for number, x, name in [("1", signal_x, signal), ("2", ground_x, "GND")]:
        pad = pcbnew.PAD(fp)
        pad.SetNumber(number)
        pad.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
        pad.SetShape(pcbnew.PAD_SHAPE_ROUNDRECT)
        pad.SetRoundRectRadiusRatio(0.2)
        pad.SetSize(vec(1.2, 1.4))
        pad.SetLayerSet(pad.SMDMask())
        pad.SetPosition(vec(x, position[1]))
        pad.SetNet(nets[name])
        fp.Add(pad)
    board.Add(fp)
    return fp


def add_capacitor(board, reference, value, position, supply, nets, radial=False):
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(reference)
    fp.SetValue(value)
    fp.SetPosition(vec(*position))
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    spacing = 5.0 if radial else 2.5
    for number, point, name in [("1", position, supply), ("2", (position[0] + spacing, position[1]), "GND")]:
        pad = pcbnew.PAD(fp)
        pad.SetNumber(number)
        pad.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
        pad.SetShape(pcbnew.PAD_SHAPE_RECT if number == "1" else pcbnew.PAD_SHAPE_CIRCLE)
        pad.SetSize(vec(2.2 if radial else 1.8, 2.2 if radial else 1.8))
        pad.SetDrillSize(vec(1.0 if radial else 0.8, 1.0 if radial else 0.8))
        pad.SetLayerSet(pad.PTHMask())
        pad.SetPosition(vec(*point))
        pad.SetNet(nets[name])
        fp.Add(pad)
    board.Add(fp)
    text(board, f"{reference} {value}", (position[0] + spacing / 2, position[1] + 2.0), 0.8)
    return fp


def add_solder_jumper(board, reference, position, nets):
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(reference)
    fp.SetValue("VM_X-VM_Y LINK")
    fp.SetPosition(vec(*position))
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    for number, x, name in [("1", position[0] - 2.75, "VM_X"), ("2", position[0] + 2.75, "VM_Y")]:
        pad = pcbnew.PAD(fp)
        pad.SetNumber(number)
        pad.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
        pad.SetShape(pcbnew.PAD_SHAPE_RECT)
        pad.SetSize(vec(5.0, 5.0))
        pad.SetLayerSet(pad.SMDMask())
        pad.SetPosition(vec(x, position[1]))
        pad.SetNet(nets[name])
        fp.Add(pad)
    board.Add(fp)
    return fp


def add_testpoint(board, reference, name, position, nets):
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(reference)
    fp.SetValue(name)
    fp.SetPosition(vec(*position))
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    pad = pcbnew.PAD(fp)
    pad.SetNumber("1")
    pad.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
    pad.SetShape(pcbnew.PAD_SHAPE_ROUNDRECT)
    pad.SetRoundRectRadiusRatio(0.2)
    pad.SetSize(vec(1.8, 1.5))
    pad.SetLayerSet(pad.SMDMask())
    pad.SetPosition(vec(*position))
    pad.SetNet(nets[name])
    fp.Add(pad)
    board.Add(fp)
    return fp


def add_ground_zone(board, ground):
    zone = pcbnew.ZONE(board)
    zone.SetLayer(pcbnew.B_Cu)
    zone.SetNet(ground)
    zone.SetLocalClearance(iu(0.35))
    zone.SetMinThickness(iu(0.25))
    zone.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
    outline = zone.Outline()
    outline.NewOutline()
    # The central Pico antenna region remains copper-free on both layers.
    for x, y in [(0.6, 12.0), (BOARD_W - 0.6, 12.0), (BOARD_W - 0.6, 84.4), (0.6, 84.4)]:
        outline.Append(iu(x), iu(y))
    board.Add(zone)


@dataclass(frozen=True)
class Route:
    name: str
    width: float
    layer: str
    points: list[tuple[float, float]]


def generate(output: Path, pico_library: Path):
    board = pcbnew.BOARD()
    settings = board.GetDesignSettings()
    settings.SetBoardThickness(iu(1.6))
    settings.m_MinClearance = iu(0.25)
    names = [
        "GND", "+3V3", "VM_X", "VM_Y",
        "A_IN1", "A_IN2", "B_IN1", "B_IN2", "C_IN1", "C_IN2", "D_IN1", "D_IN2",
        "A+", "A-", "B+", "B-", "C+", "C-", "D+", "D-",
    ]
    nets = {name: net(board, name) for name in names}
    add_board_outline(board)

    pico = pcbnew.FootprintLoad(str(pico_library), "RaspberryPi_Pico_W_SMD_HandSolder")
    if pico is None:
        raise FileNotFoundError("Official Pico W footprint not found")
    pico.SetReference("U1")
    pico.SetValue("Raspberry Pi Pico 2 W")
    pico.SetPosition(vec(*PICO_POS))
    pico.SetOrientationDegrees(180.0)
    pico.Reference().SetTextSize(vec(0.9, 0.9))
    pico.Value().SetVisible(False)
    board.Add(pico)
    pico_map = {
        "3": "GND", "20": "C_IN1", "21": "C_IN2", "22": "D_IN1",
        "24": "D_IN2", "25": "A_IN1", "26": "A_IN2", "27": "B_IN1", "28": "GND",
        "29": "B_IN2", "36": "+3V3", "38": "GND",
    }
    for pad in pico.Pads():
        if pad.GetNumber() in pico_map:
            pad.SetNet(nets[pico_map[pad.GetNumber()]])

    modules = {
        "A": add_drv8835_module(board, "U2", "A", MODULE_POSITIONS["U2"], "VM_X", "A_IN1", "A_IN2", "A+", "A-", nets),
        "B": add_drv8835_module(board, "U3", "B", MODULE_POSITIONS["U3"], "VM_X", "B_IN1", "B_IN2", "B+", "B-", nets),
        "C": add_drv8835_module(board, "U4", "C", MODULE_POSITIONS["U4"], "VM_Y", "C_IN1", "C_IN2", "C+", "C-", nets, flipped=True),
        "D": add_drv8835_module(board, "U5", "D", MODULE_POSITIONS["U5"], "VM_Y", "D_IN1", "D_IN2", "D+", "D-", nets, flipped=True),
    }

    # Power uses two 1x2 rows. The 5.08 mm VM-to-GND row gap and large plated
    # holes reduce solder-bridge risk while retaining two contacts per rail.
    jx_power = add_header(board, "J1", "X POWER SPLIT 2x2", [(5.5, 14.0), (8.04, 14.0), (5.5, 19.08), (8.04, 19.08)], ["VM_X", "VM_X", "GND", "GND"], nets, pad_size=3.0, drill=1.2, draw_outline=False)
    jy_power = add_header(board, "J2", "Y POWER SPLIT 2x2", [(71.96, 14.0), (74.5, 14.0), (71.96, 19.08), (74.5, 19.08)], ["VM_Y", "VM_Y", "GND", "GND"], nets, pad_size=3.0, drill=1.2, draw_outline=False)
    jx_out = add_header(board, "J3", "X OUTPUT", [(3.0, 38.0 + 2.54 * i) for i in range(4)], ["A+", "A-", "B+", "B-"], nets)
    jy_out = add_header(board, "J4", "Y OUTPUT", [(77.0, 38.0 + 2.54 * i) for i in range(4)], ["C+", "C-", "D+", "D-"], nets)
    cx = add_capacitor(board, "C1", "470uF 16V", (5.0, 70.0), "VM_X", nets, radial=True)
    cy = add_capacitor(board, "C2", "470uF 16V", (70.0, 70.0), "VM_Y", nets, radial=True)
    c3 = add_capacitor(board, "C3", "0.1uF", (16.0, 72.0), "VM_X", nets)
    c4 = add_capacitor(board, "C4", "0.1uF", (61.0, 72.0), "VM_Y", nets)
    sj1 = add_solder_jumper(board, "SJ1", (40.0, 74.0), nets)
    sj2 = add_solder_jumper(board, "SJ2", (40.0, 80.0), nets)

    resistors = {}
    testpoints = {}
    signal_order = ["A_IN1", "A_IN2", "B_IN1", "B_IN2", "C_IN1", "C_IN2", "D_IN1", "D_IN2"]
    gpio_labels = {
        "A_IN1": "GP19", "A_IN2": "GP20", "B_IN1": "GP21", "B_IN2": "GP22",
        "C_IN1": "GP15", "C_IN2": "GP16", "D_IN1": "GP17", "D_IN2": "GP18",
    }
    local_positions = {
        "A_IN1": (25.0, 27.0), "A_IN2": (25.0, 31.0),
        "B_IN1": (25.0, 54.0), "B_IN2": (25.0, 58.0),
        "C_IN1": (55.0, 27.0), "C_IN2": (55.0, 31.0),
        "D_IN1": (55.0, 54.0), "D_IN2": (55.0, 58.0),
    }
    for index, signal in enumerate(signal_order, 1):
        resistor_pos = local_positions[signal]
        mirrored = resistor_pos[0] > 40
        resistors[signal] = add_resistor_0805(board, f"R{index}", signal, resistor_pos, nets, mirrored=mirrored)
        tp_x = resistor_pos[0] + (-3.0 if resistor_pos[0] < 40 else 3.0)
        testpoints[signal] = add_testpoint(board, f"TP{index}", signal, (tp_x, resistor_pos[1]), nets)

    for reference, point in [("H1", (5.0, 5.0)), ("H2", (75.0, 5.0)), ("H3", (5.0, 80.0)), ("H4", (75.0, 80.0))]:
        add_mounting_hole(board, reference, point)

    routes: list[Route] = []
    def route(name, points, width, layer):
        track(board, nets[name], points, width, layer)
        routes.append(Route(name, width, "F.Cu" if layer == pcbnew.F_Cu else "B.Cu", points))

    # Parallel both bridges inside each AE-DRV8835-S. Alternating output pins use
    # opposite copper layers so the two output nets cannot collide.
    output_targets = {
        "A": (pos(jx_out, "1"), pos(jx_out, "2"), 6.0, 8.2),
        "B": (pos(jx_out, "3"), pos(jx_out, "4"), 6.0, 8.2),
        "C": (pos(jy_out, "1"), pos(jy_out, "2"), 74.0, 71.8),
        "D": (pos(jy_out, "3"), pos(jy_out, "4"), 74.0, 71.8),
    }
    for phase, fp in modules.items():
        plus_target, minus_target, plus_lane, minus_lane = output_targets[phase]
        plus_name, minus_name = f"{phase}+", f"{phase}-"
        p2, p4 = pos(fp, "2"), pos(fp, "4")
        p3, p5 = pos(fp, "3"), pos(fp, "5")
        route(plus_name, [p2, (plus_lane, p2[1])], 1.4, pcbnew.F_Cu)
        route(plus_name, [p4, (plus_lane, p4[1])], 1.4, pcbnew.F_Cu)
        route(plus_name, [(plus_lane, p2[1]), (plus_lane, p4[1]), (plus_lane, plus_target[1])], 3.0, pcbnew.F_Cu)
        route(plus_name, [(plus_lane, plus_target[1]), plus_target], 1.4, pcbnew.F_Cu)
        route(minus_name, [p3, (minus_lane, p3[1])], 1.4, pcbnew.B_Cu)
        route(minus_name, [p5, (minus_lane, p5[1])], 1.4, pcbnew.B_Cu)
        route(minus_name, [(minus_lane, p3[1]), (minus_lane, p5[1]), (minus_lane, minus_target[1])], 3.0, pcbnew.B_Cu)
        route(minus_name, [(minus_lane, minus_target[1]), minus_target], 1.4, pcbnew.B_Cu)

        in1, in2 = f"{phase}_IN1", f"{phase}_IN2"
        p10, p8, p9, p7 = pos(fp, "10"), pos(fp, "8"), pos(fp, "9"), pos(fp, "7")
        direction = 1.0 if p10[0] < 40 else -1.0
        route(in1, [p10, (p10[0] + direction * 2.2, p10[1]), (p10[0] + direction * 2.2, p8[1]), p8], 0.5, pcbnew.B_Cu)
        route(in2, [p9, (p9[0] + direction * 4.2, p9[1]), (p9[0] + direction * 4.2, p7[1]), p7], 0.5, pcbnew.F_Cu)

    # GPIO routes use four separate corridors per side. Pins at the antenna end
    # first escape outside the central RF keepout before travelling upward.
    gpio_routes = [
        ("A_IN1", "25", modules["A"], "10", [(43.0, 13.0), (43.0, 14.0), (25.0, 14.0), (25.0, 27.73)]),
        ("A_IN2", "26", modules["A"], "9", [(42.0, 15.0), (42.0, 16.0), (27.0, 16.0), (27.0, 30.27)]),
        ("B_IN1", "27", modules["B"], "10", [(41.0, 18.0), (41.0, 19.0), (23.0, 19.0), (23.0, 54.73)]),
        ("B_IN2", "29", modules["B"], "9", [(40.0, 22.0), (25.0, 22.0), (25.0, 57.27)]),
        ("C_IN1", "20", modules["C"], "10", [(67.0, 2.37), (67.0, 14.0), (85.0, 14.0), (85.0, 30.27)]),
        ("C_IN2", "21", modules["C"], "9", [(43.0, 2.37), (43.0, 52.0), (68.0, 52.0), (68.0, 18.0), (83.0, 18.0), (83.0, 27.73)]),
        ("D_IN1", "22", modules["D"], "10", [(43.0, 4.91), (43.0, 54.0), (70.0, 54.0), (70.0, 20.0), (87.0, 20.0), (87.0, 57.27)]),
        ("D_IN2", "24", modules["D"], "9", [(43.0, 9.99), (43.0, 56.0), (72.0, 56.0), (72.0, 22.0), (89.0, 22.0), (89.0, 54.73)]),
    ]
    for index, (name, pico_pad, fp, module_pad, waypoints) in enumerate(gpio_routes):
        source, destination = pos(pico, pico_pad), pos(fp, module_pad)
        layer = pcbnew.B_Cu if index % 2 == 0 else pcbnew.F_Cu
        route(name, [source, *waypoints, destination], 0.45, layer)

    # Connect local signal test points and pulldowns to the corresponding module input.
    for signal in signal_order:
        resistor = resistors[signal]
        testpoint = testpoints[signal]
        phase = signal[0]
        module_pad = "10" if signal.endswith("IN1") else "9"
        route(signal, [pos(modules[phase], module_pad), pos(testpoint, "1"), pos(resistor, "1")], 0.35, pcbnew.F_Cu)
        ground_via = (pos(resistor, "2")[0], pos(resistor, "2")[1] + 1.7)
        route("GND", [pos(resistor, "2"), ground_via], 0.4, pcbnew.F_Cu)
        via(board, nets["GND"], ground_via)

    # Logic supply trunk.
    route("+3V3", [pos(pico, "36"), (38.0, pos(pico, "36")[1]), (38.0, 67.0), (52.0, 67.0)], 0.8, pcbnew.F_Cu)
    for fp in modules.values():
        endpoint = pos(fp, "12")
        lane_x = 28.0 if endpoint[0] < 40 else 52.0
        route("+3V3", [(lane_x, 67.0), (lane_x, endpoint[1]), endpoint], 0.8, pcbnew.F_Cu)

    # 6 mm axis trunks and 3 mm branches. Both power connector pins share load.
    x_vm_junction = (6.77, 20.0)
    route("VM_X", [pos(jx_power, "1"), (5.5, 20.0), x_vm_junction], 1.4, pcbnew.F_Cu)
    route("VM_X", [pos(jx_power, "2"), (8.04, 20.0), x_vm_junction], 1.4, pcbnew.F_Cu)
    route("VM_X", [x_vm_junction, (32.0, 20.0), (32.0, 47.0)], 6.0, pcbnew.F_Cu)
    route("VM_X", [(32.0, 20.0), (9.0, 20.0), (9.0, pos(modules["A"], "1")[1]), pos(modules["A"], "1")], 1.4, pcbnew.F_Cu)
    route("VM_X", [(32.0, 47.0), (9.0, 47.0), (9.0, pos(modules["B"], "1")[1]), pos(modules["B"], "1")], 1.4, pcbnew.F_Cu)
    route("VM_X", [pos(cx, "1"), (32.0, 25.0)], 1.4, pcbnew.F_Cu)
    route("VM_X", [pos(c3, "1"), (32.0, 31.0)], 1.4, pcbnew.F_Cu)
    y_vm_junction = (73.23, 20.0)
    route("VM_Y", [pos(jy_power, "1"), (71.96, 20.0), y_vm_junction], 1.4, pcbnew.F_Cu)
    route("VM_Y", [pos(jy_power, "2"), (74.5, 20.0), y_vm_junction], 1.4, pcbnew.F_Cu)
    route("VM_Y", [y_vm_junction, (48.0, 20.0), (48.0, 65.0)], 6.0, pcbnew.F_Cu)
    route("VM_Y", [(48.0, 38.0), (71.0, 38.0), (71.0, pos(modules["C"], "1")[1]), pos(modules["C"], "1")], 1.4, pcbnew.F_Cu)
    route("VM_Y", [(48.0, 65.0), (71.0, 65.0), (71.0, pos(modules["D"], "1")[1]), pos(modules["D"], "1")], 1.4, pcbnew.F_Cu)
    route("VM_Y", [pos(cy, "1"), (48.0, 25.0)], 1.4, pcbnew.F_Cu)
    route("VM_Y", [pos(c4, "1"), (48.0, 31.0)], 1.4, pcbnew.F_Cu)

    # Wide, normally-open parallel solder links. Close both SJ1 and SJ2 when a
    # single supply is used for both axes.
    for jumper in [sj1, sj2]:
        route("VM_X", [(32.0, 47.0), (32.0, pos(jumper, "1")[1]), pos(jumper, "1")], 6.0, pcbnew.F_Cu)
        route("VM_Y", [(48.0, 65.0), (48.0, pos(jumper, "2")[1]), pos(jumper, "2")], 6.0, pcbnew.F_Cu)

    # Ground connector pins, capacitors and module pins enter the B.Cu plane.
    add_ground_zone(board, nets["GND"])

    text(board, "PICO 2 W / 4x AE-DRV8835-S", (40.0, 66.0), 1.1)
    text(board, "X PWR", (7.0, 10.8), 0.8)
    text(board, "VM", (11.0, 14.0), 0.8)
    text(board, "GND", (11.5, 19.0), 0.8)
    text(board, "Y PWR", (73.0, 10.8), 0.8)
    text(board, "VM", (69.0, 14.0), 0.8)
    text(board, "GND", (68.0, 19.0), 0.8)
    text(board, "X: A+ A- B+ B-", (5.5, 42.0), 0.8, angle=90.0)
    text(board, "Y: C+ C- D+ D-", (74.5, 42.0), 0.8, angle=90.0)
    text(board, "BRIDGE BOTH: VM_X = VM_Y", (40.0, 70.0), 0.8)
    text(board, "EXTERNAL SERIES RESISTORS REQUIRED", (40.0, 62.0), 0.85)
    text(board, "RF KEEP OUT", (40.0, 8.5), 0.8)

    output.parent.mkdir(parents=True, exist_ok=True)
    if not pcbnew.SaveBoard(str(output), board):
        raise OSError(f"Could not save {output}")
    manifest = {
        "board_mm": [BOARD_W, BOARD_H],
        "corner_radius_mm": CORNER_R,
        "layers": 2,
        "copper_oz": 1,
        "phase_trace_width_mm": 1.5,
        "axis_supply_width_mm": 2.0,
        "module": "Akizuki AE-DRV8835-S, 2.54 mm pitch, 7.62 mm row spacing",
        "pico_footprint": "KiCad 10 official RaspberryPi_Pico_W_SMD_HandSolder",
        "gpio": {"A": [19, 20], "B": [21, 22], "C": [15, 16], "D": [17, 18]},
        "connectors": {
            "J1": ["VM_X", "VM_X", "GND", "GND"],
            "J2": ["VM_Y", "VM_Y", "GND", "GND"],
            "J3": ["A+", "A-", "B+", "B-"],
            "J4": ["C+", "C-", "D+", "D-"],
        },
        "power_connector_geometry": {
            "pitch_within_row_mm": 2.54,
            "vm_to_gnd_row_gap_mm": 5.08,
            "pad_diameter_mm": 3.0,
            "plated_drill_mm": 1.2,
        },
        "testpoints": {
            "type": "front_smd",
            "size_mm": [1.8, 1.5],
            "labels": gpio_labels,
        },
        "solder_jumper_geometry": {
            "pad_size_mm": [5.0, 5.0],
            "vm_x_to_vm_y_gap_mm": 0.5,
            "bridge_method": "solder",
        },
        "routing": "Final copper is generated by autoroute_board.py and FreeRouting.",
    }
    output.with_name("routing-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=Path,
        default=BOARD_DIR / "pico2w_drv8835_control.kicad_pcb",
    )
    parser.add_argument(
        "--pico-library",
        type=Path,
        default=BOARD_DIR / "footprints" / "official-kicad",
    )
    args = parser.parse_args()
    generate(args.output.resolve(), args.pico_library.resolve())
    print(args.output)


if __name__ == "__main__":
    main()
