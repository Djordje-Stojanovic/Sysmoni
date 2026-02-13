from __future__ import annotations

import os
import pathlib
import sys
import unittest

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from render.widgets.glass_panel import GlassPanelConfig  # noqa: E402

_HAS_DISPLAY = os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY") or sys.platform == "win32"

_HAS_QT = False
try:
    from PySide6.QtWidgets import QApplication

    _HAS_QT = True
except ImportError:
    pass


class GlassPanelConfigTests(unittest.TestCase):
    def test_defaults(self) -> None:
        cfg = GlassPanelConfig()
        self.assertAlmostEqual(cfg.frost_intensity, 0.08)
        self.assertAlmostEqual(cfg.frost_scale, 3.5)
        self.assertAlmostEqual(cfg.base_alpha, 0.72)
        self.assertAlmostEqual(cfg.accent_tint_strength, 0.18)
        self.assertTrue(cfg.animate_frost)
        self.assertEqual(cfg.min_width, 100)
        self.assertEqual(cfg.min_height, 60)

    def test_frozen(self) -> None:
        cfg = GlassPanelConfig()
        with self.assertRaises(AttributeError):
            cfg.frost_intensity = 0.5  # type: ignore[misc]

    def test_validation_base_alpha_range(self) -> None:
        with self.assertRaises(ValueError):
            GlassPanelConfig(base_alpha=-0.1)
        with self.assertRaises(ValueError):
            GlassPanelConfig(base_alpha=1.1)

    def test_validation_frost_intensity_range(self) -> None:
        with self.assertRaises(ValueError):
            GlassPanelConfig(frost_intensity=-0.1)
        with self.assertRaises(ValueError):
            GlassPanelConfig(frost_intensity=1.1)

    def test_validation_accent_tint_strength_range(self) -> None:
        with self.assertRaises(ValueError):
            GlassPanelConfig(accent_tint_strength=-0.1)
        with self.assertRaises(ValueError):
            GlassPanelConfig(accent_tint_strength=1.1)

    def test_validation_min_size(self) -> None:
        with self.assertRaises(ValueError):
            GlassPanelConfig(min_width=0)
        with self.assertRaises(ValueError):
            GlassPanelConfig(min_height=0)

    def test_custom_values(self) -> None:
        cfg = GlassPanelConfig(
            frost_intensity=0.5,
            frost_scale=5.0,
            base_alpha=0.9,
            accent_tint_strength=0.3,
            animate_frost=False,
            min_width=200,
            min_height=120,
        )
        self.assertAlmostEqual(cfg.frost_intensity, 0.5)
        self.assertAlmostEqual(cfg.frost_scale, 5.0)
        self.assertAlmostEqual(cfg.base_alpha, 0.9)
        self.assertAlmostEqual(cfg.accent_tint_strength, 0.3)
        self.assertFalse(cfg.animate_frost)
        self.assertEqual(cfg.min_width, 200)
        self.assertEqual(cfg.min_height, 120)


@unittest.skipUnless(_HAS_QT and _HAS_DISPLAY, "PySide6 or display not available")
class GlassPanelWidgetTests(unittest.TestCase):
    app: QApplication | None = None

    @classmethod
    def setUpClass(cls) -> None:
        if QApplication.instance() is None:
            cls.app = QApplication([])

    def test_instantiation_default(self) -> None:
        from render.widgets.glass_panel import GlassPanel

        panel = GlassPanel()
        self.assertAlmostEqual(panel.accent_intensity, 0.0)
        from render.theme import DEFAULT_THEME
        self.assertIs(panel.theme, DEFAULT_THEME)

    def test_set_accent_intensity_clamps(self) -> None:
        from render.widgets.glass_panel import GlassPanel

        panel = GlassPanel()
        panel.set_accent_intensity(0.5)
        self.assertAlmostEqual(panel.accent_intensity, 0.5)
        panel.set_accent_intensity(-1.0)
        self.assertAlmostEqual(panel.accent_intensity, 0.0)
        panel.set_accent_intensity(5.0)
        self.assertAlmostEqual(panel.accent_intensity, 1.0)

    def test_set_theme_updates_state(self) -> None:
        from render.widgets.glass_panel import GlassPanel
        from render.theme import RenderTheme

        panel = GlassPanel()
        new_theme = RenderTheme(background_start="#112233")
        panel.set_theme(new_theme)
        self.assertIs(panel.theme, new_theme)

    def test_render_does_not_raise(self) -> None:
        from PySide6.QtGui import QPixmap
        from render.widgets.glass_panel import GlassPanel

        panel = GlassPanel()
        panel.resize(300, 200)
        pixmap = QPixmap(300, 200)
        panel.render(pixmap)

    def test_render_at_accent_boundaries(self) -> None:
        from PySide6.QtGui import QPixmap
        from render.widgets.glass_panel import GlassPanel

        panel = GlassPanel()
        panel.resize(300, 200)
        pixmap = QPixmap(300, 200)
        for intensity in (0.0, 1.0):
            panel.set_accent_intensity(intensity)
            panel.render(pixmap)

    def test_minimum_size_from_config(self) -> None:
        from render.widgets.glass_panel import GlassPanel

        cfg = GlassPanelConfig(min_width=250, min_height=150)
        panel = GlassPanel(glass_config=cfg)
        self.assertEqual(panel.minimumWidth(), 250)
        self.assertEqual(panel.minimumHeight(), 150)

    def test_init_failed_is_bool(self) -> None:
        from render.widgets.glass_panel import GlassPanel

        panel = GlassPanel()
        self.assertIsInstance(panel._init_failed, bool)


if __name__ == "__main__":
    unittest.main()
