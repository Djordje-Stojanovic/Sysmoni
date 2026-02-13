from __future__ import annotations
# pyright: reportAttributeAccessIssue=false, reportPossiblyUnboundVariable=false, reportRedeclaration=false

import datetime as dt
import pathlib
import sys
import threading
from typing import Callable

SRC_ROOT = pathlib.Path(__file__).resolve().parents[1]
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from core.poller import collect_snapshot, run_polling_loop  # noqa: E402
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


def require_qt() -> None:
    if _QT_IMPORT_ERROR is not None:
        raise RuntimeError(
            "PySide6 is required for the GUI slice. Run `uv sync` to install it."
        ) from _QT_IMPORT_ERROR


if _QT_IMPORT_ERROR is None:

    class SnapshotWorker(QObject):
        snapshot_ready = Signal(float, float, float)
        error = Signal(str)
        finished = Signal()

        def __init__(
            self,
            *,
            interval_seconds: float,
            collect: Callable[[], SystemSnapshot],
        ) -> None:
            super().__init__()
            self._interval_seconds = interval_seconds
            self._collect = collect
            self._stop_event = threading.Event()

        @Slot()
        def run(self) -> None:
            def _on_snapshot(snapshot: SystemSnapshot) -> None:
                self.snapshot_ready.emit(
                    snapshot.timestamp,
                    snapshot.cpu_percent,
                    snapshot.memory_percent,
                )

            try:
                run_polling_loop(
                    self._interval_seconds,
                    _on_snapshot,
                    stop_event=self._stop_event,
                    collect=self._collect,
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
        ) -> None:
            super().__init__()
            self.setWindowTitle("Aura | Telemetry")
            self.setMinimumSize(540, 260)
            self._build_layout()

            self._worker_thread = QThread(self)
            self._worker = SnapshotWorker(
                interval_seconds=interval_seconds,
                collect=collect,
            )
            self._worker.moveToThread(self._worker_thread)
            self._worker_thread.started.connect(self._worker.run)
            self._worker.snapshot_ready.connect(self._on_snapshot)
            self._worker.error.connect(self._on_worker_error)
            self._worker.finished.connect(self._worker_thread.quit)
            self._worker_thread.finished.connect(self._worker.deleteLater)
            self._worker_thread.start()

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
            layout.addWidget(self._status_label)
            layout.addStretch(1)

        @Slot(float, float, float)
        def _on_snapshot(self, timestamp: float, cpu_percent: float, memory_percent: float) -> None:
            snapshot = SystemSnapshot(
                timestamp=timestamp,
                cpu_percent=cpu_percent,
                memory_percent=memory_percent,
            )
            lines = format_snapshot_lines(snapshot)
            self._cpu_label.setText(lines["cpu"])
            self._memory_label.setText(lines["memory"])
            self._timestamp_label.setText(lines["timestamp"])
            self._status_label.setText("Streaming telemetry")

        @Slot(str)
        def _on_worker_error(self, message: str) -> None:
            self._status_label.setText(f"Telemetry error: {message}")

        def closeEvent(self, event: QCloseEvent) -> None:
            self._worker.stop()
            self._worker_thread.quit()
            self._worker_thread.wait(1500)
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

    window = AuraWindow()
    window.show()
    return int(app.exec())


if __name__ == "__main__":
    raise SystemExit(run())
