from __future__ import annotations

import pathlib
import sys
import unittest

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from render.theme import (  # noqa: E402
    RenderTheme,
    build_dock_stylesheet,
    build_shell_stylesheet,
)


class RenderThemeTests(unittest.TestCase):
    def test_build_shell_stylesheet_contains_expected_default_tokens(self) -> None:
        stylesheet = build_shell_stylesheet()

        self.assertIn("QWidget {", stylesheet)
        self.assertIn("stop:0 #071320, stop:1 #0d2033", stylesheet)
        self.assertIn("font-family: Segoe UI;", stylesheet)
        self.assertIn("QFrame#slotFrame {", stylesheet)
        self.assertIn("QLabel#slotTitle {", stylesheet)
        self.assertIn("QPushButton#tabButton {", stylesheet)
        self.assertIn("QPushButton#tabButton:checked {", stylesheet)
        self.assertIn("QFrame#panelFrame {", stylesheet)
        self.assertIn("QLabel#panelTitle {", stylesheet)
        self.assertIn("QLabel#title {", stylesheet)
        self.assertIn("QLabel#metric {", stylesheet)
        self.assertIn("QLabel#status {", stylesheet)
        self.assertIn("QLabel#section {", stylesheet)
        self.assertIn("QLabel#process {", stylesheet)
        self.assertIn("QPushButton#moveButton {", stylesheet)
        self.assertIn("font-family: Consolas;", stylesheet)

    def test_build_shell_stylesheet_uses_cached_value_for_equal_theme(self) -> None:
        theme_a = RenderTheme()
        theme_b = RenderTheme()

        stylesheet_a = build_shell_stylesheet(theme_a)
        stylesheet_b = build_shell_stylesheet(theme_b)

        self.assertIs(stylesheet_a, stylesheet_b)

    def test_build_shell_stylesheet_applies_custom_theme_values(self) -> None:
        custom_theme = RenderTheme(
            background_start="#000000",
            background_end="#111111",
            text_primary="#f0f0f0",
            title_color="#fafafa",
            metric_color="#44ffdd",
            metric_color_peak="#88ffee",
            status_color="#bbbbbb",
            status_color_peak="#dddddd",
            section_color="#33ccaa",
            process_color="#eeeeee",
            slot_frame_background="rgba(1, 2, 3, 0.5)",
            slot_frame_border="rgba(5, 6, 7, 0.5)",
            slot_title_color="#abcdef",
            tab_border_color="rgba(8, 9, 10, 0.5)",
            tab_background="rgba(11, 12, 13, 0.5)",
            tab_text_color="#112233",
            tab_active_background_start="#102030",
            tab_active_background_end="#203040",
            tab_active_text_color="#fefefe",
            panel_background="rgba(14, 15, 16, 0.5)",
            panel_border="rgba(17, 18, 19, 0.5)",
            panel_title_color="#123456",
            move_button_border="rgba(20, 21, 22, 0.5)",
            move_button_background_start="#304050",
            move_button_background_end="#405060",
            move_button_text="#654321",
            base_font_family="Tahoma",
            process_font_family="Courier New",
            title_font_size=30,
            metric_font_size=20,
            status_font_size=11,
            section_font_size=15,
            process_font_size=10,
        )

        stylesheet = build_shell_stylesheet(custom_theme)

        self.assertIn("stop:0 #000000, stop:1 #111111", stylesheet)
        self.assertIn("color: #f0f0f0;", stylesheet)
        self.assertIn("font-family: Tahoma;", stylesheet)
        self.assertIn("font-family: Courier New;", stylesheet)
        self.assertIn("color: #44ffdd;", stylesheet)
        self.assertIn("color: #bbbbbb;", stylesheet)
        self.assertIn("background: rgba(1, 2, 3, 0.5);", stylesheet)
        self.assertIn("border: 1px solid rgba(5, 6, 7, 0.5);", stylesheet)
        self.assertIn("color: #abcdef;", stylesheet)
        self.assertIn("border: 1px solid rgba(8, 9, 10, 0.5);", stylesheet)
        self.assertIn("background: rgba(11, 12, 13, 0.5);", stylesheet)
        self.assertIn("background: #102030;", stylesheet)
        self.assertIn("color: #fefefe;", stylesheet)
        self.assertIn("background: rgba(14, 15, 16, 0.5);", stylesheet)
        self.assertIn("border: 1px solid rgba(17, 18, 19, 0.5);", stylesheet)
        self.assertIn("color: #123456;", stylesheet)
        self.assertIn("border: 1px solid rgba(20, 21, 22, 0.5);", stylesheet)
        self.assertIn("background: #304050;", stylesheet)
        self.assertIn("color: #654321;", stylesheet)
        self.assertIn("font-size: 30px;", stylesheet)
        self.assertIn("font-size: 20px;", stylesheet)
        self.assertIn("font-size: 11px;", stylesheet)
        self.assertIn("font-size: 15px;", stylesheet)
        self.assertIn("font-size: 10px;", stylesheet)

    def test_build_shell_stylesheet_blends_dynamic_accent_tokens(self) -> None:
        cool = build_shell_stylesheet(accent_intensity=0.0)
        hot = build_shell_stylesheet(accent_intensity=1.0)

        self.assertIn("background: #205b8e;", cool)
        self.assertIn("background: #3f8fd8;", hot)
        self.assertIn("background: #122d46;", cool)
        self.assertIn("background: #266ea7;", hot)
        self.assertNotEqual(cool, hot)

    def test_build_shell_stylesheet_quantizes_accent_intensity_for_cache_hits(self) -> None:
        theme = RenderTheme()

        stylesheet_a = build_shell_stylesheet(theme, accent_intensity=0.501)
        stylesheet_b = build_shell_stylesheet(theme, accent_intensity=0.504)

        self.assertIs(stylesheet_a, stylesheet_b)

    def test_build_shell_stylesheet_clamps_out_of_range_accent(self) -> None:
        low = build_shell_stylesheet(accent_intensity=-5.0)
        high = build_shell_stylesheet(accent_intensity=9.0)

        self.assertIn("background: #205b8e;", low)
        self.assertIn("background: #3f8fd8;", high)

    def test_build_dock_stylesheet_delegates_to_shell_stylesheet(self) -> None:
        stylesheet_a = build_shell_stylesheet(accent_intensity=0.25)
        stylesheet_b = build_dock_stylesheet(accent_intensity=0.25)

        self.assertEqual(stylesheet_a, stylesheet_b)


if __name__ == "__main__":
    unittest.main()
