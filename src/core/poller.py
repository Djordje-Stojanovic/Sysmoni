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
