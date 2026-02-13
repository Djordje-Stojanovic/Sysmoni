from __future__ import annotations

import os
import pathlib
import sys
import unittest

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from render.widgets.radial_gauge import RadialGaugeConfig  # noqa: E402

_HAS_DISPLAY = os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY") or sys.platform == "win32"

_HAS_QT = False
try:
    from PySide6.QtWidgets import QApplication

    _HAS_QT = True
except ImportError:
    pass


class RadialGaugeConfigTests(unittest.TestCase):
    def test_defaults(self) -> None:
        cfg = RadialGaugeConfig()
        self.assertAlmostEqual(cfg.sweep_degrees, 270.0)
        self.assertAlmostEqual(cfg.start_angle_degrees, 225.0)
        self.assertEqual(cfg.arc_width, 10)
        self.assertTrue(cfg.show_label)
        self.assertEqual(cfg.label_format, "{:.0f}%")
        self.assertEqual(cfg.min_size, 120)

    def test_frozen(self) -> None:
        cfg = RadialGaugeConfig()
        with self.assertRaises(AttributeError):
            cfg.sweep_degrees = 180.0  # type: ignore[misc]

    def test_validation_sweep_degrees(self) -> None:
        with self.assertRaises(ValueError):
            RadialGaugeConfig(sweep_degrees=0.0)
        with self.assertRaises(ValueError):
            RadialGaugeConfig(sweep_degrees=-10.0)
        with self.assertRaises(ValueError):
            RadialGaugeConfig(sweep_degrees=361.0)

    def test_validation_arc_width(self) -> None:
        with self.assertRaises(ValueError):
            RadialGaugeConfig(arc_width=0)

    def test_validation_min_size(self) -> None:
        with self.assertRaises(ValueError):
            RadialGaugeConfig(min_size=0)

    def test_custom_values(self) -> None:
        cfg = RadialGaugeConfig(sweep_degrees=360.0, arc_width=20, min_size=200)
        self.assertAlmostEqual(cfg.sweep_degrees, 360.0)
        self.assertEqual(cfg.arc_width, 20)
        self.assertEqual(cfg.min_size, 200)


@unittest.skipUnless(_HAS_QT and _HAS_DISPLAY, "PySide6 or display not available")
class RadialGaugeWidgetTests(unittest.TestCase):
    app: QApplication | None = None

    @classmethod
    def setUpClass(cls) -> None:
        if QApplication.instance() is None:
            cls.app = QApplication([])

    def test_instantiation_default(self) -> None:
        from render.widgets.radial_gauge import RadialGauge

        gauge = RadialGauge()
        self.assertAlmostEqual(gauge.value, 0.0)
        self.assertAlmostEqual(gauge.accent_intensity, 0.0)

    def test_set_value_clamps(self) -> None:
        from render.widgets.radial_gauge import RadialGauge

        gauge = RadialGauge()
        gauge.set_value(50.0)
        self.assertAlmostEqual(gauge.value, 50.0)
        gauge.set_value(-10.0)
        self.assertAlmostEqual(gauge.value, 0.0)
        gauge.set_value(200.0)
        self.assertAlmostEqual(gauge.value, 100.0)

    def test_set_accent_intensity_clamps(self) -> None:
        from render.widgets.radial_gauge import RadialGauge

        gauge = RadialGauge()
        gauge.set_accent_intensity(0.5)
        self.assertAlmostEqual(gauge.accent_intensity, 0.5)
        gauge.set_accent_intensity(-1.0)
        self.assertAlmostEqual(gauge.accent_intensity, 0.0)
        gauge.set_accent_intensity(5.0)
        self.assertAlmostEqual(gauge.accent_intensity, 1.0)

    def test_paint_at_boundaries_does_not_raise(self) -> None:
        from PySide6.QtGui import QPixmap

        from render.widgets.radial_gauge import RadialGauge

        gauge = RadialGauge()
        gauge.resize(200, 200)
        pixmap = QPixmap(200, 200)

        for val in (0.0, 50.0, 100.0):
            gauge.set_value(val)
            gauge.render(pixmap)

    def test_minimum_size_from_config(self) -> None:
        from render.widgets.radial_gauge import RadialGauge

        cfg = RadialGaugeConfig(min_size=200)
        gauge = RadialGauge(gauge_config=cfg)
        self.assertEqual(gauge.minimumWidth(), 200)
        self.assertEqual(gauge.minimumHeight(), 200)


if __name__ == "__main__":
    unittest.main()
