from __future__ import annotations

import math
import sqlite3
import threading
import time
from pathlib import Path
from typing import Callable

from .types import SystemSnapshot

DEFAULT_RETENTION_SECONDS = 24.0 * 60.0 * 60.0


def _require_positive_finite_retention(retention_seconds: float) -> float:
    normalized_retention = float(retention_seconds)
    if not math.isfinite(normalized_retention) or normalized_retention <= 0:
        raise ValueError("retention_seconds must be a finite number greater than 0.")
    return normalized_retention


def _require_positive_limit(limit: int) -> int:
    if isinstance(limit, bool):
        raise ValueError("limit must be an integer greater than 0.")

    normalized_limit = int(limit)
    if normalized_limit <= 0:
        raise ValueError("limit must be an integer greater than 0.")
    return normalized_limit


def _prepare_db_path(db_path: str | Path) -> str:
    normalized_path = str(db_path)
    if normalized_path == ":memory:":
        return normalized_path

    Path(normalized_path).parent.mkdir(parents=True, exist_ok=True)
    return normalized_path


def _is_legacy_snapshots_schema(columns: list[sqlite3.Row]) -> bool:
    column_names = {str(column["name"]) for column in columns}
    if column_names != {"timestamp", "cpu_percent", "memory_percent"}:
        return False

    timestamp_column = next(column for column in columns if column["name"] == "timestamp")
    return int(timestamp_column["pk"]) == 1


class TelemetryStore:
    """SQLite-backed snapshot storage with rolling retention pruning."""

    def __init__(
        self,
        db_path: str | Path,
        *,
        retention_seconds: float = DEFAULT_RETENTION_SECONDS,
        now: Callable[[], float] | None = None,
    ) -> None:
        self._retention_seconds = _require_positive_finite_retention(retention_seconds)
        self._now = time.time if now is None else now
        self._lock = threading.Lock()
        self._connection = sqlite3.connect(
            _prepare_db_path(db_path),
            check_same_thread=False,
        )
        self._connection.row_factory = sqlite3.Row

        self._initialize_schema()

    def _initialize_schema(self) -> None:
        with self._connection:
            columns = self._connection.execute("PRAGMA table_info(snapshots)").fetchall()
            if not columns:
                self._create_snapshots_table()
            elif _is_legacy_snapshots_schema(columns):
                self._migrate_legacy_snapshots_table()
            elif not any(column["name"] == "id" for column in columns):
                raise RuntimeError(
                    "Unsupported snapshots table schema: expected either legacy schema "
                    "or id-backed schema."
                )

            self._connection.execute(
                "CREATE INDEX IF NOT EXISTS idx_snapshots_timestamp ON snapshots (timestamp)"
            )

    def _create_snapshots_table(self) -> None:
        self._connection.execute(
            """
            CREATE TABLE snapshots (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp REAL NOT NULL,
                cpu_percent REAL NOT NULL,
                memory_percent REAL NOT NULL
            )
            """
        )

    def _migrate_legacy_snapshots_table(self) -> None:
        self._connection.execute("ALTER TABLE snapshots RENAME TO snapshots_legacy")
        self._create_snapshots_table()
        self._connection.execute(
            """
            INSERT INTO snapshots (
                timestamp,
                cpu_percent,
                memory_percent
            )
            SELECT
                timestamp,
                cpu_percent,
                memory_percent
            FROM snapshots_legacy
            ORDER BY timestamp ASC
            """
        )
        self._connection.execute("DROP TABLE snapshots_legacy")

    def append(self, snapshot: SystemSnapshot) -> None:
        with self._lock, self._connection:
            self._connection.execute(
                """
                INSERT INTO snapshots (
                    timestamp,
                    cpu_percent,
                    memory_percent
                )
                VALUES (?, ?, ?)
                """,
                (snapshot.timestamp, snapshot.cpu_percent, snapshot.memory_percent),
            )
            cutoff = float(self._now()) - self._retention_seconds
            self._connection.execute(
                "DELETE FROM snapshots WHERE timestamp < ?",
                (cutoff,),
            )

    def latest(self, *, limit: int = 1) -> list[SystemSnapshot]:
        normalized_limit = _require_positive_limit(limit)

        with self._lock:
            rows = self._connection.execute(
                """
                SELECT timestamp, cpu_percent, memory_percent
                FROM snapshots
                ORDER BY timestamp DESC, id DESC
                LIMIT ?
                """,
                (normalized_limit,),
            ).fetchall()

        rows.reverse()
        return [
            SystemSnapshot(
                timestamp=row["timestamp"],
                cpu_percent=row["cpu_percent"],
                memory_percent=row["memory_percent"],
            )
            for row in rows
        ]

    def count(self) -> int:
        with self._lock:
            row = self._connection.execute("SELECT COUNT(*) AS total FROM snapshots").fetchone()

        if row is None:
            return 0
        return int(row["total"])

    def close(self) -> None:
        with self._lock:
            self._connection.close()

    def __enter__(self) -> "TelemetryStore":
        return self

    def __exit__(self, exc_type, exc, traceback) -> None:
        self.close()
