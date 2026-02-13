from __future__ import annotations
# pyright: reportPossiblyUnboundVariable=false, reportRedeclaration=false, reportAttributeAccessIssue=false, reportArgumentType=false

from collections import deque
from dataclasses import dataclass

_QT_AVAILABLE = False
try:
    from PySide6.QtCore import QPointF, Qt
    from PySide6.QtGui import QColor, QFont, QLinearGradient, QPainter, QPainterPath, QPen
    from PySide6.QtWidgets import QWidget

    _QT_AVAILABLE = True
except ImportError:
    pass

from render.theme import _blend_hex_color, _parse_hex_color  # noqa: E402

from ._base import AuraWidget, AuraWidgetConfig  # noqa: E402


@dataclass(frozen=True, slots=True)
class SparkLineConfig:
    buffer_size: int = 120
    line_width: float = 1.5
    gradient_alpha_top: int = 80
    gradient_alpha_bottom: int = 0
    show_latest_value: bool = True
    label_format: str = "{:.1f}%"
    min_height: int = 60

    def __post_init__(self) -> None:
        if self.buffer_size < 2:
            raise ValueError("buffer_size must be >= 2.")
        if self.line_width <= 0.0:
            raise ValueError("line_width must be > 0.")
        if self.min_height < 1:
            raise ValueError("min_height must be >= 1.")


if _QT_AVAILABLE:

    class SparkLine(AuraWidget):
        def __init__(
            self,
            spark_config: SparkLineConfig | None = None,
            widget_config: AuraWidgetConfig | None = None,
            parent: QWidget | None = None,
        ) -> None:
            super().__init__(widget_config, parent)
            self._spark_config = spark_config or SparkLineConfig()
            self._buffer: deque[float] = deque(maxlen=self._spark_config.buffer_size)
            self.setMinimumHeight(self._spark_config.min_height)

        @property
        def latest(self) -> float | None:
            return self._buffer[-1] if self._buffer else None

        @property
        def spark_config(self) -> SparkLineConfig:
            return self._spark_config

        @property
        def buffer_len(self) -> int:
            return len(self._buffer)

        def push(self, value: float) -> None:
            self._buffer.append(max(0.0, min(100.0, float(value))))
            self.update()

        def push_many(self, values: list[float]) -> None:
            for v in values:
                self._buffer.append(max(0.0, min(100.0, float(v))))
            self.update()

        def paintEvent(self, event: object) -> None:
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)

            w = self.width()
            h = self.height()
            theme = self._theme
            cfg = self._spark_config

            if len(self._buffer) < 2:
                # Placeholder text
                text_color = QColor(*_parse_hex_color(theme.text_primary), 128)
                painter.setPen(text_color)
                font = QFont(theme.base_font_family, 10)
                painter.setFont(font)
                painter.drawText(
                    self.rect(),
                    Qt.AlignmentFlag.AlignCenter,
                    "Waiting for data\u2026",
                )
                painter.end()
                return

            # Map buffer values to pixel coordinates
            n = len(self._buffer)
            margin_x = 4.0
            margin_y = 4.0
            plot_w = w - 2.0 * margin_x
            plot_h = h - 2.0 * margin_y

            points: list[QPointF] = []
            for i, val in enumerate(self._buffer):
                x = margin_x + (i / max(1, n - 1)) * plot_w
                y = margin_y + (1.0 - val / 100.0) * plot_h
                points.append(QPointF(x, y))

            # Build smooth path using Catmull-Rom -> cubic Bezier
            path = QPainterPath()
            path.moveTo(points[0])

            for i in range(len(points) - 1):
                p0 = points[max(0, i - 1)]
                p1 = points[i]
                p2 = points[min(len(points) - 1, i + 1)]
                p3 = points[min(len(points) - 1, i + 2)]

                # Catmull-Rom to cubic Bezier control points
                cp1 = QPointF(
                    p1.x() + (p2.x() - p0.x()) / 6.0,
                    p1.y() + (p2.y() - p0.y()) / 6.0,
                )
                cp2 = QPointF(
                    p2.x() - (p3.x() - p1.x()) / 6.0,
                    p2.y() - (p3.y() - p1.y()) / 6.0,
                )
                path.cubicTo(cp1, cp2, p2)

            # Gradient fill under curve
            fg_hex = _blend_hex_color(
                theme.metric_color,
                theme.metric_color_peak,
                self._accent_intensity,
            )
            fg_r, fg_g, fg_b = _parse_hex_color(fg_hex)

            fill_path = QPainterPath(path)
            fill_path.lineTo(QPointF(points[-1].x(), h))
            fill_path.lineTo(QPointF(points[0].x(), h))
            fill_path.closeSubpath()

            gradient = QLinearGradient(0, 0, 0, h)
            gradient.setColorAt(0.0, QColor(fg_r, fg_g, fg_b, cfg.gradient_alpha_top))
            gradient.setColorAt(1.0, QColor(fg_r, fg_g, fg_b, cfg.gradient_alpha_bottom))
            painter.setBrush(gradient)
            painter.setPen(Qt.PenStyle.NoPen)
            painter.drawPath(fill_path)

            # Stroke line
            line_color = QColor(fg_r, fg_g, fg_b)
            pen = QPen(line_color, cfg.line_width, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap)
            painter.setPen(pen)
            painter.setBrush(Qt.BrushStyle.NoBrush)
            painter.drawPath(path)

            # Latest value label
            if cfg.show_latest_value and self._buffer:
                label_text = cfg.label_format.format(self._buffer[-1])
                font = QFont(theme.base_font_family, 9)
                font.setWeight(QFont.Weight.DemiBold)
                painter.setFont(font)
                text_color = QColor(*_parse_hex_color(theme.text_primary))
                painter.setPen(text_color)
                painter.drawText(
                    self.rect().adjusted(0, 2, -4, 0),
                    Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignTop,
                    label_text,
                )

            painter.end()

else:

    class SparkLine(AuraWidget):  # type: ignore[no-redef]
        pass
