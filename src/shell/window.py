from __future__ import annotations
# pyright: reportAttributeAccessIssue=false, reportPossiblyUnboundVariable=false, reportRedeclaration=false, reportArgumentType=false

import os
import pathlib
import sys
import threading
import time
from dataclasses import dataclass
from typing import Callable, Literal, cast

SRC_ROOT = pathlib.Path(__file__).resolve().parents[1]
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from contracts.types import ProcessSample, SystemSnapshot  # noqa: E402
from render import (  # noqa: E402
    DEFAULT_PROCESS_ROW_COUNT,
    DEFAULT_THEME,
    RadialGauge,
    RadialGaugeConfig,
    SparkLine,
    SparkLineConfig,
    TimelineConfig,
    TimelineWidget,
    build_shell_stylesheet,
    compose_cockpit_frame,
    format_initial_status,
    format_process_rows,
    format_snapshot_lines,
    format_stream_status,
)
from runtime.dvr import query_timeline  # noqa: E402
from runtime.store import TelemetryStore  # noqa: E402
from shell.titlebar import AuraTitleBar  # noqa: E402
from telemetry.poller import collect_snapshot, collect_top_processes, run_polling_loop  # noqa: E402

_QT_IMPORT_ERROR: ImportError | None = None

try:
    from PySide6.QtCore import QObject, Qt, QThread, QTimer, Signal, Slot
    from PySide6.QtGui import QCloseEvent
    from PySide6.QtWidgets import (
        QApplication,
        QFrame,
        QHBoxLayout,
        QLabel,
        QLayout,
        QPushButton,
        QVBoxLayout,
        QWidget,
    )
except ImportError as exc:  # pragma: no cover - exercised in tests by monkeypatching
    _QT_IMPORT_ERROR = exc


def resolve_gui_db_path(env: dict[str, str] | None = None) -> str | None:
    source = os.environ if env is None else env
    raw_value = source.get("AURA_DB_PATH")
    if raw_value is None:
        return None

    db_path = raw_value.strip()
    return db_path or None

DockSlot = Literal["left", "center", "right"]
PanelId = Literal[
    "telemetry_overview",
    "top_processes",
    "dvr_timeline",
    "render_surface",
]
DOCK_SLOTS: tuple[DockSlot, ...] = ("left", "center", "right")
PANEL_IDS: tuple[PanelId, ...] = (
    "telemetry_overview",
    "top_processes",
    "dvr_timeline",
    "render_surface",
)


@dataclass(slots=True)
class PanelSpec:
    panel_id: PanelId
    title: str
    slot: DockSlot


@dataclass(slots=True)
class DockState:
    slot_tabs: dict[DockSlot, list[PanelId]]
    active_tab: dict[DockSlot, int]


def _validate_slot(slot: DockSlot | str) -> DockSlot:
    if slot not in DOCK_SLOTS:
        raise ValueError(f"Invalid dock slot: {slot}")
    return cast(DockSlot, slot)


def _validate_panel_id(panel_id: PanelId | str) -> PanelId:
    if panel_id not in PANEL_IDS:
        raise ValueError(f"Unknown panel: {panel_id}")
    return cast(PanelId, panel_id)


def _clone_dock_state(state: DockState) -> DockState:
    return DockState(
        slot_tabs={slot: list(state.slot_tabs.get(slot, [])) for slot in DOCK_SLOTS},
        active_tab={slot: int(state.active_tab.get(slot, 0)) for slot in DOCK_SLOTS},
    )


def _clamp_active_index(active_index: int, tab_count: int) -> int:
    if tab_count <= 0:
        return 0
    return max(0, min(active_index, tab_count - 1))


def build_default_panel_specs() -> dict[PanelId, PanelSpec]:
    return {
        "telemetry_overview": PanelSpec(
            panel_id="telemetry_overview",
            title="Telemetry Overview",
            slot="left",
        ),
        "top_processes": PanelSpec(
            panel_id="top_processes",
            title="Top Processes",
            slot="center",
        ),
        "dvr_timeline": PanelSpec(
            panel_id="dvr_timeline",
            title="DVR Timeline",
            slot="center",
        ),
        "render_surface": PanelSpec(
            panel_id="render_surface",
            title="Render Surface",
            slot="right",
        ),
    }


def build_default_dock_state(
    panel_specs: dict[PanelId, PanelSpec] | None = None,
) -> DockState:
    specs = build_default_panel_specs() if panel_specs is None else panel_specs
    slot_tabs: dict[DockSlot, list[PanelId]] = {slot: [] for slot in DOCK_SLOTS}
    for panel_id in PANEL_IDS:
        panel_spec = specs[panel_id]
        slot_tabs[panel_spec.slot].append(panel_id)
    return DockState(
        slot_tabs=slot_tabs,
        active_tab={slot: 0 for slot in DOCK_SLOTS},
    )


def _find_panel_slot(
    slot_tabs: dict[DockSlot, list[PanelId]],
    panel_id: PanelId,
) -> DockSlot | None:
    for slot in DOCK_SLOTS:
        if panel_id in slot_tabs.get(slot, []):
            return slot
    return None


def move_panel(
    state: DockState,
    panel_id: PanelId,
    to_slot: DockSlot,
    to_index: int | None = None,
) -> DockState:
    if panel_id not in PANEL_IDS:
        raise ValueError(f"Unknown panel: {panel_id}")

    destination_slot = _validate_slot(to_slot)
    next_state = _clone_dock_state(state)
    source_slot = _find_panel_slot(next_state.slot_tabs, panel_id)
    if source_slot is None:
        raise ValueError(f"Panel is not docked: {panel_id}")

    source_tabs = next_state.slot_tabs[source_slot]
    source_tabs.remove(panel_id)
    destination_tabs = next_state.slot_tabs[destination_slot]

    if to_index is None:
        insert_index = len(destination_tabs)
    else:
        if isinstance(to_index, bool):
            raise ValueError("to_index must be an integer.")
        insert_index = int(to_index)
        if insert_index < 0 or insert_index > len(destination_tabs):
            raise ValueError("to_index is outside the destination slot tab range.")

    destination_tabs.insert(insert_index, panel_id)

    for slot in DOCK_SLOTS:
        next_state.active_tab[slot] = _clamp_active_index(
            next_state.active_tab[slot],
            len(next_state.slot_tabs[slot]),
        )
    next_state.active_tab[destination_slot] = _clamp_active_index(
        insert_index,
        len(destination_tabs),
    )
    return next_state


def set_active_tab(
    state: DockState,
    slot: DockSlot,
    tab_index: int,
) -> DockState:
    target_slot = _validate_slot(slot)
    if isinstance(tab_index, bool):
        raise ValueError("tab_index must be an integer.")

    next_state = _clone_dock_state(state)
    tabs = next_state.slot_tabs[target_slot]
    index = int(tab_index)
    if not tabs:
        if index != 0:
            raise ValueError("tab_index must be 0 for an empty slot.")
        next_state.active_tab[target_slot] = 0
        return next_state
    if index < 0 or index >= len(tabs):
        raise ValueError("tab_index is outside the slot tab range.")

    next_state.active_tab[target_slot] = index
    return next_state


def get_active_panel(state: DockState, slot: DockSlot) -> PanelId | None:
    target_slot = _validate_slot(slot)
    tabs = state.slot_tabs.get(target_slot, [])
    if not tabs:
        return None

    active_index = _clamp_active_index(
        int(state.active_tab.get(target_slot, 0)),
        len(tabs),
    )
    return tabs[active_index]


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


def format_render_panel_status(snapshot: SystemSnapshot, stream_status: str) -> str:
    return (
        "Render bridge live | "
        f"CPU {snapshot.cpu_percent:.1f}% | "
        f"Memory {snapshot.memory_percent:.1f}% | "
        f"{stream_status}"
    )


def format_render_panel_hint(process_rows: list[str]) -> str:
    if not process_rows:
        return "Top process feed waiting for telemetry rows."
    return f"Top process | {str(process_rows[0]).strip()}"


def build_window_stylesheet(accent_intensity: float = 0.0) -> str:
    theme = DEFAULT_THEME
    return (
        build_shell_stylesheet(theme, accent_intensity=accent_intensity)
        + f"""
                QLabel#title {{
                    padding-bottom: 6px;
                    font-weight: 650;
                }}
                QWidget#auraTitleBar {{
                    background: rgba(4, 12, 22, 0.95);
                    border-bottom: 1px solid rgba(116, 172, 220, 0.30);
                }}
                QLabel#titlebarLabel {{
                    font-size: 14px;
                    font-weight: 600;
                    color: {theme.title_color};
                    background: transparent;
                }}
                QPushButton#titlebarMinBtn, QPushButton#titlebarMaxBtn {{
                    background: transparent;
                    border: none;
                    color: {theme.text_primary};
                    font-size: 14px;
                    border-radius: 4px;
                }}
                QPushButton#titlebarMinBtn:hover, QPushButton#titlebarMaxBtn:hover {{
                    background: rgba(80, 140, 200, 0.25);
                }}
                QPushButton#titlebarMinBtn:pressed, QPushButton#titlebarMaxBtn:pressed {{
                    background: rgba(80, 140, 200, 0.40);
                }}
                QPushButton#titlebarCloseBtn {{
                    background: transparent;
                    border: none;
                    color: {theme.text_primary};
                    font-size: 14px;
                    border-radius: 4px;
                }}
                QPushButton#titlebarCloseBtn:hover {{
                    background: rgba(220, 50, 50, 0.75);
                    color: #ffffff;
                }}
                QPushButton#titlebarCloseBtn:pressed {{
                    background: rgba(200, 30, 30, 0.90);
                    color: #ffffff;
                }}
                QFrame#slotFrame {{
                    background: rgba(8, 22, 36, 0.62);
                    border: 1px solid rgba(116, 172, 220, 0.45);
                    border-radius: 8px;
                }}
                QLabel#slotTitle {{
                    font-size: 12px;
                    font-weight: 600;
                    letter-spacing: 1px;
                    color: {theme.section_color};
                }}
                QPushButton#tabButton {{
                    font-size: 12px;
                    border: 1px solid rgba(99, 154, 200, 0.5);
                    border-radius: 6px;
                    background: rgba(6, 20, 33, 0.8);
                    color: #9ec9ee;
                    padding: 4px 8px;
                }}
                QPushButton#tabButton:checked {{
                    background: rgba(32, 91, 142, 0.95);
                    color: #ecf5ff;
                }}
                QFrame#panelFrame {{
                    background: rgba(5, 14, 24, 0.72);
                    border: 1px solid rgba(98, 149, 192, 0.35);
                    border-radius: 6px;
                }}
                QLabel#panelTitle {{
                    font-size: 14px;
                    font-weight: 620;
                    color: {theme.title_color};
                }}
                QPushButton#moveButton {{
                    min-width: 24px;
                    max-width: 24px;
                    min-height: 20px;
                    max-height: 20px;
                    font-size: 11px;
                    font-weight: 600;
                    border: 1px solid rgba(89, 142, 190, 0.7);
                    border-radius: 4px;
                    background: rgba(18, 45, 70, 0.8);
                    color: #cbe7ff;
                }}
                """
    )


def require_qt() -> None:
    if _QT_IMPORT_ERROR is not None:
        raise RuntimeError(
            "PySide6 is required for the GUI slice. Run `uv sync` to install it."
        ) from _QT_IMPORT_ERROR


if _QT_IMPORT_ERROR is None:

    class SnapshotWorker(QObject):
        snapshot_ready = Signal(float, float, float, str, list)
        error = Signal(str)
        finished = Signal()

        def __init__(
            self,
            *,
            interval_seconds: float,
            collect: Callable[[], SystemSnapshot],
            collect_processes: Callable[[], list[ProcessSample]],
            process_row_count: int,
            recorder: DvrRecorder,
        ) -> None:
            super().__init__()
            self._interval_seconds = interval_seconds
            self._collect = collect
            self._collect_processes = collect_processes
            self._process_row_count = process_row_count
            self._recorder = recorder
            self._stop_event = threading.Event()

        @Slot()
        def run(self) -> None:
            def _on_snapshot(snapshot: SystemSnapshot) -> None:
                process_rows: list[str]
                try:
                    process_rows = format_process_rows(
                        self._collect_processes(),
                        row_count=self._process_row_count,
                    )
                except Exception as exc:
                    process_rows = format_process_rows([], row_count=self._process_row_count)
                    if not self._stop_event.is_set():
                        self.error.emit(f"Process telemetry error: {exc}")
                self._recorder.append(snapshot)
                self.snapshot_ready.emit(
                    snapshot.timestamp,
                    snapshot.cpu_percent,
                    snapshot.memory_percent,
                    format_stream_status(self._recorder),
                    process_rows,
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
            collect_processes: Callable[[], list[ProcessSample]] | None = None,
            process_row_count: int = DEFAULT_PROCESS_ROW_COUNT,
            db_path: str | None = None,
        ) -> None:
            super().__init__()
            self.setWindowFlags(
                Qt.FramelessWindowHint
                | Qt.WindowMinMaxButtonsHint
                | Qt.Window
            )
            self.setWindowTitle("Aura | Cockpit")
            self.setMinimumSize(980, 560)
            self._process_row_count = (
                process_row_count
                if process_row_count > 0
                else DEFAULT_PROCESS_ROW_COUNT
            )
            if collect_processes is None:

                def _collect_processes() -> list[ProcessSample]:
                    return collect_top_processes(limit=self._process_row_count)

                collect_processes = _collect_processes
            self._recorder = DvrRecorder(db_path)
            self._panel_specs = build_default_panel_specs()
            self._dock_state = build_default_dock_state(self._panel_specs)
            self._is_live_mode = True
            self._frozen_snapshot: SystemSnapshot | None = None
            self._latest_live_snapshot: SystemSnapshot | None = None
            self._latest_live_status_text = format_initial_status(self._recorder)
            self._latest_live_process_rows = format_process_rows(
                [],
                row_count=self._process_row_count,
            )
            self._timeline_resolution = 500
            self._timeline_refresh_interval_seconds = 1.0
            self._timeline_last_refresh_monotonic = 0.0
            self._timeline_snapshots: list[SystemSnapshot] = []
            self._panel_widgets: dict[PanelId, QWidget] = {}
            self._slot_tab_layouts: dict[DockSlot, QHBoxLayout] = {}
            self._slot_content_layouts: dict[DockSlot, QVBoxLayout] = {}
            self._build_layout()
            self._seed_latest_snapshot()
            self._refresh_timeline_data(force=True)

            self._phase: float = 0.0
            self._last_frame_time: float = time.monotonic()
            self._last_cpu: float = 0.0
            self._last_mem: float = 0.0
            self._accent_bucket: int = 0

            self._worker_thread = QThread(self)
            self._worker = SnapshotWorker(
                interval_seconds=interval_seconds,
                collect=collect,
                collect_processes=collect_processes,
                process_row_count=self._process_row_count,
                recorder=self._recorder,
            )
            self._worker.moveToThread(self._worker_thread)
            self._worker_thread.started.connect(self._worker.run)
            self._worker.snapshot_ready.connect(self._on_snapshot)
            self._worker.error.connect(self._on_worker_error)
            self._worker.finished.connect(self._worker_thread.quit)
            self._worker_thread.finished.connect(self._worker.deleteLater)
            self._worker_thread.start()

            self._frame_timer = QTimer(self)
            self._frame_timer.setTimerType(Qt.PreciseTimer)
            self._frame_timer.setInterval(16)  # ~60 FPS
            self._frame_timer.timeout.connect(self._on_frame_tick)
            self._frame_timer.start()

        @staticmethod
        def _clear_layout(layout: QLayout, *, delete_widgets: bool) -> None:
            while layout.count() > 0:
                item = layout.takeAt(0)
                if item is None:
                    continue
                child_layout = item.layout()
                if child_layout is not None:
                    AuraWindow._clear_layout(child_layout, delete_widgets=delete_widgets)
                    continue

                widget = item.widget()
                if widget is None:
                    continue
                if delete_widgets:
                    widget.deleteLater()
                else:
                    widget.setParent(None)

        def _build_panel(
            self,
            panel_id: PanelId,
            title: str,
        ) -> tuple[QFrame, QVBoxLayout]:
            panel = QFrame()
            panel.setObjectName("panelFrame")
            panel_layout = QVBoxLayout(panel)
            panel_layout.setContentsMargins(10, 10, 10, 10)
            panel_layout.setSpacing(8)

            header_layout = QHBoxLayout()
            header_layout.setSpacing(6)

            title_label = QLabel(title)
            title_label.setObjectName("panelTitle")
            header_layout.addWidget(title_label)
            header_layout.addStretch(1)

            for slot in DOCK_SLOTS:
                move_button = QPushButton(slot[0].upper())
                move_button.setObjectName("moveButton")
                move_button.setToolTip(f"Move panel to {slot.title()} slot")
                move_button.clicked.connect(
                    lambda _checked=False, pid=panel_id, destination=slot: self._move_panel(
                        pid,
                        destination,
                    )
                )
                header_layout.addWidget(move_button)

            panel_layout.addLayout(header_layout)

            body_layout = QVBoxLayout()
            body_layout.setContentsMargins(0, 0, 0, 0)
            body_layout.setSpacing(6)
            panel_layout.addLayout(body_layout)
            panel_layout.addStretch(1)
            return panel, body_layout

        def _build_telemetry_panel(self) -> QWidget:
            panel, body_layout = self._build_panel(
                "telemetry_overview",
                self._panel_specs["telemetry_overview"].title,
            )

            gauge_row = QHBoxLayout()
            gauge_row.setSpacing(12)

            cpu_col = QVBoxLayout()
            cpu_title = QLabel("CPU")
            cpu_title.setObjectName("status")
            cpu_title.setAlignment(Qt.AlignCenter)
            cpu_col.addWidget(cpu_title)
            self._cpu_gauge = RadialGauge(RadialGaugeConfig(min_size=110))
            cpu_col.addWidget(self._cpu_gauge)
            gauge_row.addLayout(cpu_col)

            mem_col = QVBoxLayout()
            mem_title = QLabel("MEMORY")
            mem_title.setObjectName("status")
            mem_title.setAlignment(Qt.AlignCenter)
            mem_col.addWidget(mem_title)
            self._mem_gauge = RadialGauge(RadialGaugeConfig(min_size=110))
            mem_col.addWidget(self._mem_gauge)
            gauge_row.addLayout(mem_col)

            body_layout.addLayout(gauge_row)

            self._cpu_sparkline = SparkLine(SparkLineConfig(buffer_size=120))
            body_layout.addWidget(self._cpu_sparkline)

            self._mem_sparkline = SparkLine(SparkLineConfig(buffer_size=120))
            body_layout.addWidget(self._mem_sparkline)

            self._timestamp_label = QLabel("Updated --:--:-- UTC")
            self._timestamp_label.setObjectName("status")
            body_layout.addWidget(self._timestamp_label)

            self._status_label = QLabel(format_initial_status(self._recorder))
            self._status_label.setObjectName("status")
            body_layout.addWidget(self._status_label)
            body_layout.addStretch(1)
            return panel

        def _build_processes_panel(self) -> QWidget:
            panel, body_layout = self._build_panel(
                "top_processes",
                self._panel_specs["top_processes"].title,
            )

            self._process_labels = []
            for row in self._latest_live_process_rows:
                row_label = QLabel(row)
                row_label.setObjectName("process")
                self._process_labels.append(row_label)
                body_layout.addWidget(row_label)

            body_layout.addStretch(1)
            return panel

        def _build_timeline_panel(self) -> QWidget:
            panel, body_layout = self._build_panel(
                "dvr_timeline",
                self._panel_specs["dvr_timeline"].title,
            )

            self._timeline_widget = TimelineWidget(
                TimelineConfig(show_memory=True),
            )
            self._timeline_widget.scrub_position_changed.connect(self._on_timeline_scrub)
            body_layout.addWidget(self._timeline_widget, 1)

            controls_layout = QHBoxLayout()
            controls_layout.setSpacing(8)

            self._timeline_live_btn = QPushButton("Live")
            self._timeline_live_btn.setObjectName("tabButton")
            self._timeline_live_btn.clicked.connect(lambda: self._set_live_mode(True))
            self._timeline_live_btn.setEnabled(False)
            controls_layout.addWidget(self._timeline_live_btn)

            self._timeline_mode_label = QLabel(
                "Timeline live | drag scrubber to inspect history."
            )
            self._timeline_mode_label.setObjectName("status")
            controls_layout.addWidget(self._timeline_mode_label, 1)

            body_layout.addLayout(controls_layout)
            return panel

        def _build_render_panel(self) -> QWidget:
            panel, body_layout = self._build_panel(
                "render_surface",
                self._panel_specs["render_surface"].title,
            )

            header_row = QHBoxLayout()
            header_row.setSpacing(10)

            self._render_cpu_gauge = RadialGauge(
                RadialGaugeConfig(min_size=80, arc_width=8),
            )
            header_row.addWidget(self._render_cpu_gauge)

            text_col = QVBoxLayout()
            text_col.setSpacing(4)

            self._render_status_label = QLabel("Render bridge waiting for telemetry frame.")
            self._render_status_label.setObjectName("status")
            text_col.addWidget(self._render_status_label)

            self._render_timestamp_label = QLabel("Updated --:--:-- UTC")
            self._render_timestamp_label.setObjectName("status")
            text_col.addWidget(self._render_timestamp_label)

            self._render_hint_label = QLabel("Top process feed waiting for telemetry rows.")
            self._render_hint_label.setObjectName("status")
            text_col.addWidget(self._render_hint_label)

            header_row.addLayout(text_col, 1)
            body_layout.addLayout(header_row)
            body_layout.addStretch(1)
            return panel

        def _build_layout(self) -> None:
            self.setStyleSheet(build_window_stylesheet())
            root_layout = QVBoxLayout(self)
            root_layout.setContentsMargins(0, 0, 0, 0)
            root_layout.setSpacing(0)

            self._titlebar = AuraTitleBar(self)
            root_layout.addWidget(self._titlebar)

            body_layout = QVBoxLayout()
            body_layout.setContentsMargins(20, 12, 20, 18)
            body_layout.setSpacing(12)
            root_layout.addLayout(body_layout, 1)

            slots_layout = QHBoxLayout()
            slots_layout.setSpacing(12)
            body_layout.addLayout(slots_layout, 1)

            for slot in DOCK_SLOTS:
                slot_frame = QFrame()
                slot_frame.setObjectName("slotFrame")
                slot_layout = QVBoxLayout(slot_frame)
                slot_layout.setContentsMargins(10, 10, 10, 10)
                slot_layout.setSpacing(8)

                slot_label = QLabel(slot.upper())
                slot_label.setObjectName("slotTitle")
                slot_layout.addWidget(slot_label)

                tab_layout = QHBoxLayout()
                tab_layout.setSpacing(4)
                slot_layout.addLayout(tab_layout)
                self._slot_tab_layouts[slot] = tab_layout

                content_layout = QVBoxLayout()
                content_layout.setContentsMargins(0, 0, 0, 0)
                content_layout.setSpacing(6)
                slot_layout.addLayout(content_layout, 1)
                self._slot_content_layouts[slot] = content_layout

                slots_layout.addWidget(slot_frame, 1)

            self._panel_widgets = {
                "telemetry_overview": self._build_telemetry_panel(),
                "top_processes": self._build_processes_panel(),
                "dvr_timeline": self._build_timeline_panel(),
                "render_surface": self._build_render_panel(),
            }
            self._render_dock_layout()

        def _render_dock_layout(self) -> None:
            for slot in DOCK_SLOTS:
                tab_layout = self._slot_tab_layouts[slot]
                content_layout = self._slot_content_layouts[slot]
                self._clear_layout(tab_layout, delete_widgets=True)
                self._clear_layout(content_layout, delete_widgets=False)

                tabs = self._dock_state.slot_tabs.get(slot, [])
                active_panel = get_active_panel(self._dock_state, slot)
                if not tabs or active_panel is None:
                    empty_label = QLabel("No panels docked")
                    empty_label.setObjectName("status")
                    content_layout.addWidget(empty_label)
                    content_layout.addStretch(1)
                    continue

                active_index = _clamp_active_index(
                    self._dock_state.active_tab.get(slot, 0),
                    len(tabs),
                )
                for index, panel_id in enumerate(tabs):
                    tab_button = QPushButton(self._panel_specs[panel_id].title)
                    tab_button.setObjectName("tabButton")
                    tab_button.setCheckable(True)
                    tab_button.setChecked(index == active_index)
                    tab_button.clicked.connect(
                        lambda _checked=False, current_slot=slot, tab_index=index: self._activate_tab(
                            current_slot,
                            tab_index,
                        )
                    )
                    tab_layout.addWidget(tab_button)
                tab_layout.addStretch(1)

                content_layout.addWidget(self._panel_widgets[active_panel], 1)

        def _activate_tab(self, slot: str, tab_index: int) -> None:
            try:
                target_slot = _validate_slot(slot)
                self._dock_state = set_active_tab(self._dock_state, target_slot, tab_index)
            except ValueError:
                return
            self._render_dock_layout()

        def _move_panel(self, panel_id: str, destination_slot: str) -> None:
            try:
                target_panel = _validate_panel_id(panel_id)
                target_slot = _validate_slot(destination_slot)
                self._dock_state = move_panel(
                    self._dock_state,
                    target_panel,
                    target_slot,
                )
            except ValueError:
                return
            self._render_dock_layout()

        def _refresh_timeline_data(self, *, force: bool = False) -> None:
            if self._recorder.db_path is None:
                self._timeline_snapshots = []
                self._timeline_widget.set_data([])
                self._timeline_mode_label.setText(
                    "Timeline unavailable | DVR persistence disabled."
                )
                self._timeline_live_btn.setEnabled(False)
                return

            now = time.monotonic()
            if (
                not force
                and now - self._timeline_last_refresh_monotonic
                < self._timeline_refresh_interval_seconds
            ):
                return
            self._timeline_last_refresh_monotonic = now

            try:
                with TelemetryStore(self._recorder.db_path) as timeline_store:
                    snapshots = query_timeline(
                        timeline_store,
                        resolution=self._timeline_resolution,
                    )
            except Exception as exc:
                self._timeline_mode_label.setText(f"Timeline unavailable | {exc}")
                return

            self._timeline_snapshots = snapshots
            self._timeline_widget.set_data(snapshots)

            if not snapshots:
                self._timeline_mode_label.setText("No DVR samples yet.")
                self._timeline_live_btn.setEnabled(False)
                return

            if self._is_live_mode:
                self._timeline_mode_label.setText(
                    "Timeline live | drag scrubber to inspect history."
                )
                self._timeline_live_btn.setEnabled(False)
            else:
                self._timeline_mode_label.setText(
                    "Timeline scrub | click Live to resume stream."
                )
                self._timeline_live_btn.setEnabled(True)

        def _find_snapshot_at_or_before(self, timestamp: float) -> SystemSnapshot | None:
            if not self._timeline_snapshots:
                return None

            for snapshot in reversed(self._timeline_snapshots):
                if snapshot.timestamp <= timestamp:
                    return snapshot
            return self._timeline_snapshots[0]

        def _apply_snapshot_to_ui(
            self,
            snapshot: SystemSnapshot,
            status_text: str,
            process_rows: list[str],
        ) -> None:
            self._last_cpu = snapshot.cpu_percent
            self._last_mem = snapshot.memory_percent
            self._render_snapshot(snapshot)
            self._render_process_rows(process_rows)
            self._status_label.setText(status_text)
            self._render_status_label.setText(
                format_render_panel_status(snapshot, status_text)
            )
            self._render_hint_label.setText(format_render_panel_hint(process_rows))

        @Slot(float)
        def _on_timeline_scrub(self, timestamp: float) -> None:
            snapshot = self._find_snapshot_at_or_before(timestamp)
            if snapshot is None:
                return

            self._set_live_mode(False)
            self._frozen_snapshot = snapshot
            self._apply_snapshot_to_ui(
                snapshot,
                "Scrubbing DVR timeline",
                self._latest_live_process_rows,
            )

        def _set_live_mode(self, enabled: bool) -> None:
            target_live_mode = bool(enabled)
            if target_live_mode == self._is_live_mode:
                return

            self._is_live_mode = target_live_mode
            if target_live_mode:
                self._frozen_snapshot = None
                if self._latest_live_snapshot is not None:
                    self._apply_snapshot_to_ui(
                        self._latest_live_snapshot,
                        self._latest_live_status_text,
                        self._latest_live_process_rows,
                    )
                self._timeline_live_btn.setEnabled(False)
                self._refresh_timeline_data(force=True)
            else:
                self._timeline_mode_label.setText(
                    "Timeline scrub | click Live to resume stream."
                )
                self._timeline_live_btn.setEnabled(True)

        def _seed_latest_snapshot(self) -> None:
            latest_snapshot = self._recorder.get_latest_snapshot()
            if latest_snapshot is None:
                return
            seeded_status = format_initial_status(self._recorder)
            self._latest_live_snapshot = latest_snapshot
            self._latest_live_status_text = seeded_status
            self._apply_snapshot_to_ui(
                latest_snapshot,
                seeded_status,
                self._latest_live_process_rows,
            )

        def _render_snapshot(self, snapshot: SystemSnapshot) -> None:
            lines = format_snapshot_lines(snapshot)
            self._cpu_gauge.set_value(snapshot.cpu_percent)
            self._mem_gauge.set_value(snapshot.memory_percent)
            self._cpu_sparkline.push(snapshot.cpu_percent)
            self._mem_sparkline.push(snapshot.memory_percent)
            self._render_cpu_gauge.set_value(snapshot.cpu_percent)
            self._timestamp_label.setText(lines["timestamp"])
            self._render_timestamp_label.setText(lines["timestamp"])

        def _render_process_rows(self, rows: list[str]) -> None:
            normalized_rows = format_process_rows([], row_count=self._process_row_count)
            for index, row in enumerate(rows[: self._process_row_count]):
                normalized_rows[index] = row

            for label, row in zip(self._process_labels, normalized_rows):
                label.setText(row)

        @Slot()
        def _on_frame_tick(self) -> None:
            now = time.monotonic()
            elapsed = now - self._last_frame_time
            self._last_frame_time = now

            frame = compose_cockpit_frame(
                previous_phase=self._phase,
                elapsed_since_last_frame=elapsed,
                cpu_percent=self._last_cpu,
                memory_percent=self._last_mem,
            )
            self._phase = frame.phase

            self._cpu_gauge.set_accent_intensity(frame.accent_intensity)
            self._mem_gauge.set_accent_intensity(frame.accent_intensity)
            self._cpu_sparkline.set_accent_intensity(frame.accent_intensity)
            self._mem_sparkline.set_accent_intensity(frame.accent_intensity)
            self._render_cpu_gauge.set_accent_intensity(frame.accent_intensity)
            self._titlebar.set_accent_intensity(frame.accent_intensity)

            bucket = int(frame.accent_intensity * 100.0)
            if bucket != self._accent_bucket:
                self._accent_bucket = bucket
                self.setStyleSheet(build_window_stylesheet(frame.accent_intensity))

        @Slot(float, float, float, str, list)
        def _on_snapshot(
            self,
            timestamp: float,
            cpu_percent: float,
            memory_percent: float,
            status_text: str,
            process_rows: list[str],
        ) -> None:
            snapshot = SystemSnapshot(
                timestamp=timestamp,
                cpu_percent=cpu_percent,
                memory_percent=memory_percent,
            )
            normalized_rows = [str(row) for row in process_rows]
            self._latest_live_snapshot = snapshot
            self._latest_live_status_text = status_text
            self._latest_live_process_rows = normalized_rows

            if self._is_live_mode:
                self._apply_snapshot_to_ui(snapshot, status_text, normalized_rows)
            self._refresh_timeline_data()

        @Slot(str)
        def _on_worker_error(self, message: str) -> None:
            error_text = f"Telemetry error: {message}"
            self._latest_live_status_text = error_text
            if self._is_live_mode:
                self._status_label.setText(error_text)
                self._render_status_label.setText(f"Render bridge paused | {error_text}")

        def closeEvent(self, event: QCloseEvent) -> None:
            self._frame_timer.stop()
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
