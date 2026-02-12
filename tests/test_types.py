from __future__ import annotations

import math
import pathlib
import sys
import unittest


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from core.types import SystemSnapshot  # noqa: E402


class SystemSnapshotTests(unittest.TestCase):
    def test_snapshot_normalizes_numeric_fields_to_float(self) -> None:
        snapshot = SystemSnapshot(timestamp=1, cpu_percent=2, memory_percent=3)

        self.assertEqual(snapshot.timestamp, 1.0)
        self.assertEqual(snapshot.cpu_percent, 2.0)
        self.assertEqual(snapshot.memory_percent, 3.0)
        self.assertEqual(
            snapshot.to_dict(),
            {"timestamp": 1.0, "cpu_percent": 2.0, "memory_percent": 3.0},
        )

    def test_snapshot_rejects_non_finite_timestamp(self) -> None:
        for invalid_timestamp in (math.nan, math.inf, -math.inf):
            with self.subTest(invalid_timestamp=invalid_timestamp):
                with self.assertRaises(ValueError):
                    SystemSnapshot(
                        timestamp=invalid_timestamp,
                        cpu_percent=10.0,
                        memory_percent=20.0,
                    )

    def test_snapshot_rejects_non_finite_cpu_percent(self) -> None:
        for invalid_cpu_percent in (math.nan, math.inf, -math.inf):
            with self.subTest(invalid_cpu_percent=invalid_cpu_percent):
                with self.assertRaises(ValueError):
                    SystemSnapshot(
                        timestamp=1.0,
                        cpu_percent=invalid_cpu_percent,
                        memory_percent=20.0,
                    )

    def test_snapshot_rejects_non_finite_memory_percent(self) -> None:
        for invalid_memory_percent in (math.nan, math.inf, -math.inf):
            with self.subTest(invalid_memory_percent=invalid_memory_percent):
                with self.assertRaises(ValueError):
                    SystemSnapshot(
                        timestamp=1.0,
                        cpu_percent=20.0,
                        memory_percent=invalid_memory_percent,
                    )

    def test_snapshot_rejects_cpu_percent_outside_0_to_100(self) -> None:
        for invalid_cpu_percent in (-0.1, 100.1):
            with self.subTest(invalid_cpu_percent=invalid_cpu_percent):
                with self.assertRaises(ValueError):
                    SystemSnapshot(
                        timestamp=1.0,
                        cpu_percent=invalid_cpu_percent,
                        memory_percent=20.0,
                    )

    def test_snapshot_rejects_memory_percent_outside_0_to_100(self) -> None:
        for invalid_memory_percent in (-0.1, 100.1):
            with self.subTest(invalid_memory_percent=invalid_memory_percent):
                with self.assertRaises(ValueError):
                    SystemSnapshot(
                        timestamp=1.0,
                        cpu_percent=20.0,
                        memory_percent=invalid_memory_percent,
                    )


if __name__ == "__main__":
    unittest.main()
