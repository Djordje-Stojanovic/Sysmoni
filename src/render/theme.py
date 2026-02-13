from __future__ import annotations

from dataclasses import dataclass
from functools import lru_cache


@dataclass(frozen=True, slots=True)
class RenderTheme:
    background_start: str = "#071320"
    background_end: str = "#0d2033"
    text_primary: str = "#d6ebff"
    title_color: str = "#f7fbff"
    metric_color: str = "#9dd9ff"
    status_color: str = "#75b8ff"
    section_color: str = "#9dd9ff"
    process_color: str = "#cfe8ff"
    base_font_family: str = "Segoe UI"
    process_font_family: str = "Consolas"
    title_font_size: int = 26
    metric_font_size: int = 22
    status_font_size: int = 13
    section_font_size: int = 14
    process_font_size: int = 12


DEFAULT_THEME = RenderTheme()


@lru_cache(maxsize=64)
def _build_shell_stylesheet_cached(theme: RenderTheme) -> str:
    return f"""
                QWidget {{
                    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                        stop:0 {theme.background_start}, stop:1 {theme.background_end});
                    color: {theme.text_primary};
                    font-family: {theme.base_font_family};
                }}
                QLabel#title {{
                    font-size: {theme.title_font_size}px;
                    font-weight: 600;
                    color: {theme.title_color};
                }}
                QLabel#metric {{
                    font-size: {theme.metric_font_size}px;
                    font-weight: 500;
                    color: {theme.metric_color};
                }}
                QLabel#status {{
                    font-size: {theme.status_font_size}px;
                    color: {theme.status_color};
                }}
                QLabel#section {{
                    font-size: {theme.section_font_size}px;
                    font-weight: 600;
                    color: {theme.section_color};
                    margin-top: 8px;
                }}
                QLabel#process {{
                    font-family: {theme.process_font_family};
                    font-size: {theme.process_font_size}px;
                    color: {theme.process_color};
                }}
                """


def build_shell_stylesheet(theme: RenderTheme = DEFAULT_THEME) -> str:
    return _build_shell_stylesheet_cached(theme)


__all__ = ["DEFAULT_THEME", "RenderTheme", "build_shell_stylesheet"]
