from __future__ import annotations

import time
from typing import Callable

from .types import SystemSnapshot

try:
    import psutil
except ImportError:
    psutil = None  # type: ignore[assignment]


def collect_snapshot(now: Callable[[], float] | None = None) -> SystemSnapshot:
    """Collect a single telemetry snapshot using psutil."""
    if psutil is None:
        raise RuntimeError(
            "psutil is required to collect telemetry. Install dependencies first."
        )

    current_time = time.time if now is None else now
    return SystemSnapshot(
        timestamp=float(current_time()),
        cpu_percent=float(psutil.cpu_percent(interval=None)),
        memory_percent=float(psutil.virtual_memory().percent),
    )
