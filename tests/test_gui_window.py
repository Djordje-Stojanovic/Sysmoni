from __future__ import annotations

import io
import pathlib
import sys
import threading
import unittest
from contextlib import redirect_stderr


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from core.types import ProcessSample, SystemSnapshot  # noqa: E402
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
        seeded_snapshot = SystemSnapshot(
            timestamp=3.0,
            cpu_percent=30.0,
            memory_percent=40.0,
        )

        class _StoreStub:
            def __init__(self, db_path: str) -> None:
                self.db_path = db_path
                self.closed = False
                self.appended: list[SystemSnapshot] = []
                self._count = 4
                self.count_calls = 0
                self.latest_limits: list[int] = []
                created_stores.append(self)

            def append_and_count(self, snapshot: SystemSnapshot) -> int:
                self.appended.append(snapshot)
                self._count += 1
                return self._count

            def count(self) -> int:
                self.count_calls += 1
                return self._count

            def latest(self, *, limit: int = 1) -> list[SystemSnapshot]:
                self.latest_limits.append(limit)
                return [seeded_snapshot]

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
        self.assertEqual(store.latest_limits, [1])
        self.assertEqual(recorder.latest_snapshot, seeded_snapshot)
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

            def latest(self, *, limit: int = 1) -> list[SystemSnapshot]:
                return []

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

    def test_dvr_recorder_closes_store_when_startup_read_fails(self) -> None:
        created_stores: list[object] = []

        class _StoreStub:
            def __init__(self, db_path: str) -> None:
                self.db_path = db_path
                self.closed = False
                created_stores.append(self)

            def count(self) -> int:
                raise OSError("startup read failed")

            def latest(self, *, limit: int = 1) -> list[SystemSnapshot]:
                return []

            def close(self) -> None:
                self.closed = True

        original_store = gui_window.TelemetryStore
        gui_window.TelemetryStore = _StoreStub
        try:
            recorder = gui_window.DvrRecorder("telemetry.sqlite")
            status = gui_window.format_initial_status(recorder)
        finally:
            gui_window.TelemetryStore = original_store

        self.assertEqual(len(created_stores), 1)
        self.assertTrue(created_stores[0].closed)
        self.assertIn("startup read failed", recorder.error or "")
        self.assertIn("DVR unavailable", status)

    def test_dvr_recorder_close_waits_for_inflight_append(self) -> None:
        created_stores: list[object] = []
        append_entered = threading.Event()
        release_append = threading.Event()
        close_started = threading.Event()
        call_order: list[str] = []

        class _StoreStub:
            def __init__(self, db_path: str) -> None:
                self.db_path = db_path
                self.closed = False
                created_stores.append(self)

            def append_and_count(self, snapshot: SystemSnapshot) -> int:
                del snapshot
                append_entered.set()
                release_append.wait(timeout=1.0)
                call_order.append("append")
                return 1

            def count(self) -> int:
                return 0

            def latest(self, *, limit: int = 1) -> list[SystemSnapshot]:
                return []

            def close(self) -> None:
                self.closed = True
                call_order.append("close")

        original_store = gui_window.TelemetryStore
        gui_window.TelemetryStore = _StoreStub
        try:
            recorder = gui_window.DvrRecorder("telemetry.sqlite")
            snapshot = SystemSnapshot(timestamp=1.0, cpu_percent=10.0, memory_percent=20.0)

            append_thread = threading.Thread(target=lambda: recorder.append(snapshot))
            append_thread.start()
            self.assertTrue(append_entered.wait(timeout=1.0))

            def _close_recorder() -> None:
                close_started.set()
                recorder.close()

            close_thread = threading.Thread(target=_close_recorder)
            close_thread.start()
            self.assertTrue(close_started.wait(timeout=1.0))
            close_thread.join(timeout=0.05)
            self.assertTrue(close_thread.is_alive())

            release_append.set()
            append_thread.join(timeout=1.0)
            close_thread.join(timeout=1.0)
        finally:
            gui_window.TelemetryStore = original_store

        self.assertFalse(append_thread.is_alive())
        self.assertFalse(close_thread.is_alive())
        self.assertEqual(len(created_stores), 1)
        self.assertTrue(created_stores[0].closed)
        self.assertEqual(call_order, ["append", "close"])
        self.assertEqual(recorder.sample_count, 1)

    @unittest.skipIf(gui_window._QT_IMPORT_ERROR is not None, "PySide6 is unavailable")
    def test_snapshot_worker_persists_in_worker_thread_and_emits_status(self) -> None:
        snapshot = SystemSnapshot(timestamp=1.0, cpu_percent=10.0, memory_percent=20.0)
        process_samples = [
            ProcessSample(
                pid=9,
                name="proc-a",
                cpu_percent=40.0,
                memory_rss_bytes=6 * 1024 * 1024,
            )
        ]
        expected_rows = gui_window.format_process_rows(process_samples, row_count=5)
        emitted: list[tuple[float, float, float, str, list[str]]] = []
        errors: list[str] = []
        finished: list[bool] = []

        class _RecorderStub:
            def __init__(self) -> None:
                self.db_path = "telemetry.sqlite"
                self.sample_count = 0
                self.error: str | None = None
                self.appended: list[SystemSnapshot] = []

            def append(self, value: SystemSnapshot) -> None:
                self.appended.append(value)
                self.sample_count += 1

            def get_status(self) -> tuple[str | None, int | None, str | None]:
                return self.db_path, self.sample_count, self.error

        recorder = _RecorderStub()

        def _run_polling_loop(
            interval_seconds: float,
            on_snapshot,
            *,
            stop_event,
            collect,
            on_error,
            continue_on_error,
        ) -> int:
            self.assertEqual(interval_seconds, 0.5)
            self.assertFalse(stop_event.is_set())
            self.assertTrue(continue_on_error)
            self.assertIsNotNone(on_error)
            on_snapshot(collect())
            stop_event.set()
            return 1

        original_run_polling_loop = gui_window.run_polling_loop
        gui_window.run_polling_loop = _run_polling_loop
        try:
            worker = gui_window.SnapshotWorker(
                interval_seconds=0.5,
                collect=lambda: snapshot,
                collect_processes=lambda: process_samples,
                process_row_count=5,
                recorder=recorder,
            )
            worker.snapshot_ready.connect(
                lambda timestamp, cpu, memory, status, process_rows: emitted.append(
                    (timestamp, cpu, memory, status, process_rows)
                )
            )
            worker.error.connect(errors.append)
            worker.finished.connect(lambda: finished.append(True))
            worker.run()
        finally:
            gui_window.run_polling_loop = original_run_polling_loop

        self.assertEqual(recorder.appended, [snapshot])
        self.assertEqual(
            emitted,
            [
                (
                    1.0,
                    10.0,
                    20.0,
                    "Streaming telemetry | DVR samples: 1",
                    expected_rows,
                )
            ],
        )
        self.assertEqual(errors, [])
        self.assertEqual(finished, [True])

    @unittest.skipIf(gui_window._QT_IMPORT_ERROR is not None, "PySide6 is unavailable")
    def test_snapshot_worker_emits_error_and_continues_after_transient_failure(self) -> None:
        emitted: list[tuple[float, float, float, str, list[str]]] = []
        errors: list[str] = []
        finished: list[bool] = []

        class _RecorderStub:
            def __init__(self) -> None:
                self.db_path = "telemetry.sqlite"
                self.sample_count = 0
                self.error: str | None = None
                self.appended: list[SystemSnapshot] = []

            def append(self, value: SystemSnapshot) -> None:
                self.appended.append(value)
                self.sample_count += 1

            def get_status(self) -> tuple[str | None, int | None, str | None]:
                return self.db_path, self.sample_count, self.error

        recorder = _RecorderStub()
        expected_snapshot = SystemSnapshot(timestamp=2.0, cpu_percent=11.0, memory_percent=21.0)

        def _collect() -> SystemSnapshot:
            return expected_snapshot

        def _run_polling_loop(
            interval_seconds: float,
            on_snapshot,
            *,
            stop_event,
            collect,
            on_error,
            continue_on_error,
        ) -> int:
            self.assertEqual(interval_seconds, 0.5)
            self.assertTrue(continue_on_error)
            on_error(RuntimeError("sensor glitch"))
            on_snapshot(collect())
            stop_event.set()
            return 1

        original_run_polling_loop = gui_window.run_polling_loop
        gui_window.run_polling_loop = _run_polling_loop
        try:
            worker = gui_window.SnapshotWorker(
                interval_seconds=0.5,
                collect=_collect,
                collect_processes=lambda: [],
                process_row_count=5,
                recorder=recorder,
            )
            worker.snapshot_ready.connect(
                lambda timestamp, cpu, memory, status, process_rows: emitted.append(
                    (timestamp, cpu, memory, status, process_rows)
                )
            )
            worker.error.connect(errors.append)
            worker.finished.connect(lambda: finished.append(True))
            worker.run()
        finally:
            gui_window.run_polling_loop = original_run_polling_loop

        self.assertEqual(recorder.appended, [expected_snapshot])
        self.assertEqual(
            emitted,
            [
                (
                    2.0,
                    11.0,
                    21.0,
                    "Streaming telemetry | DVR samples: 1",
                    gui_window.format_process_rows([], row_count=5),
                )
            ],
        )
        self.assertEqual(errors, ["sensor glitch"])
        self.assertEqual(finished, [True])

    @unittest.skipIf(gui_window._QT_IMPORT_ERROR is not None, "PySide6 is unavailable")
    def test_snapshot_worker_emits_process_error_and_continues_streaming(self) -> None:
        snapshots = [
            SystemSnapshot(timestamp=1.0, cpu_percent=10.0, memory_percent=20.0),
            SystemSnapshot(timestamp=2.0, cpu_percent=20.0, memory_percent=30.0),
        ]
        emitted: list[tuple[float, float, float, str, list[str]]] = []
        errors: list[str] = []
        finished: list[bool] = []
        process_attempts = 0

        class _RecorderStub:
            def __init__(self) -> None:
                self.db_path = "telemetry.sqlite"
                self.sample_count = 0
                self.error: str | None = None

            def append(self, value: SystemSnapshot) -> None:
                del value
                self.sample_count += 1

            def get_status(self) -> tuple[str | None, int | None, str | None]:
                return self.db_path, self.sample_count, self.error

        def _collect() -> SystemSnapshot:
            return snapshots.pop(0)

        def _collect_processes() -> list[ProcessSample]:
            nonlocal process_attempts
            process_attempts += 1
            if process_attempts == 1:
                raise RuntimeError("process probe failed")
            return [
                ProcessSample(
                    pid=20,
                    name="proc-b",
                    cpu_percent=15.0,
                    memory_rss_bytes=5 * 1024 * 1024,
                )
            ]

        def _run_polling_loop(
            interval_seconds: float,
            on_snapshot,
            *,
            stop_event,
            collect,
            on_error,
            continue_on_error,
        ) -> int:
            self.assertEqual(interval_seconds, 0.5)
            self.assertTrue(continue_on_error)
            self.assertIsNotNone(on_error)
            on_snapshot(collect())
            on_snapshot(collect())
            stop_event.set()
            return 2

        original_run_polling_loop = gui_window.run_polling_loop
        gui_window.run_polling_loop = _run_polling_loop
        try:
            worker = gui_window.SnapshotWorker(
                interval_seconds=0.5,
                collect=_collect,
                collect_processes=_collect_processes,
                process_row_count=5,
                recorder=_RecorderStub(),
            )
            worker.snapshot_ready.connect(
                lambda timestamp, cpu, memory, status, process_rows: emitted.append(
                    (timestamp, cpu, memory, status, process_rows)
                )
            )
            worker.error.connect(errors.append)
            worker.finished.connect(lambda: finished.append(True))
            worker.run()
        finally:
            gui_window.run_polling_loop = original_run_polling_loop

        self.assertEqual(len(emitted), 2)
        self.assertEqual(
            emitted[0][0:4],
            (1.0, 10.0, 20.0, "Streaming telemetry | DVR samples: 1"),
        )
        self.assertEqual(
            emitted[0][4],
            gui_window.format_process_rows([], row_count=5),
        )
        self.assertEqual(
            emitted[1][0:4],
            (2.0, 20.0, 30.0, "Streaming telemetry | DVR samples: 2"),
        )
        self.assertEqual(
            emitted[1][4],
            gui_window.format_process_rows(
                [
                    ProcessSample(
                        pid=20,
                        name="proc-b",
                        cpu_percent=15.0,
                        memory_rss_bytes=5 * 1024 * 1024,
                    )
                ],
                row_count=5,
            ),
        )
        self.assertEqual(errors, ["Process telemetry error: process probe failed"])
        self.assertEqual(finished, [True])

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

    def test_format_process_rows_formats_samples_and_pads_placeholders(self) -> None:
        rows = gui_window.format_process_rows(
            [
                ProcessSample(
                    pid=101,
                    name="very_long_process_name_for_truncation",
                    cpu_percent=12.3,
                    memory_rss_bytes=3 * 1024 * 1024,
                ),
                ProcessSample(
                    pid=102,
                    name="worker",
                    cpu_percent=4.5,
                    memory_rss_bytes=2 * 1024 * 1024,
                ),
            ],
            row_count=3,
        )

        self.assertEqual(len(rows), 3)
        self.assertIn(" 1. very_long_process...", rows[0])
        self.assertIn("CPU  12.3%", rows[0])
        self.assertIn("RAM     3.0 MB", rows[0])
        self.assertIn(" 2. worker", rows[1])
        self.assertEqual(rows[2], " 3. collecting process data...")

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
