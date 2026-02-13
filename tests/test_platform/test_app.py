"""Tests for runtime.app — GUI lifecycle bootstrap.

All tests run **without** PySide6 by monkeypatching imports.
"""

from __future__ import annotations

import importlib
import pathlib
import sys
import types
import unittest
from unittest.mock import MagicMock, patch

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

app_mod = importlib.import_module("runtime.app")


class TestAppContext(unittest.TestCase):
    def test_frozen(self) -> None:
        ctx = app_mod.AppContext(
            db_path="/tmp/test.db",
            retention_seconds=3600.0,
            persistence_enabled=True,
            interval_seconds=1.0,
        )
        with self.assertRaises(AttributeError):
            ctx.db_path = "/other"  # type: ignore[misc]


class TestResolveAppContext(unittest.TestCase):
    def test_defaults(self) -> None:
        ctx = app_mod.resolve_app_context([])
        self.assertTrue(ctx.persistence_enabled)
        self.assertEqual(ctx.interval_seconds, 1.0)
        self.assertGreater(ctx.retention_seconds, 0)
        self.assertIsNotNone(ctx.db_path)

    def test_no_persist(self) -> None:
        ctx = app_mod.resolve_app_context(["--no-persist"])
        self.assertFalse(ctx.persistence_enabled)
        self.assertIsNone(ctx.db_path)

    def test_custom_interval(self) -> None:
        ctx = app_mod.resolve_app_context(["--interval", "0.5"])
        self.assertAlmostEqual(ctx.interval_seconds, 0.5)

    def test_custom_db_path(self) -> None:
        ctx = app_mod.resolve_app_context(["--db-path", "/my/db.sqlite"])
        self.assertEqual(ctx.db_path, "/my/db.sqlite")
        self.assertTrue(ctx.persistence_enabled)

    def test_custom_retention(self) -> None:
        ctx = app_mod.resolve_app_context(["--retention-seconds", "7200"])
        self.assertAlmostEqual(ctx.retention_seconds, 7200.0)

    def test_invalid_interval_exits(self) -> None:
        with self.assertRaises(SystemExit):
            app_mod.resolve_app_context(["--interval", "-1"])

    def test_invalid_interval_zero_exits(self) -> None:
        with self.assertRaises(SystemExit):
            app_mod.resolve_app_context(["--interval", "0"])

    def test_help_exits(self) -> None:
        with self.assertRaises(SystemExit) as cm:
            app_mod.resolve_app_context(["--help"])
        self.assertEqual(cm.exception.code, 0)


class TestCheckPySide6Available(unittest.TestCase):
    def test_succeeds_when_importable(self) -> None:
        fake_qt = types.ModuleType("PySide6.QtWidgets")
        with patch.dict(sys.modules, {"PySide6.QtWidgets": fake_qt}):
            # Should not raise
            app_mod.check_pyside6_available()

    def test_raises_when_missing(self) -> None:
        with patch.dict(sys.modules, {"PySide6.QtWidgets": None}):
            with self.assertRaises(RuntimeError) as cm:
                app_mod.check_pyside6_available()
            self.assertIn("PySide6", str(cm.exception))


class TestInstallSignalHandlers(unittest.TestCase):
    def test_does_not_raise(self) -> None:
        fake_timer = MagicMock()
        fake_qtcore = types.ModuleType("PySide6.QtCore")
        fake_qtcore.QTimer = MagicMock(return_value=fake_timer)  # type: ignore[attr-defined]
        with patch.dict(sys.modules, {"PySide6.QtCore": fake_qtcore}):
            # Re-import to pick up the mock
            importlib.reload(app_mod)
            app_mod.install_signal_handlers()
        # Restore
        importlib.reload(app_mod)


class TestLaunchGui(unittest.TestCase):
    def test_returns_2_when_pyside6_missing(self) -> None:
        with patch.object(
            app_mod,
            "check_pyside6_available",
            side_effect=RuntimeError("PySide6 missing"),
        ):
            code = app_mod.launch_gui([])
        self.assertEqual(code, 2)

    def test_returns_2_on_config_error(self) -> None:
        with patch.object(app_mod, "check_pyside6_available"):
            with patch.object(
                app_mod,
                "resolve_app_context",
                side_effect=ValueError("bad config"),
            ):
                code = app_mod.launch_gui([])
        self.assertEqual(code, 2)

    def test_returns_0_on_help(self) -> None:
        with patch.object(app_mod, "check_pyside6_available"):
            with patch.object(
                app_mod,
                "resolve_app_context",
                side_effect=SystemExit(0),
            ):
                code = app_mod.launch_gui(["--help"])
        self.assertEqual(code, 0)

    def test_dispatches_to_qt(self) -> None:
        ctx = app_mod.AppContext(
            db_path="/tmp/test.db",
            retention_seconds=86400.0,
            persistence_enabled=True,
            interval_seconds=1.0,
        )
        mock_app = MagicMock()
        mock_app.exec.return_value = 0
        mock_window = MagicMock()

        with patch.object(app_mod, "check_pyside6_available"):
            with patch.object(app_mod, "resolve_app_context", return_value=ctx):
                # Patch PySide6 imports inside launch_gui
                fake_qapp_cls = MagicMock()
                fake_qapp_cls.instance.return_value = None
                fake_qapp_cls.return_value = mock_app

                fake_qtwidgets = types.ModuleType("PySide6.QtWidgets")
                fake_qtwidgets.QApplication = fake_qapp_cls  # type: ignore[attr-defined]

                fake_window_mod = types.ModuleType("shell.window")
                fake_window_mod.AuraWindow = MagicMock(return_value=mock_window)  # type: ignore[attr-defined]

                with patch.dict(
                    sys.modules,
                    {
                        "PySide6.QtWidgets": fake_qtwidgets,
                        "shell.window": fake_window_mod,
                    },
                ):
                    with patch.object(app_mod, "install_signal_handlers"):
                        code = app_mod.launch_gui([])

        self.assertEqual(code, 0)
        mock_window.show.assert_called_once()
        mock_app.exec.assert_called_once()

    def test_no_persist_passes_none_db_path(self) -> None:
        ctx = app_mod.AppContext(
            db_path=None,
            retention_seconds=86400.0,
            persistence_enabled=False,
            interval_seconds=2.0,
        )
        mock_app = MagicMock()
        mock_app.exec.return_value = 0
        mock_window_cls = MagicMock()

        with patch.object(app_mod, "check_pyside6_available"):
            with patch.object(app_mod, "resolve_app_context", return_value=ctx):
                fake_qapp_cls = MagicMock()
                fake_qapp_cls.instance.return_value = mock_app

                fake_qtwidgets = types.ModuleType("PySide6.QtWidgets")
                fake_qtwidgets.QApplication = fake_qapp_cls  # type: ignore[attr-defined]

                fake_window_mod = types.ModuleType("shell.window")
                fake_window_mod.AuraWindow = mock_window_cls  # type: ignore[attr-defined]

                with patch.dict(
                    sys.modules,
                    {
                        "PySide6.QtWidgets": fake_qtwidgets,
                        "shell.window": fake_window_mod,
                    },
                ):
                    with patch.object(app_mod, "install_signal_handlers"):
                        code = app_mod.launch_gui([])

        self.assertEqual(code, 0)
        mock_window_cls.assert_called_once_with(
            interval_seconds=2.0,
            db_path=None,
        )


if __name__ == "__main__":
    unittest.main()
