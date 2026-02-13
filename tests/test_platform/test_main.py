from __future__ import annotations

import builtins
import errno
import importlib
import io
import pathlib
import sys
import unittest
from contextlib import redirect_stderr, redirect_stdout
from unittest.mock import patch


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from contracts.types import SystemSnapshot  # noqa: E402

app_main = importlib.import_module("runtime.main")


class MainCliTests(unittest.TestCase):
    def test_main_since_or_until_without_db_path_uses_auto_store(self) -> None:
        range_snapshots = [
            SystemSnapshot(timestamp=10.0, cpu_percent=1.0, memory_percent=2.0),
            SystemSnapshot(timestamp=11.0, cpu_percent=3.0, memory_percent=4.0),
        ]
        created_stores: list[object] = []

        class _StoreStub:
            def __init__(self, db_path: str, retention_seconds: float) -> None:
                self.db_path = db_path
                self.retention_seconds = retention_seconds
                self.closed = False
                created_stores.append(self)

            def between(
                self,
                *,
                start_timestamp: float | None = None,
                end_timestamp: float | None = None,
            ) -> list[SystemSnapshot]:
                del start_timestamp, end_timestamp
                return range_snapshots

            def close(self) -> None:
                self.closed = True

        original_store = app_main.TelemetryStore
        original_collect_snapshot = app_main.collect_snapshot
        original_run_polling_loop = app_main.run_polling_loop
        app_main.TelemetryStore = _StoreStub
        app_main.collect_snapshot = lambda: (_ for _ in ()).throw(
            AssertionError("collect_snapshot should not be called during range reads.")
        )
        app_main.run_polling_loop = lambda *args, **kwargs: (_ for _ in ()).throw(
            AssertionError("run_polling_loop should not be called during range reads.")
        )

        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main(["--since", "10.0", "--until", "11.0", "--json"])
        finally:
            app_main.TelemetryStore = original_store
            app_main.collect_snapshot = original_collect_snapshot
            app_main.run_polling_loop = original_run_polling_loop

        self.assertEqual(code, 0)
        self.assertEqual(stderr.getvalue(), "")
        self.assertEqual(
            stdout.getvalue().strip().splitlines(),
            [
                '{"cpu_percent": 1.0, "memory_percent": 2.0, "timestamp": 10.0}',
                '{"cpu_percent": 3.0, "memory_percent": 4.0, "timestamp": 11.0}',
            ],
        )
        self.assertEqual(len(created_stores), 1)
        self.assertTrue(str(created_stores[0].db_path).endswith("telemetry.sqlite"))
        self.assertTrue(created_stores[0].closed)

    def test_main_since_or_until_rejects_watch_or_latest(self) -> None:
        original_run_polling_loop = app_main.run_polling_loop
        app_main.run_polling_loop = lambda *args, **kwargs: (_ for _ in ()).throw(
            AssertionError("run_polling_loop should not be called with invalid range args.")
        )

        try:
            for argv, expected_error in (
                (
                    ["--watch", "--since", "1.0", "--db-path", "telemetry.sqlite"],
                    "--since/--until cannot be used with --watch",
                ),
                (
                    [
                        "--latest",
                        "1",
                        "--since",
                        "1.0",
                        "--db-path",
                        "telemetry.sqlite",
                    ],
                    "--since/--until cannot be used with --latest",
                ),
            ):
                stdout = io.StringIO()
                stderr = io.StringIO()
                with self.subTest(argv=argv):
                    with redirect_stdout(stdout), redirect_stderr(stderr):
                        with self.assertRaises(SystemExit) as ctx:
                            app_main.main(argv)

                    self.assertEqual(ctx.exception.code, 2)
                    self.assertEqual(stdout.getvalue(), "")
                    self.assertIn(expected_error, stderr.getvalue())
        finally:
            app_main.run_polling_loop = original_run_polling_loop

    def test_main_latest_or_range_rejects_no_persist(self) -> None:
        for argv, expected_error in (
            (
                ["--latest", "1", "--no-persist"],
                "--latest cannot be used with --no-persist",
            ),
            (
                ["--since", "1.0", "--no-persist"],
                "--since/--until cannot be used with --no-persist",
            ),
        ):
            stdout = io.StringIO()
            stderr = io.StringIO()
            with self.subTest(argv=argv):
                with redirect_stdout(stdout), redirect_stderr(stderr):
                    with self.assertRaises(SystemExit) as ctx:
                        app_main.main(argv)

                self.assertEqual(ctx.exception.code, 2)
                self.assertEqual(stdout.getvalue(), "")
                self.assertIn(expected_error, stderr.getvalue())

    def test_main_since_until_prints_requested_snapshots_from_store(self) -> None:
        range_snapshots = [
            SystemSnapshot(timestamp=10.0, cpu_percent=1.0, memory_percent=2.0),
            SystemSnapshot(timestamp=11.0, cpu_percent=3.0, memory_percent=4.0),
        ]
        created_stores: list[object] = []

        class _StoreStub:
            def __init__(self, db_path: str, retention_seconds: float) -> None:
                self.db_path = db_path
                self.closed = False
                self.received_start: float | None = None
                self.received_end: float | None = None
                created_stores.append(self)

            def between(
                self,
                *,
                start_timestamp: float | None = None,
                end_timestamp: float | None = None,
            ) -> list[SystemSnapshot]:
                self.received_start = start_timestamp
                self.received_end = end_timestamp
                return range_snapshots

            def append(self, _snapshot: SystemSnapshot) -> None:
                raise AssertionError("append should not be called during range reads.")

            def close(self) -> None:
                self.closed = True

        original_store = app_main.TelemetryStore
        original_collect_snapshot = app_main.collect_snapshot
        original_run_polling_loop = app_main.run_polling_loop
        app_main.TelemetryStore = _StoreStub
        app_main.collect_snapshot = lambda: (_ for _ in ()).throw(
            AssertionError("collect_snapshot should not be called during range reads.")
        )
        app_main.run_polling_loop = lambda *args, **kwargs: (_ for _ in ()).throw(
            AssertionError("run_polling_loop should not be called during range reads.")
        )
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main(
                    [
                        "--db-path",
                        "telemetry.sqlite",
                        "--since",
                        "10.0",
                        "--until",
                        "11.0",
                        "--json",
                    ]
                )
        finally:
            app_main.TelemetryStore = original_store
            app_main.collect_snapshot = original_collect_snapshot
            app_main.run_polling_loop = original_run_polling_loop

        self.assertEqual(code, 0)
        self.assertEqual(stderr.getvalue(), "")
        self.assertEqual(
            stdout.getvalue().strip().splitlines(),
            [
                '{"cpu_percent": 1.0, "memory_percent": 2.0, "timestamp": 10.0}',
                '{"cpu_percent": 3.0, "memory_percent": 4.0, "timestamp": 11.0}',
            ],
        )
        self.assertEqual(len(created_stores), 1)
        store = created_stores[0]
        self.assertEqual(store.db_path, "telemetry.sqlite")
        self.assertEqual(store.received_start, 10.0)
        self.assertEqual(store.received_end, 11.0)
        self.assertTrue(store.closed)

    def test_main_rejects_since_greater_than_until(self) -> None:
        stdout = io.StringIO()
        stderr = io.StringIO()

        with redirect_stdout(stdout), redirect_stderr(stderr):
            with self.assertRaises(SystemExit) as ctx:
                app_main.main(
                    [
                        "--db-path",
                        "telemetry.sqlite",
                        "--since",
                        "11.0",
                        "--until",
                        "10.0",
                    ]
                )

        self.assertEqual(ctx.exception.code, 2)
        self.assertEqual(stdout.getvalue(), "")
        self.assertIn("--since must be less than or equal to --until", stderr.getvalue())

    def test_main_latest_without_db_path_uses_auto_store(self) -> None:
        latest_snapshots = [
            SystemSnapshot(timestamp=10.0, cpu_percent=1.0, memory_percent=2.0),
        ]
        created_stores: list[object] = []

        class _StoreStub:
            def __init__(self, db_path: str, retention_seconds: float) -> None:
                self.db_path = db_path
                self.retention_seconds = retention_seconds
                self.closed = False
                created_stores.append(self)

            def latest(self, *, limit: int = 1) -> list[SystemSnapshot]:
                self.received_limit = limit
                return latest_snapshots

            def close(self) -> None:
                self.closed = True

        original_store = app_main.TelemetryStore
        app_main.TelemetryStore = _StoreStub
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main(["--latest", "1", "--json"])
        finally:
            app_main.TelemetryStore = original_store

        self.assertEqual(code, 0)
        self.assertEqual(stderr.getvalue(), "")
        self.assertEqual(
            stdout.getvalue().strip(),
            '{"cpu_percent": 1.0, "memory_percent": 2.0, "timestamp": 10.0}',
        )
        self.assertEqual(len(created_stores), 1)
        self.assertTrue(str(created_stores[0].db_path).endswith("telemetry.sqlite"))
        self.assertTrue(created_stores[0].closed)

    def test_main_latest_rejects_watch_mode(self) -> None:
        original_run_polling_loop = app_main.run_polling_loop
        app_main.run_polling_loop = lambda *args, **kwargs: (_ for _ in ()).throw(
            AssertionError("run_polling_loop should not be called with --latest.")
        )

        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                with self.assertRaises(SystemExit) as ctx:
                    app_main.main(["--watch", "--latest", "1", "--db-path", "telemetry.sqlite"])
        finally:
            app_main.run_polling_loop = original_run_polling_loop

        self.assertEqual(ctx.exception.code, 2)
        self.assertEqual(stdout.getvalue(), "")
        self.assertIn("--latest cannot be used with --watch", stderr.getvalue())

    def test_main_latest_prints_requested_snapshots_from_store(self) -> None:
        latest_snapshots = [
            SystemSnapshot(timestamp=10.0, cpu_percent=1.0, memory_percent=2.0),
            SystemSnapshot(timestamp=11.0, cpu_percent=3.0, memory_percent=4.0),
        ]
        created_stores: list[object] = []

        class _StoreStub:
            def __init__(self, db_path: str, retention_seconds: float) -> None:
                self.db_path = db_path
                self.closed = False
                self.received_limit: int | None = None
                created_stores.append(self)

            def latest(self, *, limit: int = 1) -> list[SystemSnapshot]:
                self.received_limit = limit
                return latest_snapshots

            def append(self, _snapshot: SystemSnapshot) -> None:
                raise AssertionError("append should not be called when reading latest data.")

            def close(self) -> None:
                self.closed = True

        original_store = app_main.TelemetryStore
        original_collect_snapshot = app_main.collect_snapshot
        original_run_polling_loop = app_main.run_polling_loop
        app_main.TelemetryStore = _StoreStub
        app_main.collect_snapshot = lambda: (_ for _ in ()).throw(
            AssertionError("collect_snapshot should not be called with --latest.")
        )
        app_main.run_polling_loop = lambda *args, **kwargs: (_ for _ in ()).throw(
            AssertionError("run_polling_loop should not be called with --latest.")
        )
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main(
                    [
                        "--db-path",
                        "telemetry.sqlite",
                        "--latest",
                        "2",
                        "--json",
                    ]
                )
        finally:
            app_main.TelemetryStore = original_store
            app_main.collect_snapshot = original_collect_snapshot
            app_main.run_polling_loop = original_run_polling_loop

        self.assertEqual(code, 0)
        self.assertEqual(stderr.getvalue(), "")
        self.assertEqual(
            stdout.getvalue().strip().splitlines(),
            [
                '{"cpu_percent": 1.0, "memory_percent": 2.0, "timestamp": 10.0}',
                '{"cpu_percent": 3.0, "memory_percent": 4.0, "timestamp": 11.0}',
            ],
        )
        self.assertEqual(len(created_stores), 1)
        store = created_stores[0]
        self.assertEqual(store.db_path, "telemetry.sqlite")
        self.assertEqual(store.received_limit, 2)
        self.assertTrue(store.closed)

    def test_main_count_requires_watch(self) -> None:
        stdout = io.StringIO()
        stderr = io.StringIO()

        with redirect_stdout(stdout), redirect_stderr(stderr):
            with self.assertRaises(SystemExit) as ctx:
                app_main.main(["--count", "1"])

        self.assertEqual(ctx.exception.code, 2)
        self.assertEqual(stdout.getvalue(), "")
        self.assertIn("--count requires --watch", stderr.getvalue())

    def test_main_watch_mode_rejects_non_positive_count(self) -> None:
        original_run_polling_loop = app_main.run_polling_loop
        app_main.run_polling_loop = lambda *args, **kwargs: (_ for _ in ()).throw(
            AssertionError("run_polling_loop should not be called for invalid count.")
        )

        try:
            for invalid_value in ("0", "-1"):
                stdout = io.StringIO()
                stderr = io.StringIO()
                with self.subTest(invalid_value=invalid_value):
                    with redirect_stdout(stdout), redirect_stderr(stderr):
                        with self.assertRaises(SystemExit) as ctx:
                            app_main.main(["--watch", "--count", invalid_value])

                    self.assertEqual(ctx.exception.code, 2)
                    self.assertEqual(stdout.getvalue(), "")
                    self.assertIn("count must be an integer greater than 0", stderr.getvalue())
        finally:
            app_main.run_polling_loop = original_run_polling_loop

    def test_main_persists_snapshot_when_db_path_is_provided(self) -> None:
        snapshot = SystemSnapshot(timestamp=10.5, cpu_percent=12.5, memory_percent=42.0)
        created_stores: list[object] = []

        class _StoreStub:
            def __init__(self, db_path: str, retention_seconds: float) -> None:
                self.db_path = db_path
                self.appended: list[SystemSnapshot] = []
                self.closed = False
                created_stores.append(self)

            def append(self, snapshot: SystemSnapshot) -> None:
                self.appended.append(snapshot)

            def close(self) -> None:
                self.closed = True

        original_collect_snapshot = app_main.collect_snapshot
        original_telemetry_store = app_main.TelemetryStore
        app_main.collect_snapshot = lambda: snapshot
        app_main.TelemetryStore = _StoreStub
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main(["--json", "--db-path", "telemetry.sqlite"])
        finally:
            app_main.collect_snapshot = original_collect_snapshot
            app_main.TelemetryStore = original_telemetry_store

        self.assertEqual(code, 0)
        self.assertEqual(stderr.getvalue(), "")
        self.assertEqual(
            stdout.getvalue().strip(),
            '{"cpu_percent": 12.5, "memory_percent": 42.0, "timestamp": 10.5}',
        )
        self.assertEqual(len(created_stores), 1)
        store = created_stores[0]
        self.assertEqual(store.db_path, "telemetry.sqlite")
        self.assertEqual(store.appended, [snapshot])
        self.assertTrue(store.closed)

    def test_main_continues_when_single_snapshot_store_write_fails(self) -> None:
        snapshot = SystemSnapshot(timestamp=10.5, cpu_percent=12.5, memory_percent=42.0)
        created_stores: list[object] = []

        class _StoreStub:
            def __init__(self, db_path: str, retention_seconds: float) -> None:
                self.db_path = db_path
                self.retention_seconds = retention_seconds
                self.append_attempts = 0
                self.closed = False
                created_stores.append(self)

            def append(self, _snapshot: SystemSnapshot) -> None:
                self.append_attempts += 1
                raise OSError("disk full")

            def close(self) -> None:
                self.closed = True

        original_collect_snapshot = app_main.collect_snapshot
        original_telemetry_store = app_main.TelemetryStore
        app_main.collect_snapshot = lambda: snapshot
        app_main.TelemetryStore = _StoreStub
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main(["--json", "--db-path", "telemetry.sqlite"])
        finally:
            app_main.collect_snapshot = original_collect_snapshot
            app_main.TelemetryStore = original_telemetry_store

        self.assertEqual(code, 0)
        self.assertEqual(
            stdout.getvalue().strip(),
            '{"cpu_percent": 12.5, "memory_percent": 42.0, "timestamp": 10.5}',
        )
        self.assertIn("DVR persistence disabled: disk full", stderr.getvalue())
        self.assertEqual(len(created_stores), 1)
        store = created_stores[0]
        self.assertEqual(store.db_path, "telemetry.sqlite")
        self.assertEqual(store.retention_seconds, 86400.0)
        self.assertEqual(store.append_attempts, 1)
        self.assertTrue(store.closed)

    def test_main_no_persist_skips_store_creation(self) -> None:
        snapshot = SystemSnapshot(timestamp=10.5, cpu_percent=12.5, memory_percent=42.0)

        original_collect_snapshot = app_main.collect_snapshot
        original_telemetry_store = app_main.TelemetryStore
        app_main.collect_snapshot = lambda: snapshot
        app_main.TelemetryStore = lambda *args, **kwargs: (_ for _ in ()).throw(
            AssertionError("TelemetryStore should not be created with --no-persist.")
        )
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main(["--json", "--no-persist"])
        finally:
            app_main.collect_snapshot = original_collect_snapshot
            app_main.TelemetryStore = original_telemetry_store

        self.assertEqual(code, 0)
        self.assertEqual(stderr.getvalue(), "")
        self.assertEqual(
            stdout.getvalue().strip(),
            '{"cpu_percent": 12.5, "memory_percent": 42.0, "timestamp": 10.5}',
        )

    def test_main_passes_cli_retention_seconds_to_store(self) -> None:
        snapshot = SystemSnapshot(timestamp=10.5, cpu_percent=12.5, memory_percent=42.0)
        created_stores: list[object] = []

        class _StoreStub:
            def __init__(self, db_path: str, retention_seconds: float) -> None:
                self.db_path = db_path
                self.retention_seconds = retention_seconds
                self.closed = False
                created_stores.append(self)

            def append(self, _snapshot: SystemSnapshot) -> None:
                pass

            def close(self) -> None:
                self.closed = True

        original_collect_snapshot = app_main.collect_snapshot
        original_telemetry_store = app_main.TelemetryStore
        app_main.collect_snapshot = lambda: snapshot
        app_main.TelemetryStore = _StoreStub
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main(
                    [
                        "--json",
                        "--db-path",
                        "telemetry.sqlite",
                        "--retention-seconds",
                        "30",
                    ]
                )
        finally:
            app_main.collect_snapshot = original_collect_snapshot
            app_main.TelemetryStore = original_telemetry_store

        self.assertEqual(code, 0)
        self.assertEqual(stderr.getvalue(), "")
        self.assertEqual(len(created_stores), 1)
        self.assertEqual(created_stores[0].retention_seconds, 30.0)

    def test_main_uses_env_retention_seconds_when_cli_not_set(self) -> None:
        snapshot = SystemSnapshot(timestamp=10.5, cpu_percent=12.5, memory_percent=42.0)
        created_stores: list[object] = []

        class _StoreStub:
            def __init__(self, db_path: str, retention_seconds: float) -> None:
                self.db_path = db_path
                self.retention_seconds = retention_seconds
                self.closed = False
                created_stores.append(self)

            def append(self, _snapshot: SystemSnapshot) -> None:
                pass

            def close(self) -> None:
                self.closed = True

        original_collect_snapshot = app_main.collect_snapshot
        original_telemetry_store = app_main.TelemetryStore
        app_main.collect_snapshot = lambda: snapshot
        app_main.TelemetryStore = _StoreStub
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with patch.dict("os.environ", {"AURA_RETENTION_SECONDS": "45"}, clear=False):
                with redirect_stdout(stdout), redirect_stderr(stderr):
                    code = app_main.main(["--json", "--db-path", "telemetry.sqlite"])
        finally:
            app_main.collect_snapshot = original_collect_snapshot
            app_main.TelemetryStore = original_telemetry_store

        self.assertEqual(code, 0)
        self.assertEqual(stderr.getvalue(), "")
        self.assertEqual(len(created_stores), 1)
        self.assertEqual(created_stores[0].retention_seconds, 45.0)

    def test_main_auto_store_open_failure_degrades_to_live_mode(self) -> None:
        snapshot = SystemSnapshot(timestamp=10.5, cpu_percent=12.5, memory_percent=42.0)

        original_collect_snapshot = app_main.collect_snapshot
        original_telemetry_store = app_main.TelemetryStore
        app_main.collect_snapshot = lambda: snapshot
        app_main.TelemetryStore = lambda *_args, **_kwargs: (_ for _ in ()).throw(
            OSError(errno.EINVAL, "Invalid argument")
        )
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with patch.dict(
                "os.environ",
                {
                    "AURA_DB_PATH": " ",
                    "AURA_RETENTION_SECONDS": " ",
                },
                clear=False,
            ):
                with redirect_stdout(stdout), redirect_stderr(stderr):
                    code = app_main.main(["--json"])
        finally:
            app_main.collect_snapshot = original_collect_snapshot
            app_main.TelemetryStore = original_telemetry_store

        self.assertEqual(code, 0)
        self.assertEqual(
            stdout.getvalue().strip(),
            '{"cpu_percent": 12.5, "memory_percent": 42.0, "timestamp": 10.5}',
        )
        self.assertIn("DVR persistence disabled: [Errno 22]", stderr.getvalue())

    def test_main_watch_mode_persists_each_snapshot_when_db_path_is_provided(self) -> None:
        produced = [
            SystemSnapshot(timestamp=10.0, cpu_percent=1.0, memory_percent=2.0),
            SystemSnapshot(timestamp=11.0, cpu_percent=3.0, memory_percent=4.0),
        ]
        created_stores: list[object] = []

        class _StoreStub:
            def __init__(self, db_path: str, retention_seconds: float) -> None:
                self.db_path = db_path
                self.appended: list[SystemSnapshot] = []
                self.closed = False
                created_stores.append(self)

            def append(self, snapshot: SystemSnapshot) -> None:
                self.appended.append(snapshot)

            def close(self) -> None:
                self.closed = True

        def _run_polling_loop(interval_seconds: float, on_snapshot, *, stop_event) -> int:
            for snapshot in produced:
                on_snapshot(snapshot)
            return len(produced)

        original_run_polling_loop = app_main.run_polling_loop
        original_collect_snapshot = app_main.collect_snapshot
        original_telemetry_store = app_main.TelemetryStore
        app_main.run_polling_loop = _run_polling_loop
        app_main.collect_snapshot = lambda: (_ for _ in ()).throw(
            AssertionError("collect_snapshot should not be called in watch mode.")
        )
        app_main.TelemetryStore = _StoreStub
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main(
                    ["--watch", "--json", "--db-path", "telemetry.sqlite"]
                )
        finally:
            app_main.run_polling_loop = original_run_polling_loop
            app_main.collect_snapshot = original_collect_snapshot
            app_main.TelemetryStore = original_telemetry_store

        self.assertEqual(code, 0)
        self.assertEqual(stderr.getvalue(), "")
        self.assertEqual(
            stdout.getvalue().strip().splitlines(),
            [
                '{"cpu_percent": 1.0, "memory_percent": 2.0, "timestamp": 10.0}',
                '{"cpu_percent": 3.0, "memory_percent": 4.0, "timestamp": 11.0}',
            ],
        )
        self.assertEqual(len(created_stores), 1)
        store = created_stores[0]
        self.assertEqual(store.db_path, "telemetry.sqlite")
        self.assertEqual(store.appended, produced)
        self.assertTrue(store.closed)

    def test_main_watch_mode_continues_when_store_write_fails(self) -> None:
        produced = [
            SystemSnapshot(timestamp=10.0, cpu_percent=1.0, memory_percent=2.0),
            SystemSnapshot(timestamp=11.0, cpu_percent=3.0, memory_percent=4.0),
        ]
        created_stores: list[object] = []

        class _StoreStub:
            def __init__(self, db_path: str, retention_seconds: float) -> None:
                self.db_path = db_path
                self.append_attempts = 0
                self.closed = False
                created_stores.append(self)

            def append(self, snapshot: SystemSnapshot) -> None:
                del snapshot
                self.append_attempts += 1
                raise OSError("disk full")

            def close(self) -> None:
                self.closed = True

        def _run_polling_loop(interval_seconds: float, on_snapshot, *, stop_event) -> int:
            del interval_seconds
            emitted = 0
            for snapshot in produced:
                if stop_event.is_set():
                    break
                on_snapshot(snapshot)
                emitted += 1
            return emitted

        original_run_polling_loop = app_main.run_polling_loop
        original_collect_snapshot = app_main.collect_snapshot
        original_telemetry_store = app_main.TelemetryStore
        app_main.run_polling_loop = _run_polling_loop
        app_main.collect_snapshot = lambda: (_ for _ in ()).throw(
            AssertionError("collect_snapshot should not be called in watch mode.")
        )
        app_main.TelemetryStore = _StoreStub
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main(
                    ["--watch", "--json", "--db-path", "telemetry.sqlite", "--count", "2"]
                )
        finally:
            app_main.run_polling_loop = original_run_polling_loop
            app_main.collect_snapshot = original_collect_snapshot
            app_main.TelemetryStore = original_telemetry_store

        self.assertEqual(code, 0)
        self.assertEqual(
            stdout.getvalue().strip().splitlines(),
            [
                '{"cpu_percent": 1.0, "memory_percent": 2.0, "timestamp": 10.0}',
                '{"cpu_percent": 3.0, "memory_percent": 4.0, "timestamp": 11.0}',
            ],
        )
        self.assertIn("DVR persistence disabled: disk full", stderr.getvalue())
        self.assertEqual(len(created_stores), 1)
        store = created_stores[0]
        self.assertEqual(store.db_path, "telemetry.sqlite")
        self.assertEqual(store.append_attempts, 1)
        self.assertTrue(store.closed)

    def test_main_watch_mode_stops_after_count(self) -> None:
        produced = [
            SystemSnapshot(timestamp=10.0, cpu_percent=1.0, memory_percent=2.0),
            SystemSnapshot(timestamp=11.0, cpu_percent=3.0, memory_percent=4.0),
            SystemSnapshot(timestamp=12.0, cpu_percent=5.0, memory_percent=6.0),
        ]
        observed_interval_seconds: list[float] = []

        def _run_polling_loop(interval_seconds: float, on_snapshot, *, stop_event) -> int:
            observed_interval_seconds.append(interval_seconds)
            emitted = 0
            for snapshot in produced:
                if stop_event.is_set():
                    break
                on_snapshot(snapshot)
                emitted += 1
            return emitted

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
                code = app_main.main(
                    ["--watch", "--json", "--interval", "0.5", "--count", "2", "--no-persist"]
                )
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

    def test_main_watch_mode_rejects_non_finite_interval(self) -> None:
        original_run_polling_loop = app_main.run_polling_loop
        app_main.run_polling_loop = lambda *args, **kwargs: (_ for _ in ()).throw(
            AssertionError("run_polling_loop should not be called for invalid interval.")
        )

        try:
            for invalid_value in ("nan", "inf", "-inf"):
                stdout = io.StringIO()
                stderr = io.StringIO()
                argv = ["--watch", "--interval", invalid_value]
                if invalid_value.startswith("-"):
                    argv = ["--watch", f"--interval={invalid_value}"]
                with self.subTest(invalid_value=invalid_value):
                    with redirect_stdout(stdout), redirect_stderr(stderr):
                        with self.assertRaises(SystemExit) as ctx:
                            app_main.main(argv)

                    self.assertEqual(ctx.exception.code, 2)
                    self.assertEqual(stdout.getvalue(), "")
                    self.assertIn("finite number greater than 0", stderr.getvalue())
        finally:
            app_main.run_polling_loop = original_run_polling_loop

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
        self.assertIn("finite number greater than 0", stderr.getvalue())

    def test_main_watch_mode_rejects_non_numeric_interval(self) -> None:
        original_run_polling_loop = app_main.run_polling_loop
        app_main.run_polling_loop = lambda *args, **kwargs: (_ for _ in ()).throw(
            AssertionError("run_polling_loop should not be called for invalid interval.")
        )
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                with self.assertRaises(SystemExit) as ctx:
                    app_main.main(["--watch", "--interval", "abc"])
        finally:
            app_main.run_polling_loop = original_run_polling_loop

        self.assertEqual(ctx.exception.code, 2)
        self.assertEqual(stdout.getvalue(), "")
        self.assertIn("interval must be a finite number greater than 0", stderr.getvalue())
        self.assertNotIn("_positive_interval_seconds", stderr.getvalue())

    def test_main_rejects_non_numeric_since_or_until(self) -> None:
        for argv in (
            ["--since", "abc", "--db-path", "telemetry.sqlite"],
            ["--until", "abc", "--db-path", "telemetry.sqlite"],
        ):
            stdout = io.StringIO()
            stderr = io.StringIO()

            with self.subTest(argv=argv):
                with redirect_stdout(stdout), redirect_stderr(stderr):
                    with self.assertRaises(SystemExit) as ctx:
                        app_main.main(argv)

                self.assertEqual(ctx.exception.code, 2)
                self.assertEqual(stdout.getvalue(), "")
                self.assertIn("timestamp must be a finite number", stderr.getvalue())
                self.assertNotIn("_finite_timestamp_seconds", stderr.getvalue())

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
                code = app_main.main(
                    ["--watch", "--json", "--interval", "0.5", "--no-persist"]
                )
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
            code = app_main.main(["--watch", "--no-persist"])
        finally:
            app_main.run_polling_loop = original_run_polling_loop
            builtins.print = original_print

        self.assertEqual(code, 0)
        self.assertEqual(flush_flags, [True, True])

    def test_main_watch_mode_returns_zero_on_broken_pipe(self) -> None:
        produced = [
            SystemSnapshot(timestamp=10.0, cpu_percent=1.0, memory_percent=2.0),
        ]

        def _run_polling_loop(interval_seconds: float, on_snapshot, *, stop_event) -> int:
            for snapshot in produced:
                on_snapshot(snapshot)
            return len(produced)

        def _raise_broken_pipe(*_args, **_kwargs) -> None:
            raise BrokenPipeError()

        original_run_polling_loop = app_main.run_polling_loop
        original_print = builtins.print
        app_main.run_polling_loop = _run_polling_loop
        builtins.print = _raise_broken_pipe
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main(["--watch", "--no-persist"])
        finally:
            app_main.run_polling_loop = original_run_polling_loop
            builtins.print = original_print

        self.assertEqual(code, 0)
        self.assertEqual(stdout.getvalue(), "")
        self.assertEqual(stderr.getvalue(), "")

    def test_main_watch_mode_returns_zero_on_closed_stream_oserror(self) -> None:
        produced = [
            SystemSnapshot(timestamp=10.0, cpu_percent=1.0, memory_percent=2.0),
        ]

        def _run_polling_loop(interval_seconds: float, on_snapshot, *, stop_event) -> int:
            for snapshot in produced:
                on_snapshot(snapshot)
            return len(produced)

        def _raise_closed_stream(*_args, **_kwargs) -> None:
            raise OSError(errno.EINVAL, "Invalid argument")

        original_run_polling_loop = app_main.run_polling_loop
        original_print = builtins.print
        app_main.run_polling_loop = _run_polling_loop
        builtins.print = _raise_closed_stream
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main(["--watch", "--no-persist"])
        finally:
            app_main.run_polling_loop = original_run_polling_loop
            builtins.print = original_print

        self.assertEqual(code, 0)
        self.assertEqual(stdout.getvalue(), "")
        self.assertEqual(stderr.getvalue(), "")

    def test_main_returns_error_when_store_open_raises_oserror_einval(self) -> None:
        def _raise_einval(_db_path: str, retention_seconds: float) -> object:
            del retention_seconds
            raise OSError(errno.EINVAL, "Invalid argument")

        original_store = app_main.TelemetryStore
        original_collect_snapshot = app_main.collect_snapshot
        app_main.TelemetryStore = _raise_einval
        app_main.collect_snapshot = lambda: (_ for _ in ()).throw(
            AssertionError("collect_snapshot should not run when store init fails.")
        )
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main(["--db-path", "bad.sqlite"])
        finally:
            app_main.TelemetryStore = original_store
            app_main.collect_snapshot = original_collect_snapshot

        self.assertEqual(code, 2)
        self.assertEqual(stdout.getvalue(), "")
        self.assertIn("Failed to collect telemetry snapshot: [Errno 22]", stderr.getvalue())

    def test_main_watch_mode_returns_130_when_interrupted(self) -> None:
        def _raise_interrupt(interval_seconds: float, on_snapshot, *, stop_event) -> int:
            raise KeyboardInterrupt()

        original = app_main.run_polling_loop
        app_main.run_polling_loop = _raise_interrupt
        stdout = io.StringIO()
        stderr = io.StringIO()
        try:
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = app_main.main(["--watch", "--no-persist"])
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
                code = app_main.main(["--no-persist"])
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
                code = app_main.main(["--no-persist"])
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
                code = app_main.main(["--json", "--no-persist"])
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

