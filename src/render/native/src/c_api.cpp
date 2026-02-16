#include "c_api_internal.hpp"

using namespace aura::render::detail;

extern "C" {

const char* aura_last_error(void) {
    return g_last_error[0] == '\0' ? kFallbackLastError : g_last_error;
}

void aura_clear_error(void) {
    clear_last_error();
}

double aura_sanitize_percent(double value) {
    return call_with_fallback<double>("aura_sanitize_percent", 0.0, [&]() {
        return aura::render_native::sanitize_percent(value);
    });
}

double aura_sanitize_non_negative(double value) {
    return call_with_fallback<double>("aura_sanitize_non_negative", 0.0, [&]() {
        return aura::render_native::sanitize_non_negative(value);
    });
}

int aura_quantize_accent_intensity(double accent_intensity) {
    return call_with_fallback<int>("aura_quantize_accent_intensity", 0, [&]() {
        return aura::render_native::quantize_accent_intensity(accent_intensity);
    });
}

int aura_widget_backend_available(void) {
    return call_with_fallback<int>("aura_widget_backend_available", 0, [&]() {
        return aura::render_native::widget_backend_available() ? 1 : 0;
    });
}

const char* aura_widget_backend_name(void) {
    static std::string backend = "qt_widgets_rhi_stub";
    try {
        backend = aura::render_native::widget_backend_name();
        clear_last_error();
        return backend.c_str();
    } catch (const std::exception& ex) {
        set_last_error_exception("aura_widget_backend_name", ex);
    } catch (...) {
        set_last_error_unknown("aura_widget_backend_name");
    }
    return "qt_widgets_rhi_stub";
}

double aura_advance_phase(
    double phase,
    double delta_seconds,
    double cycles_per_second,
    AuraFrameDiscipline discipline
) {
    return call_with_fallback<double>("aura_advance_phase", normalize_phase(phase), [&]() {
        return aura::render_native::advance_phase(
            phase,
            delta_seconds,
            cycles_per_second,
            to_internal(discipline)
        );
    });
}

double aura_compute_accent_intensity(
    double cpu_percent,
    double memory_percent,
    double phase,
    double floor,
    double ceiling,
    double pulse_strength
) {
    return call_with_fallback<double>(
        "aura_compute_accent_intensity",
        fallback_accent_floor(floor),
        [&]() {
            return aura::render_native::compute_accent_intensity(
                cpu_percent,
                memory_percent,
                phase,
                floor,
                ceiling,
                pulse_strength
            );
        }
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
    return call_with_fallback<AuraCockpitFrameState>(
        "aura_compose_cockpit_frame",
        fallback_cockpit_frame(previous_phase),
        [&]() {
            const aura::render_native::CockpitFrameState state =
                aura::render_native::compose_cockpit_frame(
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
    );
}

AuraRenderStyleTokens aura_compute_style_tokens(AuraRenderStyleTokensInput input) {
    return call_with_fallback<AuraRenderStyleTokens>(
        "aura_compute_style_tokens",
        fallback_style_tokens(input.previous_phase),
        [&]() {
            const aura::render_native::QtRenderStyleTokens tokens =
                aura::render_native::compute_qt_style_tokens(input.previous_phase, to_internal(input));
            return to_external(tokens);
        }
    );
}

AuraStyleSequencer* aura_style_sequencer_create(AuraStyleSequencerConfig config) {
    try {
        const AuraStyleSequencerConfig sanitized = sanitize_style_sequencer_config(config);
        auto* sequencer = new AuraStyleSequencer{};
        sequencer->discipline = aura::render_native::FrameDiscipline{
            sanitized.target_fps,
            sanitized.max_catchup_frames,
        };
        sequencer->pulse_hz = sanitized.pulse_hz;
        sequencer->rise_half_life_seconds = sanitized.rise_half_life_seconds;
        sequencer->fall_half_life_seconds = sanitized.fall_half_life_seconds;
        sequencer->phase = 0.0;
        sequencer->smoothed_cpu_percent = 0.0;
        sequencer->smoothed_memory_percent = 0.0;
        sequencer->has_smoothed_samples = false;
        clear_style_sequencer_error(sequencer);
        clear_last_error();
        return sequencer;
    } catch (const std::exception& ex) {
        set_last_error_exception("aura_style_sequencer_create", ex);
        return nullptr;
    } catch (...) {
        set_last_error_unknown("aura_style_sequencer_create");
        return nullptr;
    }
}

void aura_style_sequencer_destroy(AuraStyleSequencer* sequencer) {
    (void)call_void_with_error("aura_style_sequencer_destroy", [&]() {
        delete sequencer;
    });
}

void aura_style_sequencer_reset(AuraStyleSequencer* sequencer, double phase_seed) {
    if (sequencer == nullptr) {
        set_last_error("aura_style_sequencer_reset", kInvalidStyleSequencerHandle);
        return;
    }

    const bool ok = call_void_with_error("aura_style_sequencer_reset", [&]() {
        sequencer->phase = normalize_phase(phase_seed);
        sequencer->smoothed_cpu_percent = 0.0;
        sequencer->smoothed_memory_percent = 0.0;
        sequencer->has_smoothed_samples = false;
        clear_style_sequencer_error(sequencer);
    });
    if (!ok) {
        set_style_sequencer_error(sequencer, "reset failed");
    }
}

AuraRenderStyleTokens aura_style_sequencer_tick(
    AuraStyleSequencer* sequencer,
    AuraStyleSequencerInput input
) {
    if (sequencer == nullptr) {
        set_last_error("aura_style_sequencer_tick", kInvalidStyleSequencerHandle);
        return fallback_style_tokens(0.0);
    }

    try {
        const double cpu_percent = aura::render_native::sanitize_percent(input.cpu_percent);
        const double memory_percent = aura::render_native::sanitize_percent(input.memory_percent);
        const double elapsed_seconds = resolve_elapsed_seconds(*sequencer, input.elapsed_since_last_frame);

        if (!sequencer->has_smoothed_samples) {
            sequencer->smoothed_cpu_percent = cpu_percent;
            sequencer->smoothed_memory_percent = memory_percent;
            sequencer->has_smoothed_samples = true;
        } else {
            sequencer->smoothed_cpu_percent = apply_asymmetric_smoothing(
                sequencer->smoothed_cpu_percent,
                cpu_percent,
                elapsed_seconds,
                sequencer->rise_half_life_seconds,
                sequencer->fall_half_life_seconds
            );
            sequencer->smoothed_memory_percent = apply_asymmetric_smoothing(
                sequencer->smoothed_memory_percent,
                memory_percent,
                elapsed_seconds,
                sequencer->rise_half_life_seconds,
                sequencer->fall_half_life_seconds
            );
        }

        aura::render_native::QtRenderFrameInput frame_input{};
        frame_input.cpu_percent = sequencer->smoothed_cpu_percent;
        frame_input.memory_percent = sequencer->smoothed_memory_percent;
        frame_input.elapsed_since_last_frame = elapsed_seconds;
        frame_input.pulse_hz = sequencer->pulse_hz;
        frame_input.target_fps = sequencer->discipline.target_fps;
        frame_input.max_catchup_frames = sequencer->discipline.max_catchup_frames;

        const aura::render_native::QtRenderStyleTokens tokens =
            aura::render_native::compute_qt_style_tokens(sequencer->phase, frame_input);
        sequencer->phase = tokens.phase;
        clear_style_sequencer_error(sequencer);
        clear_last_error();
        return to_external(tokens);
    } catch (const std::exception& ex) {
        set_style_sequencer_error(sequencer, std::string(ex.what()));
        set_last_error_exception("aura_style_sequencer_tick", ex);
        return fallback_style_tokens(sequencer->phase);
    } catch (...) {
        set_style_sequencer_error(sequencer, "unknown exception");
        set_last_error_unknown("aura_style_sequencer_tick");
        return fallback_style_tokens(sequencer->phase);
    }
}

const char* aura_style_sequencer_last_error(const AuraStyleSequencer* sequencer) {
    if (sequencer == nullptr) {
        set_last_error("aura_style_sequencer_last_error", kInvalidStyleSequencerHandle);
        return kInvalidStyleSequencerHandle;
    }

    try {
        clear_last_error();
        return sequencer->last_error.c_str();
    } catch (const std::exception& ex) {
        set_last_error_exception("aura_style_sequencer_last_error", ex);
        return "unable to read style sequencer error";
    } catch (...) {
        set_last_error_unknown("aura_style_sequencer_last_error");
        return "unable to read style sequencer error";
    }
}

}  // extern "C"
