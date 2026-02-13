"""Tests for the custom frameless titlebar widget."""

from __future__ import annotations

import pathlib
import sys
from unittest.mock import MagicMock

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

import pytest  # noqa: E402
from PySide6.QtCore import Qt  # noqa: E402
from PySide6.QtWidgets import QApplication, QLabel, QPushButton, QWidget  # noqa: E402

from shell.titlebar import AuraTitleBar  # noqa: E402
from shell.window import build_window_stylesheet  # noqa: E402


@pytest.fixture(scope="session")
def qapp():
    app = QApplication.instance()
    if app is None:
        app = QApplication([])
    return app


@pytest.fixture()
def titlebar(qapp):
    parent = QWidget()
    tb = AuraTitleBar(parent)
    yield tb
    parent.close()


class TestTitleBarStructure:
    def test_has_title_label(self, titlebar: AuraTitleBar) -> None:
        label = titlebar.findChild(QLabel, "titlebarLabel")
        assert label is not None
        assert label.text() == "Aura Cockpit"

    def test_has_three_buttons(self, titlebar: AuraTitleBar) -> None:
        min_btn = titlebar.findChild(QPushButton, "titlebarMinBtn")
        max_btn = titlebar.findChild(QPushButton, "titlebarMaxBtn")
        close_btn = titlebar.findChild(QPushButton, "titlebarCloseBtn")
        assert min_btn is not None
        assert max_btn is not None
        assert close_btn is not None

    def test_fixed_height(self, titlebar: AuraTitleBar) -> None:
        assert titlebar.maximumHeight() == 36
        assert titlebar.minimumHeight() == 36

    def test_object_name(self, titlebar: AuraTitleBar) -> None:
        assert titlebar.objectName() == "auraTitleBar"


class TestTitleBarActions:
    def test_minimize_calls_show_minimized(self, titlebar: AuraTitleBar) -> None:
        parent = titlebar.window()
        parent.showMinimized = MagicMock()
        titlebar._on_minimize()
        parent.showMinimized.assert_called_once()

    def test_close_calls_close(self, titlebar: AuraTitleBar) -> None:
        parent = titlebar.window()
        parent.close = MagicMock()
        titlebar._on_close()
        parent.close.assert_called_once()

    def test_toggle_maximize_from_normal(self, titlebar: AuraTitleBar) -> None:
        parent = titlebar.window()
        parent.isMaximized = MagicMock(return_value=False)
        parent.showMaximized = MagicMock()
        titlebar._on_toggle_maximize()
        parent.showMaximized.assert_called_once()

    def test_toggle_maximize_from_maximized(self, titlebar: AuraTitleBar) -> None:
        parent = titlebar.window()
        parent.isMaximized = MagicMock(return_value=True)
        parent.showNormal = MagicMock()
        titlebar._on_toggle_maximize()
        parent.showNormal.assert_called_once()


class TestAccentIntensity:
    def test_set_accent_intensity_no_error(self, titlebar: AuraTitleBar) -> None:
        titlebar.set_accent_intensity(0.0)
        titlebar.set_accent_intensity(0.5)
        titlebar.set_accent_intensity(1.0)

    def test_clamps_values(self, titlebar: AuraTitleBar) -> None:
        titlebar.set_accent_intensity(-1.0)
        assert titlebar._accent_intensity == 0.0
        titlebar.set_accent_intensity(5.0)
        assert titlebar._accent_intensity == 1.0


class TestDragMove:
    def test_mouse_press_calls_start_system_move(self, titlebar: AuraTitleBar) -> None:
        mock_handle = MagicMock()
        parent = titlebar.window()
        parent.windowHandle = MagicMock(return_value=mock_handle)

        from PySide6.QtCore import QPointF
        from PySide6.QtGui import QMouseEvent

        event = QMouseEvent(
            QMouseEvent.Type.MouseButtonPress,
            QPointF(10, 10),
            Qt.LeftButton,
            Qt.LeftButton,
            Qt.NoModifier,
        )
        titlebar.mousePressEvent(event)
        mock_handle.startSystemMove.assert_called_once()


class TestStylesheetIntegration:
    def test_stylesheet_contains_titlebar_rules(self) -> None:
        ss = build_window_stylesheet(0.0)
        assert "auraTitleBar" in ss
        assert "titlebarLabel" in ss
        assert "titlebarCloseBtn" in ss
        assert "titlebarMinBtn" in ss
        assert "titlebarMaxBtn" in ss
