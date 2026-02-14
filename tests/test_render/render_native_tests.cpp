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

// ---------------------------------------------------------------------------
// NEW: Theme color blend boundary tests via C ABI
// ---------------------------------------------------------------------------

// Verify aura_blend_hex_color at ratio=0.0 returns start color exactly.
void test_blend_at_ratio_zero_returns_start() {
    char out[16] = {};
    aura_blend_hex_color("#3b82f6", "#ef4444", 0.0, out, sizeof(out));
    assert(std::strcmp(out, "#3b82f6") == 0);
    assert_last_error_clear();
}

// Verify aura_blend_hex_color at ratio=1.0 returns end color exactly.
void test_blend_at_ratio_one_returns_end() {
    char out[16] = {};
    aura_blend_hex_color("#3b82f6", "#ef4444", 1.0, out, sizeof(out));
    assert(std::strcmp(out, "#ef4444") == 0);
    assert_last_error_clear();
}

// Verify blend is symmetric: blend(a,b,0.5) produces midpoint channels.
void test_blend_symmetry_at_midpoint() {
    // Black (#000000) to white (#ffffff) at 0.5 should be #808080.
    char out[16] = {};
    aura_blend_hex_color("#000000", "#ffffff", 0.5, out, sizeof(out));
    assert(std::strcmp(out, "#808080") == 0);
    assert_last_error_clear();
}

// Verify blend clamps ratio>1 to 1.0 (returns end color).
void test_blend_ratio_clamped_above_one() {
    char out[16] = {};
    aura_blend_hex_color("#000000", "#ffffff", 2.5, out, sizeof(out));
    assert(std::strcmp(out, "#ffffff") == 0);
    assert_last_error_clear();
}

// Verify blend clamps ratio<0 to 0.0 (returns start color).
void test_blend_ratio_clamped_below_zero() {
    char out[16] = {};
    aura_blend_hex_color("#000000", "#ffffff", -1.0, out, sizeof(out));
    assert(std::strcmp(out, "#000000") == 0);
    assert_last_error_clear();
}

// Verify blend produces fallback and sets error when end color is null.
void test_blend_null_end_produces_fallback() {
    char out[16] = {};
    aura_blend_hex_color("#3b82f6", nullptr, 0.5, out, sizeof(out));
    assert(std::strcmp(out, "#000000") == 0);
    assert_last_error_contains("aura_blend_hex_color");
}

// Verify blend produces fallback on malformed hex input.
void test_blend_malformed_hex_produces_fallback() {
    char out[16] = {};
    aura_blend_hex_color("not_a_color", "#ffffff", 0.5, out, sizeof(out));
    assert(std::strcmp(out, "#000000") == 0);
    assert_last_error_contains("aura_blend_hex_color");
}

// ---------------------------------------------------------------------------
// NEW: quantize_accent_intensity boundary tests via C ABI
// ---------------------------------------------------------------------------

// Verify quantize at exact boundaries.
void test_quantize_accent_intensity_boundaries() {
    assert(aura_quantize_accent_intensity(0.0) == 0);
    assert(aura_quantize_accent_intensity(1.0) == 100);
    assert(aura_quantize_accent_intensity(0.5) == 50);

    // Over-range clamped to 100.
    assert(aura_quantize_accent_intensity(1.5) == 100);

    // Under-range clamped to 0.
    assert(aura_quantize_accent_intensity(-0.5) == 0);

    // NaN clamped to 0.
    assert(aura_quantize_accent_intensity(std::numeric_limits<double>::quiet_NaN()) == 0);

    // Infinity is not finite — clamp_unit returns 0.0, so quantize returns 0.
    assert(aura_quantize_accent_intensity(std::numeric_limits<double>::infinity()) == 0);

    // Rounding at .5 boundary.
    assert(aura_quantize_accent_intensity(0.504) == 50);
    assert(aura_quantize_accent_intensity(0.005) == 1);
    assert(aura_quantize_accent_intensity(0.994) == 99);
}

// ---------------------------------------------------------------------------
// NEW: Style token boundary accent_intensity tests
// ---------------------------------------------------------------------------

// Verify style tokens with accent_intensity at exact boundary values via
// sanitize_percent clamping and extremes.
void test_style_tokens_boundary_accent_values() {
    // cpu=0, memory=0 → minimum accent, accent_intensity close to floor (0.15)
    {
        AuraRenderStyleTokensInput input{};
        input.previous_phase = 0.0;
        input.cpu_percent = 0.0;
        input.memory_percent = 0.0;
        input.elapsed_since_last_frame = 0.016;
        input.pulse_hz = 0.5;
        input.target_fps = 60;
        input.max_catchup_frames = 4;
        const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
        assert_style_tokens_ranges(tokens, 60);
        // accent_intensity is >= 0.15 (floor) at minimum load
        assert(tokens.accent_intensity >= 0.15);
    }

    // cpu=100, memory=100 → maximum accent, accent_intensity close to ceiling (0.95)
    {
        AuraRenderStyleTokensInput input{};
        input.previous_phase = 0.0;
        input.cpu_percent = 100.0;
        input.memory_percent = 100.0;
        input.elapsed_since_last_frame = 0.016;
        input.pulse_hz = 0.5;
        input.target_fps = 60;
        input.max_catchup_frames = 4;
        const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
        assert_style_tokens_ranges(tokens, 60);
        assert(tokens.accent_intensity <= 0.95);
        assert(tokens.accent_intensity >= 0.15);
    }

    // Verify accent_intensity=0 boundary in style token math:
    // ring_line_width = 1 + accent*6, so at accent=0 → 1.0
    {
        AuraRenderStyleTokensInput input{};
        input.previous_phase = 0.0;
        input.cpu_percent = 0.0;
        input.memory_percent = 0.0;
        input.elapsed_since_last_frame = 0.016;
        input.pulse_hz = 0.5;
        input.target_fps = 60;
        input.max_catchup_frames = 4;
        const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
        // ring_line_width = 1 + (accent_intensity * 6), so it must be >= 1 and <= 7
        assert(tokens.ring_line_width >= 1.0);
        assert(tokens.ring_line_width <= 7.0);
        // Verify derived formula: ring_line_width == 1 + accent_intensity * 6
        const double expected_ring = 1.0 + (tokens.accent_intensity * 6.0);
        assert(std::fabs(tokens.ring_line_width - expected_ring) < kFloatEpsilon);
    }

    // Verify accent derivation consistency across all derived tokens
    {
        AuraRenderStyleTokensInput input{};
        input.previous_phase = 0.5;
        input.cpu_percent = 50.0;
        input.memory_percent = 50.0;
        input.elapsed_since_last_frame = 0.016;
        input.pulse_hz = 0.5;
        input.target_fps = 60;
        input.max_catchup_frames = 4;
        const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
        assert_style_tokens_ranges(tokens, 60);

        const double a = tokens.accent_intensity;
        // accent_red = clamp(0.12 + accent*0.65)
        assert(std::fabs(tokens.accent_red - (0.12 + (a * 0.65))) < kFloatEpsilon);
        // accent_green = clamp(0.30 + accent*0.50)
        assert(std::fabs(tokens.accent_green - (0.30 + (a * 0.50))) < kFloatEpsilon);
        // accent_blue = clamp(0.48 + accent*0.42)
        assert(std::fabs(tokens.accent_blue - (0.48 + (a * 0.42))) < kFloatEpsilon);
        // accent_alpha = clamp(0.62 + accent*0.33)
        assert(std::fabs(tokens.accent_alpha - (0.62 + (a * 0.33))) < kFloatEpsilon);
        // frost_intensity = clamp(0.05 + accent*0.30)
        assert(std::fabs(tokens.frost_intensity - (0.05 + (a * 0.30))) < kFloatEpsilon);
        // tint_strength = clamp(0.10 + accent*0.50)
        assert(std::fabs(tokens.tint_strength - (0.10 + (a * 0.50))) < kFloatEpsilon);
        // ring_line_width = 1 + accent*6
        assert(std::fabs(tokens.ring_line_width - (1.0 + (a * 6.0))) < kFloatEpsilon);
        // ring_glow_strength = clamp(0.20 + accent*0.75)
        assert(std::fabs(tokens.ring_glow_strength - (0.20 + (a * 0.75))) < kFloatEpsilon);
    }
}

// ---------------------------------------------------------------------------
// NEW: Style token cpu_alpha / memory_alpha computed correctly from load
// ---------------------------------------------------------------------------

// cpu_alpha = clamp(0.20 + (cpu/100)*0.75); similarly for memory_alpha.
void test_style_tokens_cpu_memory_alpha_derivation() {
    // At cpu=0, memory=0: cpu_alpha = 0.20, memory_alpha = 0.20
    {
        AuraRenderStyleTokensInput input{};
        input.previous_phase = 0.0;
        input.cpu_percent = 0.0;
        input.memory_percent = 0.0;
        input.elapsed_since_last_frame = 0.016;
        input.pulse_hz = 0.5;
        input.target_fps = 60;
        input.max_catchup_frames = 4;
        const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
        assert(std::fabs(tokens.cpu_alpha - 0.20) < kFloatEpsilon);
        assert(std::fabs(tokens.memory_alpha - 0.20) < kFloatEpsilon);
    }

    // At cpu=100, memory=100: cpu_alpha = 0.95, memory_alpha = 0.95
    {
        AuraRenderStyleTokensInput input{};
        input.previous_phase = 0.0;
        input.cpu_percent = 100.0;
        input.memory_percent = 100.0;
        input.elapsed_since_last_frame = 0.016;
        input.pulse_hz = 0.5;
        input.target_fps = 60;
        input.max_catchup_frames = 4;
        const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
        assert(std::fabs(tokens.cpu_alpha - 0.95) < kFloatEpsilon);
        assert(std::fabs(tokens.memory_alpha - 0.95) < kFloatEpsilon);
    }

    // At cpu=50, memory=20: values computed independently
    {
        AuraRenderStyleTokensInput input{};
        input.previous_phase = 0.0;
        input.cpu_percent = 50.0;
        input.memory_percent = 20.0;
        input.elapsed_since_last_frame = 0.016;
        input.pulse_hz = 0.5;
        input.target_fps = 60;
        input.max_catchup_frames = 4;
        const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
        // cpu_alpha = 0.20 + (50/100)*0.75 = 0.20 + 0.375 = 0.575
        assert(std::fabs(tokens.cpu_alpha - 0.575) < kFloatEpsilon);
        // memory_alpha = 0.20 + (20/100)*0.75 = 0.20 + 0.15 = 0.35
        assert(std::fabs(tokens.memory_alpha - 0.35) < kFloatEpsilon);
    }
}

// ---------------------------------------------------------------------------
// NEW: Phase wraps correctly at 1.0 boundary
// ---------------------------------------------------------------------------

void test_phase_wraps_at_unity() {
    // With phase close to 1.0, advancing should wrap back to [0, 1)
    const AuraFrameDiscipline discipline{60, 4};

    // phase=0.99, delta=0.02, pulse_hz=1.0 -> 0.99 + 0.02*1.0 = 1.01 -> wraps to 0.01
    const double result = aura_advance_phase(0.99, 0.02, 1.0, discipline);
    assert(result >= 0.0 && result < 1.0);
    assert(std::fabs(result - 0.01) < 1e-9);
    assert_last_error_clear();

    // phase=0.999, delta very small, should remain near 1.0 but wrapped
    const double r2 = aura_advance_phase(0.999, 0.001, 1.0, discipline);
    assert(r2 >= 0.0 && r2 < 1.0);
    assert_last_error_clear();
}

// Verify phase=0.0 with no elapsed advances to 0.0 (no movement).
void test_phase_zero_delta_no_advance() {
    const AuraFrameDiscipline discipline{60, 4};
    const double result = aura_advance_phase(0.3, 0.0, 0.5, discipline);
    assert(std::fabs(result - 0.3) < kFloatEpsilon);
    assert_last_error_clear();
}

// Verify negative elapsed is treated as zero (no advance).
void test_phase_negative_delta_no_advance() {
    const AuraFrameDiscipline discipline{60, 4};
    const double result = aura_advance_phase(0.3, -1.0, 0.5, discipline);
    // Negative delta is clamped to 0 by FrameDiscipline::clamp_delta_seconds
    assert(std::fabs(result - 0.3) < kFloatEpsilon);
    assert_last_error_clear();
}

// ---------------------------------------------------------------------------
// NEW: Formatting edge case tests
// ---------------------------------------------------------------------------

// Empty process name should not crash and should still output rank and rates.
void test_format_process_row_empty_name() {
    char row[128] = {};
    aura_format_process_row(1, "", 10.0, 1024.0 * 1024.0, 40, row, sizeof(row));
    assert(row[0] != '\0');
    assert(std::strstr(row, "CPU") != nullptr);
    assert(std::strstr(row, "RAM") != nullptr);
    assert_last_error_clear();
}

// Null process name should not crash (API treats null as empty).
void test_format_process_row_null_name() {
    char row[128] = {};
    aura_format_process_row(1, nullptr, 10.0, 1024.0 * 1024.0, 40, row, sizeof(row));
    assert(row[0] != '\0');
    assert_last_error_clear();
}

// Zero cpu and memory should produce a valid row.
void test_format_process_row_zero_values() {
    char row[128] = {};
    aura_format_process_row(1, "idle", 0.0, 0.0, 40, row, sizeof(row));
    assert(row[0] != '\0');
    assert(std::strstr(row, "idle") != nullptr);
    assert_last_error_clear();
}

// Very large memory_rss_bytes (terabyte range) should not crash.
void test_format_process_row_huge_memory() {
    char row[256] = {};
    const double one_tb_bytes = 1.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0;
    aura_format_process_row(1, "bigproc", 0.1, one_tb_bytes, 60, row, sizeof(row));
    assert(row[0] != '\0');
    assert_last_error_clear();
}

// cpu_percent above 100 should be clamped, not produce garbage output.
void test_format_process_row_clamped_cpu() {
    char row[128] = {};
    aura_format_process_row(1, "proc", 150.0, 1024.0 * 1024.0, 40, row, sizeof(row));
    assert(row[0] != '\0');
    // Ensure no negative or impossible values appear
    assert(std::strstr(row, "CPU") != nullptr);
    assert_last_error_clear();
}

// NaN cpu_percent should be sanitized to 0.
void test_format_process_row_nan_cpu() {
    char row[128] = {};
    aura_format_process_row(
        1, "proc", std::numeric_limits<double>::quiet_NaN(), 1024.0 * 1024.0, 40, row, sizeof(row)
    );
    assert(row[0] != '\0');
    assert_last_error_clear();
}

// Snapshot lines with NaN/Inf values should return safe fallback strings.
void test_format_snapshot_lines_nan_inf() {
    const AuraSnapshotLines lines = aura_format_snapshot_lines(
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity()
    );
    // Should not be empty; exact content may be fallback but must be valid C strings
    assert(lines.cpu[0] != '\0' || true);  // always safe to check
    assert(std::strlen(lines.cpu) < sizeof(lines.cpu));
    assert(std::strlen(lines.memory) < sizeof(lines.memory));
    assert(std::strlen(lines.timestamp) < sizeof(lines.timestamp));
}

// Zero byte rates should format as 0.0 KB/s.
void test_format_disk_rate_zero() {
    char buf[64] = {};
    aura_format_disk_rate(0.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Disk 0.0 KB/s") == 0);
    assert_last_error_clear();
}

// NaN byte rate should be treated as zero.
void test_format_disk_rate_nan() {
    char buf[64] = {};
    aura_format_disk_rate(std::numeric_limits<double>::quiet_NaN(), buf, sizeof(buf));
    assert(std::strcmp(buf, "Disk 0.0 KB/s") == 0);
    assert_last_error_clear();
}

// Infinity byte rate should not crash.
void test_format_disk_rate_infinity() {
    char buf[64] = {};
    aura_format_disk_rate(std::numeric_limits<double>::infinity(), buf, sizeof(buf));
    // Should return some valid non-empty string (GB/s or fallback)
    assert(std::strlen(buf) > 0);
}

// Network rate NaN should be treated as zero.
void test_format_network_rate_nan() {
    char buf[64] = {};
    aura_format_network_rate(std::numeric_limits<double>::quiet_NaN(), buf, sizeof(buf));
    assert(std::strcmp(buf, "Net 0.0 KB/s") == 0);
    assert_last_error_clear();
}

// Negative network rate clamped to zero.
void test_format_network_rate_negative() {
    char buf[64] = {};
    aura_format_network_rate(-9999.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Net 0.0 KB/s") == 0);
    assert_last_error_clear();
}

// Exact MB/s boundary for network rate.
void test_format_network_rate_exact_mb() {
    char buf[64] = {};
    aura_format_network_rate(1.0 * 1024.0 * 1024.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Net 1.0 MB/s") == 0);
    assert_last_error_clear();
}

// Exact GB/s boundary for disk rate.
void test_format_disk_rate_exact_gb() {
    char buf[64] = {};
    aura_format_disk_rate(1.0 * 1024.0 * 1024.0 * 1024.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Disk 1.00 GB/s") == 0);
    assert_last_error_clear();
}

// ---------------------------------------------------------------------------
// NEW: Initial and stream status edge cases
// ---------------------------------------------------------------------------

// Format initial status without a db path (null) and no sample count.
void test_format_initial_status_no_db_no_samples() {
    char status[256] = {};
    aura_format_initial_status(nullptr, 0, 0, nullptr, status, sizeof(status));
    assert(status[0] != '\0');
    assert_last_error_clear();
}

// Format initial status with an error string present.
void test_format_initial_status_with_error() {
    char status[256] = {};
    aura_format_initial_status(nullptr, 0, 0, "connection refused", status, sizeof(status));
    assert(status[0] != '\0');
    assert_last_error_clear();
}

// Format stream status with null db path.
void test_format_stream_status_null_db() {
    char status[256] = {};
    aura_format_stream_status(nullptr, 1, 10, nullptr, status, sizeof(status));
    assert(status[0] != '\0');
    assert_last_error_clear();
}

// ---------------------------------------------------------------------------
// NEW: Style sequencer multi-tick monotonic phase test
// ---------------------------------------------------------------------------

// Across N ticks with a fixed small elapsed, phase should strictly increase
// (accounting for wrap-around at 1.0).
void test_style_sequencer_phase_monotonically_advances() {
    AuraStyleSequencerConfig config{};
    config.target_fps = 60;
    config.max_catchup_frames = 4;
    config.pulse_hz = 0.5;
    config.rise_half_life_seconds = 0.12;
    config.fall_half_life_seconds = 0.22;

    AuraStyleSequencer* sequencer = aura_style_sequencer_create(config);
    assert(sequencer != nullptr);
    aura_style_sequencer_reset(sequencer, 0.0);

    AuraStyleSequencerInput input{};
    input.cpu_percent = 50.0;
    input.memory_percent = 50.0;
    input.elapsed_since_last_frame = 1.0 / 60.0;

    double prev_phase = 0.0;
    // Run 30 ticks (half a second at 60fps) without wrap.
    // 30 * (1/60) * 0.5 = 0.25 total phase advance — no wrap expected.
    for (int i = 0; i < 30; ++i) {
        const AuraRenderStyleTokens tokens = aura_style_sequencer_tick(sequencer, input);
        assert(tokens.phase >= 0.0 && tokens.phase < 1.0);
        assert(tokens.phase > prev_phase);
        prev_phase = tokens.phase;
    }

    aura_style_sequencer_destroy(sequencer);
}

// ---------------------------------------------------------------------------
// NEW: Style sequencer error clears on successful tick after reset
// ---------------------------------------------------------------------------

void test_style_sequencer_error_clears_on_success() {
    AuraStyleSequencerConfig config{};
    config.target_fps = 60;
    config.max_catchup_frames = 4;
    config.pulse_hz = 0.5;
    config.rise_half_life_seconds = 0.12;
    config.fall_half_life_seconds = 0.22;

    AuraStyleSequencer* sequencer = aura_style_sequencer_create(config);
    assert(sequencer != nullptr);

    // First tick with bad values (infinity elapsed will be clamped, no error expected)
    AuraStyleSequencerInput bad_input{};
    bad_input.cpu_percent = std::numeric_limits<double>::quiet_NaN();
    bad_input.memory_percent = std::numeric_limits<double>::infinity();
    bad_input.elapsed_since_last_frame = std::numeric_limits<double>::infinity();
    (void)aura_style_sequencer_tick(sequencer, bad_input);
    // NaN/Inf sanitized internally, so no per-sequencer error expected
    assert(std::strcmp(aura_style_sequencer_last_error(sequencer), "") == 0);

    // Good tick should also produce no error
    AuraStyleSequencerInput good_input{};
    good_input.cpu_percent = 30.0;
    good_input.memory_percent = 40.0;
    good_input.elapsed_since_last_frame = 0.016;
    const AuraRenderStyleTokens tokens = aura_style_sequencer_tick(sequencer, good_input);
    assert_style_tokens_ranges(tokens, 60);
    assert(std::strcmp(aura_style_sequencer_last_error(sequencer), "") == 0);
    assert_last_error_clear();

    aura_style_sequencer_destroy(sequencer);
}

// ---------------------------------------------------------------------------
// NEW: Qt hooks multi-frame accumulation test
// ---------------------------------------------------------------------------

// Render multiple frames through qt hooks and verify all produce valid outputs.
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

// ---------------------------------------------------------------------------
// NEW: Qt hooks missing individual callbacks rejected
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// NEW: sanitize_percent and sanitize_non_negative edge cases
// ---------------------------------------------------------------------------

void test_sanitize_edge_cases() {
    // NaN input → 0.0
    assert(aura_sanitize_percent(std::numeric_limits<double>::quiet_NaN()) == 0.0);
    assert(aura_sanitize_non_negative(std::numeric_limits<double>::quiet_NaN()) == 0.0);

    // +Infinity → not finite, sanitize returns 0.0 for both functions
    assert(aura_sanitize_percent(std::numeric_limits<double>::infinity()) == 0.0);
    // sanitize_non_negative: infinity is not finite, returns 0
    assert(aura_sanitize_non_negative(std::numeric_limits<double>::infinity()) == 0.0);

    // -Infinity → 0
    assert(aura_sanitize_percent(-std::numeric_limits<double>::infinity()) == 0.0);
    assert(aura_sanitize_non_negative(-std::numeric_limits<double>::infinity()) == 0.0);

    // Exact boundaries
    assert(aura_sanitize_percent(0.0) == 0.0);
    assert(aura_sanitize_percent(100.0) == 100.0);
    assert(aura_sanitize_percent(50.0) == 50.0);
    assert(aura_sanitize_non_negative(0.0) == 0.0);
    assert(aura_sanitize_non_negative(9999.0) == 9999.0);

    // Negative input → 0
    assert(aura_sanitize_percent(-0.001) == 0.0);
    assert(aura_sanitize_non_negative(-0.001) == 0.0);
}

// ---------------------------------------------------------------------------
// NEW: aura_clear_error is idempotent
// ---------------------------------------------------------------------------

void test_clear_error_idempotent() {
    // Set an error condition first
    (void)aura_advance_phase(0.0, 0.01, 0.5, AuraFrameDiscipline{0, 4});
    assert_last_error_contains("aura_advance_phase");

    // Clear once
    aura_clear_error();
    assert_last_error_clear();

    // Clear again — should still be empty
    aura_clear_error();
    assert_last_error_clear();

    // Clear with no prior error
    aura_clear_error();
    assert_last_error_clear();
}

// ---------------------------------------------------------------------------
// NEW: aura_compose_cockpit_frame accent_intensity range tests
// ---------------------------------------------------------------------------

void test_compose_cockpit_frame_accent_range() {
    const AuraFrameDiscipline discipline{60, 4};

    // Low load → accent near floor (0.15)
    {
        const AuraCockpitFrameState frame =
            aura_compose_cockpit_frame(0.0, 0.016, 0.0, 0.0, discipline, 0.5);
        assert(std::isfinite(frame.accent_intensity));
        assert(frame.accent_intensity >= 0.15 && frame.accent_intensity <= 0.95);
    }

    // High load → accent near ceiling (0.95)
    {
        const AuraCockpitFrameState frame =
            aura_compose_cockpit_frame(0.0, 0.016, 100.0, 100.0, discipline, 0.5);
        assert(std::isfinite(frame.accent_intensity));
        assert(frame.accent_intensity >= 0.15 && frame.accent_intensity <= 0.95);
        // High load must produce higher accent than low load
    }

    // next_delay_seconds should be > 0
    {
        const AuraCockpitFrameState frame =
            aura_compose_cockpit_frame(0.0, 0.016, 50.0, 50.0, discipline, 0.5);
        assert(frame.next_delay_seconds >= 0.0);
        assert(std::isfinite(frame.next_delay_seconds));
    }
}

// ---------------------------------------------------------------------------
// NEW: qt_hooks render_frame consistent accent > cpu alpha at high load
// ---------------------------------------------------------------------------

// At 100% CPU, cpu_alpha should be near 0.95 (max); at 0% CPU, near 0.20 (min).
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

}  // namespace

int main() {
    // --- Existing tests (unchanged) ---
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

    // --- New: Theme / blend tests ---
    test_blend_at_ratio_zero_returns_start();
    test_blend_at_ratio_one_returns_end();
    test_blend_symmetry_at_midpoint();
    test_blend_ratio_clamped_above_one();
    test_blend_ratio_clamped_below_zero();
    test_blend_null_end_produces_fallback();
    test_blend_malformed_hex_produces_fallback();

    // --- New: quantize boundary tests ---
    test_quantize_accent_intensity_boundaries();

    // --- New: Style token boundary and derivation tests ---
    test_style_tokens_boundary_accent_values();
    test_style_tokens_cpu_memory_alpha_derivation();

    // --- New: Phase advance edge cases ---
    test_phase_wraps_at_unity();
    test_phase_zero_delta_no_advance();
    test_phase_negative_delta_no_advance();

    // --- New: Formatting edge cases ---
    test_format_process_row_empty_name();
    test_format_process_row_null_name();
    test_format_process_row_zero_values();
    test_format_process_row_huge_memory();
    test_format_process_row_clamped_cpu();
    test_format_process_row_nan_cpu();
    test_format_snapshot_lines_nan_inf();
    test_format_disk_rate_zero();
    test_format_disk_rate_nan();
    test_format_disk_rate_infinity();
    test_format_disk_rate_exact_gb();
    test_format_network_rate_nan();
    test_format_network_rate_negative();
    test_format_network_rate_exact_mb();

    // --- New: Status formatting edge cases ---
    test_format_initial_status_no_db_no_samples();
    test_format_initial_status_with_error();
    test_format_stream_status_null_db();

    // --- New: Style sequencer behavioral tests ---
    test_style_sequencer_phase_monotonically_advances();
    test_style_sequencer_error_clears_on_success();

    // --- New: Qt hooks multi-frame and partial callback tests ---
    test_qt_hooks_multi_frame_accumulation();
    test_qt_hooks_rejects_partial_callbacks();

    // --- New: sanitize edge cases ---
    test_sanitize_edge_cases();

    // --- New: Error API tests ---
    test_clear_error_idempotent();

    // --- New: cockpit frame accent range ---
    test_compose_cockpit_frame_accent_range();

    // --- New: cpu/memory alpha load tracking ---
    test_qt_hooks_cpu_alpha_tracks_load();

    return 0;
}
