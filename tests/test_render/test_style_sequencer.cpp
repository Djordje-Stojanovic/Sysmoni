#include "render_test_helpers.h"
#include <limits>

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
