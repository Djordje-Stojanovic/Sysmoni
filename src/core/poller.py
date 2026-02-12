from __future__ import annotations

import time
from typing import Callable

from .types import SystemSnapshot

try:
    import psutil
except ImportError:
    psutil = None  # type: ignore[assignment]

_cpu_percent_primed = False


def collect_snapshot(now: Callable[[], float] | None = None) -> SystemSnapshot:
    """Collect a single telemetry snapshot using psutil."""
    if psutil is None:
        raise RuntimeError(
            "psutil is required to collect telemetry. Install dependencies first."
        )

    global _cpu_percent_primed
    current_time = time.time if now is None else now
    cpu_interval: float | None = None if _cpu_percent_primed else 0.1
    cpu_percent = float(psutil.cpu_percent(interval=cpu_interval))
    _cpu_percent_primed = True

    return SystemSnapshot(
        timestamp=float(current_time()),
        cpu_percent=cpu_percent,
        memory_percent=float(psutil.virtual_memory().percent),
    )
