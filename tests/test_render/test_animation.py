from __future__ import annotations

import pathlib
import sys
import unittest

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from render.animation import (  # noqa: E402
    DEFAULT_FRAME_DISCIPLINE,
    FrameDiscipline,
    advance_phase,
    compute_accent_intensity,
)


class RenderAnimationTests(unittest.TestCase):
    def test_default_frame_discipline_targets_60_fps(self) -> None:
        self.assertEqual(DEFAULT_FRAME_DISCIPLINE.target_fps, 60)
        self.assertAlmostEqual(DEFAULT_FRAME_DISCIPLINE.frame_interval_seconds, 1.0 / 60.0)
        self.assertAlmostEqual(DEFAULT_FRAME_DISCIPLINE.max_delta_seconds, 4.0 / 60.0)

    def test_frame_discipline_validates_positive_values(self) -> None:
        with self.assertRaises(ValueError):
            FrameDiscipline(target_fps=0)
        with self.assertRaises(ValueError):
            FrameDiscipline(max_catchup_frames=0)

    def test_frame_discipline_clamps_delta_and_exposes_next_delay(self) -> None:
        discipline = FrameDiscipline(target_fps=50, max_catchup_frames=2)

        self.assertEqual(discipline.clamp_delta_seconds(-1.0), 0.0)
        self.assertAlmostEqual(discipline.clamp_delta_seconds(0.5), 0.04, places=6)
        self.assertAlmostEqual(
            discipline.next_frame_delay_seconds(0.005),
            0.015,
            places=6,
        )
        self.assertEqual(discipline.next_frame_delay_seconds(0.5), 0.0)

    def test_advance_phase_wraps_and_uses_clamped_delta(self) -> None:
        discipline = FrameDiscipline(target_fps=60, max_catchup_frames=2)

        wrapped = advance_phase(
            0.99,
            0.5,
            cycles_per_second=1.0,
            discipline=discipline,
        )

        self.assertAlmostEqual(wrapped, 0.0233333333, places=6)
        with self.assertRaises(ValueError):
            advance_phase(0.1, 0.016, cycles_per_second=0.0, discipline=discipline)

    def test_compute_accent_intensity_stays_bounded_and_tracks_load(self) -> None:
        low_load = compute_accent_intensity(5.0, 10.0, phase=0.25)
        high_load = compute_accent_intensity(95.0, 85.0, phase=0.25)
        pulse_high = compute_accent_intensity(40.0, 40.0, phase=0.25, pulse_strength=0.2)
        pulse_low = compute_accent_intensity(40.0, 40.0, phase=0.75, pulse_strength=0.2)

        self.assertGreater(high_load, low_load)
        self.assertGreater(pulse_high, pulse_low)
        self.assertGreaterEqual(low_load, 0.15)
        self.assertLessEqual(high_load, 0.95)

    def test_compute_accent_intensity_validates_floor_and_ceiling(self) -> None:
        with self.assertRaises(ValueError):
            compute_accent_intensity(10.0, 20.0, phase=0.0, floor=-0.1)
        with self.assertRaises(ValueError):
            compute_accent_intensity(10.0, 20.0, phase=0.0, ceiling=1.5)
        with self.assertRaises(ValueError):
            compute_accent_intensity(10.0, 20.0, phase=0.0, floor=0.8, ceiling=0.2)


if __name__ == "__main__":
    unittest.main()
