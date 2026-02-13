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


def _normalize_optional_finite_timestamp(
    timestamp: float | None,
    *,
    field_name: str,
) -> float | None:
    if timestamp is None:
        return None

    normalized_timestamp = float(timestamp)
    if not math.isfinite(normalized_timestamp):
        raise ValueError(f"{field_name} must be a finite number.")
    return normalized_timestamp


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


def _is_supported_id_backed_schema(columns: list[sqlite3.Row]) -> bool:
    column_names = {str(column["name"]) for column in columns}
    required_columns = {"id", "timestamp", "cpu_percent", "memory_percent"}
    if not required_columns.issubset(column_names):
        return False

    id_column = next(column for column in columns if column["name"] == "id")
    return int(id_column["pk"]) == 1


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

        try:
            self._initialize_schema()
            with self._lock, self._connection:
                self._prune_expired_locked()
        except Exception:
            self._connection.close()
            raise

    def _initialize_schema(self) -> None:
        with self._connection:
            columns = self._connection.execute("PRAGMA table_info(snapshots)").fetchall()
            if not columns:
                self._create_snapshots_table()
            elif _is_legacy_snapshots_schema(columns):
                self._migrate_legacy_snapshots_table()
            elif not _is_supported_id_backed_schema(columns):
                raise RuntimeError(
                    "Unsupported snapshots table schema: expected either legacy schema "
                    "or id-backed schema with primary key `id` and telemetry columns."
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

    def _prune_expired_locked(self) -> None:
        cutoff = float(self._now()) - self._retention_seconds
        self._connection.execute(
            "DELETE FROM snapshots WHERE timestamp < ?",
            (cutoff,),
        )

    def _insert_snapshot_locked(self, snapshot: SystemSnapshot) -> None:
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

    def _count_rows_locked(self) -> int:
        row = self._connection.execute("SELECT COUNT(*) AS total FROM snapshots").fetchone()
        if row is None:
            return 0
        return int(row["total"])

    def append(self, snapshot: SystemSnapshot) -> None:
        with self._lock, self._connection:
            self._insert_snapshot_locked(snapshot)
            self._prune_expired_locked()

    def append_and_count(self, snapshot: SystemSnapshot) -> int:
        with self._lock, self._connection:
            self._insert_snapshot_locked(snapshot)
            self._prune_expired_locked()
            return self._count_rows_locked()

    def latest(self, *, limit: int = 1) -> list[SystemSnapshot]:
        normalized_limit = _require_positive_limit(limit)

        with self._lock, self._connection:
            self._prune_expired_locked()
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

    def between(
        self,
        *,
        start_timestamp: float | None = None,
        end_timestamp: float | None = None,
    ) -> list[SystemSnapshot]:
        normalized_start = _normalize_optional_finite_timestamp(
            start_timestamp,
            field_name="start_timestamp",
        )
        normalized_end = _normalize_optional_finite_timestamp(
            end_timestamp,
            field_name="end_timestamp",
        )

        if (
            normalized_start is not None
            and normalized_end is not None
            and normalized_start > normalized_end
        ):
            raise ValueError(
                "start_timestamp must be less than or equal to end_timestamp."
            )

        filters: list[str] = []
        params: list[float] = []
        if normalized_start is not None:
            filters.append("timestamp >= ?")
            params.append(normalized_start)
        if normalized_end is not None:
            filters.append("timestamp <= ?")
            params.append(normalized_end)

        where_clause = ""
        if filters:
            where_clause = "WHERE " + " AND ".join(filters)

        with self._lock, self._connection:
            self._prune_expired_locked()
            rows = self._connection.execute(
                f"""
                SELECT timestamp, cpu_percent, memory_percent
                FROM snapshots
                {where_clause}
                ORDER BY timestamp ASC, id ASC
                """,
                tuple(params),
            ).fetchall()

        return [
            SystemSnapshot(
                timestamp=row["timestamp"],
                cpu_percent=row["cpu_percent"],
                memory_percent=row["memory_percent"],
            )
            for row in rows
        ]

    def count(self) -> int:
        with self._lock, self._connection:
            self._prune_expired_locked()
            return self._count_rows_locked()

    def close(self) -> None:
        with self._lock:
            self._connection.close()

    def __enter__(self) -> "TelemetryStore":
        return self

    def __exit__(self, exc_type, exc, traceback) -> None:
        self.close()
