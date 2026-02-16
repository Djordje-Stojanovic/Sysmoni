#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "aura_shell/cockpit_types.hpp"

namespace aura { namespace shell { namespace detail {

inline std::string trim_to_max_chars(const std::string& value, const std::size_t max_chars) {
    if (value.size() <= max_chars) {
        return value;
    }
    if (max_chars <= 3U) {
        return value.substr(0, max_chars);
    }
    return value.substr(0, max_chars - 3U) + "...";
}

inline std::string optional_or(const std::optional<std::string>& value, const std::string& fallback) {
    if (value.has_value() && !value->empty()) {
        return *value;
    }
    return fallback;
}

inline double clamp_unit(const double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return std::clamp(value, 0.0, 1.0);
}

inline double clamp_percent_100(const double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return std::clamp(value, 0.0, 100.0);
}

inline int clamp_severity_level(const int value) {
    return std::clamp(value, 0, 3);
}

inline int clamp_quality_hint(const int value) {
    return value > 0 ? 1 : 0;
}

inline int recommended_delay_ms(const double next_delay_seconds, const int quality_hint) {
    double delay_seconds = std::isfinite(next_delay_seconds) && next_delay_seconds > 0.0
                               ? next_delay_seconds
                               : (1.0 / 60.0);
    if (quality_hint > 0) {
        delay_seconds = std::max(delay_seconds, 1.0 / 45.0);
    }
    return std::clamp(static_cast<int>(std::llround(delay_seconds * 1000.0)), 16, 1000);
}

inline int fps_from_delay_ms(const int delay_ms) {
    const int safe_delay_ms = std::max(1, delay_ms);
    return std::clamp(static_cast<int>(std::lround(1000.0 / static_cast<double>(safe_delay_ms))), 1, 120);
}

inline int count_timeline_anomalies(const std::vector<TimelinePoint>& points) {
    if (points.size() < 2U) {
        return 0;
    }

    int count = 0;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const double cpu_jump = std::fabs(points[i].cpu_percent - points[i - 1U].cpu_percent);
        const double memory_jump = std::fabs(points[i].memory_percent - points[i - 1U].memory_percent);
        const double gpu_jump = std::fabs(points[i].gpu_percent - points[i - 1U].gpu_percent);
        if (cpu_jump >= 15.0 || memory_jump >= 15.0 || gpu_jump >= 15.0) {
            ++count;
        }
    }
    return count;
}

inline RenderStyleTokens fallback_style_tokens(
    const double phase,
    const double accent_intensity,
    const double cpu_percent,
    const double memory_percent
) {
    RenderStyleTokens tokens;
    if (!std::isfinite(phase)) {
        tokens.phase = 0.0;
    } else {
        tokens.phase = std::fmod(phase, 1.0);
    }
    if (tokens.phase < 0.0) {
        tokens.phase += 1.0;
    }
    tokens.next_delay_seconds = 1.0 / 60.0;
    tokens.accent_intensity = clamp_unit(accent_intensity);
    tokens.accent_red = clamp_unit(0.20 + tokens.accent_intensity * 0.50);
    tokens.accent_green = clamp_unit(0.45 + tokens.accent_intensity * 0.25);
    tokens.accent_blue = 0.75;
    tokens.accent_alpha = clamp_unit(0.15 + tokens.accent_intensity * 0.35);
    tokens.frost_intensity = clamp_unit(0.25 + tokens.accent_intensity * 0.55);
    tokens.tint_strength = clamp_unit(0.35 + tokens.accent_intensity * 0.45);
    tokens.ring_line_width = std::clamp(1.0 + (tokens.accent_intensity * 6.0), 1.0, 7.0);
    tokens.ring_glow_strength = tokens.accent_intensity;
    tokens.cpu_alpha = clamp_unit(0.30 + clamp_percent_100(cpu_percent) / 100.0 * 0.70);
    tokens.memory_alpha = clamp_unit(0.30 + clamp_percent_100(memory_percent) / 100.0 * 0.70);
    tokens.severity_level = 0;
    tokens.motion_scale = 1.0;
    tokens.quality_hint = 0;
    tokens.timeline_anomaly_alpha = 0.05;
    return tokens;
}

}}}  // namespace aura::shell::detail
