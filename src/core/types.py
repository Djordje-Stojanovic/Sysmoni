from __future__ import annotations

import math
from dataclasses import asdict, dataclass


@dataclass(slots=True, frozen=True)
class SystemSnapshot:
    """Single-point system telemetry sample."""

    timestamp: float
    cpu_percent: float
    memory_percent: float

    def __post_init__(self) -> None:
        timestamp = float(self.timestamp)
        cpu_percent = float(self.cpu_percent)
        memory_percent = float(self.memory_percent)

        if not math.isfinite(timestamp):
            raise ValueError("timestamp must be a finite number.")
        if not math.isfinite(cpu_percent):
            raise ValueError("cpu_percent must be a finite number.")
        if not math.isfinite(memory_percent):
            raise ValueError("memory_percent must be a finite number.")
        if not 0.0 <= cpu_percent <= 100.0:
            raise ValueError("cpu_percent must be between 0 and 100.")
        if not 0.0 <= memory_percent <= 100.0:
            raise ValueError("memory_percent must be between 0 and 100.")

        object.__setattr__(self, "timestamp", timestamp)
        object.__setattr__(self, "cpu_percent", cpu_percent)
        object.__setattr__(self, "memory_percent", memory_percent)

    def to_dict(self) -> dict[str, float]:
        return asdict(self)
