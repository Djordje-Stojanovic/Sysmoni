from __future__ import annotations

from dataclasses import dataclass

from .animation import (
    DEFAULT_FRAME_DISCIPLINE,
    FrameDiscipline,
    advance_phase,
    compute_accent_intensity,
)


@dataclass(frozen=True, slots=True)
class CockpitFrameState:
    phase: float
    accent_intensity: float
    next_delay_seconds: float


def compose_cockpit_frame(
    *,
    previous_phase: float,
    elapsed_since_last_frame: float,
    cpu_percent: float,
    memory_percent: float,
    discipline: FrameDiscipline = DEFAULT_FRAME_DISCIPLINE,
    pulse_hz: float = 0.35,
) -> CockpitFrameState:
    phase = advance_phase(
        previous_phase,
        elapsed_since_last_frame,
        cycles_per_second=pulse_hz,
        discipline=discipline,
    )
    accent_intensity = compute_accent_intensity(
        cpu_percent,
        memory_percent,
        phase,
    )
    next_delay_seconds = discipline.next_frame_delay_seconds(elapsed_since_last_frame)
    return CockpitFrameState(
        phase=phase,
        accent_intensity=accent_intensity,
        next_delay_seconds=next_delay_seconds,
    )


__all__ = ["CockpitFrameState", "compose_cockpit_frame"]
