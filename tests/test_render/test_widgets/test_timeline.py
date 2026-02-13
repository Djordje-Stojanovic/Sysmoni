from __future__ import annotations

import os
import pathlib
import sys
from typing import cast
import unittest

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from contracts.types import SystemSnapshot  # noqa: E402
from render.widgets.timeline import TimelineConfig, _format_ago  # noqa: E402

_HAS_DISPLAY = os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY") or sys.platform == "win32"

_HAS_QT = False
try:
    from PySide6.QtWidgets import QApplication

    _HAS_QT = True
except ImportError:
    pass


def _make_snapshots(count: int, start_ts: float = 1000.0, interval: float = 1.0) -> list[SystemSnapshot]:
    """Generate a list of SystemSnapshot objects for testing."""
    return [
        SystemSnapshot(
            timestamp=start_ts + i * interval,
            cpu_percent=10.0 + (i % 20) * 4.0,
            memory_percent=30.0 + (i % 10) * 2.0,
        )
        for i in range(count)
    ]


class TimelineConfigTests(unittest.TestCase):
    def test_defaults(self) -> None:
        cfg = TimelineConfig()
        self.assertAlmostEqual(cfg.line_width, 1.5)
        self.assertEqual(cfg.gradient_alpha_top, 60)
        self.assertEqual(cfg.gradient_alpha_bottom, 0)
        self.assertAlmostEqual(cfg.scrubber_width, 2.0)
        self.assertAlmostEqual(cfg.scrubber_handle_radius, 5.0)
        self.assertEqual(cfg.axis_height, 20)
        self.assertEqual(cfg.min_height, 80)
        self.assertEqual(cfg.min_width, 200)
        self.assertFalse(cfg.show_memory)

    def test_frozen(self) -> None:
        cfg = TimelineConfig()
        with self.assertRaises(AttributeError):
            cfg.line_width = 3.0  # type: ignore[misc]

    def test_validation_line_width(self) -> None:
        with self.assertRaises(ValueError):
            TimelineConfig(line_width=0.0)
        with self.assertRaises(ValueError):
            TimelineConfig(line_width=-1.0)

    def test_validation_gradient_alpha_top(self) -> None:
        with self.assertRaises(ValueError):
            TimelineConfig(gradient_alpha_top=-1)
        with self.assertRaises(ValueError):
            TimelineConfig(gradient_alpha_top=256)

    def test_validation_gradient_alpha_bottom(self) -> None:
        with self.assertRaises(ValueError):
            TimelineConfig(gradient_alpha_bottom=-1)
        with self.assertRaises(ValueError):
            TimelineConfig(gradient_alpha_bottom=256)

    def test_validation_scrubber_width(self) -> None:
        with self.assertRaises(ValueError):
            TimelineConfig(scrubber_width=0.0)

    def test_validation_scrubber_handle_radius(self) -> None:
        with self.assertRaises(ValueError):
            TimelineConfig(scrubber_handle_radius=0.0)

    def test_validation_axis_height(self) -> None:
        with self.assertRaises(ValueError):
            TimelineConfig(axis_height=-1)

    def test_validation_min_height(self) -> None:
        with self.assertRaises(ValueError):
            TimelineConfig(min_height=0)

    def test_validation_min_width(self) -> None:
        with self.assertRaises(ValueError):
            TimelineConfig(min_width=0)

    def test_custom_values(self) -> None:
        cfg = TimelineConfig(
            line_width=3.0,
            gradient_alpha_top=100,
            scrubber_width=4.0,
            min_height=120,
            min_width=400,
            show_memory=True,
        )
        self.assertAlmostEqual(cfg.line_width, 3.0)
        self.assertEqual(cfg.gradient_alpha_top, 100)
        self.assertAlmostEqual(cfg.scrubber_width, 4.0)
        self.assertEqual(cfg.min_height, 120)
        self.assertEqual(cfg.min_width, 400)
        self.assertTrue(cfg.show_memory)


class FormatAgoTests(unittest.TestCase):
    def test_now(self) -> None:
        self.assertEqual(_format_ago(0.0), "now")
        self.assertEqual(_format_ago(0.5), "now")

    def test_seconds(self) -> None:
        self.assertEqual(_format_ago(1.0), "1s ago")
        self.assertEqual(_format_ago(45.0), "45s ago")
        self.assertEqual(_format_ago(59.9), "59s ago")

    def test_minutes(self) -> None:
        self.assertEqual(_format_ago(60.0), "1m ago")
        self.assertEqual(_format_ago(300.0), "5m ago")
        self.assertEqual(_format_ago(3599.0), "59m ago")

    def test_hours(self) -> None:
        self.assertEqual(_format_ago(3600.0), "1h ago")
        self.assertEqual(_format_ago(7200.0), "2h ago")
        self.assertEqual(_format_ago(5400.0), "1.5h ago")

    def test_days(self) -> None:
        self.assertEqual(_format_ago(86400.0), "1d ago")
        self.assertEqual(_format_ago(172800.0), "2d ago")


@unittest.skipUnless(_HAS_QT and _HAS_DISPLAY, "PySide6 or display not available")
class TimelineWidgetTests(unittest.TestCase):
    app: QApplication | None = None

    @classmethod
    def setUpClass(cls) -> None:
        if QApplication.instance() is None:
            cls.app = QApplication([])

    def test_instantiation_default(self) -> None:
        from render.widgets.timeline import TimelineWidget

        widget = TimelineWidget()
        self.assertEqual(widget.snapshot_count, 0)
        self.assertAlmostEqual(widget.scrub_ratio, 1.0)
        self.assertIsNone(widget.scrub_timestamp)

    def test_set_data(self) -> None:
        from render.widgets.timeline import TimelineWidget

        widget = TimelineWidget()
        snaps = _make_snapshots(5)
        widget.set_data(snaps)
        self.assertEqual(widget.snapshot_count, 5)

    def test_scrub_timestamp_interpolation(self) -> None:
        from render.widgets.timeline import TimelineWidget

        widget = TimelineWidget()
        snaps = _make_snapshots(10, start_ts=100.0, interval=10.0)
        widget.set_data(snaps)
        # ratio 0.0 -> first timestamp
        widget._scrub_ratio = 0.0
        self.assertAlmostEqual(widget.scrub_timestamp, 100.0)  # type: ignore[arg-type]
        # ratio 1.0 -> last timestamp
        widget._scrub_ratio = 1.0
        self.assertAlmostEqual(widget.scrub_timestamp, 190.0)  # type: ignore[arg-type]
        # ratio 0.5 -> midpoint
        widget._scrub_ratio = 0.5
        self.assertAlmostEqual(widget.scrub_timestamp, 145.0)  # type: ignore[arg-type]

    def test_paint_empty_does_not_raise(self) -> None:
        from PySide6.QtGui import QPixmap

        from render.widgets.timeline import TimelineWidget

        widget = TimelineWidget()
        widget.resize(400, 100)
        pixmap = QPixmap(400, 100)
        widget.render(pixmap)

    def test_paint_with_data_does_not_raise(self) -> None:
        from PySide6.QtGui import QPixmap

        from render.widgets.timeline import TimelineWidget

        widget = TimelineWidget()
        widget.resize(400, 100)
        widget.set_data(_make_snapshots(20))
        pixmap = QPixmap(400, 100)
        widget.render(pixmap)

    def test_paint_with_memory_enabled(self) -> None:
        from PySide6.QtGui import QPixmap

        from render.widgets.timeline import TimelineWidget

        cfg = TimelineConfig(show_memory=True)
        widget = TimelineWidget(timeline_config=cfg)
        widget.resize(400, 100)
        widget.set_data(_make_snapshots(20))
        pixmap = QPixmap(400, 100)
        widget.render(pixmap)

    def test_paint_with_non_finite_samples_does_not_raise(self) -> None:
        from PySide6.QtGui import QPixmap

        from render.widgets.timeline import TimelineWidget

        class _SnapshotStub:
            def __init__(
                self,
                *,
                timestamp: float,
                cpu_percent: float,
                memory_percent: float,
            ) -> None:
                self.timestamp = timestamp
                self.cpu_percent = cpu_percent
                self.memory_percent = memory_percent

        widget = TimelineWidget()
        widget.resize(400, 100)
        widget.set_data(
            cast(
                list[SystemSnapshot],
                [
                    _SnapshotStub(
                        timestamp=100.0,
                        cpu_percent=float("nan"),
                        memory_percent=float("inf"),
                    ),
                    _SnapshotStub(
                        timestamp=101.0,
                        cpu_percent=float("-inf"),
                        memory_percent=50.0,
                    ),
                ],
            )
        )
        pixmap = QPixmap(400, 100)
        widget.render(pixmap)

    def test_minimum_size_from_config(self) -> None:
        from render.widgets.timeline import TimelineWidget

        cfg = TimelineConfig(min_height=120, min_width=300)
        widget = TimelineWidget(timeline_config=cfg)
        self.assertEqual(widget.minimumHeight(), 120)
        self.assertEqual(widget.minimumWidth(), 300)

    def test_set_data_replaces_previous(self) -> None:
        from render.widgets.timeline import TimelineWidget

        widget = TimelineWidget()
        widget.set_data(_make_snapshots(10))
        self.assertEqual(widget.snapshot_count, 10)
        widget.set_data(_make_snapshots(3))
        self.assertEqual(widget.snapshot_count, 3)

    def test_signal_emission(self) -> None:
        from render.widgets.timeline import TimelineWidget

        widget = TimelineWidget()
        widget.resize(400, 100)
        widget.set_data(_make_snapshots(10, start_ts=100.0, interval=10.0))
        received: list[float] = []
        widget.scrub_position_changed.connect(received.append)
        widget._update_scrub_from_mouse(204.0)
        self.assertEqual(len(received), 1)
        self.assertIsInstance(received[0], float)


if __name__ == "__main__":
    unittest.main()
