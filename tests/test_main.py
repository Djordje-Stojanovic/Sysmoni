from __future__ import annotations

import builtins
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
    def test_main_watch_mode_rejects_non_positive_interval(self) -> None:
        original_run_polling_loop = app_main.run_polling_loop
        app_main.run_polling_loop = lambda *args, **kwargs: (_ for _ in ()).throw(
            AssertionError("run_polling_loop should not be called for invalid interval.")
        )
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                with self.assertRaises(SystemExit) as ctx:
                    app_main.main(["--watch", "--interval", "0"])
        finally:
            app_main.run_polling_loop = original_run_polling_loop

        self.assertEqual(ctx.exception.code, 2)
        self.assertEqual(stdout.getvalue(), "")
        self.assertIn("interval must be greater than 0", stderr.getvalue())

    def test_main_watch_mode_streams_json_snapshots(self) -> None:
        produced = [
            SystemSnapshot(timestamp=10.0, cpu_percent=1.0, memory_percent=2.0),
            SystemSnapshot(timestamp=11.0, cpu_percent=3.0, memory_percent=4.0),
        ]
        observed_interval_seconds: list[float] = []

        def _run_polling_loop(interval_seconds: float, on_snapshot, *, stop_event) -> int:
            observed_interval_seconds.append(interval_seconds)
            for snapshot in produced:
                on_snapshot(snapshot)
            return len(produced)

        original_run_polling_loop = app_main.run_polling_loop
        original_collect_snapshot = app_main.collect_snapshot
        app_main.run_polling_loop = _run_polling_loop
        app_main.collect_snapshot = lambda: (_ for _ in ()).throw(
            AssertionError("collect_snapshot should not be called in watch mode.")
        )
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main(["--watch", "--json", "--interval", "0.5"])
        finally:
            app_main.run_polling_loop = original_run_polling_loop
            app_main.collect_snapshot = original_collect_snapshot

        self.assertEqual(code, 0)
        self.assertEqual(stderr.getvalue(), "")
        self.assertEqual(observed_interval_seconds, [0.5])
        self.assertEqual(
            stdout.getvalue().strip().splitlines(),
            [
                '{"cpu_percent": 1.0, "memory_percent": 2.0, "timestamp": 10.0}',
                '{"cpu_percent": 3.0, "memory_percent": 4.0, "timestamp": 11.0}',
            ],
        )

    def test_main_watch_mode_flushes_each_snapshot(self) -> None:
        produced = [
            SystemSnapshot(timestamp=10.0, cpu_percent=1.0, memory_percent=2.0),
            SystemSnapshot(timestamp=11.0, cpu_percent=3.0, memory_percent=4.0),
        ]
        flush_flags: list[bool] = []

        def _run_polling_loop(interval_seconds: float, on_snapshot, *, stop_event) -> int:
            for snapshot in produced:
                on_snapshot(snapshot)
            return len(produced)

        def _capture_print(*_args, **kwargs) -> None:
            flush_flags.append(bool(kwargs.get("flush", False)))

        original_run_polling_loop = app_main.run_polling_loop
        original_print = builtins.print
        app_main.run_polling_loop = _run_polling_loop
        builtins.print = _capture_print
        try:
            code = app_main.main(["--watch"])
        finally:
            app_main.run_polling_loop = original_run_polling_loop
            builtins.print = original_print

        self.assertEqual(code, 0)
        self.assertEqual(flush_flags, [True, True])

    def test_main_watch_mode_returns_130_when_interrupted(self) -> None:
        def _raise_interrupt(interval_seconds: float, on_snapshot, *, stop_event) -> int:
            raise KeyboardInterrupt()

        original = app_main.run_polling_loop
        app_main.run_polling_loop = _raise_interrupt
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main(["--watch"])
        finally:
            app_main.run_polling_loop = original

        self.assertEqual(code, 130)
        self.assertEqual(stdout.getvalue(), "")
        self.assertIn("Interrupted by user.", stderr.getvalue())

    def test_main_returns_130_when_interrupted(self) -> None:
        def _raise_interrupt() -> SystemSnapshot:
            raise KeyboardInterrupt()

        original = app_main.collect_snapshot
        app_main.collect_snapshot = _raise_interrupt
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main([])
        finally:
            app_main.collect_snapshot = original

        self.assertEqual(code, 130)
        self.assertEqual(stdout.getvalue(), "")
        self.assertIn("Interrupted by user.", stderr.getvalue())

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
