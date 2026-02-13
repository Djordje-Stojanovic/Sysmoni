from __future__ import annotations

import math
import threading
import time
from typing import Callable

from contracts.types import ProcessSample, SystemSnapshot

try:
    import psutil
except ImportError:
    psutil = None  # type: ignore[assignment]

_cpu_percent_primed = False
_cpu_prime_lock = threading.Lock()
_process_name_cache: dict[int, str] = {}
_process_name_cache_lock = threading.Lock()


def _require_positive_finite_interval(interval_seconds: float) -> float:
    if isinstance(interval_seconds, bool):
        raise ValueError("interval_seconds must be a finite number greater than 0.")

    normalized_interval = float(interval_seconds)
    if not math.isfinite(normalized_interval) or normalized_interval <= 0:
        raise ValueError("interval_seconds must be a finite number greater than 0.")
    return normalized_interval


def _require_positive_process_limit(limit: int) -> int:
    if isinstance(limit, bool):
        raise ValueError("limit must be an integer greater than 0.")

    normalized_limit = int(limit)
    if normalized_limit <= 0:
        raise ValueError("limit must be an integer greater than 0.")
    return normalized_limit


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


def collect_top_processes(*, limit: int = 20) -> list[ProcessSample]:
    """Collect top process snapshots sorted by CPU and RSS memory usage."""
    normalized_limit = _require_positive_process_limit(limit)

    if psutil is None:
        raise RuntimeError(
            "psutil is required to collect telemetry. Install dependencies first."
        )

    process_errors: tuple[type[BaseException], ...] = tuple(
        process_error
        for process_error in (
            getattr(psutil, "NoSuchProcess", None),
            getattr(psutil, "AccessDenied", None),
            getattr(psutil, "ZombieProcess", None),
        )
        if isinstance(process_error, type) and issubclass(process_error, BaseException)
    )
    recoverable_errors = process_errors + (AttributeError, TypeError, ValueError)

    samples: list[ProcessSample] = []
    observed_pids: set[int] = set()
    for process in psutil.process_iter(
        attrs=["pid", "name", "cpu_percent", "memory_info"],
    ):
        try:
            info = process.info
            if not isinstance(info, dict):
                raise TypeError("psutil process info must be a dict.")

            pid = int(info["pid"])
            raw_name = info.get("name")
            explicit_name = str(raw_name).strip() if raw_name is not None else ""
            if explicit_name:
                name = explicit_name
            else:
                with _process_name_cache_lock:
                    cached_name = _process_name_cache.get(pid)
                name = cached_name if cached_name is not None else f"pid-{pid}"

            raw_cpu_percent = info.get("cpu_percent")
            cpu_percent = 0.0 if raw_cpu_percent is None else float(raw_cpu_percent)

            memory_info = info.get("memory_info")
            memory_rss_bytes = int(getattr(memory_info, "rss", 0))

            sample = ProcessSample(
                pid=pid,
                name=name,
                cpu_percent=cpu_percent,
                memory_rss_bytes=memory_rss_bytes,
            )
            samples.append(sample)
            observed_pids.add(sample.pid)
            if explicit_name:
                with _process_name_cache_lock:
                    _process_name_cache[sample.pid] = explicit_name
        except recoverable_errors:
            continue

    with _process_name_cache_lock:
        stale_pids = [
            cached_pid
            for cached_pid in _process_name_cache
            if cached_pid not in observed_pids
        ]
        for stale_pid in stale_pids:
            del _process_name_cache[stale_pid]

    samples.sort(
        key=lambda sample: (
            -sample.cpu_percent,
            -sample.memory_rss_bytes,
            sample.pid,
        )
    )
    return samples[:normalized_limit]


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
