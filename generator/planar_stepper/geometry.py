"""Pure geometry and electrical estimates for the two-axis coil board.

A/B form the X-axis field with vertical active conductors on F.Cu.  C/D are
the same pattern rotated 90 degrees on B.Cu.  The layer-switch vias for B and
D sit just outside the 50 mm active square; this is essential because a via
inside the square would short the perpendicular phase on the opposite layer.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from math import ceil, floor, hypot


COPPER_RESISTIVITY_OHM_M = 1.724e-8
ONE_OUNCE_COPPER_MM = 0.035
FOUR_CARD_MOTION_WIDTH_MM = 182.5
FOUR_CARD_MOTION_HEIGHT_MM = 110.0
FOUR_CARD_BOARD_WIDTH_MM = 229.0
FOUR_CARD_BOARD_HEIGHT_MM = 156.5


@dataclass(frozen=True)
class Point:
    x: float
    y: float


@dataclass(frozen=True)
class Segment:
    start: Point
    end: Point

    @property
    def length_mm(self) -> float:
        return hypot(self.end.x - self.start.x, self.end.y - self.start.y)


@dataclass(frozen=True)
class LayeredSegment:
    segment: Segment
    copper_layer: str

    @property
    def length_mm(self) -> float:
        return self.segment.length_mm


@dataclass(frozen=True)
class BoardConfig:
    """All dimensions are millimetres."""

    board_width: float = 90.0
    board_height: float = 90.0
    layout_width: float | None = None
    layout_height: float | None = None
    board_thickness: float = 1.6
    motion_width: float = 50.0
    motion_height: float = 50.0
    coil_pitch: float = 5.0
    trace_width: float = 1.8
    trace_spacing: float = 0.50
    phase_offset: float = 2.50
    copper_thickness: float = ONE_OUNCE_COPPER_MM
    return_offset_a: float = 5.00
    switched_via_escape: float = 2.50
    via_diameter: float = 2.0
    via_drill: float = 1.0
    mounting_hole_diameter: float = 3.2
    mounting_hole_inset: float = 4.0
    header_x: float = 85.0
    header_y: float = 85.0
    header_pitch: float = 2.54
    surface_terminal_lands: bool = False
    terminal_group_gap: float = 10.00
    terminal_land_along: float = 1.60
    terminal_land_across: float = 2.00
    terminal_connection_width: float = 0.50

    @property
    def routing_width(self) -> float:
        return self.board_width if self.layout_width is None else self.layout_width

    @property
    def routing_height(self) -> float:
        return self.board_height if self.layout_height is None else self.layout_height

    def validate(self) -> None:
        if min(
            self.board_width,
            self.board_height,
            self.board_thickness,
            self.motion_width,
            self.motion_height,
        ) <= 0:
            raise ValueError("Board and active-area dimensions must be positive")
        if min(self.routing_width, self.routing_height) <= 0:
            raise ValueError("Routing dimensions must be positive")
        if self.routing_width > self.board_width or self.routing_height > self.board_height:
            raise ValueError("Routing dimensions must fit inside the physical board")
        if self.trace_width <= 0 or self.trace_spacing < 0:
            raise ValueError("Trace width must be positive and spacing must be non-negative")
        if self.coil_pitch <= 0:
            raise ValueError("Phase pitch must be positive")
        required_gap = self.trace_width + self.trace_spacing
        if self.phase_offset < required_gap or self.coil_pitch - self.phase_offset < required_gap:
            raise ValueError(
                "Interleaved A/B spacing requires both phase offset and "
                "(phase pitch - offset) to be at least trace width + spacing"
            )
        if self.motion_width < self.coil_pitch:
            raise ValueError("Active width must fit at least two conductors per phase")
        vertical_margin = (self.routing_height - self.motion_height) / 2.0
        horizontal_margin = (self.routing_width - self.motion_width) / 2.0
        required_margin = 10.0 + self.trace_width / 2.0
        if vertical_margin < required_margin or horizontal_margin < required_margin:
            raise ValueError("Board needs at least 10 mm routing margin around the active square")
        if not 0 < self.switched_via_escape < self.return_offset_a:
            raise ValueError("Layer-switch vias must sit before the same-layer return corridor")
        if not (0 < self.via_drill < self.via_diameter):
            raise ValueError("Via drill must be positive and smaller than via diameter")
        if self.via_diameter > self.phase_offset:
            raise ValueError("Via diameter must not exceed the A/B phase offset")
        if self.copper_thickness <= 0:
            raise ValueError("Copper thickness must be positive")
        if self.surface_terminal_lands:
            if min(
                self.terminal_group_gap,
                self.terminal_land_along,
                self.terminal_land_across,
                self.terminal_connection_width,
            ) <= 0:
                raise ValueError("Surface terminal dimensions must be positive")
            if self.terminal_land_along + self.trace_spacing > self.header_pitch:
                raise ValueError(
                    "Surface terminal lands need at least the configured clearance"
                )
            half_along = self.terminal_land_along / 2.0
            half_across = self.terminal_land_across / 2.0
            group_shift = 3.0 * self.header_pitch + self.terminal_group_gap
            header_y0 = self.routing_height / 2.0 - 1.5 * self.header_pitch
            header_x0 = self.routing_width / 2.0 - 1.5 * self.header_pitch
            if not (
                half_across <= self.header_x <= self.board_width - half_across
                and half_across <= self.header_y <= self.board_height - half_across
                and header_y0 - group_shift - half_along >= 0.0
                and header_y0
                + 3.0 * self.header_pitch
                + group_shift
                + half_along
                <= self.board_height
                and header_x0 - group_shift - half_along >= 0.0
                and header_x0
                + 3.0 * self.header_pitch
                + group_shift
                + half_along
                <= self.board_width
            ):
                raise ValueError("Surface terminal groups must stay inside the board")


def four_card_flex_config() -> BoardConfig:
    """Return the large FPC preset matching the four-card routing jig."""

    return BoardConfig(
        board_width=FOUR_CARD_BOARD_WIDTH_MM,
        board_height=FOUR_CARD_BOARD_HEIGHT_MM,
        layout_width=222.5,
        layout_height=150.0,
        board_thickness=0.2,
        motion_width=FOUR_CARD_MOTION_WIDTH_MM,
        motion_height=FOUR_CARD_MOTION_HEIGHT_MM,
        header_x=217.5,
        header_y=145.0,
        surface_terminal_lands=True,
        terminal_connection_width=1.8,
    )


@dataclass
class PhaseGeometry:
    name: str
    net_name: str
    pin_names: tuple[str, str]
    active_segments: list[LayeredSegment] = field(default_factory=list)
    return_segments: list[LayeredSegment] = field(default_factory=list)
    lead_segments: list[LayeredSegment] = field(default_factory=list)
    via_points: list[Point] = field(default_factory=list)

    @property
    def segments(self) -> list[LayeredSegment]:
        return [*self.active_segments, *self.return_segments, *self.lead_segments]

    @property
    def length_mm(self) -> float:
        # Via barrel resistance is intentionally omitted; track copper dominates
        # this first-order 1 oz estimate.
        return sum(segment.length_mm for segment in self.segments)

    def resistance_ohm(self, config: BoardConfig) -> float:
        length_m = self.length_mm / 1000.0
        area_m2 = config.trace_width * 1e-3 * config.copper_thickness * 1e-3
        return COPPER_RESISTIVITY_OHM_M * length_m / area_m2

    def active_area_mm2(self, config: BoardConfig) -> float:
        points = [
            point
            for layered in self.active_segments
            for point in (layered.segment.start, layered.segment.end)
        ]
        width = max(p.x for p in points) - min(p.x for p in points) + config.trace_width
        height = max(p.y for p in points) - min(p.y for p in points) + config.trace_width
        return width * height

@dataclass(frozen=True)
class HeaderPin:
    number: str
    label: str
    net_name: str
    position: Point


@dataclass
class LayoutGeometry:
    config: BoardConfig
    phases: list[PhaseGeometry]
    header_pins: list[HeaderPin]
    motion_top_left: Point
    motion_bottom_right: Point

    @property
    def active_envelope_area_mm2(self) -> float:
        return self.config.motion_width * self.config.motion_height


def _polyline(points: list[Point], layer: str) -> list[LayeredSegment]:
    return [
        LayeredSegment(Segment(start, end), layer)
        for start, end in zip(points, points[1:])
        if start != end
    ]


def _active_strokes(xs: list[float], y_top: float, y_bottom: float) -> list[LayeredSegment]:
    return [
        LayeredSegment(Segment(Point(x, y_top), Point(x, y_bottom)), "F.Cu")
        for x in xs
    ]


def _active_strokes_horizontal(
    ys: list[float], x_left: float, x_right: float
) -> list[LayeredSegment]:
    return [
        LayeredSegment(Segment(Point(x_left, y), Point(x_right, y)), "B.Cu")
        for y in ys
    ]


def _returns(
    xs: list[float],
    y_top: float,
    y_bottom: float,
    offset: float,
    layer: str,
) -> list[LayeredSegment]:
    """Connect adjacent strokes, alternating bottom and top U-turns."""

    segments: list[LayeredSegment] = []
    for index, (x1, x2) in enumerate(zip(xs, xs[1:])):
        boundary_y = y_bottom if index % 2 == 0 else y_top
        return_y = boundary_y + offset if index % 2 == 0 else boundary_y - offset
        segments.extend(
            _polyline(
                [Point(x1, boundary_y), Point(x1, return_y), Point(x2, return_y), Point(x2, boundary_y)],
                layer,
            )
        )
    return segments


def _returns_horizontal(
    ys: list[float],
    x_left: float,
    x_right: float,
    offset: float,
    layer: str,
) -> list[LayeredSegment]:
    """Connect adjacent horizontal strokes, alternating right and left turns."""

    segments: list[LayeredSegment] = []
    for index, (y1, y2) in enumerate(zip(ys, ys[1:])):
        boundary_x = x_right if index % 2 == 0 else x_left
        return_x = boundary_x + offset if index % 2 == 0 else boundary_x - offset
        segments.extend(
            _polyline(
                [Point(boundary_x, y1), Point(return_x, y1), Point(return_x, y2), Point(boundary_x, y2)],
                layer,
            )
        )
    return segments


def _switched_vertical_returns(
    xs: list[float], y_top: float, y_bottom: float, escape: float
) -> tuple[list[LayeredSegment], list[Point]]:
    """Escape F.Cu endpoints, then connect alternating pairs on B.Cu."""

    vias = [Point(x, y) for x in xs for y in (y_top - escape, y_bottom + escape)]
    segments: list[LayeredSegment] = []
    for x in xs:
        segments.extend(
            _polyline([Point(x, y_top), Point(x, y_top - escape)], "F.Cu")
        )
        segments.extend(
            _polyline([Point(x, y_bottom), Point(x, y_bottom + escape)], "F.Cu")
        )
    for index, (x1, x2) in enumerate(zip(xs, xs[1:])):
        y = y_bottom + escape if index % 2 == 0 else y_top - escape
        segments.extend(_polyline([Point(x1, y), Point(x2, y)], "B.Cu"))
    return segments, vias


def _switched_horizontal_returns(
    ys: list[float], x_left: float, x_right: float, escape: float
) -> tuple[list[LayeredSegment], list[Point]]:
    """Escape B.Cu endpoints, then connect alternating pairs on F.Cu."""

    vias = [Point(x, y) for y in ys for x in (x_left - escape, x_right + escape)]
    segments: list[LayeredSegment] = []
    for y in ys:
        segments.extend(
            _polyline([Point(x_left, y), Point(x_left - escape, y)], "B.Cu")
        )
        segments.extend(
            _polyline([Point(x_right, y), Point(x_right + escape, y)], "B.Cu")
        )
    for index, (y1, y2) in enumerate(zip(ys, ys[1:])):
        x = x_right + escape if index % 2 == 0 else x_left - escape
        segments.extend(_polyline([Point(x, y1), Point(x, y2)], "F.Cu"))
    return segments, vias


def build_layout(config: BoardConfig) -> LayoutGeometry:
    """Calculate perpendicular A/B and C/D phase pairs on two copper layers."""

    config.validate()
    routing_width = config.routing_width
    routing_height = config.routing_height
    motion_left = (routing_width - config.motion_width) / 2.0
    motion_right = motion_left + config.motion_width
    motion_top = (routing_height - config.motion_height) / 2.0
    motion_bottom = motion_top + config.motion_height

    a_count = floor(config.motion_width / config.coil_pitch) + 1
    # The offset phase stops before the far boundary when the remaining span
    # is an exact pitch multiple.  This preserves the outer lead corridor.
    b_count = ceil((config.motion_width - config.phase_offset) / config.coil_pitch)
    a_xs = [motion_left + index * config.coil_pitch for index in range(a_count)]
    b_xs = [motion_left + config.phase_offset + index * config.coil_pitch for index in range(b_count)]

    c_count = floor(config.motion_height / config.coil_pitch) + 1
    d_count = ceil((config.motion_height - config.phase_offset) / config.coil_pitch)
    c_ys = [motion_top + index * config.coil_pitch for index in range(c_count)]
    d_ys = [motion_top + config.phase_offset + index * config.coil_pitch for index in range(d_count)]

    header_y0 = routing_height / 2.0 - 1.5 * config.header_pitch
    header_x0 = routing_width / 2.0 - 1.5 * config.header_pitch
    ab_specs = [
        ("1", "A+", "COIL_A"),
        ("2", "A-", "COIL_A"),
        ("3", "B+", "COIL_B"),
        ("4", "B-", "COIL_B"),
    ]
    cd_specs = [
        ("1", "C+", "COIL_C"),
        ("2", "C-", "COIL_C"),
        ("3", "D+", "COIL_D"),
        ("4", "D-", "COIL_D"),
    ]
    ab_pins = [
        HeaderPin(number, label, net, Point(config.header_x, header_y0 + index * config.header_pitch))
        for index, (number, label, net) in enumerate(ab_specs)
    ]
    cd_pins = [
        HeaderPin(number, label, net, Point(header_x0 + index * config.header_pitch, config.header_y))
        for index, (number, label, net) in enumerate(cd_specs)
    ]
    pins = [*ab_pins, *cd_pins]

    a = PhaseGeometry(name="A", net_name="COIL_A", pin_names=("A+", "A-"))
    a.active_segments = _active_strokes(a_xs, motion_top, motion_bottom)
    a.return_segments = _returns(
        a_xs, motion_top, motion_bottom, config.return_offset_a, "F.Cu"
    )

    b = PhaseGeometry(name="B", net_name="COIL_B", pin_names=("B+", "B-"))
    b.active_segments = _active_strokes(b_xs, motion_top, motion_bottom)
    b.return_segments, b.via_points = _switched_vertical_returns(
        b_xs, motion_top, motion_bottom, config.switched_via_escape
    )

    c = PhaseGeometry(name="C", net_name="COIL_C", pin_names=("C+", "C-"))
    c.active_segments = _active_strokes_horizontal(c_ys, motion_left, motion_right)
    c.return_segments = _returns_horizontal(
        c_ys, motion_left, motion_right, config.return_offset_a, "B.Cu"
    )

    d = PhaseGeometry(name="D", net_name="COIL_D", pin_names=("D+", "D-"))
    d.active_segments = _active_strokes_horizontal(d_ys, motion_left, motion_right)
    d.return_segments, d.via_points = _switched_horizontal_returns(
        d_ys, motion_left, motion_right, config.switched_via_escape
    )

    west_10 = motion_left - 10.0
    west_7_5 = motion_left - 7.5
    west_5 = motion_left - 5.0
    east_7_5 = routing_width - 7.5
    east_10 = routing_width - 10.0
    north_10 = motion_top - 10.0
    north_7_5 = motion_top - 7.5
    north_5 = motion_top - 5.0
    south_7_5 = routing_height - 7.5
    south_10 = routing_height - 10.0
    south_12_5 = routing_height - 12.5

    # Both paths start at the top of their leftmost stroke.  The endpoint is at
    # the bottom for an odd stroke count and at the top for an even count.
    a_end_y = motion_bottom if len(a_xs) % 2 == 1 else motion_top
    a.lead_segments = _polyline(
        [
            Point(a_xs[0], motion_top),
            Point(west_7_5, motion_top),
            Point(west_7_5, north_10),
            Point(east_7_5, north_10),
            Point(east_7_5, ab_pins[0].position.y),
            ab_pins[0].position,
        ],
        "F.Cu",
    ) + _polyline(
        [
            Point(a_xs[-1], a_end_y),
            Point(east_10, a_end_y),
            Point(east_10, ab_pins[1].position.y),
            ab_pins[1].position,
        ],
        "F.Cu",
    )
    b_start_via = Point(b_xs[0], motion_top - config.switched_via_escape)
    b_end_y = (
        motion_bottom + config.switched_via_escape
        if len(b_xs) % 2 == 1
        else motion_top - config.switched_via_escape
    )
    b_end_via = Point(b_xs[-1], b_end_y)
    b.lead_segments = _polyline(
        [
            b_start_via,
            Point(b_start_via.x, north_7_5),
            Point(east_7_5, north_7_5),
            Point(east_7_5, ab_pins[2].position.y),
            ab_pins[2].position,
        ],
        "B.Cu",
    )
    if b_end_y < motion_top:
        b.lead_segments += _polyline(
            [
                b_end_via,
                Point(b_end_via.x, north_5),
                Point(east_10, north_5),
                Point(east_10, ab_pins[3].position.y),
                ab_pins[3].position,
            ],
            "B.Cu",
        )
    else:
        # With an odd stroke count the free end is below the active area.
        # Route it outside all C-phase B.Cu returns before approaching J1.
        b.lead_segments += _polyline(
            [
                b_end_via,
                Point(east_10, b_end_via.y),
                Point(east_10, ab_pins[3].position.y),
                ab_pins[3].position,
            ],
            "B.Cu",
        )

    c_end_x = motion_right if len(c_ys) % 2 == 1 else motion_left
    c.lead_segments = _polyline(
        [
            Point(motion_left, c_ys[0]),
            Point(west_10, c_ys[0]),
            Point(west_10, south_7_5),
            Point(cd_pins[0].position.x, south_7_5),
            cd_pins[0].position,
        ],
        "B.Cu",
    ) + _polyline(
        [
            Point(c_end_x, c_ys[-1]),
            Point(east_10, c_ys[-1]),
            Point(east_10, south_10),
            Point(cd_pins[1].position.x, south_10),
            cd_pins[1].position,
        ],
        "B.Cu",
    )

    d_start_via = Point(motion_left - config.switched_via_escape, d_ys[0])
    d_end_x = motion_right + config.switched_via_escape if len(d_ys) % 2 == 1 else motion_left - config.switched_via_escape
    d_end_via = Point(d_end_x, d_ys[-1])
    d.lead_segments = _polyline(
        [
            d_start_via,
            Point(west_7_5, d_start_via.y),
            Point(west_7_5, south_10),
            Point(cd_pins[2].position.x, south_10),
            cd_pins[2].position,
        ],
        "F.Cu",
    ) + _polyline(
        [
            d_end_via,
            Point(west_5, d_end_via.y),
            Point(west_5, south_12_5),
            Point(cd_pins[3].position.x, south_12_5),
            cd_pins[3].position,
        ],
        "F.Cu",
    )

    return LayoutGeometry(
        config=config,
        phases=[a, b, c, d],
        header_pins=pins,
        motion_top_left=Point(motion_left, motion_top),
        motion_bottom_right=Point(motion_right, motion_bottom),
    )


def format_report(layout: LayoutGeometry) -> str:
    lines = ["Two-axis interleaved planar stepper board generated:"]
    for phase in layout.phases:
        layer_lengths: dict[str, float] = {}
        for layered in phase.segments:
            layer_lengths[layered.copper_layer] = layer_lengths.get(layered.copper_layer, 0.0) + layered.length_mm
        split = ", ".join(f"{layer} {length:.1f} mm" for layer, length in sorted(layer_lengths.items()))
        lines.append(
            f"  {phase.name} phase: {phase.length_mm:.1f} mm ({split}), "
            f"estimated R = {phase.resistance_ohm(layout.config):.3f} ohm"
        )
    lines.append(
        f"  Active straight area: {layout.config.motion_width:.1f} x "
        f"{layout.config.motion_height:.1f} mm = {layout.active_envelope_area_mm2:.1f} mm^2"
    )
    return "\n".join(lines)
