from __future__ import annotations

import math
import threading
import time
from dataclasses import asdict, dataclass
from typing import Callable

try:
    import psutil
except ImportError:
    psutil = None  # type: ignore[assignment]


@dataclass(slots=True, frozen=True)
class _RawDiskCounters:
    """Raw psutil disk counters with timestamp for diffing."""

    timestamp: float
    read_bytes: int
    write_bytes: int
    read_count: int
    write_count: int


@dataclass(slots=True, frozen=True)
class DiskSnapshot:
    """Single-point disk I/O telemetry sample."""

    timestamp: float
    read_bytes_per_sec: float
    write_bytes_per_sec: float
    read_ops_per_sec: float
    write_ops_per_sec: float
    total_read_bytes: int
    total_write_bytes: int

    def __post_init__(self) -> None:
        if isinstance(self.total_read_bytes, bool):
            raise ValueError("total_read_bytes must be a non-negative integer.")
        if isinstance(self.total_write_bytes, bool):
            raise ValueError("total_write_bytes must be a non-negative integer.")

        timestamp = float(self.timestamp)
        read_bytes_per_sec = float(self.read_bytes_per_sec)
        write_bytes_per_sec = float(self.write_bytes_per_sec)
        read_ops_per_sec = float(self.read_ops_per_sec)
        write_ops_per_sec = float(self.write_ops_per_sec)
        total_read_bytes = int(self.total_read_bytes)
        total_write_bytes = int(self.total_write_bytes)

        if not math.isfinite(timestamp):
            raise ValueError("timestamp must be a finite number.")
        if not math.isfinite(read_bytes_per_sec):
            raise ValueError("read_bytes_per_sec must be a finite number.")
        if not math.isfinite(write_bytes_per_sec):
            raise ValueError("write_bytes_per_sec must be a finite number.")
        if not math.isfinite(read_ops_per_sec):
            raise ValueError("read_ops_per_sec must be a finite number.")
        if not math.isfinite(write_ops_per_sec):
            raise ValueError("write_ops_per_sec must be a finite number.")
        if read_bytes_per_sec < 0.0:
            raise ValueError("read_bytes_per_sec must be non-negative.")
        if write_bytes_per_sec < 0.0:
            raise ValueError("write_bytes_per_sec must be non-negative.")
        if read_ops_per_sec < 0.0:
            raise ValueError("read_ops_per_sec must be non-negative.")
        if write_ops_per_sec < 0.0:
            raise ValueError("write_ops_per_sec must be non-negative.")
        if total_read_bytes < 0:
            raise ValueError("total_read_bytes must be a non-negative integer.")
        if total_write_bytes < 0:
            raise ValueError("total_write_bytes must be a non-negative integer.")

        object.__setattr__(self, "timestamp", timestamp)
        object.__setattr__(self, "read_bytes_per_sec", read_bytes_per_sec)
        object.__setattr__(self, "write_bytes_per_sec", write_bytes_per_sec)
        object.__setattr__(self, "read_ops_per_sec", read_ops_per_sec)
        object.__setattr__(self, "write_ops_per_sec", write_ops_per_sec)
        object.__setattr__(self, "total_read_bytes", total_read_bytes)
        object.__setattr__(self, "total_write_bytes", total_write_bytes)

    def to_dict(self) -> dict[str, float | int]:
        return asdict(self)


_prev_disk_counters: _RawDiskCounters | None = None
_prev_disk_counters_lock = threading.Lock()


def _reset_disk_state() -> None:
    """Reset module-level state. Test helper only."""
    global _prev_disk_counters
    with _prev_disk_counters_lock:
        _prev_disk_counters = None


def collect_disk_snapshot(
    now: Callable[[], float] | None = None,
) -> DiskSnapshot:
    """Collect a single disk I/O telemetry snapshot using psutil."""
    if psutil is None:
        raise RuntimeError(
            "psutil is required to collect telemetry. Install dependencies first."
        )

    global _prev_disk_counters
    current_time = time.time if now is None else now
    ts = float(current_time())

    counters = psutil.disk_io_counters(perdisk=False)
    if counters is None:
        raise RuntimeError(
            "disk_io_counters() returned None. Disk I/O stats may not be "
            "available on this platform."
        )

    raw = _RawDiskCounters(
        timestamp=ts,
        read_bytes=counters.read_bytes,
        write_bytes=counters.write_bytes,
        read_count=counters.read_count,
        write_count=counters.write_count,
    )

    with _prev_disk_counters_lock:
        prev = _prev_disk_counters
        _prev_disk_counters = raw

    read_bytes_per_sec = 0.0
    write_bytes_per_sec = 0.0
    read_ops_per_sec = 0.0
    write_ops_per_sec = 0.0

    if prev is not None:
        elapsed = raw.timestamp - prev.timestamp
        if elapsed > 0.0:
            d_read_bytes = raw.read_bytes - prev.read_bytes
            d_write_bytes = raw.write_bytes - prev.write_bytes
            d_read_count = raw.read_count - prev.read_count
            d_write_count = raw.write_count - prev.write_count

            if d_read_bytes >= 0 and d_write_bytes >= 0 and d_read_count >= 0 and d_write_count >= 0:
                read_bytes_per_sec = d_read_bytes / elapsed
                write_bytes_per_sec = d_write_bytes / elapsed
                read_ops_per_sec = d_read_count / elapsed
                write_ops_per_sec = d_write_count / elapsed

    return DiskSnapshot(
        timestamp=ts,
        read_bytes_per_sec=read_bytes_per_sec,
        write_bytes_per_sec=write_bytes_per_sec,
        read_ops_per_sec=read_ops_per_sec,
        write_ops_per_sec=write_ops_per_sec,
        total_read_bytes=raw.read_bytes,
        total_write_bytes=raw.write_bytes,
    )
