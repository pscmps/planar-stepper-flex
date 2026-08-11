#!/usr/bin/env python3
"""Render an SVG preview of the generated planar stepper routing."""

from __future__ import annotations

import argparse
import math
import sys
from dataclasses import replace
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from planar_stepper.geometry import (
    BoardConfig,
    Point,
    build_layout,
    format_report,
    four_card_flex_config,
)


SCALE = 8.0


def _svg_point(point: Point) -> tuple[float, float]:
    return point.x * SCALE, point.y * SCALE


def _line(start: Point, end: Point, color: str, width: float, opacity: float = 1.0, dash: str = "") -> str:
    x1, y1 = _svg_point(start)
    x2, y2 = _svg_point(end)
    dash_attr = f' stroke-dasharray="{dash}"' if dash else ""
    return (
        f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" '
        f'stroke="{color}" stroke-width="{width * SCALE:.2f}" stroke-linecap="round" '
        f'stroke-linejoin="round" opacity="{opacity:.2f}"{dash_attr}/>'
    )


def _circle(center: Point, radius: float, fill: str, stroke: str = "#202020") -> str:
    x, y = _svg_point(center)
    return (
        f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{radius * SCALE:.2f}" '
        f'fill="{fill}" stroke="{stroke}" stroke-width="{0.12 * SCALE:.2f}"/>'
    )


def render_svg(path: Path, config: BoardConfig) -> str:
    layout = build_layout(config)
    width = config.board_width * SCALE
    height = config.board_height * SCALE
    tl = layout.motion_top_left
    br = layout.motion_bottom_right

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width:.0f}" height="{height:.0f}" viewBox="0 0 {width:.0f} {height:.0f}">',
        '<rect width="100%" height="100%" fill="#f7f5ef"/>',
        f'<rect x="{tl.x * SCALE:.2f}" y="{tl.y * SCALE:.2f}" width="{(br.x - tl.x) * SCALE:.2f}" height="{(br.y - tl.y) * SCALE:.2f}" fill="#ffffff" stroke="#777" stroke-width="{0.15 * SCALE:.2f}"/>',
    ]

    colors = {"A": "#d44f35", "B": "#2f6fb7", "C": "#2b9a66", "D": "#8b57b5"}

    # Draw the back copper first as a narrow dashed centreline.  This is a
    # topology diagram, so keeping it narrower than the physical copper makes
    # front/back crossings legible instead of looking like accidental shorts.
    for phase in layout.phases:
        for routed in phase.segments:
            if routed.copper_layer == "B.Cu" and routed not in phase.active_segments:
                parts.append(
                    _line(
                        routed.segment.start,
                        routed.segment.end,
                        colors[phase.name],
                        0.45,
                        opacity=0.85,
                        dash="5 5",
                    )
                )
    for phase in layout.phases:
        for routed in phase.segments:
            if routed.copper_layer == "F.Cu" or routed in phase.active_segments:
                parts.append(
                    _line(
                        routed.segment.start,
                        routed.segment.end,
                        colors[phase.name],
                        config.trace_width,
                    )
                )
    for phase in layout.phases:
        for point in phase.via_points:
            parts.append(_circle(point, 0.38, colors[phase.name]))

    for pin in layout.header_pins:
        parts.append(_circle(pin.position, 0.55, "#ffffff"))
        x, y = _svg_point(Point(pin.position.x - 4.2, pin.position.y + 0.35))
        parts.append(f'<text x="{x:.2f}" y="{y:.2f}" font-size="10" font-family="Arial" fill="#202020">{pin.label}</text>')

    parts.append(
        '<text x="16" y="24" font-size="14" font-family="Arial" fill="#202020">'
        'A/B vertical on F.Cu; C/D horizontal on B.Cu; B.Cu outer routes dashed</text>'
    )
    parts.append("</svg>")
    path.write_text("\n".join(parts), encoding="utf-8", newline="\n")
    return format_report(layout)


def _draw_dashed_line(draw, start: Point, end: Point, color: str, width: int) -> None:
    """Draw a dashed segment in preview pixels."""

    x1, y1 = _svg_point(start)
    x2, y2 = _svg_point(end)
    length = math.hypot(x2 - x1, y2 - y1)
    if length == 0:
        return
    dash, gap = 10.0, 7.0
    position = 0.0
    while position < length:
        finish = min(position + dash, length)
        start_ratio = position / length
        end_ratio = finish / length
        draw.line(
            (
                x1 + (x2 - x1) * start_ratio,
                y1 + (y2 - y1) * start_ratio,
                x1 + (x2 - x1) * end_ratio,
                y1 + (y2 - y1) * end_ratio,
            ),
            fill=color,
            width=width,
        )
        position += dash + gap


def render_png(path: Path, config: BoardConfig) -> None:
    """Render the same phase-colored view as a mobile-friendly PNG."""

    from PIL import Image, ImageDraw

    layout = build_layout(config)
    image = Image.new(
        "RGB",
        (round(config.board_width * SCALE), round(config.board_height * SCALE)),
        "#f7f5ef",
    )
    draw = ImageDraw.Draw(image)
    tl = layout.motion_top_left
    br = layout.motion_bottom_right
    draw.rectangle(
        (*_svg_point(tl), *_svg_point(br)),
        fill="#ffffff",
        outline="#777777",
        width=1,
    )

    colors = {"A": "#d44f35", "B": "#2f6fb7", "C": "#2b9a66", "D": "#8b57b5"}
    trace_pixels = max(1, round(config.trace_width * SCALE))
    back_trace_pixels = max(2, round(0.45 * SCALE))

    # Bottom copper is a thin dashed centreline in this explanatory preview.
    for phase in layout.phases:
        color = colors[phase.name]
        for routed in phase.segments:
            if routed.copper_layer == "B.Cu" and routed not in phase.active_segments:
                _draw_dashed_line(
                    draw,
                    routed.segment.start,
                    routed.segment.end,
                    color,
                    back_trace_pixels,
                )

    # Front copper is drawn at its actual scaled width over the back layer.
    for phase in layout.phases:
        color = colors[phase.name]
        for routed in phase.segments:
            if routed.copper_layer == "F.Cu" or routed in phase.active_segments:
                draw.line(
                    (*_svg_point(routed.segment.start), *_svg_point(routed.segment.end)),
                    fill=color,
                    width=trace_pixels,
                    joint="curve",
                )
    for phase in layout.phases:
        color = colors[phase.name]
        for point in phase.via_points:
            x, y = _svg_point(point)
            radius = config.via_diameter * SCALE / 2.0
            draw.ellipse(
                (x - radius, y - radius, x + radius, y + radius),
                fill=color,
                outline="#202020",
                width=1,
            )

    image.save(path, format="PNG", optimize=True)


def main() -> None:
    parser = argparse.ArgumentParser(description="Render SVG preview from pure geometry")
    parser.add_argument("output", nargs="?")
    parser.add_argument(
        "--preset",
        choices=("50mm", "4cards"),
        default="50mm",
    )
    parser.add_argument("--pitch", type=float)
    parser.add_argument("--width", type=float, dest="trace_width")
    parser.add_argument("--spacing", type=float, dest="trace_spacing")
    parser.add_argument("--offset", type=float, dest="phase_offset")
    parser.add_argument("--motion-x", type=float, dest="motion_width")
    parser.add_argument("--motion-y", type=float, dest="motion_height")
    args = parser.parse_args()
    config = four_card_flex_config() if args.preset == "4cards" else BoardConfig()
    overrides = {
        "coil_pitch": args.pitch,
        "trace_width": args.trace_width,
        "trace_spacing": args.trace_spacing,
        "phase_offset": args.phase_offset,
        "motion_width": args.motion_width,
        "motion_height": args.motion_height,
    }
    config = replace(
        config,
        **{name: value for name, value in overrides.items() if value is not None},
    )
    output = Path(
        args.output
        or (
            "build/flex-4cards-overlay.svg"
            if args.preset == "4cards"
            else "build/flex-50mm-overlay.svg"
        )
    )
    print(render_svg(output, config))
    render_png(output.with_suffix(".png"), config)
    print(f"  Saved: {output.resolve()}")
    print(f"  Saved: {output.with_suffix('.png').resolve()}")


if __name__ == "__main__":
    main()
