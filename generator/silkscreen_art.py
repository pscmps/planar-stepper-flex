#!/usr/bin/env python3
"""Extract and apply the hand-drawn KiCad silkscreen footprint."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


ART_NAME = "Hand_Drawn_Planar_Stepper"


def _balanced_block(text: str, start: int) -> tuple[str, int]:
    depth = 0
    quoted = False
    escaped = False
    for index in range(start, len(text)):
        character = text[index]
        if quoted:
            if escaped:
                escaped = False
            elif character == "\\":
                escaped = True
            elif character == '"':
                quoted = False
            continue
        if character == '"':
            quoted = True
        elif character == "(":
            depth += 1
        elif character == ")":
            depth -= 1
            if depth == 0:
                return text[start : index + 1], index + 1
    raise ValueError("Unbalanced KiCad s-expression")


def extract_art(board_path: Path, output_path: Path) -> None:
    text = board_path.read_text(encoding="utf-8")
    start = text.find('(footprint "LOGO"')
    if start < 0:
        start = text.find(f'(footprint "{ART_NAME}"')
    if start < 0:
        raise ValueError("Embedded LOGO footprint was not found")
    block, _ = _balanced_block(text, start)
    block = re.sub(r'^\(footprint "[^"]+"', f'(footprint "{ART_NAME}"', block, count=1)

    reference_start = block.find('(property "Reference"')
    if reference_start >= 0:
        reference, reference_end = _balanced_block(block, reference_start)
        if "(hide yes)" not in reference:
            reference = reference.replace(
                '(layer "F.SilkS")', '(layer "F.SilkS")\n\t\t\t(hide yes)', 1
            )
            block = block[:reference_start] + reference + block[reference_end:]

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(block + "\n", encoding="utf-8", newline="\n")


def apply_art(board_path: Path, art_path: Path, at_x: float, at_y: float) -> None:
    board = board_path.read_text(encoding="utf-8")
    if f'(footprint "{ART_NAME}"' in board:
        raise ValueError("The target board already contains the hand-drawn artwork")
    art = art_path.read_text(encoding="utf-8").strip()
    art = re.sub(
        r'\(at\s+[-+0-9.]+\s+[-+0-9.]+(?:\s+[-+0-9.]+)?\)',
        f'(at {at_x:g} {at_y:g})',
        art,
        count=1,
    )
    marker = board.find("\n\t(footprint ")
    if marker < 0:
        marker = board.find("\n\t(gr_")
    if marker < 0:
        raise ValueError("No top-level insertion point found in target board")
    board_path.write_text(
        board[: marker + 1] + "\t" + art.replace("\n", "\n\t") + "\n" + board[marker + 1 :],
        encoding="utf-8",
        newline="\n",
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    extract = subparsers.add_parser("extract")
    extract.add_argument("board", type=Path)
    extract.add_argument("output", type=Path)
    apply = subparsers.add_parser("apply")
    apply.add_argument("board", type=Path)
    apply.add_argument("art", type=Path)
    apply.add_argument("--at-x", type=float, default=109.5)
    apply.add_argument("--at-y", type=float, default=74.5)
    args = parser.parse_args()
    if args.command == "extract":
        extract_art(args.board, args.output)
    else:
        apply_art(args.board, args.art, args.at_x, args.at_y)


if __name__ == "__main__":
    main()
