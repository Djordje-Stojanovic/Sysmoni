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
