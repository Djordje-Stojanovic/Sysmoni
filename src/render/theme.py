from __future__ import annotations

from dataclasses import dataclass
from functools import lru_cache
import math
import re


@dataclass(frozen=True, slots=True)
class RenderTheme:
    background_start: str = "#071320"
    background_end: str = "#0d2033"
    text_primary: str = "#d6ebff"
    title_color: str = "#f7fbff"
    metric_color: str = "#9dd9ff"
    metric_color_peak: str = "#d9f0ff"
    status_color: str = "#75b8ff"
    status_color_peak: str = "#b8dcff"
    section_color: str = "#9dd9ff"
    process_color: str = "#cfe8ff"
    slot_frame_background: str = "rgba(8, 22, 36, 0.62)"
    slot_frame_border: str = "rgba(116, 172, 220, 0.45)"
    slot_title_color: str = "#9dd9ff"
    tab_border_color: str = "rgba(99, 154, 200, 0.5)"
    tab_background: str = "rgba(6, 20, 33, 0.8)"
    tab_text_color: str = "#9ec9ee"
    tab_active_background_start: str = "#205b8e"
    tab_active_background_end: str = "#3f8fd8"
    tab_active_text_color: str = "#ecf5ff"
    panel_background: str = "rgba(5, 14, 24, 0.72)"
    panel_border: str = "rgba(98, 149, 192, 0.35)"
    panel_title_color: str = "#f5fbff"
    move_button_border: str = "rgba(89, 142, 190, 0.7)"
    move_button_background_start: str = "#122d46"
    move_button_background_end: str = "#266ea7"
    move_button_text: str = "#cbe7ff"
    base_font_family: str = "Segoe UI"
    process_font_family: str = "Consolas"
    title_font_size: int = 26
    metric_font_size: int = 22
    status_font_size: int = 13
    section_font_size: int = 14
    process_font_size: int = 12


DEFAULT_THEME = RenderTheme()


_HEX_COLOR_PATTERN = re.compile(r"^#[0-9a-fA-F]{6}$")


def _clamp_unit(value: float) -> float:
    if math.isnan(value):
        return 0.0
    return max(0.0, min(1.0, float(value)))


def _parse_hex_color(value: str) -> tuple[int, int, int]:
    if _HEX_COLOR_PATTERN.fullmatch(value) is None:
        raise ValueError(f"Expected #RRGGBB color, got {value!r}.")
    return int(value[1:3], 16), int(value[3:5], 16), int(value[5:7], 16)


def _blend_hex_color(start: str, end: str, ratio: float) -> str:
    start_rgb = _parse_hex_color(start)
    end_rgb = _parse_hex_color(end)
    t = _clamp_unit(ratio)
    red = round(start_rgb[0] + ((end_rgb[0] - start_rgb[0]) * t))
    green = round(start_rgb[1] + ((end_rgb[1] - start_rgb[1]) * t))
    blue = round(start_rgb[2] + ((end_rgb[2] - start_rgb[2]) * t))
    return f"#{red:02x}{green:02x}{blue:02x}"


def _quantize_accent_intensity(accent_intensity: float) -> int:
    return int(round(_clamp_unit(accent_intensity) * 100.0))


@lru_cache(maxsize=64)
def _build_shell_stylesheet_cached(theme: RenderTheme, accent_intensity_bucket: int) -> str:
    accent_ratio = accent_intensity_bucket / 100.0
    metric_color = _blend_hex_color(theme.metric_color, theme.metric_color_peak, accent_ratio)
    status_color = _blend_hex_color(theme.status_color, theme.status_color_peak, accent_ratio)
    tab_active_background = _blend_hex_color(
        theme.tab_active_background_start,
        theme.tab_active_background_end,
        accent_ratio,
    )
    move_button_background = _blend_hex_color(
        theme.move_button_background_start,
        theme.move_button_background_end,
        accent_ratio,
    )
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
                    padding-bottom: 6px;
                }}
                QFrame#slotFrame {{
                    background: {theme.slot_frame_background};
                    border: 1px solid {theme.slot_frame_border};
                    border-radius: 8px;
                }}
                QLabel#slotTitle {{
                    font-size: 12px;
                    font-weight: 600;
                    letter-spacing: 1px;
                    color: {theme.slot_title_color};
                }}
                QPushButton#tabButton {{
                    font-size: 12px;
                    border: 1px solid {theme.tab_border_color};
                    border-radius: 6px;
                    background: {theme.tab_background};
                    color: {theme.tab_text_color};
                    padding: 4px 8px;
                }}
                QPushButton#tabButton:checked {{
                    background: {tab_active_background};
                    color: {theme.tab_active_text_color};
                }}
                QFrame#panelFrame {{
                    background: {theme.panel_background};
                    border: 1px solid {theme.panel_border};
                    border-radius: 6px;
                }}
                QLabel#panelTitle {{
                    font-size: {theme.section_font_size}px;
                    font-weight: 620;
                    color: {theme.panel_title_color};
                }}
                QLabel#metric {{
                    font-size: {theme.metric_font_size}px;
                    font-weight: 500;
                    color: {metric_color};
                }}
                QLabel#status {{
                    font-size: {theme.status_font_size}px;
                    color: {status_color};
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
                QPushButton#moveButton {{
                    min-width: 24px;
                    max-width: 24px;
                    min-height: 20px;
                    max-height: 20px;
                    font-size: 11px;
                    font-weight: 600;
                    border: 1px solid {theme.move_button_border};
                    border-radius: 4px;
                    background: {move_button_background};
                    color: {theme.move_button_text};
                }}
                """


def build_shell_stylesheet(
    theme: RenderTheme = DEFAULT_THEME,
    *,
    accent_intensity: float = 0.0,
) -> str:
    accent_intensity_bucket = _quantize_accent_intensity(accent_intensity)
    return _build_shell_stylesheet_cached(theme, accent_intensity_bucket)


def build_dock_stylesheet(
    theme: RenderTheme = DEFAULT_THEME,
    *,
    accent_intensity: float = 0.0,
) -> str:
    return build_shell_stylesheet(theme, accent_intensity=accent_intensity)


__all__ = ["DEFAULT_THEME", "RenderTheme", "build_dock_stylesheet", "build_shell_stylesheet"]
