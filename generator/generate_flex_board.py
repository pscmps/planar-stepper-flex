#!/usr/bin/env python3
"""Generate the public planar-stepper FPC designs with KiCad 10 Python."""

from __future__ import annotations

import argparse
from dataclasses import replace
from pathlib import Path

from planar_stepper.generator import generate_board_file
from planar_stepper.geometry import BoardConfig, four_card_flex_config
from silkscreen_art import apply_art


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ART = Path(__file__).resolve().parent / "assets" / "hand-drawn-silkscreen.kicad_mod"


def build_config(args: argparse.Namespace) -> BoardConfig:
    config = four_card_flex_config() if args.preset == "4cards" else BoardConfig()
    custom_area = args.active_width is not None or args.active_height is not None

    active_width = args.active_width if args.active_width is not None else config.motion_width
    active_height = args.active_height if args.active_height is not None else config.motion_height
    if custom_area or args.board_width is not None or args.board_height is not None:
        board_width = args.board_width if args.board_width is not None else active_width + 40.0
        board_height = args.board_height if args.board_height is not None else active_height + 40.0
        config = replace(
            config,
            board_width=board_width,
            board_height=board_height,
            layout_width=None,
            layout_height=None,
            motion_width=active_width,
            motion_height=active_height,
            header_x=board_width - 5.0,
            header_y=board_height - 5.0,
            surface_terminal_lands=False,
            terminal_connection_width=0.5,
        )

    overrides = {
        "coil_pitch": args.pitch,
        "trace_width": args.trace_width,
        "trace_spacing": args.trace_spacing,
        "phase_offset": args.phase_offset,
    }
    config = replace(
        config,
        **{name: value for name, value in overrides.items() if value is not None},
    )
    config.validate()
    return config


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="KiCad 10 XY planar-stepper FPC generator"
    )
    parser.add_argument("--preset", choices=("50mm", "4cards"), default="50mm")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--active-width", "--motion-x", type=float, dest="active_width")
    parser.add_argument("--active-height", "--motion-y", type=float, dest="active_height")
    parser.add_argument("--board-width", type=float)
    parser.add_argument("--board-height", type=float)
    parser.add_argument("--pitch", type=float)
    parser.add_argument("--trace-width", "--width", type=float, dest="trace_width")
    parser.add_argument("--trace-spacing", "--spacing", type=float, dest="trace_spacing")
    parser.add_argument("--phase-offset", "--offset", type=float, dest="phase_offset")
    parser.add_argument(
        "--with-art",
        action="store_true",
        help="Add the hand-drawn front-silkscreen artwork to the unchanged 4cards preset",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    config = build_config(args)
    output = args.output or ROOT / "build" / f"flex-{args.preset}.kicad_pcb"

    if args.with_art:
        is_stock_four_cards = (
            args.preset == "4cards"
            and args.active_width is None
            and args.active_height is None
            and args.board_width is None
            and args.board_height is None
        )
        if not is_stock_four_cards:
            raise SystemExit("--with-art is only valid with the unchanged 4cards preset")

    output.parent.mkdir(parents=True, exist_ok=True)
    generate_board_file(output, config)
    if args.with_art:
        apply_art(output, DEFAULT_ART, at_x=109.5, at_y=74.5)
    print(f"Generated: {output}")


if __name__ == "__main__":
    main()
