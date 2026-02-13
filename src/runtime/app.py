"""GUI application lifecycle bootstrap for Aura.

Provides ``launch_gui()`` — the single entry-point that resolves config
through the platform-aware ``runtime.config`` pipeline, then hands off to
:class:`shell.window.AuraWindow` for the actual GUI lifecycle.

All PySide6 / shell imports are **deferred** so this module stays importable
in headless test environments.
"""

from __future__ import annotations

import argparse
import signal
import sys
from dataclasses import dataclass
from typing import Sequence


@dataclass(frozen=True)
class AppContext:
    """Resolved configuration for a GUI launch."""

    db_path: str | None
    retention_seconds: float
    persistence_enabled: bool
    interval_seconds: float


def _build_gui_parser() -> argparse.ArgumentParser:
    """Minimal parser that accepts the GUI-relevant subset of CLI flags."""
    from runtime.main import (
        _positive_interval_seconds,
        _positive_retention_seconds,
    )

    parser = argparse.ArgumentParser(
        description="Aura GUI launcher.",
        add_help=True,
    )
    parser.add_argument(
        "--interval",
        type=_positive_interval_seconds,
        default=1.0,
        help="Polling interval in seconds (default: 1.0).",
    )
    parser.add_argument(
        "--no-persist",
        action="store_true",
        help="Disable DVR persistence.",
    )
    parser.add_argument(
        "--retention-seconds",
        type=_positive_retention_seconds,
        default=None,
        help="Retention horizon in seconds for persisted telemetry.",
    )
    parser.add_argument(
        "--db-path",
        default=None,
        help="SQLite DB path override.",
    )
    return parser


def resolve_app_context(argv: Sequence[str] | None = None) -> AppContext:
    """Parse *argv* and resolve a full :class:`AppContext` via runtime config."""
    from runtime.config import resolve_runtime_config

    parser = _build_gui_parser()
    args = parser.parse_args(list(argv) if argv is not None else [])
    config = resolve_runtime_config(args)

    return AppContext(
        db_path=config.db_path,
        retention_seconds=config.retention_seconds,
        persistence_enabled=config.persistence_enabled,
        interval_seconds=args.interval,
    )


def check_pyside6_available() -> None:
    """Raise :class:`RuntimeError` if PySide6 cannot be imported."""
    try:
        import PySide6.QtWidgets  # noqa: F401
    except ImportError as exc:
        raise RuntimeError(
            "PySide6 is required for the Aura GUI. Install it with: uv sync"
        ) from exc


def install_signal_handlers() -> None:
    """Make Ctrl+C work inside the Qt event loop.

    The standard pattern: reset SIGINT to default so the interpreter raises
    ``KeyboardInterrupt``, then use a 200 ms ``QTimer`` to give Python a
    chance to run signal handlers between Qt event-loop iterations.
    """
    from PySide6.QtCore import QTimer

    signal.signal(signal.SIGINT, signal.SIG_DFL)

    # The timer forces the Python interpreter to briefly regain control
    # from the C++ Qt event loop so it can service pending signals.
    timer = QTimer()
    timer.start(200)
    timer.timeout.connect(lambda: None)
    # prevent garbage collection — caller doesn't need to hold a reference
    install_signal_handlers._keep_alive = timer  # type: ignore[attr-defined]


def launch_gui(argv: Sequence[str] | None = None) -> int:
    """Resolve config, create the Qt application, and run the GUI.

    Returns an exit code suitable for ``sys.exit()``.
    """
    # 1. Early check — before any Qt import
    try:
        check_pyside6_available()
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    # 2. Resolve config (may print argparse errors and exit)
    try:
        ctx = resolve_app_context(argv)
    except SystemExit as exc:
        # argparse calls sys.exit on --help or error
        return int(exc.code) if exc.code is not None else 0
    except Exception as exc:
        print(f"Configuration error: {exc}", file=sys.stderr)
        return 2

    # 3. Deferred Qt / shell imports
    from PySide6.QtWidgets import QApplication

    from shell.window import AuraWindow

    # 4. Create QApplication (guard double-create)
    app = QApplication.instance()
    if app is None:
        app = QApplication(sys.argv)

    # 5. Ctrl+C support
    install_signal_handlers()

    # 6. Create and show the window
    db_path = ctx.db_path if ctx.persistence_enabled else None
    window = AuraWindow(
        interval_seconds=ctx.interval_seconds,
        db_path=db_path,
    )
    window.show()
    return int(app.exec())
