from __future__ import annotations

import math
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


def _require_positive_finite_interval(interval_seconds: float) -> float:
    if isinstance(interval_seconds, bool):
        raise ValueError("interval_seconds must be a finite number greater than 0.")

    normalized_interval = float(interval_seconds)
    if not math.isfinite(normalized_interval) or normalized_interval <= 0:
        raise ValueError("interval_seconds must be a finite number greater than 0.")
    return normalized_interval


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
    on_error: Callable[[Exception], None] | None = None,
    continue_on_error: bool = False,
) -> int:
    """Collect snapshots at a fixed interval until the stop event is set."""
    interval_seconds = _require_positive_finite_interval(interval_seconds)

    def _wait(timeout_seconds: float) -> None:
        stop_event.wait(timeout_seconds)

    return poll_snapshots(
        on_snapshot,
        interval_seconds=interval_seconds,
        should_stop=stop_event.is_set,
        sleep=_wait,
        monotonic=monotonic,
        collect=collect,
        on_error=on_error,
        continue_on_error=continue_on_error,
    )


def poll_snapshots(
    on_snapshot: Callable[[SystemSnapshot], None],
    *,
    interval_seconds: float = 1.0,
    should_stop: Callable[[], bool] | None = None,
    sleep: Callable[[float], None] | None = None,
    monotonic: Callable[[], float] | None = None,
    collect: Callable[[], SystemSnapshot] | None = None,
    on_error: Callable[[Exception], None] | None = None,
    continue_on_error: bool = False,
) -> int:
    """Collect snapshots at a fixed interval until `should_stop` returns True."""
    interval_seconds = _require_positive_finite_interval(interval_seconds)

    stop = (lambda: False) if should_stop is None else should_stop
    sleeper = time.sleep if sleep is None else sleep
    clock = time.monotonic if monotonic is None else monotonic
    collect_snapshot_fn = collect or collect_snapshot
    emitted = 0

    while not stop():
        cycle_started_at = clock()
        try:
            on_snapshot(collect_snapshot_fn())
            emitted += 1
        except Exception as exc:
            if on_error is not None:
                on_error(exc)
            if not continue_on_error:
                raise

        remaining = interval_seconds - (clock() - cycle_started_at)
        if remaining > 0 and not stop():
            sleeper(remaining)

    return emitted
