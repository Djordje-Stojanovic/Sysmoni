from __future__ import annotations
# pyright: reportPossiblyUnboundVariable=false, reportRedeclaration=false

from dataclasses import dataclass

_QT_AVAILABLE = False
try:
    from PySide6.QtWidgets import QWidget

    _QT_AVAILABLE = True
except ImportError:
    pass

from render.theme import DEFAULT_THEME, RenderTheme, _clamp_unit  # noqa: E402


@dataclass(frozen=True, slots=True)
class AuraWidgetConfig:
    theme: RenderTheme = DEFAULT_THEME
    initial_accent_intensity: float = 0.0


if _QT_AVAILABLE:

    class AuraWidget(QWidget):
        def __init__(
            self,
            config: AuraWidgetConfig | None = None,
            parent: QWidget | None = None,
        ) -> None:
            super().__init__(parent)
            cfg = config or AuraWidgetConfig()
            self._theme = cfg.theme
            self._accent_intensity = _clamp_unit(cfg.initial_accent_intensity)

        @property
        def accent_intensity(self) -> float:
            return self._accent_intensity

        @property
        def theme(self) -> RenderTheme:
            return self._theme

        def set_accent_intensity(self, value: float) -> None:
            clamped = _clamp_unit(value)
            if clamped != self._accent_intensity:
                self._accent_intensity = clamped
                self.update()

        def set_theme(self, theme: RenderTheme) -> None:
            self._theme = theme
            self.update()

else:

    class AuraWidget:  # type: ignore[no-redef]
        def __init__(self, *args: object, **kwargs: object) -> None:
            raise RuntimeError(
                "PySide6 is required for Aura widgets. Run `uv sync` to install it."
            )
