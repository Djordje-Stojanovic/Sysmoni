from __future__ import annotations

import os
import pathlib
import sys
import unittest

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from render.widgets.sparkline import SparkLineConfig  # noqa: E402

_HAS_DISPLAY = os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY") or sys.platform == "win32"

_HAS_QT = False
try:
    from PySide6.QtWidgets import QApplication

    _HAS_QT = True
except ImportError:
    pass


class SparkLineConfigTests(unittest.TestCase):
    def test_defaults(self) -> None:
        cfg = SparkLineConfig()
        self.assertEqual(cfg.buffer_size, 120)
        self.assertAlmostEqual(cfg.line_width, 1.5)
        self.assertEqual(cfg.gradient_alpha_top, 80)
        self.assertEqual(cfg.gradient_alpha_bottom, 0)
        self.assertTrue(cfg.show_latest_value)
        self.assertEqual(cfg.label_format, "{:.1f}%")
        self.assertEqual(cfg.min_height, 60)

    def test_frozen(self) -> None:
        cfg = SparkLineConfig()
        with self.assertRaises(AttributeError):
            cfg.buffer_size = 60  # type: ignore[misc]

    def test_validation_buffer_size(self) -> None:
        with self.assertRaises(ValueError):
            SparkLineConfig(buffer_size=1)
        with self.assertRaises(ValueError):
            SparkLineConfig(buffer_size=0)

    def test_validation_line_width(self) -> None:
        with self.assertRaises(ValueError):
            SparkLineConfig(line_width=0.0)
        with self.assertRaises(ValueError):
            SparkLineConfig(line_width=-1.0)

    def test_validation_min_height(self) -> None:
        with self.assertRaises(ValueError):
            SparkLineConfig(min_height=0)

    def test_custom_values(self) -> None:
        cfg = SparkLineConfig(buffer_size=60, line_width=3.0, min_height=100)
        self.assertEqual(cfg.buffer_size, 60)
        self.assertAlmostEqual(cfg.line_width, 3.0)
        self.assertEqual(cfg.min_height, 100)


@unittest.skipUnless(_HAS_QT and _HAS_DISPLAY, "PySide6 or display not available")
class SparkLineWidgetTests(unittest.TestCase):
    app: QApplication | None = None

    @classmethod
    def setUpClass(cls) -> None:
        if QApplication.instance() is None:
            cls.app = QApplication([])

    def test_instantiation_default(self) -> None:
        from render.widgets.sparkline import SparkLine

        spark = SparkLine()
        self.assertIsNone(spark.latest)
        self.assertEqual(spark.buffer_len, 0)

    def test_push_and_latest(self) -> None:
        from render.widgets.sparkline import SparkLine

        spark = SparkLine()
        spark.push(42.5)
        self.assertAlmostEqual(spark.latest, 42.5)  # type: ignore[arg-type]
        self.assertEqual(spark.buffer_len, 1)

    def test_push_clamps(self) -> None:
        from render.widgets.sparkline import SparkLine

        spark = SparkLine()
        spark.push(-10.0)
        self.assertAlmostEqual(spark.latest, 0.0)  # type: ignore[arg-type]
        spark.push(200.0)
        self.assertAlmostEqual(spark.latest, 100.0)  # type: ignore[arg-type]

    def test_push_non_finite_values_clamp_to_zero(self) -> None:
        from render.widgets.sparkline import SparkLine

        spark = SparkLine()
        spark.push(float("nan"))
        self.assertAlmostEqual(spark.latest, 0.0)  # type: ignore[arg-type]
        spark.push(float("inf"))
        self.assertAlmostEqual(spark.latest, 0.0)  # type: ignore[arg-type]
        spark.push(float("-inf"))
        self.assertAlmostEqual(spark.latest, 0.0)  # type: ignore[arg-type]

    def test_push_many(self) -> None:
        from render.widgets.sparkline import SparkLine

        spark = SparkLine()
        spark.push_many([10.0, 20.0, 30.0])
        self.assertEqual(spark.buffer_len, 3)
        self.assertAlmostEqual(spark.latest, 30.0)  # type: ignore[arg-type]

    def test_push_many_sanitizes_non_finite_values(self) -> None:
        from render.widgets.sparkline import SparkLine

        spark = SparkLine()
        spark.push_many([10.0, float("nan"), float("inf")])
        self.assertEqual(spark.buffer_len, 3)
        self.assertAlmostEqual(spark.latest, 0.0)  # type: ignore[arg-type]

    def test_buffer_eviction(self) -> None:
        from render.widgets.sparkline import SparkLine

        cfg = SparkLineConfig(buffer_size=3)
        spark = SparkLine(spark_config=cfg)
        spark.push_many([1.0, 2.0, 3.0, 4.0, 5.0])
        self.assertEqual(spark.buffer_len, 3)
        self.assertAlmostEqual(spark.latest, 5.0)  # type: ignore[arg-type]

    def test_paint_empty_does_not_raise(self) -> None:
        from PySide6.QtGui import QPixmap

        from render.widgets.sparkline import SparkLine

        spark = SparkLine()
        spark.resize(300, 80)
        pixmap = QPixmap(300, 80)
        spark.render(pixmap)

    def test_paint_with_data_does_not_raise(self) -> None:
        from PySide6.QtGui import QPixmap

        from render.widgets.sparkline import SparkLine

        spark = SparkLine()
        spark.resize(300, 80)
        spark.push_many([float(i) for i in range(50)])
        pixmap = QPixmap(300, 80)
        spark.render(pixmap)

    def test_paint_full_buffer_does_not_raise(self) -> None:
        from PySide6.QtGui import QPixmap

        from render.widgets.sparkline import SparkLine

        cfg = SparkLineConfig(buffer_size=10)
        spark = SparkLine(spark_config=cfg)
        spark.resize(300, 80)
        spark.push_many([float(i * 10) for i in range(10)])
        pixmap = QPixmap(300, 80)
        spark.render(pixmap)

    def test_minimum_height_from_config(self) -> None:
        from render.widgets.sparkline import SparkLine

        cfg = SparkLineConfig(min_height=100)
        spark = SparkLine(spark_config=cfg)
        self.assertEqual(spark.minimumHeight(), 100)


if __name__ == "__main__":
    unittest.main()
