from __future__ import annotations

import math
from dataclasses import asdict, dataclass


@dataclass(slots=True, frozen=True)
class ProcessSample:
    """Single-point process telemetry sample."""

    pid: int
    name: str
    cpu_percent: float
    memory_rss_bytes: int

    def __post_init__(self) -> None:
        if isinstance(self.pid, bool):
            raise ValueError("pid must be an integer greater than 0.")
        if isinstance(self.memory_rss_bytes, bool):
            raise ValueError("memory_rss_bytes must be a non-negative integer.")

        pid = int(self.pid)
        name = str(self.name).strip()
        cpu_percent = float(self.cpu_percent)
        memory_rss_bytes = int(self.memory_rss_bytes)

        if pid <= 0:
            raise ValueError("pid must be an integer greater than 0.")
        if not name:
            raise ValueError("name must be a non-empty string.")
        if not math.isfinite(cpu_percent):
            raise ValueError("cpu_percent must be a finite number.")
        if cpu_percent < 0.0:
            raise ValueError("cpu_percent must be greater than or equal to 0.")
        if memory_rss_bytes < 0:
            raise ValueError("memory_rss_bytes must be a non-negative integer.")

        object.__setattr__(self, "pid", pid)
        object.__setattr__(self, "name", name)
        object.__setattr__(self, "cpu_percent", cpu_percent)
        object.__setattr__(self, "memory_rss_bytes", memory_rss_bytes)

    def to_dict(self) -> dict[str, float | int | str]:
        return asdict(self)


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
