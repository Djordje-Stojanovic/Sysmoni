from __future__ import annotations

import pathlib
import sys
import unittest

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from render.animation import FrameDiscipline  # noqa: E402
from render.compositor import CockpitFrameState, compose_cockpit_frame  # noqa: E402


class RenderCompositorTests(unittest.TestCase):
    def test_compose_cockpit_frame_emits_frame_state(self) -> None:
        discipline = FrameDiscipline(target_fps=60, max_catchup_frames=4)

        frame = compose_cockpit_frame(
            previous_phase=0.2,
            elapsed_since_last_frame=0.005,
            cpu_percent=35.0,
            memory_percent=55.0,
            discipline=discipline,
            pulse_hz=0.5,
        )

        self.assertIsInstance(frame, CockpitFrameState)
        self.assertAlmostEqual(frame.phase, 0.2025, places=6)
        self.assertGreaterEqual(frame.accent_intensity, 0.15)
        self.assertLessEqual(frame.accent_intensity, 0.95)
        self.assertAlmostEqual(frame.next_delay_seconds, (1.0 / 60.0) - 0.005, places=6)

    def test_compose_cockpit_frame_clamps_large_elapsed_and_sets_zero_delay(self) -> None:
        discipline = FrameDiscipline(target_fps=60, max_catchup_frames=2)

        frame = compose_cockpit_frame(
            previous_phase=0.99,
            elapsed_since_last_frame=2.0,
            cpu_percent=90.0,
            memory_percent=90.0,
            discipline=discipline,
            pulse_hz=1.0,
        )

        self.assertAlmostEqual(frame.phase, 0.0233333333, places=6)
        self.assertEqual(frame.next_delay_seconds, 0.0)

    def test_compose_cockpit_frame_validates_positive_pulse_hz(self) -> None:
        with self.assertRaises(ValueError):
            compose_cockpit_frame(
                previous_phase=0.0,
                elapsed_since_last_frame=0.01,
                cpu_percent=10.0,
                memory_percent=10.0,
                pulse_hz=0.0,
            )


if __name__ == "__main__":
    unittest.main()
