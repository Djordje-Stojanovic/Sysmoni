from __future__ import annotations

import math
import threading
import time
from dataclasses import asdict, dataclass
from typing import Callable

from . import native_backend

try:
    import psutil
except ImportError:
    psutil = None  # type: ignore[assignment]


@dataclass(slots=True, frozen=True)
class _RawCounters:
    """Raw psutil network counters with timestamp for diffing."""

    timestamp: float
    bytes_sent: int
    bytes_recv: int
    packets_sent: int
    packets_recv: int


@dataclass(slots=True, frozen=True)
class NetworkSnapshot:
    """Single-point network telemetry sample."""

    timestamp: float
    bytes_sent_per_sec: float
    bytes_recv_per_sec: float
    packets_sent_per_sec: float
    packets_recv_per_sec: float
    total_bytes_sent: int
    total_bytes_recv: int

    def __post_init__(self) -> None:
        if isinstance(self.total_bytes_sent, bool):
            raise ValueError("total_bytes_sent must be a non-negative integer.")
        if isinstance(self.total_bytes_recv, bool):
            raise ValueError("total_bytes_recv must be a non-negative integer.")

        timestamp = float(self.timestamp)
        bytes_sent_per_sec = float(self.bytes_sent_per_sec)
        bytes_recv_per_sec = float(self.bytes_recv_per_sec)
        packets_sent_per_sec = float(self.packets_sent_per_sec)
        packets_recv_per_sec = float(self.packets_recv_per_sec)
        total_bytes_sent = int(self.total_bytes_sent)
        total_bytes_recv = int(self.total_bytes_recv)

        if not math.isfinite(timestamp):
            raise ValueError("timestamp must be a finite number.")
        if not math.isfinite(bytes_sent_per_sec):
            raise ValueError("bytes_sent_per_sec must be a finite number.")
        if not math.isfinite(bytes_recv_per_sec):
            raise ValueError("bytes_recv_per_sec must be a finite number.")
        if not math.isfinite(packets_sent_per_sec):
            raise ValueError("packets_sent_per_sec must be a finite number.")
        if not math.isfinite(packets_recv_per_sec):
            raise ValueError("packets_recv_per_sec must be a finite number.")
        if bytes_sent_per_sec < 0.0:
            raise ValueError("bytes_sent_per_sec must be non-negative.")
        if bytes_recv_per_sec < 0.0:
            raise ValueError("bytes_recv_per_sec must be non-negative.")
        if packets_sent_per_sec < 0.0:
            raise ValueError("packets_sent_per_sec must be non-negative.")
        if packets_recv_per_sec < 0.0:
            raise ValueError("packets_recv_per_sec must be non-negative.")
        if total_bytes_sent < 0:
            raise ValueError("total_bytes_sent must be a non-negative integer.")
        if total_bytes_recv < 0:
            raise ValueError("total_bytes_recv must be a non-negative integer.")

        object.__setattr__(self, "timestamp", timestamp)
        object.__setattr__(self, "bytes_sent_per_sec", bytes_sent_per_sec)
        object.__setattr__(self, "bytes_recv_per_sec", bytes_recv_per_sec)
        object.__setattr__(self, "packets_sent_per_sec", packets_sent_per_sec)
        object.__setattr__(self, "packets_recv_per_sec", packets_recv_per_sec)
        object.__setattr__(self, "total_bytes_sent", total_bytes_sent)
        object.__setattr__(self, "total_bytes_recv", total_bytes_recv)

    def to_dict(self) -> dict[str, float | int]:
        return asdict(self)


_prev_counters: _RawCounters | None = None
_prev_counters_lock = threading.Lock()


def _reset_network_state() -> None:
    """Reset module-level state. Test helper only."""
    global _prev_counters
    with _prev_counters_lock:
        _prev_counters = None


def collect_network_snapshot(
    now: Callable[[], float] | None = None,
) -> NetworkSnapshot:
    """Collect a single network telemetry snapshot using native or psutil collectors."""
    global _prev_counters
    current_time = time.time if now is None else now
    ts = float(current_time())

    raw: _RawCounters
    try:
        native_counters = native_backend.collect_network_counters()
    except RuntimeError:
        native_counters = None

    if native_counters is not None:
        raw = _RawCounters(
            timestamp=ts,
            bytes_sent=native_counters.bytes_sent,
            bytes_recv=native_counters.bytes_recv,
            packets_sent=native_counters.packets_sent,
            packets_recv=native_counters.packets_recv,
        )
    else:
        if psutil is None:
            raise RuntimeError(
                "native telemetry unavailable and psutil is missing; install dependencies "
                "or build src/telemetry/native/bin/aura_telemetry_native.dll."
            )

        counters = psutil.net_io_counters(pernic=False)
        if counters is None:
            raise RuntimeError(
                "net_io_counters() returned None. Network I/O stats may not be "
                "available on this platform."
            )

        raw = _RawCounters(
            timestamp=ts,
            bytes_sent=counters.bytes_sent,
            bytes_recv=counters.bytes_recv,
            packets_sent=counters.packets_sent,
            packets_recv=counters.packets_recv,
        )

    with _prev_counters_lock:
        prev = _prev_counters
        _prev_counters = raw

    bytes_sent_per_sec = 0.0
    bytes_recv_per_sec = 0.0
    packets_sent_per_sec = 0.0
    packets_recv_per_sec = 0.0

    if prev is not None:
        elapsed = raw.timestamp - prev.timestamp
        if elapsed > 0.0:
            d_bytes_sent = raw.bytes_sent - prev.bytes_sent
            d_bytes_recv = raw.bytes_recv - prev.bytes_recv
            d_packets_sent = raw.packets_sent - prev.packets_sent
            d_packets_recv = raw.packets_recv - prev.packets_recv

            if (
                d_bytes_sent >= 0
                and d_bytes_recv >= 0
                and d_packets_sent >= 0
                and d_packets_recv >= 0
            ):
                bytes_sent_per_sec = d_bytes_sent / elapsed
                bytes_recv_per_sec = d_bytes_recv / elapsed
                packets_sent_per_sec = d_packets_sent / elapsed
                packets_recv_per_sec = d_packets_recv / elapsed

    return NetworkSnapshot(
        timestamp=ts,
        bytes_sent_per_sec=bytes_sent_per_sec,
        bytes_recv_per_sec=bytes_recv_per_sec,
        packets_sent_per_sec=packets_sent_per_sec,
        packets_recv_per_sec=packets_recv_per_sec,
        total_bytes_sent=raw.bytes_sent,
        total_bytes_recv=raw.bytes_recv,
    )
