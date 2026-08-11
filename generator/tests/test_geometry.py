import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from collections import defaultdict

from planar_stepper.geometry import (
    BoardConfig,
    build_layout,
    four_card_flex_config,
)


class GeometryTests(unittest.TestCase):
    def assert_unbranched_phase(self, phase):
        graph = defaultdict(set)
        for item in phase.segments:
            start = (item.segment.start, item.copper_layer)
            end = (item.segment.end, item.copper_layer)
            graph[start].add(end)
            graph[end].add(start)
        for point in phase.via_points:
            front = (point, "F.Cu")
            back = (point, "B.Cu")
            graph[front].add(back)
            graph[back].add(front)

        stack = [next(iter(graph))]
        visited = set()
        while stack:
            node = stack.pop()
            if node in visited:
                continue
            visited.add(node)
            stack.extend(graph[node] - visited)

        self.assertEqual(visited, set(graph))
        self.assertEqual(sum(len(neighbors) == 1 for neighbors in graph.values()), 2)
        self.assertTrue(all(len(neighbors) in (1, 2) for neighbors in graph.values()))

    def test_defaults_match_requested_interleaved_pattern(self):
        config = BoardConfig()
        layout = build_layout(config)
        a, b, c, d = layout.phases

        self.assertEqual((config.board_width, config.board_height), (90.0, 90.0))
        self.assertEqual((config.motion_width, config.motion_height), (50.0, 50.0))
        self.assertEqual(config.trace_width, 1.8)
        self.assertEqual(config.coil_pitch, 5.0)
        self.assertEqual(config.phase_offset, 2.5)
        self.assertEqual(len(a.active_segments), 11)
        self.assertEqual(len(b.active_segments), 10)
        self.assertEqual(len(c.active_segments), 11)
        self.assertEqual(len(d.active_segments), 10)

        a_xs = [item.segment.start.x for item in a.active_segments]
        b_xs = [item.segment.start.x for item in b.active_segments]
        self.assertEqual(a_xs, [20.0 + 5.0 * index for index in range(11)])
        self.assertEqual(b_xs, [22.5 + 5.0 * index for index in range(10)])
        combined = sorted(a_xs + b_xs)
        self.assertTrue(all(abs(right - left - 2.5) < 1e-9 for left, right in zip(combined, combined[1:])))

        copper_edge_gaps = [right - left - config.trace_width for left, right in zip(combined, combined[1:])]
        self.assertTrue(all(abs(gap - 0.7) < 1e-9 for gap in copper_edge_gaps))

    def test_perpendicular_active_strokes_fill_the_same_square(self):
        layout = build_layout(BoardConfig())
        a, b, c, d = layout.phases
        for phase in (a, b):
            for item in phase.active_segments:
                self.assertEqual(item.copper_layer, "F.Cu")
                self.assertEqual((item.segment.start.y, item.segment.end.y), (20.0, 70.0))
        for phase in (c, d):
            for item in phase.active_segments:
                self.assertEqual(item.copper_layer, "B.Cu")
                self.assertEqual((item.segment.start.x, item.segment.end.x), (20.0, 70.0))

    def test_b_returns_change_layer_at_every_stroke_end(self):
        layout = build_layout(BoardConfig())
        a, b, c, d = layout.phases
        self.assertTrue(all(item.copper_layer == "F.Cu" for item in a.return_segments))
        self.assertEqual(a.via_points, [])
        self.assertEqual(c.via_points, [])
        self.assertEqual(len(b.via_points), 20)
        self.assertEqual(len(d.via_points), 20)

        # Vias are outside the active square, so they cannot short a
        # perpendicular active conductor on the other copper layer.
        self.assertTrue(all(point.y in (17.5, 72.5) for point in b.via_points))
        self.assertTrue(all(point.x in (17.5, 72.5) for point in d.via_points))
        self.assertTrue(all(
            not (20.0 <= point.x <= 70.0 and 20.0 <= point.y <= 70.0)
            for point in [*b.via_points, *d.via_points]
        ))

    def test_each_phase_is_one_unbranched_path(self):
        layout = build_layout(BoardConfig())
        for phase in layout.phases:
            self.assert_unbranched_phase(phase)

    def test_four_card_flex_preset(self):
        config = four_card_flex_config()
        layout = build_layout(config)
        a, b, c, d = layout.phases

        self.assertEqual((config.board_width, config.board_height), (229.0, 156.5))
        self.assertEqual((config.routing_width, config.routing_height), (222.5, 150.0))
        self.assertEqual((config.motion_width, config.motion_height), (182.5, 110.0))
        self.assertEqual(config.board_thickness, 0.2)
        self.assertTrue(config.surface_terminal_lands)
        self.assertEqual(config.terminal_group_gap, 10.0)
        self.assertEqual(
            (config.terminal_land_along, config.terminal_land_across),
            (1.6, 2.0),
        )
        self.assertEqual(config.terminal_connection_width, config.trace_width)
        self.assertAlmostEqual(
            config.header_pitch - config.terminal_land_along,
            0.94,
        )
        self.assertAlmostEqual(
            3.0 * config.header_pitch + config.terminal_group_gap,
            17.62,
        )
        self.assertEqual((len(a.active_segments), len(b.active_segments)), (37, 36))
        self.assertEqual((len(c.active_segments), len(d.active_segments)), (23, 22))
        self.assertEqual((len(b.via_points), len(d.via_points)), (72, 44))
        self.assertEqual(
            [pin.position for pin in layout.header_pins[:4]],
            [
                type(layout.motion_top_left)(217.5, 71.19),
                type(layout.motion_top_left)(217.5, 73.73),
                type(layout.motion_top_left)(217.5, 76.27),
                type(layout.motion_top_left)(217.5, 78.81),
            ],
        )

        for phase in layout.phases:
            self.assert_unbranched_phase(phase)
            for routed in phase.segments:
                for point in (routed.segment.start, routed.segment.end):
                    self.assertGreaterEqual(point.x, 0.0)
                    self.assertLessEqual(point.x, config.board_width)
                    self.assertGreaterEqual(point.y, 0.0)
                    self.assertLessEqual(point.y, config.board_height)

    def test_invalid_interleave_clearance_is_rejected(self):
        with self.assertRaises(ValueError):
            build_layout(BoardConfig(trace_width=2.1))
        with self.assertRaises(ValueError):
            build_layout(BoardConfig(phase_offset=2.0))

    def test_surface_terminal_lands_require_clearance_and_board_margin(self):
        with self.assertRaises(ValueError):
            build_layout(
                BoardConfig(
                    surface_terminal_lands=True,
                    terminal_land_along=2.1,
                )
            )
        with self.assertRaises(ValueError):
            build_layout(
                BoardConfig(
                    surface_terminal_lands=True,
                    header_x=0.5,
                )
            )

    def test_header_order(self):
        layout = build_layout(BoardConfig())
        self.assertEqual(
            [pin.label for pin in layout.header_pins],
            ["A+", "A-", "B+", "B-", "C+", "C-", "D+", "D-"],
        )
        self.assertEqual(
            [pin.net_name for pin in layout.header_pins],
            ["COIL_A", "COIL_A", "COIL_B", "COIL_B", "COIL_C", "COIL_C", "COIL_D", "COIL_D"],
        )


if __name__ == "__main__":
    unittest.main()
