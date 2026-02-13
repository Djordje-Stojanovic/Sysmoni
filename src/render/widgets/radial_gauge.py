from __future__ import annotations
# pyright: reportPossiblyUnboundVariable=false, reportRedeclaration=false, reportAttributeAccessIssue=false, reportArgumentType=false

from dataclasses import dataclass

_QT_AVAILABLE = False
try:
    from PySide6.QtCore import QRectF, Qt
    from PySide6.QtGui import QColor, QFont, QPainter, QPen
    from PySide6.QtWidgets import QWidget

    _QT_AVAILABLE = True
except ImportError:
    pass

from render.theme import _blend_hex_color, _parse_hex_color  # noqa: E402

from ._base import AuraWidget, AuraWidgetConfig  # noqa: E402


@dataclass(frozen=True, slots=True)
class RadialGaugeConfig:
    sweep_degrees: float = 270.0
    start_angle_degrees: float = 225.0
    arc_width: int = 10
    show_label: bool = True
    label_format: str = "{:.0f}%"
    min_size: int = 120

    def __post_init__(self) -> None:
        if self.sweep_degrees <= 0.0 or self.sweep_degrees > 360.0:
            raise ValueError("sweep_degrees must be in (0, 360].")
        if self.arc_width < 1:
            raise ValueError("arc_width must be >= 1.")
        if self.min_size < 1:
            raise ValueError("min_size must be >= 1.")


if _QT_AVAILABLE:

    class RadialGauge(AuraWidget):
        def __init__(
            self,
            gauge_config: RadialGaugeConfig | None = None,
            widget_config: AuraWidgetConfig | None = None,
            parent: QWidget | None = None,
        ) -> None:
            super().__init__(widget_config, parent)
            self._gauge_config = gauge_config or RadialGaugeConfig()
            self._value: float = 0.0
            self.setMinimumSize(
                self._gauge_config.min_size,
                self._gauge_config.min_size,
            )

        @property
        def value(self) -> float:
            return self._value

        @property
        def gauge_config(self) -> RadialGaugeConfig:
            return self._gauge_config

        def set_value(self, value: float) -> None:
            clamped = max(0.0, min(100.0, float(value)))
            if clamped != self._value:
                self._value = clamped
                self.update()

        def paintEvent(self, event: object) -> None:
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)

            cfg = self._gauge_config
            side = min(self.width(), self.height())
            margin = cfg.arc_width + 2
            rect = QRectF(
                (self.width() - side) / 2.0 + margin,
                (self.height() - side) / 2.0 + margin,
                side - 2.0 * margin,
                side - 2.0 * margin,
            )

            # Qt arcs use 1/16th degree units, measured counter-clockwise from 3 o'clock
            start_angle_16 = int(cfg.start_angle_degrees * 16)
            sweep_16 = int(-cfg.sweep_degrees * 16)  # negative = clockwise

            # Background arc — dim metric color at 25% alpha
            theme = self._theme
            bg_r, bg_g, bg_b = _parse_hex_color(theme.metric_color)
            bg_color = QColor(bg_r, bg_g, bg_b, 64)  # ~25% of 255
            bg_pen = QPen(bg_color, cfg.arc_width, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap)
            painter.setPen(bg_pen)
            painter.drawArc(rect, start_angle_16, sweep_16)

            # Foreground arc — accent-blended color
            fg_hex = _blend_hex_color(
                theme.metric_color,
                theme.metric_color_peak,
                self._accent_intensity,
            )
            fg_r, fg_g, fg_b = _parse_hex_color(fg_hex)
            fg_color = QColor(fg_r, fg_g, fg_b)
            fg_pen = QPen(fg_color, cfg.arc_width, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap)
            painter.setPen(fg_pen)
            value_sweep_16 = int(sweep_16 * (self._value / 100.0))
            painter.drawArc(rect, start_angle_16, value_sweep_16)

            # Center label
            if cfg.show_label:
                label_text = cfg.label_format.format(self._value)
                font = QFont(theme.base_font_family, max(8, side // 6))
                font.setWeight(QFont.Weight.DemiBold)
                painter.setFont(font)
                text_color = QColor(*_parse_hex_color(theme.text_primary))
                painter.setPen(text_color)
                painter.drawText(rect, Qt.AlignmentFlag.AlignCenter, label_text)

            painter.end()

else:

    class RadialGauge(AuraWidget):  # type: ignore[no-redef]
        pass
