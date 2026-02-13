from __future__ import annotations
# pyright: reportPossiblyUnboundVariable=false, reportRedeclaration=false, reportAttributeAccessIssue=false, reportArgumentType=false

from dataclasses import dataclass

_QT_AVAILABLE = False
try:
    from PySide6.QtCore import QPointF, Qt, Signal
    from PySide6.QtGui import (
        QColor,
        QFont,
        QLinearGradient,
        QMouseEvent,
        QPainter,
        QPainterPath,
        QPen,
    )
    from PySide6.QtWidgets import QToolTip, QWidget

    _QT_AVAILABLE = True
except ImportError:
    pass

from contracts.types import SystemSnapshot  # noqa: E402
from render.theme import _blend_hex_color, _parse_hex_color  # noqa: E402

from ._base import AuraWidget, AuraWidgetConfig  # noqa: E402


def _format_ago(seconds: float) -> str:
    """Format a duration in seconds as a human-readable 'ago' string."""
    if seconds < 1.0:
        return "now"
    if seconds < 60.0:
        s = int(seconds)
        return f"{s}s ago"
    if seconds < 3600.0:
        m = int(seconds / 60.0)
        return f"{m}m ago"
    if seconds < 86400.0:
        h = round(seconds / 3600.0, 1)
        if h == int(h):
            return f"{int(h)}h ago"
        return f"{h}h ago"
    d = round(seconds / 86400.0, 1)
    if d == int(d):
        return f"{int(d)}d ago"
    return f"{d}d ago"


@dataclass(frozen=True, slots=True)
class TimelineConfig:
    line_width: float = 1.5
    gradient_alpha_top: int = 60
    gradient_alpha_bottom: int = 0
    scrubber_width: float = 2.0
    scrubber_handle_radius: float = 5.0
    axis_height: int = 20
    min_height: int = 80
    min_width: int = 200
    show_memory: bool = False

    def __post_init__(self) -> None:
        if self.line_width <= 0.0:
            raise ValueError("line_width must be > 0.")
        if not (0 <= self.gradient_alpha_top <= 255):
            raise ValueError("gradient_alpha_top must be 0–255.")
        if not (0 <= self.gradient_alpha_bottom <= 255):
            raise ValueError("gradient_alpha_bottom must be 0–255.")
        if self.scrubber_width <= 0.0:
            raise ValueError("scrubber_width must be > 0.")
        if self.scrubber_handle_radius <= 0.0:
            raise ValueError("scrubber_handle_radius must be > 0.")
        if self.axis_height < 0:
            raise ValueError("axis_height must be >= 0.")
        if self.min_height < 1:
            raise ValueError("min_height must be >= 1.")
        if self.min_width < 1:
            raise ValueError("min_width must be >= 1.")


if _QT_AVAILABLE:

    class TimelineWidget(AuraWidget):
        scrub_position_changed = Signal(float)

        def __init__(
            self,
            timeline_config: TimelineConfig | None = None,
            widget_config: AuraWidgetConfig | None = None,
            parent: QWidget | None = None,
        ) -> None:
            super().__init__(widget_config, parent)
            self._timeline_config = timeline_config or TimelineConfig()
            self._snapshots: list[SystemSnapshot] = []
            self._scrub_ratio: float = 1.0
            self._dragging: bool = False
            self.setMinimumHeight(self._timeline_config.min_height)
            self.setMinimumWidth(self._timeline_config.min_width)
            self.setMouseTracking(True)

        @property
        def timeline_config(self) -> TimelineConfig:
            return self._timeline_config

        @property
        def snapshot_count(self) -> int:
            return len(self._snapshots)

        @property
        def scrub_ratio(self) -> float:
            return self._scrub_ratio

        @property
        def scrub_timestamp(self) -> float | None:
            if len(self._snapshots) < 2:
                return None
            t0 = self._snapshots[0].timestamp
            t1 = self._snapshots[-1].timestamp
            return t0 + (t1 - t0) * self._scrub_ratio

        def set_data(self, snapshots: list[SystemSnapshot]) -> None:
            self._snapshots = list(snapshots)
            self.update()

        def _update_scrub_from_mouse(self, x: float) -> None:
            margin_x = 4.0
            plot_w = self.width() - 2.0 * margin_x
            if plot_w <= 0:
                return
            ratio = max(0.0, min(1.0, (x - margin_x) / plot_w))
            if ratio != self._scrub_ratio:
                self._scrub_ratio = ratio
                ts = self.scrub_timestamp
                if ts is not None:
                    self.scrub_position_changed.emit(ts)
                self.update()

        def mousePressEvent(self, event: QMouseEvent) -> None:
            if event.button() == Qt.MouseButton.LeftButton:
                self._dragging = True
                self._update_scrub_from_mouse(event.position().x())

        def mouseMoveEvent(self, event: QMouseEvent) -> None:
            if self._dragging:
                self._update_scrub_from_mouse(event.position().x())
            elif self._snapshots:
                self._show_hover_tooltip(event)

        def mouseReleaseEvent(self, event: QMouseEvent) -> None:
            if event.button() == Qt.MouseButton.LeftButton:
                self._dragging = False

        def _show_hover_tooltip(self, event: QMouseEvent) -> None:
            if len(self._snapshots) < 2:
                return
            margin_x = 4.0
            plot_w = self.width() - 2.0 * margin_x
            if plot_w <= 0:
                return
            ratio = max(0.0, min(1.0, (event.position().x() - margin_x) / plot_w))
            idx = int(ratio * (len(self._snapshots) - 1))
            idx = max(0, min(len(self._snapshots) - 1, idx))
            snap = self._snapshots[idx]
            now = self._snapshots[-1].timestamp
            ago = _format_ago(now - snap.timestamp)
            text = f"{ago}  CPU: {snap.cpu_percent:.1f}%  MEM: {snap.memory_percent:.1f}%"
            QToolTip.showText(event.globalPosition().toPoint(), text, self)

        def paintEvent(self, event: object) -> None:
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)

            if len(self._snapshots) < 2:
                self._paint_placeholder(painter)
                painter.end()
                return

            cfg = self._timeline_config
            w = self.width()
            h = self.height()
            plot_h = h - cfg.axis_height

            self._paint_curve(painter, "cpu", w, plot_h)
            if cfg.show_memory:
                self._paint_curve(painter, "memory", w, plot_h)
            self._paint_scrubber(painter, w, plot_h)
            self._paint_time_axis(painter, w, h, plot_h)

            painter.end()

        def _paint_placeholder(self, painter: QPainter) -> None:
            theme = self._theme
            text_color = QColor(*_parse_hex_color(theme.text_primary), 128)
            painter.setPen(text_color)
            font = QFont(theme.base_font_family, 10)
            painter.setFont(font)
            painter.drawText(
                self.rect(),
                Qt.AlignmentFlag.AlignCenter,
                "Waiting for data\u2026",
            )

        def _paint_curve(
            self, painter: QPainter, metric: str, w: int, plot_h: int
        ) -> None:
            theme = self._theme
            cfg = self._timeline_config
            n = len(self._snapshots)
            margin_x = 4.0
            margin_y = 4.0
            pw = w - 2.0 * margin_x
            ph = plot_h - 2.0 * margin_y

            points: list[QPointF] = []
            for i, snap in enumerate(self._snapshots):
                x = margin_x + (i / max(1, n - 1)) * pw
                val = snap.cpu_percent if metric == "cpu" else snap.memory_percent
                y = margin_y + (1.0 - val / 100.0) * ph
                points.append(QPointF(x, y))

            # Catmull-Rom -> cubic Bezier
            path = QPainterPath()
            path.moveTo(points[0])
            for i in range(len(points) - 1):
                p0 = points[max(0, i - 1)]
                p1 = points[i]
                p2 = points[min(len(points) - 1, i + 1)]
                p3 = points[min(len(points) - 1, i + 2)]
                cp1 = QPointF(
                    p1.x() + (p2.x() - p0.x()) / 6.0,
                    p1.y() + (p2.y() - p0.y()) / 6.0,
                )
                cp2 = QPointF(
                    p2.x() - (p3.x() - p1.x()) / 6.0,
                    p2.y() - (p3.y() - p1.y()) / 6.0,
                )
                path.cubicTo(cp1, cp2, p2)

            # Pick colors based on metric
            if metric == "cpu":
                fg_hex = _blend_hex_color(
                    theme.metric_color, theme.metric_color_peak, self._accent_intensity
                )
            else:
                fg_hex = _blend_hex_color(
                    theme.status_color, theme.status_color_peak, self._accent_intensity
                )
            fg_r, fg_g, fg_b = _parse_hex_color(fg_hex)

            # Gradient fill
            fill_path = QPainterPath(path)
            fill_path.lineTo(QPointF(points[-1].x(), plot_h))
            fill_path.lineTo(QPointF(points[0].x(), plot_h))
            fill_path.closeSubpath()

            gradient = QLinearGradient(0, 0, 0, plot_h)
            gradient.setColorAt(0.0, QColor(fg_r, fg_g, fg_b, cfg.gradient_alpha_top))
            gradient.setColorAt(
                1.0, QColor(fg_r, fg_g, fg_b, cfg.gradient_alpha_bottom)
            )
            painter.setBrush(gradient)
            painter.setPen(Qt.PenStyle.NoPen)
            painter.drawPath(fill_path)

            # Stroke line
            pen = QPen(
                QColor(fg_r, fg_g, fg_b),
                cfg.line_width,
                Qt.PenStyle.SolidLine,
                Qt.PenCapStyle.RoundCap,
            )
            painter.setPen(pen)
            painter.setBrush(Qt.BrushStyle.NoBrush)
            painter.drawPath(path)

        def _paint_scrubber(self, painter: QPainter, w: int, plot_h: int) -> None:
            cfg = self._timeline_config
            theme = self._theme
            margin_x = 4.0
            pw = w - 2.0 * margin_x
            x = margin_x + self._scrub_ratio * pw

            scrub_color = QColor(*_parse_hex_color(theme.text_primary))

            # Vertical line
            pen = QPen(scrub_color, cfg.scrubber_width)
            painter.setPen(pen)
            painter.drawLine(QPointF(x, 0), QPointF(x, plot_h))

            # Handle circle
            painter.setBrush(scrub_color)
            painter.setPen(Qt.PenStyle.NoPen)
            painter.drawEllipse(
                QPointF(x, plot_h / 2.0), cfg.scrubber_handle_radius, cfg.scrubber_handle_radius
            )

        def _paint_time_axis(
            self, painter: QPainter, w: int, h: int, plot_h: int
        ) -> None:
            theme = self._theme
            margin_x = 4.0
            pw = w - 2.0 * margin_x

            text_color = QColor(*_parse_hex_color(theme.text_primary), 160)
            painter.setPen(text_color)
            font = QFont(theme.base_font_family, 8)
            painter.setFont(font)

            now_ts = self._snapshots[-1].timestamp
            first_ts = self._snapshots[0].timestamp
            total_span = now_ts - first_ts

            label_count = 5
            for i in range(label_count):
                ratio = i / max(1, label_count - 1)
                x = margin_x + ratio * pw
                seconds_ago = total_span * (1.0 - ratio)
                label = _format_ago(seconds_ago)
                text_rect_w = 60
                painter.drawText(
                    int(x - text_rect_w / 2),
                    plot_h + 2,
                    text_rect_w,
                    self._timeline_config.axis_height - 2,
                    Qt.AlignmentFlag.AlignHCenter | Qt.AlignmentFlag.AlignTop,
                    label,
                )

else:

    class TimelineWidget(AuraWidget):  # type: ignore[no-redef]
        pass
