#include "render_test_helpers.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace {

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

}  // namespace

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

void test_style_tokens_match_qt_hook_outputs() {
    QtHookProbe probe{};
    const AuraQtRenderCallbacks callbacks = make_qt_callbacks();
    AuraQtRenderHooks* hooks = aura_qt_hooks_create(&callbacks, &probe);
    assert(hooks != nullptr);

    AuraRenderStyleTokensInput token_input{};
    token_input.previous_phase = 0.0;
    token_input.cpu_percent = 35.0;
    token_input.memory_percent = 55.0;
    token_input.elapsed_since_last_frame = 0.008;
    token_input.pulse_hz = 0.5;
    token_input.target_fps = 60;
    token_input.max_catchup_frames = 4;
    const AuraRenderStyleTokens tokens = aura_compute_style_tokens(token_input);

    AuraQtRenderFrameInput hook_input{};
    hook_input.cpu_percent = token_input.cpu_percent;
    hook_input.memory_percent = token_input.memory_percent;
    hook_input.elapsed_since_last_frame = token_input.elapsed_since_last_frame;
    hook_input.pulse_hz = token_input.pulse_hz;
    hook_input.target_fps = token_input.target_fps;
    hook_input.max_catchup_frames = token_input.max_catchup_frames;

    assert(aura_qt_hooks_render_frame(hooks, hook_input) == 1);
    assert_qt_stage_order(probe);
    assert(std::fabs(probe.accent_red - tokens.accent_red) < kFloatEpsilon);
    assert(std::fabs(probe.accent_green - tokens.accent_green) < kFloatEpsilon);
    assert(std::fabs(probe.accent_blue - tokens.accent_blue) < kFloatEpsilon);
    assert(std::fabs(probe.accent_alpha - tokens.accent_alpha) < kFloatEpsilon);
    assert(std::fabs(probe.frost_intensity - tokens.frost_intensity) < kFloatEpsilon);
    assert(std::fabs(probe.tint_strength - tokens.tint_strength) < kFloatEpsilon);
    assert(std::fabs(probe.ring_line_width - tokens.ring_line_width) < kFloatEpsilon);
    assert(std::fabs(probe.ring_glow_strength - tokens.ring_glow_strength) < kFloatEpsilon);
    assert(std::fabs(probe.cpu_alpha - tokens.cpu_alpha) < kFloatEpsilon);
    assert(std::fabs(probe.memory_alpha - tokens.memory_alpha) < kFloatEpsilon);

    const double accent_from_ring = (probe.ring_line_width - 1.0) / 6.0;
    assert(std::fabs(tokens.accent_intensity - accent_from_ring) < kFloatEpsilon);

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

void test_qt_hooks_multi_frame_accumulation() {
    QtHookProbe probe{};
    const AuraQtRenderCallbacks callbacks = make_qt_callbacks();
    AuraQtRenderHooks* hooks = aura_qt_hooks_create(&callbacks, &probe);
    assert(hooks != nullptr);

    AuraQtRenderFrameInput input{};
    input.elapsed_since_last_frame = 1.0 / 60.0;
    input.pulse_hz = 0.5;
    input.target_fps = 60;
    input.max_catchup_frames = 4;

    // Simulate 10 frames at increasing CPU load
    for (int frame = 0; frame < 10; ++frame) {
        probe.stages.clear();
        probe.all_finite = true;
        input.cpu_percent = static_cast<double>(frame * 10);
        input.memory_percent = static_cast<double>((10 - frame) * 10);

        assert(aura_qt_hooks_render_frame(hooks, input) == 1);
        assert_qt_stage_order(probe);
        assert(probe.all_finite);
        assert(probe.ring_line_width >= 1.0 && probe.ring_line_width <= 7.0);
        assert(probe.cpu_alpha >= 0.0 && probe.cpu_alpha <= 1.0);
        assert(probe.memory_alpha >= 0.0 && probe.memory_alpha <= 1.0);
    }

    assert(std::strcmp(aura_qt_hooks_last_error(hooks), "") == 0);
    aura_qt_hooks_destroy(hooks);
}

void test_qt_hooks_rejects_partial_callbacks() {
    QtHookProbe probe{};

    // Missing begin_frame
    {
        AuraQtRenderCallbacks cbs = make_qt_callbacks();
        cbs.begin_frame = nullptr;
        assert(aura_qt_hooks_create(&cbs, &probe) == nullptr);
    }

    // Missing set_accent_rgba
    {
        AuraQtRenderCallbacks cbs = make_qt_callbacks();
        cbs.set_accent_rgba = nullptr;
        assert(aura_qt_hooks_create(&cbs, &probe) == nullptr);
    }

    // Missing set_panel_frost
    {
        AuraQtRenderCallbacks cbs = make_qt_callbacks();
        cbs.set_panel_frost = nullptr;
        assert(aura_qt_hooks_create(&cbs, &probe) == nullptr);
    }

    // Missing set_ring_style
    {
        AuraQtRenderCallbacks cbs = make_qt_callbacks();
        cbs.set_ring_style = nullptr;
        assert(aura_qt_hooks_create(&cbs, &probe) == nullptr);
    }

    // Missing set_timeline_emphasis
    {
        AuraQtRenderCallbacks cbs = make_qt_callbacks();
        cbs.set_timeline_emphasis = nullptr;
        assert(aura_qt_hooks_create(&cbs, &probe) == nullptr);
    }

    // Missing commit_frame
    {
        AuraQtRenderCallbacks cbs = make_qt_callbacks();
        cbs.commit_frame = nullptr;
        assert(aura_qt_hooks_create(&cbs, &probe) == nullptr);
    }
}

void test_qt_hooks_cpu_alpha_tracks_load() {
    // High CPU
    {
        QtHookProbe probe{};
        const AuraQtRenderCallbacks callbacks = make_qt_callbacks();
        AuraQtRenderHooks* hooks = aura_qt_hooks_create(&callbacks, &probe);
        assert(hooks != nullptr);

        AuraQtRenderFrameInput input{};
        input.cpu_percent = 100.0;
        input.memory_percent = 0.0;
        input.elapsed_since_last_frame = 0.016;
        input.pulse_hz = 0.5;
        input.target_fps = 60;
        input.max_catchup_frames = 4;
        assert(aura_qt_hooks_render_frame(hooks, input) == 1);
        assert(std::fabs(probe.cpu_alpha - 0.95) < kFloatEpsilon);
        assert(std::fabs(probe.memory_alpha - 0.20) < kFloatEpsilon);
        aura_qt_hooks_destroy(hooks);
    }

    // Low CPU
    {
        QtHookProbe probe{};
        const AuraQtRenderCallbacks callbacks = make_qt_callbacks();
        AuraQtRenderHooks* hooks = aura_qt_hooks_create(&callbacks, &probe);
        assert(hooks != nullptr);

        AuraQtRenderFrameInput input{};
        input.cpu_percent = 0.0;
        input.memory_percent = 100.0;
        input.elapsed_since_last_frame = 0.016;
        input.pulse_hz = 0.5;
        input.target_fps = 60;
        input.max_catchup_frames = 4;
        assert(aura_qt_hooks_render_frame(hooks, input) == 1);
        assert(std::fabs(probe.cpu_alpha - 0.20) < kFloatEpsilon);
        assert(std::fabs(probe.memory_alpha - 0.95) < kFloatEpsilon);
        aura_qt_hooks_destroy(hooks);
    }
}
