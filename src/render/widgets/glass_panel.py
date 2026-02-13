from __future__ import annotations

# pyright: reportPossiblyUnboundVariable=false, reportRedeclaration=false, reportAttributeAccessIssue=false, reportArgumentType=false

import logging
import time
from dataclasses import dataclass

_QT_AVAILABLE = False
_GL_AVAILABLE = False
try:
    from PySide6.QtCore import Qt
    from PySide6.QtGui import QColor, QPainter
    from PySide6.QtWidgets import QWidget

    _QT_AVAILABLE = True
except ImportError:
    pass

if _QT_AVAILABLE:
    try:
        from PySide6.QtOpenGLWidgets import QOpenGLWidget
        from PySide6.QtOpenGL import (
            QOpenGLBuffer,
            QOpenGLShader,
            QOpenGLShaderProgram,
            QOpenGLVertexArrayObject,
        )

        _GL_AVAILABLE = True
    except ImportError:
        pass

from render.theme import (  # noqa: E402
    RenderTheme,
    _clamp_unit,
    _parse_hex_color,
    _blend_hex_color,
)

from ._base import AuraWidgetConfig  # noqa: E402

_log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

@dataclass(frozen=True, slots=True)
class GlassPanelConfig:
    frost_intensity: float = 0.08
    frost_scale: float = 3.5
    base_alpha: float = 0.72
    accent_tint_strength: float = 0.18
    animate_frost: bool = True
    min_width: int = 100
    min_height: int = 60

    def __post_init__(self) -> None:
        if not (0.0 <= self.frost_intensity <= 1.0):
            raise ValueError("frost_intensity must be in [0, 1].")
        if not (0.0 <= self.base_alpha <= 1.0):
            raise ValueError("base_alpha must be in [0, 1].")
        if not (0.0 <= self.accent_tint_strength <= 1.0):
            raise ValueError("accent_tint_strength must be in [0, 1].")
        if self.min_width < 1:
            raise ValueError("min_width must be >= 1.")
        if self.min_height < 1:
            raise ValueError("min_height must be >= 1.")


# ---------------------------------------------------------------------------
# Shaders (GLSL 330 core)
# ---------------------------------------------------------------------------

_VERTEX_SHADER = """\
#version 330 core
layout(location = 0) in vec2 a_position;
out vec2 v_uv;
void main() {
    v_uv = a_position * 0.5 + 0.5;
    gl_Position = vec4(a_position, 0.0, 1.0);
}
"""

_FRAGMENT_SHADER = """\
#version 330 core
in vec2 v_uv;
out vec4 fragColor;

uniform float u_time;
uniform float u_accent_intensity;
uniform vec3 u_base_color;
uniform vec3 u_accent_color;
uniform float u_frost_intensity;
uniform float u_frost_scale;
uniform float u_base_alpha;
uniform float u_accent_tint;
uniform vec2 u_resolution;

// Simple hash-based noise (no texture dependency)
float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);  // smoothstep
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 3; i++) {
        value += amplitude * noise(p);
        p *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

void main() {
    vec2 uv = v_uv;
    float drift = u_time * 0.015;

    // 3-octave fbm frost
    float frost = fbm(uv * u_frost_scale + drift);
    frost = frost * u_frost_intensity;

    // Base panel color
    vec3 color = u_base_color;

    // Blend accent tint
    vec3 accent = u_accent_color * u_accent_intensity;
    color = mix(color, accent, u_accent_tint * u_accent_intensity);

    // Add frost as brightness variation
    color += frost;

    // Edge vignette for depth
    vec2 center = uv - 0.5;
    float vignette = 1.0 - dot(center, center) * 0.8;
    color *= vignette;

    fragColor = vec4(color, u_base_alpha);
}
"""


# ---------------------------------------------------------------------------
# Widget
# ---------------------------------------------------------------------------

if _QT_AVAILABLE and _GL_AVAILABLE:

    class GlassPanel(QOpenGLWidget):
        def __init__(
            self,
            glass_config: GlassPanelConfig | None = None,
            widget_config: AuraWidgetConfig | None = None,
            parent: QWidget | None = None,
        ) -> None:
            super().__init__(parent)
            cfg_w = widget_config or AuraWidgetConfig()
            self._glass_config = glass_config or GlassPanelConfig()
            self._theme: RenderTheme = cfg_w.theme
            self._accent_intensity: float = _clamp_unit(cfg_w.initial_accent_intensity)
            self._program: QOpenGLShaderProgram | None = None
            self._vao: QOpenGLVertexArrayObject | None = None
            self._vbo: QOpenGLBuffer | None = None
            self._init_failed: bool = False
            self._start_time: float = time.monotonic()
            self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground, True)
            self.setMinimumSize(
                self._glass_config.min_width,
                self._glass_config.min_height,
            )

        # -- AuraWidget-compatible API --

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

        # -- OpenGL lifecycle --

        def initializeGL(self) -> None:
            try:
                program = QOpenGLShaderProgram(self)
                if not program.addShaderFromSourceCode(
                    QOpenGLShader.ShaderTypeBit.Vertex, _VERTEX_SHADER
                ):
                    raise RuntimeError(f"Vertex shader: {program.log()}")
                if not program.addShaderFromSourceCode(
                    QOpenGLShader.ShaderTypeBit.Fragment, _FRAGMENT_SHADER
                ):
                    raise RuntimeError(f"Fragment shader: {program.log()}")
                if not program.link():
                    raise RuntimeError(f"Shader link: {program.log()}")

                self._program = program

                # Fullscreen quad as triangle strip: 4 vertices in NDC
                import ctypes
                import array

                quad_data = array.array("f", [
                    -1.0, -1.0,
                     1.0, -1.0,
                    -1.0,  1.0,
                     1.0,  1.0,
                ])
                raw_bytes = quad_data.tobytes()

                vao = QOpenGLVertexArrayObject(self)
                vao.create()
                vao.bind()

                vbo = QOpenGLBuffer(QOpenGLBuffer.Type.VertexBuffer)
                vbo.create()
                vbo.bind()
                vbo.allocate(raw_bytes, len(raw_bytes))

                program.bind()
                program.enableAttributeArray(0)
                program.setAttributeBuffer(
                    0, 0x1406, 0, 2, 2 * ctypes.sizeof(ctypes.c_float)  # GL_FLOAT
                )
                program.release()
                vbo.release()
                vao.release()

                self._vao = vao
                self._vbo = vbo

                # Cache uniform locations
                self._u_time = program.uniformLocation(b"u_time")
                self._u_accent_intensity = program.uniformLocation(b"u_accent_intensity")
                self._u_base_color = program.uniformLocation(b"u_base_color")
                self._u_accent_color = program.uniformLocation(b"u_accent_color")
                self._u_frost_intensity = program.uniformLocation(b"u_frost_intensity")
                self._u_frost_scale = program.uniformLocation(b"u_frost_scale")
                self._u_base_alpha = program.uniformLocation(b"u_base_alpha")
                self._u_accent_tint = program.uniformLocation(b"u_accent_tint")
                self._u_resolution = program.uniformLocation(b"u_resolution")

            except Exception:
                _log.warning("GlassPanel: OpenGL init failed, using fallback", exc_info=True)
                self._init_failed = True

        def resizeGL(self, w: int, h: int) -> None:
            from PySide6.QtOpenGL import QOpenGLFunctions_3_3_Core  # noqa: F811

            funcs = QOpenGLFunctions_3_3_Core()
            if funcs.initializeOpenGLFunctions():
                funcs.glViewport(0, 0, w, h)

        def paintGL(self) -> None:
            if self._init_failed or self._program is None or self._vao is None:
                self._paint_fallback()
                return

            program = self._program
            vao = self._vao
            gcfg = self._glass_config
            theme = self._theme

            program.bind()
            vao.bind()

            # Time uniform for frost drift
            elapsed = time.monotonic() - self._start_time
            program.setUniformValue(self._u_time, float(elapsed))
            program.setUniformValue(self._u_accent_intensity, float(self._accent_intensity))

            # Base color from theme background
            base_r, base_g, base_b = _parse_hex_color(theme.background_start)
            program.setUniformValue(self._u_base_color, base_r / 255.0, base_g / 255.0, base_b / 255.0)

            # Accent color blended between metric_color and metric_color_peak
            accent_hex = _blend_hex_color(
                theme.metric_color, theme.metric_color_peak, self._accent_intensity
            )
            acc_r, acc_g, acc_b = _parse_hex_color(accent_hex)
            program.setUniformValue(self._u_accent_color, acc_r / 255.0, acc_g / 255.0, acc_b / 255.0)

            program.setUniformValue(self._u_frost_intensity, float(gcfg.frost_intensity))
            program.setUniformValue(self._u_frost_scale, float(gcfg.frost_scale))
            program.setUniformValue(self._u_base_alpha, float(gcfg.base_alpha))
            program.setUniformValue(self._u_accent_tint, float(gcfg.accent_tint_strength))
            program.setUniformValue(self._u_resolution, float(self.width()), float(self.height()))

            from PySide6.QtOpenGL import QOpenGLFunctions_3_3_Core

            funcs = QOpenGLFunctions_3_3_Core()
            if funcs.initializeOpenGLFunctions():
                funcs.glDrawArrays(0x0005, 0, 4)  # GL_TRIANGLE_STRIP

            vao.release()
            program.release()

            if gcfg.animate_frost:
                self.update()

        def _paint_fallback(self) -> None:
            """QPainter solid-rect fallback when OpenGL is unavailable."""
            painter = QPainter(self)
            r, g, b = _parse_hex_color(self._theme.background_start)
            alpha = int(self._glass_config.base_alpha * 255)
            painter.fillRect(self.rect(), QColor(r, g, b, alpha))
            painter.end()

elif _QT_AVAILABLE:
    # PySide6 present but QOpenGLWidgets unavailable — fallback to QPainter-only

    class GlassPanel(QWidget):  # type: ignore[no-redef]
        def __init__(
            self,
            glass_config: GlassPanelConfig | None = None,
            widget_config: AuraWidgetConfig | None = None,
            parent: QWidget | None = None,
        ) -> None:
            super().__init__(parent)
            cfg_w = widget_config or AuraWidgetConfig()
            self._glass_config = glass_config or GlassPanelConfig()
            self._theme: RenderTheme = cfg_w.theme
            self._accent_intensity: float = _clamp_unit(cfg_w.initial_accent_intensity)
            self._init_failed: bool = True
            self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground, True)
            self.setMinimumSize(
                self._glass_config.min_width,
                self._glass_config.min_height,
            )

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

        def paintEvent(self, event: object) -> None:
            painter = QPainter(self)
            r, g, b = _parse_hex_color(self._theme.background_start)
            alpha = int(self._glass_config.base_alpha * 255)
            painter.fillRect(self.rect(), QColor(r, g, b, alpha))
            painter.end()

else:

    class GlassPanel:  # type: ignore[no-redef]
        def __init__(self, *args: object, **kwargs: object) -> None:
            raise RuntimeError(
                "PySide6 is required for Aura widgets. Run `uv sync` to install it."
            )
