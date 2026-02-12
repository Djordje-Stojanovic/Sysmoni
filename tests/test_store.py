from __future__ import annotations

import math
import pathlib
import sqlite3
import shutil
import sys
import unittest
import uuid
from contextlib import closing


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

    def test_store_creates_missing_parent_directories(self) -> None:
        tmpdir = PROJECT_ROOT / f"__store_test_{uuid.uuid4().hex}"
        tmpdir.mkdir()
        self.addCleanup(shutil.rmtree, tmpdir, True)

        db_path = tmpdir / "nested" / "telemetry.sqlite"
        self.assertFalse(db_path.parent.exists())

        with TelemetryStore(db_path, now=lambda: 100.0) as store:
            store.append(
                SystemSnapshot(
                    timestamp=95.0,
                    cpu_percent=12.5,
                    memory_percent=42.0,
                )
            )
            self.assertEqual(store.count(), 1)

        self.assertTrue(db_path.parent.exists())
        self.assertTrue(db_path.exists())

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

    def test_store_retains_duplicate_timestamp_samples(self) -> None:
        with TelemetryStore(":memory:", now=lambda: 200.0) as store:
            first = SystemSnapshot(timestamp=100.0, cpu_percent=10.0, memory_percent=20.0)
            second = SystemSnapshot(timestamp=100.0, cpu_percent=11.0, memory_percent=21.0)

            store.append(first)
            store.append(second)
            snapshots = store.latest(limit=10)
            self.assertEqual(store.count(), 2)

        self.assertEqual(
            [snapshot.to_dict() for snapshot in snapshots],
            [first.to_dict(), second.to_dict()],
        )

    def test_store_migrates_legacy_timestamp_primary_key_schema(self) -> None:
        tmpdir = PROJECT_ROOT / f"__store_legacy_{uuid.uuid4().hex}"
        tmpdir.mkdir()
        self.addCleanup(shutil.rmtree, tmpdir, True)

        db_path = tmpdir / "telemetry.sqlite"
        with closing(sqlite3.connect(db_path)) as connection:
            with connection:
                connection.execute(
                    """
                    CREATE TABLE snapshots (
                        timestamp REAL PRIMARY KEY,
                        cpu_percent REAL NOT NULL,
                        memory_percent REAL NOT NULL
                    )
                    """
                )
                connection.execute(
                    """
                    INSERT INTO snapshots (timestamp, cpu_percent, memory_percent)
                    VALUES (?, ?, ?)
                    """,
                    (100.0, 10.0, 20.0),
                )

        with TelemetryStore(db_path, now=lambda: 200.0) as store:
            store.append(
                SystemSnapshot(timestamp=101.0, cpu_percent=11.0, memory_percent=21.0)
            )
            store.append(
                SystemSnapshot(timestamp=101.0, cpu_percent=12.0, memory_percent=22.0)
            )
            snapshots = store.latest(limit=10)
            self.assertEqual(store.count(), 3)

        with closing(sqlite3.connect(db_path)) as connection:
            columns = connection.execute("PRAGMA table_info(snapshots)").fetchall()

        primary_key_columns = [column[1] for column in columns if column[5] == 1]
        self.assertEqual(primary_key_columns, ["id"])
        self.assertEqual([snapshot.timestamp for snapshot in snapshots], [100.0, 101.0, 101.0])
        self.assertEqual(
            [snapshot.cpu_percent for snapshot in snapshots],
            [10.0, 11.0, 12.0],
        )

    def test_store_rejects_malformed_id_backed_schema_at_startup(self) -> None:
        tmpdir = PROJECT_ROOT / f"__store_invalid_{uuid.uuid4().hex}"
        tmpdir.mkdir()
        self.addCleanup(shutil.rmtree, tmpdir, True)

        db_path = tmpdir / "telemetry.sqlite"
        with closing(sqlite3.connect(db_path)) as connection:
            with connection:
                connection.execute(
                    """
                    CREATE TABLE snapshots (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        timestamp REAL NOT NULL
                    )
                    """
                )

        with self.assertRaises(RuntimeError) as ctx:
            TelemetryStore(db_path)

        self.assertIn("Unsupported snapshots table schema", str(ctx.exception))
        db_path.unlink()
        self.assertFalse(db_path.exists())

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

    def test_between_returns_snapshots_within_inclusive_range(self) -> None:
        with TelemetryStore(":memory:", now=lambda: 200.0) as store:
            store.append(
                SystemSnapshot(timestamp=100.0, cpu_percent=10.0, memory_percent=20.0)
            )
            store.append(
                SystemSnapshot(timestamp=101.0, cpu_percent=11.0, memory_percent=21.0)
            )
            store.append(
                SystemSnapshot(timestamp=102.0, cpu_percent=12.0, memory_percent=22.0)
            )

            snapshots = store.between(start_timestamp=100.5, end_timestamp=102.0)

        self.assertEqual([snapshot.timestamp for snapshot in snapshots], [101.0, 102.0])

    def test_between_rejects_invalid_timestamps(self) -> None:
        with TelemetryStore(":memory:") as store:
            with self.assertRaises(ValueError):
                store.between(start_timestamp=math.nan)

            with self.assertRaises(ValueError):
                store.between(end_timestamp=math.inf)

            with self.assertRaises(ValueError):
                store.between(start_timestamp=5.0, end_timestamp=4.0)

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
