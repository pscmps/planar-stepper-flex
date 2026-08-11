#!/usr/bin/env python3
"""Prepare a FreeRouting DSN and import its SES into the KiCad board."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

import pcbnew


def tokenize(source: str) -> list[str]:
    return re.findall(r'"(?:[^"\\]|\\.)*"|\(|\)|[^\s()]+', source)


def parse_sexpr(source: str):
    tokens = tokenize(source)
    index = 0

    def parse_item():
        nonlocal index
        token = tokens[index]
        index += 1
        if token != "(":
            return token[1:-1] if token.startswith('"') else token
        result = []
        while tokens[index] != ")":
            result.append(parse_item())
        index += 1
        return result

    result = parse_item()
    if index != len(tokens):
        raise ValueError("Unexpected trailing SES tokens")
    return result


def remove_routing(board: pcbnew.BOARD) -> None:
    for item in list(board.GetTracks()):
        if not item.IsLocked():
            board.RemoveNative(item)
    while board.Zones():
        board.RemoveNative(next(iter(board.Zones())))


def configure_netclasses(board: pcbnew.BOARD, logic_clearance: float = 0.25,
                         logic_width: float = 0.40,
                         ground_clearance: float = 0.25) -> None:
    settings = board.GetDesignSettings().m_NetSettings
    definitions = {
        "Logic": (logic_width, logic_clearance, 1.0, 0.5),
        # 1.5 mm on external 1 oz copper is the 3 A phase-route target. Wider
        # axis supply copper is added after routing where pin pitch permits it.
        "Phase3A": (1.50, 0.25, 1.4, 0.7),
        "AxisPower": (2.00, 0.30, 1.8, 0.9),
        "Ground": (1.00, ground_clearance, 1.4, 0.7),
    }
    for name, (width, clearance, via_diameter, via_drill) in definitions.items():
        netclass = pcbnew.NETCLASS(name)
        netclass.SetTrackWidth(pcbnew.FromMM(width))
        netclass.SetClearance(pcbnew.FromMM(clearance))
        netclass.SetViaDiameter(pcbnew.FromMM(via_diameter))
        netclass.SetViaDrill(pcbnew.FromMM(via_drill))
        settings.SetNetclass(name, netclass)
    for name in ["A+", "A-", "B+", "B-", "C+", "C-", "D+", "D-"]:
        settings.SetNetclassPatternAssignment(name, "Phase3A")
    for name in ["VM_X", "VM_Y"]:
        settings.SetNetclassPatternAssignment(name, "AxisPower")
    settings.SetNetclassPatternAssignment("GND", "Ground")
    for name in [
        "+3V3", "VCC", "A_IN1", "A_IN2", "B_IN1", "B_IN2",
        "C_IN1", "C_IN2", "D_IN1", "D_IN2",
        "G0_SERVO", "G2", "G5", "G12", "G13", "G15", "G16", "G17",
        "G26",
    ]:
        settings.SetNetclassPatternAssignment(name, "Logic")
    settings.RecomputeEffectiveNetclasses()


def prepare(board_path: Path, output_board: Path, dsn_path: Path,
            logic_clearance: float = 0.25, logic_width: float = 0.40,
            ground_clearance: float = 0.25) -> None:
    board = pcbnew.LoadBoard(str(board_path))
    remove_routing(board)
    configure_netclasses(board, logic_clearance, logic_width, ground_clearance)
    output_board.parent.mkdir(parents=True, exist_ok=True)
    if not pcbnew.SaveBoard(str(output_board), board):
        raise OSError(f"Could not save {output_board}")
    if not pcbnew.ExportSpecctraDSN(board, str(dsn_path)):
        raise OSError(f"Could not export {dsn_path}")


def find_child(item, name):
    for child in item:
        if isinstance(child, list) and child and child[0] == name:
            return child
    raise KeyError(name)


def add_track(board, target_net, layer_name, width_raw, points):
    layer_map = {
        "F.Cu": pcbnew.F_Cu,
        "In1.Cu": pcbnew.In1_Cu,
        "In2.Cu": pcbnew.In2_Cu,
        "B.Cu": pcbnew.B_Cu,
    }
    if layer_name not in layer_map:
        raise ValueError(f"Unsupported SES copper layer: {layer_name}")
    layer = layer_map[layer_name]
    width = pcbnew.FromMM(float(width_raw) / 10000.0)
    converted = [(float(x) / 10000.0, -float(y) / 10000.0) for x, y in zip(points[0::2], points[1::2])]
    for start, end in zip(converted, converted[1:]):
        track = pcbnew.PCB_TRACK(board)
        track.SetStart(pcbnew.VECTOR2I(pcbnew.FromMM(start[0]), pcbnew.FromMM(start[1])))
        track.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(end[0]), pcbnew.FromMM(end[1])))
        track.SetWidth(width)
        track.SetLayer(layer)
        track.SetNet(target_net)
        board.Add(track)


def add_via(board, target_net, padstack, x_raw, y_raw):
    match = re.search(r"_(\d+):(\d+)_um$", padstack)
    if match is None:
        raise ValueError(f"Unknown via padstack {padstack}")
    diameter, drill = (float(value) / 1000.0 for value in match.groups())
    via = pcbnew.PCB_VIA(board)
    via.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(float(x_raw) / 10000.0), pcbnew.FromMM(-float(y_raw) / 10000.0)))
    via.SetWidth(pcbnew.FromMM(diameter))
    via.SetDrill(pcbnew.FromMM(drill))
    via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    via.SetNet(target_net)
    board.Add(via)


def add_ground_zone(
    board: pcbnew.BOARD,
    minimum_y_mm: float,
    edge_inset_mm: float,
    remove_islands: bool,
    both_layers: bool,
) -> None:
    ground = board.FindNet("GND")
    if ground is None:
        raise KeyError("GND net not found")
    bounds = board.GetBoardEdgesBoundingBox()
    left = pcbnew.ToMM(bounds.GetLeft()) + edge_inset_mm
    right = pcbnew.ToMM(bounds.GetRight()) - edge_inset_mm
    bottom = pcbnew.ToMM(bounds.GetBottom()) - edge_inset_mm
    layers = (pcbnew.F_Cu, pcbnew.B_Cu) if both_layers else (pcbnew.B_Cu,)
    for layer in layers:
        zone = pcbnew.ZONE(board)
        zone.SetLayer(layer)
        zone.SetNet(ground)
        zone.SetLocalClearance(pcbnew.FromMM(0.30))
        zone.SetMinThickness(pcbnew.FromMM(0.25))
        zone.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
        if remove_islands:
            zone.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        outline = zone.Outline()
        outline.NewOutline()
        for x, y in [
            (left, minimum_y_mm),
            (right, minimum_y_mm),
            (right, bottom),
            (left, bottom),
        ]:
            outline.Append(pcbnew.FromMM(x), pcbnew.FromMM(y))
        board.Add(zone)


def import_ses(
    input_board: Path,
    ses_path: Path,
    output_board: Path,
    ground_zone_minimum_y_mm: float,
    ground_zone_edge_inset_mm: float,
    remove_ground_islands: bool,
    ground_zone_both_layers: bool,
) -> None:
    board = pcbnew.LoadBoard(str(input_board))
    root = parse_sexpr(ses_path.read_text(encoding="utf-8"))
    routes = find_child(root, "routes")
    network = find_child(routes, "network_out")
    imported_wires = 0
    imported_vias = 0
    for net_item in network[1:]:
        if not isinstance(net_item, list) or not net_item or net_item[0] != "net":
            continue
        net_name = net_item[1]
        target_net = board.FindNet(net_name)
        if target_net is None:
            raise KeyError(f"SES net not present in board: {net_name}")
        for item in net_item[2:]:
            if not isinstance(item, list) or not item:
                continue
            if item[0] == "wire":
                path = find_child(item, "path")
                add_track(board, target_net, path[1], path[2], path[3:])
                imported_wires += 1
            elif item[0] == "via":
                add_via(board, target_net, item[1], item[2], item[3])
                imported_vias += 1
    if imported_wires == 0:
        raise ValueError("SES contains no routed wires")
    add_ground_zone(
        board,
        ground_zone_minimum_y_mm,
        ground_zone_edge_inset_mm,
        remove_ground_islands,
        ground_zone_both_layers,
    )
    if not pcbnew.SaveBoard(str(output_board), board):
        raise OSError(f"Could not save {output_board}")
    input_project = input_board.with_suffix(".kicad_pro")
    output_project = output_board.with_suffix(".kicad_pro")
    if input_project.exists():
        project = json.loads(input_project.read_text(encoding="utf-8"))
        project.setdefault("meta", {})["filename"] = output_project.name
        output_project.write_text(json.dumps(project, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    manifest_path = output_board.with_name("routing-manifest.json")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
    widths: dict[str, set[float]] = {}
    track_count = 0
    via_count = 0
    for item in board.GetTracks():
        if isinstance(item, pcbnew.PCB_VIA):
            via_count += 1
            continue
        track_count += 1
        widths.setdefault(item.GetNetname(), set()).add(round(pcbnew.ToMM(item.GetWidth()), 3))
    manifest["routing_result"] = {
        "engine": "FreeRouting 2.2.4",
        "track_segments": track_count,
        "vias": via_count,
        "ground_zones": sum(1 for zone in board.Zones() if zone.GetNetname() == "GND"),
        "net_widths_mm": {name: sorted(values) for name, values in sorted(widths.items())},
    }
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Imported {imported_wires} wires and {imported_vias} vias")


def main() -> None:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    prep = subparsers.add_parser("prepare")
    prep.add_argument("board", type=Path)
    prep.add_argument("output_board", type=Path)
    prep.add_argument("dsn", type=Path)
    prep.add_argument("--logic-clearance", type=float, default=0.25)
    prep.add_argument("--logic-width", type=float, default=0.40)
    prep.add_argument("--ground-clearance", type=float, default=0.25)
    imp = subparsers.add_parser("import")
    imp.add_argument("board", type=Path)
    imp.add_argument("ses", type=Path)
    imp.add_argument("output_board", type=Path)
    imp.add_argument("--ground-zone-min-y", type=float, default=12.0)
    imp.add_argument("--ground-zone-edge-inset", type=float, default=0.625)
    imp.add_argument("--remove-ground-islands", action="store_true")
    imp.add_argument("--ground-zone-both-layers", action="store_true")
    args = parser.parse_args()
    if args.command == "prepare":
        prepare(args.board.resolve(), args.output_board.resolve(), args.dsn.resolve(),
                args.logic_clearance, args.logic_width, args.ground_clearance)
    else:
        import_ses(
            args.board.resolve(),
            args.ses.resolve(),
            args.output_board.resolve(),
            args.ground_zone_min_y,
            args.ground_zone_edge_inset,
            args.remove_ground_islands,
            args.ground_zone_both_layers,
        )


if __name__ == "__main__":
    main()
