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
            self._connection.execute(
                """
                CREATE TABLE IF NOT EXISTS snapshots (
                    timestamp REAL PRIMARY KEY,
                    cpu_percent REAL NOT NULL,
                    memory_percent REAL NOT NULL
                )
                """
            )

    def append(self, snapshot: SystemSnapshot) -> None:
        with self._lock, self._connection:
            self._connection.execute(
                """
                INSERT OR REPLACE INTO snapshots (
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
                ORDER BY timestamp DESC
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
