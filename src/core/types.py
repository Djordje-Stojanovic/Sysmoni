from __future__ import annotations

from dataclasses import asdict, dataclass


@dataclass(slots=True, frozen=True)
class SystemSnapshot:
    """Single-point system telemetry sample."""

    timestamp: float
    cpu_percent: float
    memory_percent: float

    def to_dict(self) -> dict[str, float]:
        return asdict(self)
