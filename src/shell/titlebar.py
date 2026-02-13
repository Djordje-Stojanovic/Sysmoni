"""Custom frameless titlebar for the Aura window."""

from __future__ import annotations
# pyright: reportAttributeAccessIssue=false

from PySide6.QtCore import Qt
from PySide6.QtGui import QMouseEvent
from PySide6.QtWidgets import QHBoxLayout, QLabel, QPushButton, QWidget

from render.theme import DEFAULT_THEME, _blend_hex_color

_TITLEBAR_HEIGHT = 36


class AuraTitleBar(QWidget):
    """Draggable titlebar with minimize / maximize-restore / close buttons."""

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setFixedHeight(_TITLEBAR_HEIGHT)
        self.setObjectName("auraTitleBar")

        layout = QHBoxLayout(self)
        layout.setContentsMargins(12, 0, 4, 0)
        layout.setSpacing(0)

        self._title_label = QLabel("Aura Cockpit")
        self._title_label.setObjectName("titlebarLabel")
        layout.addWidget(self._title_label)

        layout.addStretch(1)

        self._minimize_btn = self._make_button("titlebarMinBtn", "\u2013")  # en-dash
        self._maximize_btn = self._make_button("titlebarMaxBtn", "\u25a1")  # □
        self._close_btn = self._make_button("titlebarCloseBtn", "\u2715")  # ✕

        self._minimize_btn.clicked.connect(self._on_minimize)
        self._maximize_btn.clicked.connect(self._on_toggle_maximize)
        self._close_btn.clicked.connect(self._on_close)

        layout.addWidget(self._minimize_btn)
        layout.addWidget(self._maximize_btn)
        layout.addWidget(self._close_btn)

        self._accent_intensity: float = 0.0

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def set_accent_intensity(self, intensity: float) -> None:
        """Tint the title label border-bottom based on system load."""
        self._accent_intensity = max(0.0, min(1.0, float(intensity)))
        theme = DEFAULT_THEME
        accent_color = _blend_hex_color(
            theme.status_color, theme.status_color_peak, self._accent_intensity
        )
        self._title_label.setStyleSheet(f"color: {accent_color};")

    # ------------------------------------------------------------------
    # Window control slots
    # ------------------------------------------------------------------

    def _on_minimize(self) -> None:
        win = self.window()
        if win is not None:
            win.showMinimized()

    def _on_toggle_maximize(self) -> None:
        win = self.window()
        if win is None:
            return
        if win.isMaximized():
            win.showNormal()
            self._maximize_btn.setText("\u25a1")
        else:
            win.showMaximized()
            self._maximize_btn.setText("\u2752")  # ❒ restore icon

    def _on_close(self) -> None:
        win = self.window()
        if win is not None:
            win.close()

    # ------------------------------------------------------------------
    # Drag-to-move + double-click maximize toggle
    # ------------------------------------------------------------------

    def mousePressEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.LeftButton:
            win = self.window()
            if win is not None:
                handle = win.windowHandle()
                if handle is not None:
                    handle.startSystemMove()
        super().mousePressEvent(event)

    def mouseDoubleClickEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.LeftButton:
            self._on_toggle_maximize()
        super().mouseDoubleClickEvent(event)

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    @staticmethod
    def _make_button(object_name: str, text: str) -> QPushButton:
        btn = QPushButton(text)
        btn.setObjectName(object_name)
        btn.setFixedSize(36, 28)
        btn.setFocusPolicy(Qt.NoFocus)
        return btn
