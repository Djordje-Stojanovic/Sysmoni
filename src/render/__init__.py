"""Render primitives and visual helpers owned by the render module."""

from .formatting import (
    DEFAULT_PROCESS_ROW_COUNT,
    PROCESS_NAME_MAX_CHARS,
    format_process_row,
    format_process_rows,
    format_snapshot_lines,
)
from .status import RecorderStatusProvider, format_initial_status, format_stream_status
from .theme import DEFAULT_THEME, RenderTheme, build_shell_stylesheet

__all__ = [
    "DEFAULT_PROCESS_ROW_COUNT",
    "PROCESS_NAME_MAX_CHARS",
    "RecorderStatusProvider",
    "RenderTheme",
    "DEFAULT_THEME",
    "build_shell_stylesheet",
    "format_initial_status",
    "format_process_row",
    "format_process_rows",
    "format_snapshot_lines",
    "format_stream_status",
]
