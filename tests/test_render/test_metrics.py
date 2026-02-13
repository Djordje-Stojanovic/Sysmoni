from __future__ import annotations

import pathlib
import sys
import unittest

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from render._metrics import sanitize_non_negative, sanitize_percent  # noqa: E402


class RenderMetricsTests(unittest.TestCase):
    def test_sanitize_percent_clamps_range_and_non_finite(self) -> None:
        self.assertEqual(sanitize_percent(-1.0), 0.0)
        self.assertEqual(sanitize_percent(42.5), 42.5)
        self.assertEqual(sanitize_percent(120.0), 100.0)
        self.assertEqual(sanitize_percent(float("nan")), 0.0)
        self.assertEqual(sanitize_percent(float("inf")), 0.0)
        self.assertEqual(sanitize_percent(float("-inf")), 0.0)

    def test_sanitize_non_negative_clamps_negative_and_non_finite(self) -> None:
        self.assertEqual(sanitize_non_negative(-1.0), 0.0)
        self.assertEqual(sanitize_non_negative(12.5), 12.5)
        self.assertEqual(sanitize_non_negative(float("nan")), 0.0)
        self.assertEqual(sanitize_non_negative(float("inf")), 0.0)
        self.assertEqual(sanitize_non_negative(float("-inf")), 0.0)


if __name__ == "__main__":
    unittest.main()
