#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "aura_render.h"

namespace {

constexpr double kFloatEpsilon = 1e-9;

void assert_last_error_contains(const char* needle) {
    const char* error = aura_last_error();
    assert(error != nullptr);
    assert(std::strstr(error, needle) != nullptr);
}

void assert_last_error_clear() {
    const char* error = aura_last_error();
    assert(error != nullptr);
    assert(std::strcmp(error, "") == 0);
}

void assert_style_tokens_ranges(const AuraRenderStyleTokens& tokens, int target_fps) {
    assert(std::isfinite(tokens.phase));
    assert(std::isfinite(tokens.next_delay_seconds));
    assert(std::isfinite(tokens.accent_intensity));
    assert(std::isfinite(tokens.accent_red));
    assert(std::isfinite(tokens.accent_green));
    assert(std::isfinite(tokens.accent_blue));
    assert(std::isfinite(tokens.accent_alpha));
    assert(std::isfinite(tokens.frost_intensity));
    assert(std::isfinite(tokens.tint_strength));
    assert(std::isfinite(tokens.ring_line_width));
    assert(std::isfinite(tokens.ring_glow_strength));
    assert(std::isfinite(tokens.cpu_alpha));
    assert(std::isfinite(tokens.memory_alpha));

    assert(tokens.phase >= 0.0 && tokens.phase < 1.0);
    assert(tokens.next_delay_seconds >= 0.0);
    assert(tokens.accent_intensity >= 0.0 && tokens.accent_intensity <= 1.0);
    assert(tokens.accent_red >= 0.0 && tokens.accent_red <= 1.0);
    assert(tokens.accent_green >= 0.0 && tokens.accent_green <= 1.0);
    assert(tokens.accent_blue >= 0.0 && tokens.accent_blue <= 1.0);
    assert(tokens.accent_alpha >= 0.0 && tokens.accent_alpha <= 1.0);
    assert(tokens.frost_intensity >= 0.0 && tokens.frost_intensity <= 1.0);
    assert(tokens.tint_strength >= 0.0 && tokens.tint_strength <= 1.0);
    assert(tokens.ring_line_width > 0.0 && tokens.ring_line_width <= 7.0);
    assert(tokens.ring_glow_strength >= 0.0 && tokens.ring_glow_strength <= 1.0);
    assert(tokens.cpu_alpha >= 0.0 && tokens.cpu_alpha <= 1.0);
    assert(tokens.memory_alpha >= 0.0 && tokens.memory_alpha <= 1.0);

    const int safe_target_fps = target_fps > 0 ? target_fps : 60;
    assert(tokens.next_delay_seconds <= (1.0 / static_cast<double>(safe_target_fps)) + 1e-6);
}

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

void test_style_tokens_nominal_ranges() {
    AuraRenderStyleTokensInput input{};
    input.previous_phase = 0.2;
    input.cpu_percent = 35.0;
    input.memory_percent = 55.0;
    input.elapsed_since_last_frame = 0.008;
    input.pulse_hz = 0.5;
    input.target_fps = 60;
    input.max_catchup_frames = 4;

    const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
    assert_style_tokens_ranges(tokens, input.target_fps);
}

void test_style_tokens_sanitization_and_defaults() {
    AuraRenderStyleTokensInput input{};
    input.previous_phase = std::numeric_limits<double>::quiet_NaN();
    input.cpu_percent = std::numeric_limits<double>::quiet_NaN();
    input.memory_percent = std::numeric_limits<double>::infinity();
    input.elapsed_since_last_frame = 10.0;
    input.pulse_hz = -2.0;
    input.target_fps = 0;
    input.max_catchup_frames = -4;

    const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
    assert_style_tokens_ranges(tokens, 60);
}

void test_style_tokens_phase_progression() {
    AuraRenderStyleTokensInput input{};
    input.previous_phase = 0.2;
    input.cpu_percent = 45.0;
    input.memory_percent = 60.0;
    input.elapsed_since_last_frame = 0.01;
    input.pulse_hz = 0.5;
    input.target_fps = 60;
    input.max_catchup_frames = 4;

    const AuraRenderStyleTokens first = aura_compute_style_tokens(input);
    assert(std::fabs(first.phase - 0.205) < 1e-6);

    input.previous_phase = first.phase;
    const AuraRenderStyleTokens second = aura_compute_style_tokens(input);
    assert(std::fabs(second.phase - 0.21) < 1e-6);
    assert(second.phase > first.phase);
}

void assert_style_tokens_close(
    const AuraRenderStyleTokens& actual,
    const AuraRenderStyleTokens& expected,
    double epsilon
) {
    assert(std::fabs(actual.phase - expected.phase) <= epsilon);
    assert(std::fabs(actual.next_delay_seconds - expected.next_delay_seconds) <= epsilon);
    assert(std::fabs(actual.accent_intensity - expected.accent_intensity) <= epsilon);
    assert(std::fabs(actual.accent_red - expected.accent_red) <= epsilon);
    assert(std::fabs(actual.accent_green - expected.accent_green) <= epsilon);
    assert(std::fabs(actual.accent_blue - expected.accent_blue) <= epsilon);
    assert(std::fabs(actual.accent_alpha - expected.accent_alpha) <= epsilon);
    assert(std::fabs(actual.frost_intensity - expected.frost_intensity) <= epsilon);
    assert(std::fabs(actual.tint_strength - expected.tint_strength) <= epsilon);
    assert(std::fabs(actual.ring_line_width - expected.ring_line_width) <= epsilon);
    assert(std::fabs(actual.ring_glow_strength - expected.ring_glow_strength) <= epsilon);
    assert(std::fabs(actual.cpu_alpha - expected.cpu_alpha) <= epsilon);
    assert(std::fabs(actual.memory_alpha - expected.memory_alpha) <= epsilon);
}

void test_style_sequencer_lifecycle_and_null_safety() {
    aura_clear_error();
    assert_last_error_clear();

    AuraStyleSequencerConfig config{};
    config.target_fps = 0;
    config.max_catchup_frames = -1;
    config.pulse_hz = -5.0;
    config.rise_half_life_seconds = 0.0;
    config.fall_half_life_seconds = -2.0;

    AuraStyleSequencer* sequencer = aura_style_sequencer_create(config);
    assert(sequencer != nullptr);
    assert(std::strcmp(aura_style_sequencer_last_error(sequencer), "") == 0);

    AuraStyleSequencerInput input{};
    input.cpu_percent = 10.0;
    input.memory_percent = 20.0;
    input.elapsed_since_last_frame = 0.016;

    const AuraRenderStyleTokens fallback = aura_style_sequencer_tick(nullptr, input);
    assert_style_tokens_ranges(fallback, 60);
    assert_last_error_contains("aura_style_sequencer_tick");

    aura_style_sequencer_reset(nullptr, 0.0);
    assert_last_error_contains("aura_style_sequencer_reset");

    assert(std::strcmp(aura_style_sequencer_last_error(nullptr), "invalid style sequencer handle") == 0);
    assert_last_error_contains("aura_style_sequencer_last_error");

    aura_style_sequencer_destroy(sequencer);
    aura_style_sequencer_destroy(nullptr);
}

void test_style_sequencer_deterministic_reset_progression() {
    AuraStyleSequencerConfig config{};
    config.target_fps = 60;
    config.max_catchup_frames = 4;
    config.pulse_hz = 0.5;
    config.rise_half_life_seconds = 0.12;
    config.fall_half_life_seconds = 0.22;

    AuraStyleSequencer* sequencer = aura_style_sequencer_create(config);
    assert(sequencer != nullptr);

    AuraStyleSequencerInput input{};
    input.cpu_percent = 37.0;
    input.memory_percent = 61.0;
    input.elapsed_since_last_frame = 0.01;

    const AuraRenderStyleTokens pass1_a = aura_style_sequencer_tick(sequencer, input);
    const AuraRenderStyleTokens pass1_b = aura_style_sequencer_tick(sequencer, input);
    const AuraRenderStyleTokens pass1_c = aura_style_sequencer_tick(sequencer, input);

    aura_style_sequencer_reset(sequencer, 0.0);
    assert(std::strcmp(aura_style_sequencer_last_error(sequencer), "") == 0);

    const AuraRenderStyleTokens pass2_a = aura_style_sequencer_tick(sequencer, input);
    const AuraRenderStyleTokens pass2_b = aura_style_sequencer_tick(sequencer, input);
    const AuraRenderStyleTokens pass2_c = aura_style_sequencer_tick(sequencer, input);

    assert_style_tokens_close(pass1_a, pass2_a, 1e-9);
    assert_style_tokens_close(pass1_b, pass2_b, 1e-9);
    assert_style_tokens_close(pass1_c, pass2_c, 1e-9);

    aura_style_sequencer_destroy(sequencer);
}

void test_style_sequencer_asymmetric_smoothing() {
    AuraStyleSequencerConfig config{};
    config.target_fps = 60;
    config.max_catchup_frames = 4;
    config.pulse_hz = 0.5;
    config.rise_half_life_seconds = 0.10;
    config.fall_half_life_seconds = 0.80;

    AuraStyleSequencer* sequencer = aura_style_sequencer_create(config);
    assert(sequencer != nullptr);

    AuraStyleSequencerInput input{};
    input.cpu_percent = 10.0;
    input.memory_percent = 10.0;
    input.elapsed_since_last_frame = 1.0 / 60.0;
    const AuraRenderStyleTokens baseline = aura_style_sequencer_tick(sequencer, input);

    input.cpu_percent = 100.0;
    input.memory_percent = 100.0;
    const AuraRenderStyleTokens rising = aura_style_sequencer_tick(sequencer, input);
    assert(rising.accent_intensity > baseline.accent_intensity);

    AuraRenderStyleTokensInput stateless_high{};
    stateless_high.previous_phase = baseline.phase;
    stateless_high.cpu_percent = 100.0;
    stateless_high.memory_percent = 100.0;
    stateless_high.elapsed_since_last_frame = 1.0 / 60.0;
    stateless_high.pulse_hz = 0.5;
    stateless_high.target_fps = 60;
    stateless_high.max_catchup_frames = 4;
    const AuraRenderStyleTokens stateless = aura_compute_style_tokens(stateless_high);
    assert(rising.accent_intensity < stateless.accent_intensity);

    input.cpu_percent = 0.0;
    input.memory_percent = 0.0;
    const AuraRenderStyleTokens falling = aura_style_sequencer_tick(sequencer, input);
    assert(falling.accent_intensity < rising.accent_intensity);

    const double rise_delta = rising.accent_intensity - baseline.accent_intensity;
    const double fall_delta = rising.accent_intensity - falling.accent_intensity;
    assert(rise_delta > 0.0);
    assert(fall_delta > 0.0);
    assert(rise_delta > fall_delta);

    aura_style_sequencer_destroy(sequencer);
}

void test_style_sequencer_frame_spike_clamping() {
    AuraStyleSequencerConfig config{};
    config.target_fps = 60;
    config.max_catchup_frames = 2;
    config.pulse_hz = 1.0;
    config.rise_half_life_seconds = 0.12;
    config.fall_half_life_seconds = 0.22;

    AuraStyleSequencer* sequencer = aura_style_sequencer_create(config);
    assert(sequencer != nullptr);
    aura_style_sequencer_reset(sequencer, 0.0);

    AuraStyleSequencerInput input{};
    input.cpu_percent = 90.0;
    input.memory_percent = 90.0;
    input.elapsed_since_last_frame = 10.0;

    const AuraRenderStyleTokens tokens = aura_style_sequencer_tick(sequencer, input);
    assert_style_tokens_ranges(tokens, 60);
    assert(tokens.phase <= (2.0 / 60.0) + 1e-6);
    assert(tokens.next_delay_seconds <= (1.0 / 60.0) + 1e-6);

    aura_style_sequencer_destroy(sequencer);
}

void test_style_sequencer_sanitization() {
    AuraStyleSequencerConfig config{};
    config.target_fps = 60;
    config.max_catchup_frames = 4;
    config.pulse_hz = 0.5;
    config.rise_half_life_seconds = 0.12;
    config.fall_half_life_seconds = 0.22;

    AuraStyleSequencer* sequencer = aura_style_sequencer_create(config);
    assert(sequencer != nullptr);

    AuraStyleSequencerInput input{};
    input.cpu_percent = std::numeric_limits<double>::quiet_NaN();
    input.memory_percent = std::numeric_limits<double>::infinity();
    input.elapsed_since_last_frame = std::numeric_limits<double>::infinity();

    const AuraRenderStyleTokens tokens = aura_style_sequencer_tick(sequencer, input);
    assert_style_tokens_ranges(tokens, 60);
    assert(std::strcmp(aura_style_sequencer_last_error(sequencer), "") == 0);
    assert_last_error_clear();

    aura_style_sequencer_destroy(sequencer);
}

void test_style_sequencer_stateless_parity_with_tiny_half_life() {
    AuraStyleSequencerConfig config{};
    config.target_fps = 60;
    config.max_catchup_frames = 4;
    config.pulse_hz = 0.5;
    config.rise_half_life_seconds = 1e-9;
    config.fall_half_life_seconds = 1e-9;

    AuraStyleSequencer* sequencer = aura_style_sequencer_create(config);
    assert(sequencer != nullptr);

    AuraStyleSequencerInput seq_input1{};
    seq_input1.cpu_percent = 45.0;
    seq_input1.memory_percent = 61.0;
    seq_input1.elapsed_since_last_frame = 0.01;

    AuraRenderStyleTokensInput stateless_input1{};
    stateless_input1.previous_phase = 0.0;
    stateless_input1.cpu_percent = seq_input1.cpu_percent;
    stateless_input1.memory_percent = seq_input1.memory_percent;
    stateless_input1.elapsed_since_last_frame = seq_input1.elapsed_since_last_frame;
    stateless_input1.pulse_hz = config.pulse_hz;
    stateless_input1.target_fps = config.target_fps;
    stateless_input1.max_catchup_frames = config.max_catchup_frames;

    const AuraRenderStyleTokens seq_tokens1 = aura_style_sequencer_tick(sequencer, seq_input1);
    const AuraRenderStyleTokens stateless_tokens1 = aura_compute_style_tokens(stateless_input1);
    assert_style_tokens_close(seq_tokens1, stateless_tokens1, 1e-6);

    AuraStyleSequencerInput seq_input2{};
    seq_input2.cpu_percent = 82.0;
    seq_input2.memory_percent = 25.0;
    seq_input2.elapsed_since_last_frame = 0.01;

    AuraRenderStyleTokensInput stateless_input2{};
    stateless_input2.previous_phase = stateless_tokens1.phase;
    stateless_input2.cpu_percent = seq_input2.cpu_percent;
    stateless_input2.memory_percent = seq_input2.memory_percent;
    stateless_input2.elapsed_since_last_frame = seq_input2.elapsed_since_last_frame;
    stateless_input2.pulse_hz = config.pulse_hz;
    stateless_input2.target_fps = config.target_fps;
    stateless_input2.max_catchup_frames = config.max_catchup_frames;

    const AuraRenderStyleTokens seq_tokens2 = aura_style_sequencer_tick(sequencer, seq_input2);
    const AuraRenderStyleTokens stateless_tokens2 = aura_compute_style_tokens(stateless_input2);
    assert_style_tokens_close(seq_tokens2, stateless_tokens2, 1e-6);

    aura_style_sequencer_destroy(sequencer);
}

void test_theme() {
    char out[16] = {};
    aura_blend_hex_color("#205b8e", "#3f8fd8", 1.0, out, sizeof(out));
    assert(std::strcmp(out, "#3f8fd8") == 0);
    assert(aura_quantize_accent_intensity(0.504) == 50);
}

void test_c_api_error_surface_and_fallbacks() {
    aura_clear_error();
    assert_last_error_clear();

    const AuraFrameDiscipline invalid_discipline{0, 4};
    const double fallback_phase = aura_advance_phase(0.2, 0.01, 0.5, invalid_discipline);
    assert(std::isfinite(fallback_phase));
    assert(fallback_phase >= 0.0 && fallback_phase < 1.0);
    assert_last_error_contains("aura_advance_phase");

    const AuraFrameDiscipline valid_discipline{60, 4};
    (void)aura_advance_phase(0.2, 0.01, 0.5, valid_discipline);
    assert_last_error_clear();

    const double accent = aura_compute_accent_intensity(10.0, 20.0, 0.2, 0.8, 0.2, 0.2);
    assert(std::fabs(accent - 0.8) < kFloatEpsilon);
    assert_last_error_contains("aura_compute_accent_intensity");

    const AuraCockpitFrameState fallback_frame = aura_compose_cockpit_frame(
        std::numeric_limits<double>::quiet_NaN(),
        10.0,
        10.0,
        20.0,
        AuraFrameDiscipline{0, 0},
        -1.0
    );
    assert(std::isfinite(fallback_frame.phase));
    assert(std::isfinite(fallback_frame.accent_intensity));
    assert(std::isfinite(fallback_frame.next_delay_seconds));
    assert(std::fabs(fallback_frame.accent_intensity - 0.15) < kFloatEpsilon);
    assert(fallback_frame.next_delay_seconds > 0.0);
    assert_last_error_contains("aura_compose_cockpit_frame");

    char blended[16] = {};
    aura_blend_hex_color(nullptr, "#ffffff", 0.5, blended, sizeof(blended));
    assert(std::strcmp(blended, "#000000") == 0);
    assert_last_error_contains("aura_blend_hex_color");

    aura_blend_hex_color("#000000", "#ffffff", 0.5, blended, sizeof(blended));
    assert(std::strcmp(blended, "#808080") == 0);
    assert_last_error_clear();

    assert(aura_qt_hooks_create(nullptr, nullptr) == nullptr);
    assert_last_error_contains("aura_qt_hooks_create");
    aura_clear_error();
    assert_last_error_clear();
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

void test_format_disk_rate() {
    char buf[64] = {};

    // Sub-MB: should format as KB/s
    aura_format_disk_rate(512.0 * 1024.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Disk 512.0 KB/s") == 0);

    // Exactly 1 MB/s
    aura_format_disk_rate(1024.0 * 1024.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Disk 1.0 MB/s") == 0);

    // Large value: GB/s
    aura_format_disk_rate(2.5 * 1024.0 * 1024.0 * 1024.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Disk 2.50 GB/s") == 0);

    // Zero
    aura_format_disk_rate(0.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Disk 0.0 KB/s") == 0);

    // Negative clamped to zero
    aura_format_disk_rate(-100.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Disk 0.0 KB/s") == 0);
}

void test_format_network_rate() {
    char buf[64] = {};

    // Sub-MB: should format as KB/s
    aura_format_network_rate(256.0 * 1024.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Net 256.0 KB/s") == 0);

    // MB/s range
    aura_format_network_rate(10.0 * 1024.0 * 1024.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Net 10.0 MB/s") == 0);

    // GB/s range
    aura_format_network_rate(1.0 * 1024.0 * 1024.0 * 1024.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Net 1.00 GB/s") == 0);

    // Small value
    aura_format_network_rate(100.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Net 0.1 KB/s") == 0);
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

}  // namespace

int main() {
    test_metrics();
    test_animation();
    test_style_tokens_nominal_ranges();
    test_style_tokens_sanitization_and_defaults();
    test_style_tokens_phase_progression();
    test_style_sequencer_lifecycle_and_null_safety();
    test_style_sequencer_deterministic_reset_progression();
    test_style_sequencer_asymmetric_smoothing();
    test_style_sequencer_frame_spike_clamping();
    test_style_sequencer_sanitization();
    test_style_sequencer_stateless_parity_with_tiny_half_life();
    test_theme();
    test_c_api_error_surface_and_fallbacks();
    test_formatting_and_status();
    test_format_disk_rate();
    test_format_network_rate();
    test_widgets_backend();
    test_qt_hooks_caps_and_lifecycle();
    test_qt_hooks_callback_order_and_ranges();
    test_qt_hooks_sanitization_and_clamping();
    test_style_tokens_match_qt_hook_outputs();
    test_qt_hooks_error_surface();
    return 0;
}
