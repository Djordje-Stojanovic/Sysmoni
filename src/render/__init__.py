"""Render primitives and visual helpers owned by the render module."""

from .animation import (
    DEFAULT_FRAME_DISCIPLINE,
    DEFAULT_TARGET_FPS,
    FrameDiscipline,
    advance_phase,
    compute_accent_intensity,
)
from .compositor import CockpitFrameState, compose_cockpit_frame
from .formatting import (
    DEFAULT_PROCESS_ROW_COUNT,
    PROCESS_NAME_MAX_CHARS,
    format_process_row,
    format_process_rows,
    format_snapshot_lines,
)
from .status import RecorderStatusProvider, format_initial_status, format_stream_status
from .theme import DEFAULT_THEME, RenderTheme, build_dock_stylesheet, build_shell_stylesheet

__all__ = [
    "CockpitFrameState",
    "DEFAULT_PROCESS_ROW_COUNT",
    "DEFAULT_FRAME_DISCIPLINE",
    "DEFAULT_TARGET_FPS",
    "PROCESS_NAME_MAX_CHARS",
    "FrameDiscipline",
    "RecorderStatusProvider",
    "RenderTheme",
    "DEFAULT_THEME",
    "advance_phase",
    "build_dock_stylesheet",
    "build_shell_stylesheet",
    "compose_cockpit_frame",
    "compute_accent_intensity",
    "format_initial_status",
    "format_process_row",
    "format_process_rows",
    "format_snapshot_lines",
    "format_stream_status",
]
