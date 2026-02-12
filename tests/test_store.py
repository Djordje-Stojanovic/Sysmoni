from __future__ import annotations

import math
import pathlib
import sys
import unittest


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from core.store import TelemetryStore  # noqa: E402
from core.types import SystemSnapshot  # noqa: E402


class TelemetryStoreTests(unittest.TestCase):
    def test_store_rejects_non_positive_or_non_finite_retention(self) -> None:
        for invalid_retention in (0.0, -1.0, math.nan, math.inf, -math.inf):
            with self.subTest(invalid_retention=invalid_retention):
                with self.assertRaises(ValueError):
                    TelemetryStore(":memory:", retention_seconds=invalid_retention)

    def test_store_appends_and_reads_latest_snapshot(self) -> None:
        with TelemetryStore(":memory:", now=lambda: 100.0) as store:
            snapshot = SystemSnapshot(
                timestamp=95.0,
                cpu_percent=12.5,
                memory_percent=42.0,
            )
            store.append(snapshot)

            self.assertEqual(store.count(), 1)
            self.assertEqual(store.latest(), [snapshot])

    def test_latest_requires_positive_limit(self) -> None:
        with TelemetryStore(":memory:") as store:
            for invalid_limit in (0, -1, False):
                with self.subTest(invalid_limit=invalid_limit):
                    with self.assertRaises(ValueError):
                        store.latest(limit=invalid_limit)

    def test_latest_returns_results_in_chronological_order(self) -> None:
        with TelemetryStore(":memory:", now=lambda: 200.0) as store:
            store.append(
                SystemSnapshot(timestamp=100.0, cpu_percent=10.0, memory_percent=20.0)
            )
            store.append(
                SystemSnapshot(timestamp=102.0, cpu_percent=11.0, memory_percent=21.0)
            )
            store.append(
                SystemSnapshot(timestamp=101.0, cpu_percent=12.0, memory_percent=22.0)
            )

            snapshots = store.latest(limit=2)

        self.assertEqual([snapshot.timestamp for snapshot in snapshots], [101.0, 102.0])

    def test_append_prunes_samples_older_than_retention_window(self) -> None:
        current_time = 100.0

        def _now() -> float:
            return current_time

        with TelemetryStore(
            ":memory:",
            retention_seconds=5.0,
            now=_now,
        ) as store:
            store.append(
                SystemSnapshot(timestamp=97.0, cpu_percent=10.0, memory_percent=20.0)
            )
            store.append(
                SystemSnapshot(timestamp=100.0, cpu_percent=11.0, memory_percent=21.0)
            )
            self.assertEqual(store.count(), 2)

            current_time = 106.0
            store.append(
                SystemSnapshot(timestamp=106.0, cpu_percent=12.0, memory_percent=22.0)
            )
            snapshots = store.latest(limit=10)
            self.assertEqual(store.count(), 1)

        self.assertEqual([snapshot.timestamp for snapshot in snapshots], [106.0])


if __name__ == "__main__":
    unittest.main()
