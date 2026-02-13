from __future__ import annotations

import pathlib
import sys
import unittest

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from render.theme import RenderTheme, build_shell_stylesheet  # noqa: E402


class RenderThemeTests(unittest.TestCase):
    def test_build_shell_stylesheet_contains_expected_default_tokens(self) -> None:
        stylesheet = build_shell_stylesheet()

        self.assertIn("QWidget {", stylesheet)
        self.assertIn("stop:0 #071320, stop:1 #0d2033", stylesheet)
        self.assertIn("font-family: Segoe UI;", stylesheet)
        self.assertIn("QLabel#title {", stylesheet)
        self.assertIn("QLabel#metric {", stylesheet)
        self.assertIn("QLabel#status {", stylesheet)
        self.assertIn("QLabel#section {", stylesheet)
        self.assertIn("QLabel#process {", stylesheet)
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
            status_color="#bbbbbb",
            section_color="#33ccaa",
            process_color="#eeeeee",
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
        self.assertIn("font-size: 30px;", stylesheet)
        self.assertIn("font-size: 20px;", stylesheet)
        self.assertIn("font-size: 11px;", stylesheet)
        self.assertIn("font-size: 15px;", stylesheet)
        self.assertIn("font-size: 10px;", stylesheet)


if __name__ == "__main__":
    unittest.main()
