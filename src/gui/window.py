from __future__ import annotations
# pyright: reportAttributeAccessIssue=false, reportPossiblyUnboundVariable=false, reportRedeclaration=false

import datetime as dt
import os
import pathlib
import sys
import threading
from typing import Callable

SRC_ROOT = pathlib.Path(__file__).resolve().parents[1]
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from core.poller import collect_snapshot, run_polling_loop  # noqa: E402
from core.store import TelemetryStore  # noqa: E402
from core.types import SystemSnapshot  # noqa: E402

_QT_IMPORT_ERROR: ImportError | None = None

try:
    from PySide6.QtCore import QObject, Qt, QThread, Signal, Slot
    from PySide6.QtGui import QCloseEvent
    from PySide6.QtWidgets import QApplication, QLabel, QVBoxLayout, QWidget
except ImportError as exc:  # pragma: no cover - exercised in tests by monkeypatching
    _QT_IMPORT_ERROR = exc


def format_snapshot_lines(snapshot: SystemSnapshot) -> dict[str, str]:
    utc_timestamp = dt.datetime.fromtimestamp(
        snapshot.timestamp,
        tz=dt.timezone.utc,
    ).strftime("%H:%M:%S UTC")
    return {
        "cpu": f"CPU {snapshot.cpu_percent:.1f}%",
        "memory": f"Memory {snapshot.memory_percent:.1f}%",
        "timestamp": f"Updated {utc_timestamp}",
    }


def resolve_gui_db_path(env: dict[str, str] | None = None) -> str | None:
    source = os.environ if env is None else env
    raw_value = source.get("AURA_DB_PATH")
    if raw_value is None:
        return None

    db_path = raw_value.strip()
    return db_path or None


class DvrRecorder:
    """Persists GUI snapshots into the local telemetry store when configured."""

    def __init__(self, db_path: str | None) -> None:
        self._lock = threading.Lock()
        self.db_path = db_path
        self.sample_count: int | None = None
        self.latest_snapshot: SystemSnapshot | None = None
        self.error: str | None = None
        self._store: TelemetryStore | None = None

        if db_path is None:
            return

        store: TelemetryStore | None = None
        try:
            store = TelemetryStore(db_path)
            sample_count = store.count()
            latest_snapshot: SystemSnapshot | None = None
            latest_snapshots = store.latest(limit=1)
            if latest_snapshots:
                latest_snapshot = latest_snapshots[0]
        except Exception as exc:
            with self._lock:
                self.error = str(exc)
            if store is not None:
                try:
                    store.close()
                except Exception:
                    pass
            return

        with self._lock:
            self._store = store
            self.sample_count = sample_count
            self.latest_snapshot = latest_snapshot

    def append(self, snapshot: SystemSnapshot) -> None:
        with self._lock:
            if self._store is None:
                return

            try:
                self.sample_count = self._store.append_and_count(snapshot)
            except Exception as exc:
                self.error = str(exc)
                self._close_locked()

    def get_latest_snapshot(self) -> SystemSnapshot | None:
        with self._lock:
            return self.latest_snapshot

    def get_status(self) -> tuple[str | None, int | None, str | None]:
        with self._lock:
            return self.db_path, self.sample_count, self.error

    def _close_locked(self) -> None:
        if self._store is None:
            return

        try:
            self._store.close()
        finally:
            self._store = None

    def close(self) -> None:
        with self._lock:
            self._close_locked()


def format_initial_status(recorder: DvrRecorder) -> str:
    db_path, sample_count, error = recorder.get_status()
    if db_path is None:
        return "Collecting telemetry..."
    if error is not None:
        return f"Collecting telemetry... | DVR unavailable: {error}"
    sample_count = 0 if sample_count is None else sample_count
    return f"Collecting telemetry... | DVR samples: {sample_count}"


def format_stream_status(recorder: DvrRecorder) -> str:
    db_path, sample_count, error = recorder.get_status()
    if db_path is None:
        return "Streaming telemetry"
    if error is not None:
        return f"Streaming telemetry | DVR unavailable: {error}"
    sample_count = 0 if sample_count is None else sample_count
    return f"Streaming telemetry | DVR samples: {sample_count}"


def require_qt() -> None:
    if _QT_IMPORT_ERROR is not None:
        raise RuntimeError(
            "PySide6 is required for the GUI slice. Run `uv sync` to install it."
        ) from _QT_IMPORT_ERROR


if _QT_IMPORT_ERROR is None:

    class SnapshotWorker(QObject):
        snapshot_ready = Signal(float, float, float, str)
        error = Signal(str)
        finished = Signal()

        def __init__(
            self,
            *,
            interval_seconds: float,
            collect: Callable[[], SystemSnapshot],
            recorder: DvrRecorder,
        ) -> None:
            super().__init__()
            self._interval_seconds = interval_seconds
            self._collect = collect
            self._recorder = recorder
            self._stop_event = threading.Event()

        @Slot()
        def run(self) -> None:
            def _on_snapshot(snapshot: SystemSnapshot) -> None:
                self._recorder.append(snapshot)
                self.snapshot_ready.emit(
                    snapshot.timestamp,
                    snapshot.cpu_percent,
                    snapshot.memory_percent,
                    format_stream_status(self._recorder),
                )

            def _on_error(exc: Exception) -> None:
                if self._stop_event.is_set():
                    return
                self.error.emit(str(exc))

            try:
                run_polling_loop(
                    self._interval_seconds,
                    _on_snapshot,
                    stop_event=self._stop_event,
                    collect=self._collect,
                    on_error=_on_error,
                    continue_on_error=True,
                )
            except Exception as exc:
                if not self._stop_event.is_set():
                    self.error.emit(str(exc))
            finally:
                self.finished.emit()

        def stop(self) -> None:
            self._stop_event.set()


    class AuraWindow(QWidget):
        def __init__(
            self,
            *,
            interval_seconds: float = 1.0,
            collect: Callable[[], SystemSnapshot] = collect_snapshot,
            db_path: str | None = None,
        ) -> None:
            super().__init__()
            self.setWindowTitle("Aura | Telemetry")
            self.setMinimumSize(540, 260)
            self._recorder = DvrRecorder(db_path)
            self._build_layout()
            self._seed_latest_snapshot()

            self._worker_thread = QThread(self)
            self._worker = SnapshotWorker(
                interval_seconds=interval_seconds,
                collect=collect,
                recorder=self._recorder,
            )
            self._worker.moveToThread(self._worker_thread)
            self._worker_thread.started.connect(self._worker.run)
            self._worker.snapshot_ready.connect(self._on_snapshot)
            self._worker.error.connect(self._on_worker_error)
            self._worker.finished.connect(self._worker_thread.quit)
            self._worker_thread.finished.connect(self._worker.deleteLater)
            self._worker_thread.start()

        def _seed_latest_snapshot(self) -> None:
            latest_snapshot = self._recorder.get_latest_snapshot()
            if latest_snapshot is None:
                return
            self._render_snapshot(latest_snapshot)

        def _render_snapshot(self, snapshot: SystemSnapshot) -> None:
            lines = format_snapshot_lines(snapshot)
            self._cpu_label.setText(lines["cpu"])
            self._memory_label.setText(lines["memory"])
            self._timestamp_label.setText(lines["timestamp"])

        def _build_layout(self) -> None:
            self.setStyleSheet(
                """
                QWidget {
                    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                        stop:0 #071320, stop:1 #0d2033);
                    color: #d6ebff;
                    font-family: Segoe UI;
                }
                QLabel#title {
                    font-size: 26px;
                    font-weight: 600;
                    color: #f7fbff;
                }
                QLabel#metric {
                    font-size: 22px;
                    font-weight: 500;
                    color: #9dd9ff;
                }
                QLabel#status {
                    font-size: 13px;
                    color: #75b8ff;
                }
                """
            )
            layout = QVBoxLayout(self)
            layout.setContentsMargins(28, 24, 28, 24)
            layout.setSpacing(10)

            self._title_label = QLabel("Aura Live Telemetry")
            self._title_label.setObjectName("title")
            self._title_label.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
            layout.addWidget(self._title_label)

            self._cpu_label = QLabel("CPU --.-%")
            self._cpu_label.setObjectName("metric")
            layout.addWidget(self._cpu_label)

            self._memory_label = QLabel("Memory --.-%")
            self._memory_label.setObjectName("metric")
            layout.addWidget(self._memory_label)

            self._timestamp_label = QLabel("Updated --:--:-- UTC")
            self._timestamp_label.setObjectName("status")
            layout.addWidget(self._timestamp_label)

            self._status_label = QLabel("Collecting telemetry...")
            self._status_label.setObjectName("status")
            self._status_label.setText(format_initial_status(self._recorder))
            layout.addWidget(self._status_label)
            layout.addStretch(1)

        @Slot(float, float, float, str)
        def _on_snapshot(
            self,
            timestamp: float,
            cpu_percent: float,
            memory_percent: float,
            status_text: str,
        ) -> None:
            snapshot = SystemSnapshot(
                timestamp=timestamp,
                cpu_percent=cpu_percent,
                memory_percent=memory_percent,
            )
            self._render_snapshot(snapshot)
            self._status_label.setText(status_text)

        @Slot(str)
        def _on_worker_error(self, message: str) -> None:
            self._status_label.setText(f"Telemetry error: {message}")

        def closeEvent(self, event: QCloseEvent) -> None:
            self._worker.stop()
            self._worker_thread.quit()
            self._worker_thread.wait(1500)
            self._recorder.close()
            super().closeEvent(event)

else:

    class AuraWindow:  # pragma: no cover - only used when Qt is unavailable
        def __init__(self, **_: object) -> None:
            require_qt()


def run(argv: list[str] | None = None) -> int:
    del argv  # Reserved for future window args without changing signature.
    try:
        require_qt()
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    app = QApplication.instance()
    if app is None:
        app = QApplication(sys.argv)

    window = AuraWindow(db_path=resolve_gui_db_path())
    window.show()
    return int(app.exec())


if __name__ == "__main__":
    raise SystemExit(run())
