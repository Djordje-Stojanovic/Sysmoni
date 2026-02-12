from __future__ import annotations

import threading
import time
from typing import Callable

from .types import SystemSnapshot

try:
    import psutil
except ImportError:
    psutil = None  # type: ignore[assignment]

_cpu_percent_primed = False
_cpu_prime_lock = threading.Lock()


def collect_snapshot(now: Callable[[], float] | None = None) -> SystemSnapshot:
    """Collect a single telemetry snapshot using psutil."""
    if psutil is None:
        raise RuntimeError(
            "psutil is required to collect telemetry. Install dependencies first."
        )

    global _cpu_percent_primed
    current_time = time.time if now is None else now

    if _cpu_percent_primed:
        cpu_percent = float(psutil.cpu_percent(interval=None))
    else:
        with _cpu_prime_lock:
            if _cpu_percent_primed:
                cpu_percent = float(psutil.cpu_percent(interval=None))
            else:
                cpu_percent = float(psutil.cpu_percent(interval=0.1))
                _cpu_percent_primed = True

    return SystemSnapshot(
        timestamp=float(current_time()),
        cpu_percent=cpu_percent,
        memory_percent=float(psutil.virtual_memory().percent),
    )


def run_polling_loop(
    interval_seconds: float,
    on_snapshot: Callable[[SystemSnapshot], None],
    *,
    stop_event: threading.Event,
    collect: Callable[[], SystemSnapshot] | None = None,
    monotonic: Callable[[], float] | None = None,
) -> int:
    """Collect snapshots at a fixed interval until the stop event is set."""
    if interval_seconds <= 0:
        raise ValueError("interval_seconds must be greater than 0")

    def _wait(timeout_seconds: float) -> None:
        stop_event.wait(timeout_seconds)

    return poll_snapshots(
        on_snapshot,
        interval_seconds=interval_seconds,
        should_stop=stop_event.is_set,
        sleep=_wait,
        monotonic=monotonic,
        collect=collect,
    )


def poll_snapshots(
    on_snapshot: Callable[[SystemSnapshot], None],
    *,
    interval_seconds: float = 1.0,
    should_stop: Callable[[], bool] | None = None,
    sleep: Callable[[float], None] | None = None,
    monotonic: Callable[[], float] | None = None,
    collect: Callable[[], SystemSnapshot] | None = None,
) -> int:
    """Collect snapshots at a fixed interval until `should_stop` returns True."""
    if interval_seconds <= 0:
        raise ValueError("interval_seconds must be greater than 0.")

    stop = (lambda: False) if should_stop is None else should_stop
    sleeper = time.sleep if sleep is None else sleep
    clock = time.monotonic if monotonic is None else monotonic
    collect_snapshot_fn = collect or collect_snapshot
    emitted = 0

    while not stop():
        cycle_started_at = clock()
        on_snapshot(collect_snapshot_fn())
        emitted += 1

        remaining = interval_seconds - (clock() - cycle_started_at)
        if remaining > 0 and not stop():
            sleeper(remaining)

    return emitted
