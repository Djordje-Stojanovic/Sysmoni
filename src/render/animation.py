from __future__ import annotations

from dataclasses import dataclass
import math

DEFAULT_TARGET_FPS = 60


def _clamp_unit(value: float) -> float:
    if math.isnan(value):
        return 0.0
    return max(0.0, min(1.0, float(value)))


@dataclass(frozen=True, slots=True)
class FrameDiscipline:
    target_fps: int = DEFAULT_TARGET_FPS
    max_catchup_frames: int = 4

    def __post_init__(self) -> None:
        if self.target_fps <= 0:
            raise ValueError("target_fps must be greater than 0.")
        if self.max_catchup_frames <= 0:
            raise ValueError("max_catchup_frames must be greater than 0.")

    @property
    def frame_interval_seconds(self) -> float:
        return 1.0 / float(self.target_fps)

    @property
    def max_delta_seconds(self) -> float:
        return self.frame_interval_seconds * float(self.max_catchup_frames)

    def clamp_delta_seconds(self, delta_seconds: float) -> float:
        if math.isnan(delta_seconds) or delta_seconds <= 0.0:
            return 0.0
        return min(float(delta_seconds), self.max_delta_seconds)

    def next_frame_delay_seconds(self, elapsed_since_last_frame: float) -> float:
        elapsed = self.clamp_delta_seconds(elapsed_since_last_frame)
        return max(0.0, self.frame_interval_seconds - elapsed)


DEFAULT_FRAME_DISCIPLINE = FrameDiscipline()


def advance_phase(
    phase: float,
    delta_seconds: float,
    *,
    cycles_per_second: float = 0.35,
    discipline: FrameDiscipline = DEFAULT_FRAME_DISCIPLINE,
) -> float:
    if cycles_per_second <= 0.0:
        raise ValueError("cycles_per_second must be greater than 0.")

    normalized_phase = phase % 1.0
    clamped_delta = discipline.clamp_delta_seconds(delta_seconds)
    return (normalized_phase + (clamped_delta * cycles_per_second)) % 1.0


def compute_accent_intensity(
    cpu_percent: float,
    memory_percent: float,
    phase: float,
    *,
    floor: float = 0.15,
    ceiling: float = 0.95,
    pulse_strength: float = 0.2,
) -> float:
    if floor < 0.0 or floor > 1.0:
        raise ValueError("floor must be in the [0.0, 1.0] range.")
    if ceiling < 0.0 or ceiling > 1.0:
        raise ValueError("ceiling must be in the [0.0, 1.0] range.")
    if ceiling < floor:
        raise ValueError("ceiling must be greater than or equal to floor.")

    load_ratio = max(
        _clamp_unit(cpu_percent / 100.0),
        _clamp_unit(memory_percent / 100.0),
    )
    pulse_ratio = (math.sin((phase % 1.0) * (2.0 * math.pi)) + 1.0) * 0.5
    pulse_weight = _clamp_unit(pulse_strength)
    composite = _clamp_unit(load_ratio + ((pulse_ratio - 0.5) * pulse_weight))
    return floor + ((ceiling - floor) * composite)


__all__ = [
    "DEFAULT_FRAME_DISCIPLINE",
    "DEFAULT_TARGET_FPS",
    "FrameDiscipline",
    "advance_phase",
    "compute_accent_intensity",
]
