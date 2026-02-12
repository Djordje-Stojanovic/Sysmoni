from __future__ import annotations

import io
import pathlib
import sys
import unittest
from contextlib import redirect_stderr, redirect_stdout


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

import main as app_main  # noqa: E402
from core.types import SystemSnapshot  # noqa: E402


class MainCliTests(unittest.TestCase):
    def test_main_returns_error_when_snapshot_collection_crashes(self) -> None:
        def _raise_value_error() -> SystemSnapshot:
            raise ValueError("sensor read failed")

        original = app_main.collect_snapshot
        app_main.collect_snapshot = _raise_value_error
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main([])
        finally:
            app_main.collect_snapshot = original

        self.assertEqual(code, 2)
        self.assertEqual(stdout.getvalue(), "")
        self.assertIn("Failed to collect telemetry snapshot: sensor read failed", stderr.getvalue())

    def test_main_prints_json_snapshot_when_collection_succeeds(self) -> None:
        original = app_main.collect_snapshot
        app_main.collect_snapshot = lambda: SystemSnapshot(
            timestamp=10.5,
            cpu_percent=12.5,
            memory_percent=42.0,
        )
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main(["--json"])
        finally:
            app_main.collect_snapshot = original

        self.assertEqual(code, 0)
        self.assertEqual(stderr.getvalue(), "")
        self.assertEqual(
            stdout.getvalue().strip(),
            '{"cpu_percent": 12.5, "memory_percent": 42.0, "timestamp": 10.5}',
        )


if __name__ == "__main__":
    unittest.main()
