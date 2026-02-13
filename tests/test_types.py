from __future__ import annotations

import math
import pathlib
import sys
import unittest


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from core.types import ProcessSample, SystemSnapshot  # noqa: E402


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


class ProcessSampleTests(unittest.TestCase):
    def test_process_sample_normalizes_numeric_fields(self) -> None:
        sample = ProcessSample(
            pid=100,
            name=" python ",
            cpu_percent=12,
            memory_rss_bytes=2048,
        )

        self.assertEqual(sample.pid, 100)
        self.assertEqual(sample.name, "python")
        self.assertEqual(sample.cpu_percent, 12.0)
        self.assertEqual(sample.memory_rss_bytes, 2048)
        self.assertEqual(
            sample.to_dict(),
            {
                "pid": 100,
                "name": "python",
                "cpu_percent": 12.0,
                "memory_rss_bytes": 2048,
            },
        )

    def test_process_sample_rejects_invalid_pid(self) -> None:
        for invalid_pid in (0, -1, True):
            with self.subTest(invalid_pid=invalid_pid):
                with self.assertRaises(ValueError):
                    ProcessSample(
                        pid=invalid_pid,
                        name="python",
                        cpu_percent=1.0,
                        memory_rss_bytes=1,
                    )

    def test_process_sample_rejects_blank_name(self) -> None:
        for invalid_name in ("", "   "):
            with self.subTest(invalid_name=invalid_name):
                with self.assertRaises(ValueError):
                    ProcessSample(
                        pid=1,
                        name=invalid_name,
                        cpu_percent=1.0,
                        memory_rss_bytes=1,
                    )

    def test_process_sample_rejects_invalid_cpu_percent(self) -> None:
        for invalid_cpu_percent in (-0.1, math.nan, math.inf, -math.inf):
            with self.subTest(invalid_cpu_percent=invalid_cpu_percent):
                with self.assertRaises(ValueError):
                    ProcessSample(
                        pid=1,
                        name="python",
                        cpu_percent=invalid_cpu_percent,
                        memory_rss_bytes=1,
                    )

    def test_process_sample_rejects_invalid_memory_rss(self) -> None:
        for invalid_memory_rss in (-1, True):
            with self.subTest(invalid_memory_rss=invalid_memory_rss):
                with self.assertRaises(ValueError):
                    ProcessSample(
                        pid=1,
                        name="python",
                        cpu_percent=1.0,
                        memory_rss_bytes=invalid_memory_rss,
                    )


if __name__ == "__main__":
    unittest.main()
