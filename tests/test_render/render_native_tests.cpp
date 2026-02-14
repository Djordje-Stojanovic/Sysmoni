#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "aura_render.h"

namespace {

void test_metrics() {
    assert(aura_sanitize_percent(-1.0) == 0.0);
    assert(aura_sanitize_percent(42.5) == 42.5);
    assert(aura_sanitize_percent(120.0) == 100.0);
    assert(aura_sanitize_non_negative(-1.0) == 0.0);
    assert(aura_sanitize_non_negative(12.0) == 12.0);
}

void test_animation() {
    const AuraFrameDiscipline discipline{60, 4};
    const double phase = aura_advance_phase(0.2, 0.005, 0.5, discipline);
    assert(std::fabs(phase - 0.2025) < 1e-6);

    const AuraCockpitFrameState frame =
        aura_compose_cockpit_frame(0.2, 0.005, 35.0, 55.0, discipline, 0.5);
    assert(frame.accent_intensity >= 0.15);
    assert(frame.accent_intensity <= 0.95);
}

void test_theme() {
    char out[16] = {};
    aura_blend_hex_color("#205b8e", "#3f8fd8", 1.0, out, sizeof(out));
    assert(std::strcmp(out, "#3f8fd8") == 0);
    assert(aura_quantize_accent_intensity(0.504) == 50);
}

void test_formatting_and_status() {
    const AuraSnapshotLines lines = aura_format_snapshot_lines(0.0, 12.34, 56.78);
    assert(std::strcmp(lines.cpu, "CPU 12.3%") == 0);
    assert(std::strcmp(lines.memory, "Memory 56.8%") == 0);
    assert(std::strcmp(lines.timestamp, "Updated 00:00:00 UTC") == 0);

    char row[128] = {};
    aura_format_process_row(2, "worker", 4.5, 2.0 * 1024.0 * 1024.0, 20, row, sizeof(row));
    assert(std::strstr(row, "2.") != nullptr);
    assert(std::strstr(row, "worker") != nullptr);
    assert(std::strstr(row, "CPU") != nullptr);
    assert(std::strstr(row, "RAM") != nullptr);

    char status[128] = {};
    aura_format_initial_status("telemetry.sqlite", 1, 7, nullptr, status, sizeof(status));
    assert(std::strcmp(status, "Collecting telemetry... | DVR samples: 7") == 0);

    aura_format_stream_status("telemetry.sqlite", 1, 4, "disk full", status, sizeof(status));
    assert(std::strcmp(status, "Streaming telemetry | DVR unavailable: disk full") == 0);
}

void test_widgets_backend() {
    const char* backend = aura_widget_backend_name();
    assert(backend != nullptr);
    assert(std::strlen(backend) > 0);
    const int available = aura_widget_backend_available();
    assert(available == 0 || available == 1);
}

struct QtHookProbe {
    std::vector<std::string> stages;
    double accent_red{0.0};
    double accent_green{0.0};
    double accent_blue{0.0};
    double accent_alpha{0.0};
    double frost_intensity{0.0};
    double tint_strength{0.0};
    double ring_line_width{0.0};
    double ring_glow_strength{0.0};
    double cpu_alpha{0.0};
    double memory_alpha{0.0};
    bool all_finite{true};
};

void track_finite(QtHookProbe* probe, double value) {
    probe->all_finite = probe->all_finite && std::isfinite(value);
}

void cb_begin_frame(void* user_data) {
    auto* probe = static_cast<QtHookProbe*>(user_data);
    probe->stages.emplace_back("begin");
}

void cb_set_accent_rgba(void* user_data, double red, double green, double blue, double alpha) {
    auto* probe = static_cast<QtHookProbe*>(user_data);
    probe->stages.emplace_back("accent");
    probe->accent_red = red;
    probe->accent_green = green;
    probe->accent_blue = blue;
    probe->accent_alpha = alpha;
    track_finite(probe, red);
    track_finite(probe, green);
    track_finite(probe, blue);
    track_finite(probe, alpha);
}

void cb_set_panel_frost(void* user_data, double frost_intensity, double tint_strength) {
    auto* probe = static_cast<QtHookProbe*>(user_data);
    probe->stages.emplace_back("frost");
    probe->frost_intensity = frost_intensity;
    probe->tint_strength = tint_strength;
    track_finite(probe, frost_intensity);
    track_finite(probe, tint_strength);
}

void cb_set_ring_style(void* user_data, double line_width, double glow_strength) {
    auto* probe = static_cast<QtHookProbe*>(user_data);
    probe->stages.emplace_back("ring");
    probe->ring_line_width = line_width;
    probe->ring_glow_strength = glow_strength;
    track_finite(probe, line_width);
    track_finite(probe, glow_strength);
}

void cb_set_timeline_emphasis(void* user_data, double cpu_alpha, double memory_alpha) {
    auto* probe = static_cast<QtHookProbe*>(user_data);
    probe->stages.emplace_back("timeline");
    probe->cpu_alpha = cpu_alpha;
    probe->memory_alpha = memory_alpha;
    track_finite(probe, cpu_alpha);
    track_finite(probe, memory_alpha);
}

void cb_commit_frame(void* user_data) {
    auto* probe = static_cast<QtHookProbe*>(user_data);
    probe->stages.emplace_back("commit");
}

void cb_begin_frame_throws(void* /*user_data*/) {
    throw std::runtime_error("boom");
}

AuraQtRenderCallbacks make_qt_callbacks(AuraQtBeginFrameFn begin_frame = cb_begin_frame) {
    AuraQtRenderCallbacks callbacks{};
    callbacks.begin_frame = begin_frame;
    callbacks.set_accent_rgba = cb_set_accent_rgba;
    callbacks.set_panel_frost = cb_set_panel_frost;
    callbacks.set_ring_style = cb_set_ring_style;
    callbacks.set_timeline_emphasis = cb_set_timeline_emphasis;
    callbacks.commit_frame = cb_commit_frame;
    return callbacks;
}

void assert_qt_stage_order(const QtHookProbe& probe) {
    assert(probe.stages.size() == 6U);
    assert(probe.stages[0] == "begin");
    assert(probe.stages[1] == "accent");
    assert(probe.stages[2] == "frost");
    assert(probe.stages[3] == "ring");
    assert(probe.stages[4] == "timeline");
    assert(probe.stages[5] == "commit");
}

void test_qt_hooks_caps_and_lifecycle() {
    const AuraQtRenderBackendCaps caps = aura_qt_hooks_backend_caps();
    assert(caps.available == 0 || caps.available == 1);
    assert(caps.supports_callbacks == 1);
    assert(caps.preferred_fps == 60);

    QtHookProbe probe{};
    AuraQtRenderCallbacks incomplete = make_qt_callbacks();
    incomplete.commit_frame = nullptr;
    assert(aura_qt_hooks_create(&incomplete, &probe) == nullptr);

    const AuraQtRenderCallbacks callbacks = make_qt_callbacks();
    AuraQtRenderHooks* hooks = aura_qt_hooks_create(&callbacks, &probe);
    assert(hooks != nullptr);
    assert(std::strcmp(aura_qt_hooks_last_error(hooks), "") == 0);
    aura_qt_hooks_destroy(hooks);
}

void test_qt_hooks_callback_order_and_ranges() {
    QtHookProbe probe{};
    const AuraQtRenderCallbacks callbacks = make_qt_callbacks();
    AuraQtRenderHooks* hooks = aura_qt_hooks_create(&callbacks, &probe);
    assert(hooks != nullptr);

    AuraQtRenderFrameInput input{};
    input.cpu_percent = 35.0;
    input.memory_percent = 55.0;
    input.elapsed_since_last_frame = 0.008;
    input.pulse_hz = 0.5;
    input.target_fps = 60;
    input.max_catchup_frames = 4;

    assert(aura_qt_hooks_render_frame(hooks, input) == 1);
    assert_qt_stage_order(probe);
    assert(probe.all_finite);

    assert(probe.accent_red >= 0.0 && probe.accent_red <= 1.0);
    assert(probe.accent_green >= 0.0 && probe.accent_green <= 1.0);
    assert(probe.accent_blue >= 0.0 && probe.accent_blue <= 1.0);
    assert(probe.accent_alpha >= 0.0 && probe.accent_alpha <= 1.0);
    assert(probe.frost_intensity >= 0.0 && probe.frost_intensity <= 1.0);
    assert(probe.tint_strength >= 0.0 && probe.tint_strength <= 1.0);
    assert(probe.ring_line_width > 0.0);
    assert(probe.ring_glow_strength >= 0.0 && probe.ring_glow_strength <= 1.0);
    assert(probe.cpu_alpha >= 0.0 && probe.cpu_alpha <= 1.0);
    assert(probe.memory_alpha >= 0.0 && probe.memory_alpha <= 1.0);
    assert(std::strcmp(aura_qt_hooks_last_error(hooks), "") == 0);

    aura_qt_hooks_destroy(hooks);
}

void test_qt_hooks_sanitization_and_clamping() {
    QtHookProbe probe{};
    const AuraQtRenderCallbacks callbacks = make_qt_callbacks();
    AuraQtRenderHooks* hooks = aura_qt_hooks_create(&callbacks, &probe);
    assert(hooks != nullptr);

    AuraQtRenderFrameInput input{};
    input.cpu_percent = std::numeric_limits<double>::quiet_NaN();
    input.memory_percent = std::numeric_limits<double>::infinity();
    input.elapsed_since_last_frame = 10.0;
    input.pulse_hz = -3.0;
    input.target_fps = 0;
    input.max_catchup_frames = -8;

    assert(aura_qt_hooks_render_frame(hooks, input) == 1);
    assert_qt_stage_order(probe);
    assert(probe.all_finite);
    assert(probe.ring_line_width > 0.0);
    assert(probe.ring_line_width <= 7.0);
    assert(probe.cpu_alpha >= 0.0 && probe.cpu_alpha <= 1.0);
    assert(probe.memory_alpha >= 0.0 && probe.memory_alpha <= 1.0);

    aura_qt_hooks_destroy(hooks);
}

void test_qt_hooks_error_surface() {
    AuraQtRenderFrameInput input{};
    input.cpu_percent = 12.0;
    input.memory_percent = 34.0;
    input.elapsed_since_last_frame = 0.01;
    input.pulse_hz = 0.5;
    input.target_fps = 60;
    input.max_catchup_frames = 4;

    assert(aura_qt_hooks_render_frame(nullptr, input) == 0);
    assert(std::strcmp(aura_qt_hooks_last_error(nullptr), "invalid render hooks handle") == 0);

    QtHookProbe probe{};
    const AuraQtRenderCallbacks callbacks = make_qt_callbacks(cb_begin_frame_throws);
    AuraQtRenderHooks* hooks = aura_qt_hooks_create(&callbacks, &probe);
    assert(hooks != nullptr);
    assert(aura_qt_hooks_render_frame(hooks, input) == 0);
    const char* error = aura_qt_hooks_last_error(hooks);
    assert(error != nullptr);
    assert(std::strstr(error, "callback invocation failed") != nullptr);
    aura_qt_hooks_destroy(hooks);
}

}  // namespace

int main() {
    test_metrics();
    test_animation();
    test_theme();
    test_formatting_and_status();
    test_widgets_backend();
    test_qt_hooks_caps_and_lifecycle();
    test_qt_hooks_callback_order_and_ranges();
    test_qt_hooks_sanitization_and_clamping();
    test_qt_hooks_error_surface();
    return 0;
}
