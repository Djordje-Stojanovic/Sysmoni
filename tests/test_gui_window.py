from __future__ import annotations

import io
import pathlib
import sys
import unittest
from contextlib import redirect_stderr


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from core.types import SystemSnapshot  # noqa: E402
from gui import window as gui_window  # noqa: E402


class GuiWindowTests(unittest.TestCase):
    def test_resolve_gui_db_path_handles_missing_blank_and_set_values(self) -> None:
        self.assertIsNone(gui_window.resolve_gui_db_path({}))
        self.assertIsNone(gui_window.resolve_gui_db_path({"AURA_DB_PATH": "   "}))
        self.assertEqual(
            gui_window.resolve_gui_db_path({"AURA_DB_PATH": " telemetry.sqlite "}),
            "telemetry.sqlite",
        )

    def test_dvr_recorder_updates_sample_count_and_stream_status(self) -> None:
        created_stores: list[object] = []

        class _StoreStub:
            def __init__(self, db_path: str) -> None:
                self.db_path = db_path
                self.closed = False
                self.appended: list[SystemSnapshot] = []
                self._count = 4
                self.count_calls = 0
                created_stores.append(self)

            def append_and_count(self, snapshot: SystemSnapshot) -> int:
                self.appended.append(snapshot)
                self._count += 1
                return self._count

            def count(self) -> int:
                self.count_calls += 1
                return self._count

            def close(self) -> None:
                self.closed = True

        original_store = gui_window.TelemetryStore
        gui_window.TelemetryStore = _StoreStub
        try:
            recorder = gui_window.DvrRecorder("telemetry.sqlite")
            snapshot = SystemSnapshot(timestamp=1.0, cpu_percent=10.0, memory_percent=20.0)

            recorder.append(snapshot)
            status = gui_window.format_stream_status(recorder)
            recorder.close()
        finally:
            gui_window.TelemetryStore = original_store

        self.assertEqual(len(created_stores), 1)
        store = created_stores[0]
        self.assertEqual(store.db_path, "telemetry.sqlite")
        self.assertEqual(store.appended, [snapshot])
        self.assertEqual(store.count_calls, 1)
        self.assertEqual(recorder.sample_count, 5)
        self.assertEqual(status, "Streaming telemetry | DVR samples: 5")
        self.assertTrue(store.closed)

    def test_dvr_recorder_disables_store_after_write_error(self) -> None:
        created_stores: list[object] = []

        class _StoreStub:
            def __init__(self, db_path: str) -> None:
                self.db_path = db_path
                self.closed = False
                self._count = 1
                created_stores.append(self)

            def append_and_count(self, snapshot: SystemSnapshot) -> int:
                raise OSError("disk full")

            def count(self) -> int:
                return self._count

            def close(self) -> None:
                self.closed = True

        original_store = gui_window.TelemetryStore
        gui_window.TelemetryStore = _StoreStub
        try:
            recorder = gui_window.DvrRecorder("telemetry.sqlite")
            recorder.append(
                SystemSnapshot(timestamp=1.0, cpu_percent=10.0, memory_percent=20.0)
            )
            status = gui_window.format_stream_status(recorder)
        finally:
            gui_window.TelemetryStore = original_store

        self.assertEqual(len(created_stores), 1)
        self.assertTrue(created_stores[0].closed)
        self.assertIn("disk full", recorder.error or "")
        self.assertIn("DVR unavailable", status)

    def test_format_status_functions_without_dvr_path(self) -> None:
        recorder = gui_window.DvrRecorder(None)

        self.assertEqual(gui_window.format_initial_status(recorder), "Collecting telemetry...")
        self.assertEqual(gui_window.format_stream_status(recorder), "Streaming telemetry")

    def test_format_snapshot_lines_formats_live_metrics(self) -> None:
        snapshot = SystemSnapshot(
            timestamp=0.0,
            cpu_percent=12.34,
            memory_percent=56.78,
        )

        lines = gui_window.format_snapshot_lines(snapshot)

        self.assertEqual(lines["cpu"], "CPU 12.3%")
        self.assertEqual(lines["memory"], "Memory 56.8%")
        self.assertEqual(lines["timestamp"], "Updated 00:00:00 UTC")

    def test_require_qt_raises_runtime_error_when_qt_is_missing(self) -> None:
        original_error = gui_window._QT_IMPORT_ERROR
        gui_window._QT_IMPORT_ERROR = ImportError("No module named PySide6")
        try:
            with self.assertRaises(RuntimeError) as ctx:
                gui_window.require_qt()
        finally:
            gui_window._QT_IMPORT_ERROR = original_error

        self.assertIn("PySide6 is required for the GUI slice", str(ctx.exception))

    def test_run_returns_error_code_when_qt_is_missing(self) -> None:
        original_error = gui_window._QT_IMPORT_ERROR
        gui_window._QT_IMPORT_ERROR = ImportError("No module named PySide6")
        stderr = io.StringIO()
        try:
            with redirect_stderr(stderr):
                exit_code = gui_window.run([])
        finally:
            gui_window._QT_IMPORT_ERROR = original_error

        self.assertEqual(exit_code, 2)
        self.assertIn("PySide6 is required for the GUI slice", stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
