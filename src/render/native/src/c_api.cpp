#include "aura_render.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <string>
#include <utility>

#include "render_native/formatting.hpp"
#include "render_native/math.hpp"
#include "render_native/qt_hooks.hpp"
#include "render_native/status.hpp"
#include "render_native/theme.hpp"
#include "render_native/widgets.hpp"

struct AuraQtRenderHooks {
    explicit AuraQtRenderHooks(aura::render_native::QtRenderCallbacks callbacks, void* user_data)
        : hooks(std::move(callbacks), user_data) {}

    aura::render_native::QtRenderHooks hooks;
};

namespace {

void write_c_string(const std::string& value, char* out, size_t out_size) {
    if (out == nullptr || out_size == 0) {
        return;
    }
    const size_t copy_size = std::min(value.size(), out_size - 1);
    std::memcpy(out, value.data(), copy_size);
    out[copy_size] = '\0';
}

std::optional<std::string> optional_string_from_nullable(const char* value) {
    if (value == nullptr || value[0] == '\0') {
        return std::nullopt;
    }
    return std::string(value);
}

aura::render_native::FrameDiscipline to_internal(AuraFrameDiscipline discipline) {
    return aura::render_native::FrameDiscipline{
        discipline.target_fps,
        discipline.max_catchup_frames,
    };
}

aura::render_native::QtRenderCallbacks to_internal(AuraQtRenderCallbacks callbacks) {
    return aura::render_native::QtRenderCallbacks{
        callbacks.begin_frame,
        callbacks.set_accent_rgba,
        callbacks.set_panel_frost,
        callbacks.set_ring_style,
        callbacks.set_timeline_emphasis,
        callbacks.commit_frame,
    };
}

aura::render_native::QtRenderFrameInput to_internal(AuraQtRenderFrameInput input) {
    return aura::render_native::QtRenderFrameInput{
        input.cpu_percent,
        input.memory_percent,
        input.elapsed_since_last_frame,
        input.pulse_hz,
        input.target_fps,
        input.max_catchup_frames,
    };
}

}  // namespace

extern "C" {

double aura_sanitize_percent(double value) {
    return aura::render_native::sanitize_percent(value);
}

double aura_sanitize_non_negative(double value) {
    return aura::render_native::sanitize_non_negative(value);
}

int aura_quantize_accent_intensity(double accent_intensity) {
    return aura::render_native::quantize_accent_intensity(accent_intensity);
}

int aura_widget_backend_available(void) {
    return aura::render_native::widget_backend_available() ? 1 : 0;
}

const char* aura_widget_backend_name(void) {
    static std::string backend = aura::render_native::widget_backend_name();
    return backend.c_str();
}

double aura_advance_phase(
    double phase,
    double delta_seconds,
    double cycles_per_second,
    AuraFrameDiscipline discipline
) {
    return aura::render_native::advance_phase(
        phase,
        delta_seconds,
        cycles_per_second,
        to_internal(discipline)
    );
}

double aura_compute_accent_intensity(
    double cpu_percent,
    double memory_percent,
    double phase,
    double floor,
    double ceiling,
    double pulse_strength
) {
    return aura::render_native::compute_accent_intensity(
        cpu_percent,
        memory_percent,
        phase,
        floor,
        ceiling,
        pulse_strength
    );
}

AuraCockpitFrameState aura_compose_cockpit_frame(
    double previous_phase,
    double elapsed_since_last_frame,
    double cpu_percent,
    double memory_percent,
    AuraFrameDiscipline discipline,
    double pulse_hz
) {
    const aura::render_native::CockpitFrameState state = aura::render_native::compose_cockpit_frame(
        previous_phase,
        elapsed_since_last_frame,
        cpu_percent,
        memory_percent,
        to_internal(discipline),
        pulse_hz
    );
    return AuraCockpitFrameState{
        state.phase,
        state.accent_intensity,
        state.next_delay_seconds,
    };
}

void aura_blend_hex_color(
    const char* start,
    const char* end,
    double ratio,
    char* out_hex,
    size_t out_hex_size
) {
    const std::string value = aura::render_native::blend_hex_color(
        std::string(start == nullptr ? "" : start),
        std::string(end == nullptr ? "" : end),
        ratio
    );
    write_c_string(value, out_hex, out_hex_size);
}

AuraSnapshotLines aura_format_snapshot_lines(
    double timestamp,
    double cpu_percent,
    double memory_percent
) {
    const aura::render_native::SnapshotLines lines =
        aura::render_native::format_snapshot_lines(timestamp, cpu_percent, memory_percent);
    AuraSnapshotLines out{};
    write_c_string(lines.cpu, out.cpu, sizeof(out.cpu));
    write_c_string(lines.memory, out.memory, sizeof(out.memory));
    write_c_string(lines.timestamp, out.timestamp, sizeof(out.timestamp));
    return out;
}

void aura_format_process_row(
    int rank,
    const char* name,
    double cpu_percent,
    double memory_rss_bytes,
    int max_chars,
    char* out_row,
    size_t out_row_size
) {
    const std::string value = aura::render_native::format_process_row(
        rank,
        std::string(name == nullptr ? "" : name),
        cpu_percent,
        memory_rss_bytes,
        max_chars
    );
    write_c_string(value, out_row, out_row_size);
}

void aura_format_initial_status(
    const char* db_path,
    int sample_count_is_set,
    int sample_count,
    const char* error,
    char* out_status,
    size_t out_status_size
) {
    const std::optional<int> sample_opt = sample_count_is_set != 0 ? std::optional<int>(sample_count)
                                                                    : std::nullopt;
    const std::string value = aura::render_native::format_initial_status(
        optional_string_from_nullable(db_path),
        sample_opt,
        optional_string_from_nullable(error)
    );
    write_c_string(value, out_status, out_status_size);
}

void aura_format_stream_status(
    const char* db_path,
    int sample_count_is_set,
    int sample_count,
    const char* error,
    char* out_status,
    size_t out_status_size
) {
    const std::optional<int> sample_opt = sample_count_is_set != 0 ? std::optional<int>(sample_count)
                                                                    : std::nullopt;
    const std::string value = aura::render_native::format_stream_status(
        optional_string_from_nullable(db_path),
        sample_opt,
        optional_string_from_nullable(error)
    );
    write_c_string(value, out_status, out_status_size);
}

AuraQtRenderBackendCaps aura_qt_hooks_backend_caps(void) {
    const aura::render_native::QtRenderBackendCaps caps = aura::render_native::qt_backend_caps();
    return AuraQtRenderBackendCaps{
        caps.available ? 1 : 0,
        caps.supports_callbacks ? 1 : 0,
        caps.preferred_fps,
    };
}

AuraQtRenderHooks* aura_qt_hooks_create(const AuraQtRenderCallbacks* callbacks, void* user_data) {
    if (callbacks == nullptr) {
        return nullptr;
    }

    aura::render_native::QtRenderCallbacks internal_callbacks = to_internal(*callbacks);
    if (!aura::render_native::qt_callbacks_complete(internal_callbacks)) {
        return nullptr;
    }

    try {
        return new AuraQtRenderHooks(std::move(internal_callbacks), user_data);
    } catch (...) {
        return nullptr;
    }
}

void aura_qt_hooks_destroy(AuraQtRenderHooks* hooks) {
    delete hooks;
}

int aura_qt_hooks_render_frame(AuraQtRenderHooks* hooks, AuraQtRenderFrameInput input) {
    if (hooks == nullptr) {
        return 0;
    }
    return hooks->hooks.render_frame(to_internal(input)) ? 1 : 0;
}

const char* aura_qt_hooks_last_error(const AuraQtRenderHooks* hooks) {
    if (hooks == nullptr) {
        return "invalid render hooks handle";
    }
    return hooks->hooks.last_error().c_str();
}

}  // extern "C"
