#pragma once

#include <stddef.h>

#if defined(_WIN32) && defined(AURA_RENDER_NATIVE_EXPORTS)
#define AURA_RENDER_API __declspec(dllexport)
#elif defined(_WIN32)
#define AURA_RENDER_API __declspec(dllimport)
#else
#define AURA_RENDER_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AuraFrameDiscipline {
    int target_fps;
    int max_catchup_frames;
} AuraFrameDiscipline;

typedef struct AuraCockpitFrameState {
    double phase;
    double accent_intensity;
    double next_delay_seconds;
} AuraCockpitFrameState;

typedef struct AuraSnapshotLines {
    char cpu[64];
    char memory[64];
    char timestamp[64];
} AuraSnapshotLines;

typedef struct AuraQtRenderHooks AuraQtRenderHooks;
typedef struct AuraStyleSequencer AuraStyleSequencer;

typedef void (*AuraQtBeginFrameFn)(void* user_data);
typedef void (*AuraQtSetAccentRgbaFn)(
    void* user_data,
    double red,
    double green,
    double blue,
    double alpha
);
typedef void (*AuraQtSetPanelFrostFn)(void* user_data, double frost_intensity, double tint_strength);
typedef void (*AuraQtSetRingStyleFn)(void* user_data, double line_width, double glow_strength);
typedef void (*AuraQtSetTimelineEmphasisFn)(void* user_data, double cpu_alpha, double memory_alpha);
typedef void (*AuraQtCommitFrameFn)(void* user_data);

typedef struct AuraQtRenderCallbacks {
    AuraQtBeginFrameFn begin_frame;
    AuraQtSetAccentRgbaFn set_accent_rgba;
    AuraQtSetPanelFrostFn set_panel_frost;
    AuraQtSetRingStyleFn set_ring_style;
    AuraQtSetTimelineEmphasisFn set_timeline_emphasis;
    AuraQtCommitFrameFn commit_frame;
} AuraQtRenderCallbacks;

typedef struct AuraQtRenderFrameInput {
    double cpu_percent;
    double memory_percent;
    double elapsed_since_last_frame;
    double pulse_hz;
    int target_fps;
    int max_catchup_frames;
} AuraQtRenderFrameInput;

typedef struct AuraRenderStyleTokensInput {
    double previous_phase;
    double cpu_percent;
    double memory_percent;
    double elapsed_since_last_frame;
    double pulse_hz;
    int target_fps;
    int max_catchup_frames;
} AuraRenderStyleTokensInput;

typedef struct AuraRenderStyleTokens {
    double phase;
    double next_delay_seconds;
    double accent_intensity;
    double accent_red;
    double accent_green;
    double accent_blue;
    double accent_alpha;
    double frost_intensity;
    double tint_strength;
    double ring_line_width;
    double ring_glow_strength;
    double cpu_alpha;
    double memory_alpha;
    int severity_level;
    double motion_scale;
    int quality_hint;
    double timeline_anomaly_alpha;
} AuraRenderStyleTokens;

typedef struct AuraStyleSequencerConfig {
    int target_fps;
    int max_catchup_frames;
    double pulse_hz;
    double rise_half_life_seconds;
    double fall_half_life_seconds;
} AuraStyleSequencerConfig;

typedef struct AuraStyleSequencerInput {
    double cpu_percent;
    double memory_percent;
    double elapsed_since_last_frame;
} AuraStyleSequencerInput;

typedef struct AuraQtRenderBackendCaps {
    int available;
    int supports_callbacks;
    int preferred_fps;
} AuraQtRenderBackendCaps;

/*
 * C ABI boundary contract:
 * - Functions never throw C++ exceptions across the ABI.
 * - On fallback/error paths, callers can inspect aura_last_error().
 * - On success, aura_last_error() is cleared to an empty string.
 */
AURA_RENDER_API const char* aura_last_error(void);
AURA_RENDER_API void aura_clear_error(void);

AURA_RENDER_API double aura_sanitize_percent(double value);
AURA_RENDER_API double aura_sanitize_non_negative(double value);
AURA_RENDER_API int aura_quantize_accent_intensity(double accent_intensity);
AURA_RENDER_API int aura_widget_backend_available(void);
AURA_RENDER_API const char* aura_widget_backend_name(void);

AURA_RENDER_API double aura_advance_phase(
    double phase,
    double delta_seconds,
    double cycles_per_second,
    AuraFrameDiscipline discipline
);

AURA_RENDER_API double aura_compute_accent_intensity(
    double cpu_percent,
    double memory_percent,
    double phase,
    double floor,
    double ceiling,
    double pulse_strength
);

AURA_RENDER_API AuraCockpitFrameState aura_compose_cockpit_frame(
    double previous_phase,
    double elapsed_since_last_frame,
    double cpu_percent,
    double memory_percent,
    AuraFrameDiscipline discipline,
    double pulse_hz
);

AURA_RENDER_API AuraRenderStyleTokens
aura_compute_style_tokens(AuraRenderStyleTokensInput input);

AURA_RENDER_API AuraStyleSequencer*
aura_style_sequencer_create(AuraStyleSequencerConfig config);

AURA_RENDER_API void aura_style_sequencer_destroy(AuraStyleSequencer* sequencer);

AURA_RENDER_API void aura_style_sequencer_reset(
    AuraStyleSequencer* sequencer,
    double phase_seed
);

AURA_RENDER_API AuraRenderStyleTokens
aura_style_sequencer_tick(AuraStyleSequencer* sequencer, AuraStyleSequencerInput input);

AURA_RENDER_API const char*
aura_style_sequencer_last_error(const AuraStyleSequencer* sequencer);

AURA_RENDER_API void aura_blend_hex_color(
    const char* start,
    const char* end,
    double ratio,
    char* out_hex,
    size_t out_hex_size
);

AURA_RENDER_API AuraSnapshotLines aura_format_snapshot_lines(
    double timestamp,
    double cpu_percent,
    double memory_percent
);

AURA_RENDER_API void aura_format_process_row(
    int rank,
    const char* name,
    double cpu_percent,
    double memory_rss_bytes,
    int max_chars,
    char* out_row,
    size_t out_row_size
);

AURA_RENDER_API void aura_format_initial_status(
    const char* db_path,
    int sample_count_is_set,
    int sample_count,
    const char* error,
    char* out_status,
    size_t out_status_size
);

AURA_RENDER_API void aura_format_stream_status(
    const char* db_path,
    int sample_count_is_set,
    int sample_count,
    const char* error,
    char* out_status,
    size_t out_status_size
);

AURA_RENDER_API void aura_format_disk_rate(
    double bytes_per_second,
    char* out_rate,
    size_t out_rate_size
);

AURA_RENDER_API void aura_format_network_rate(
    double bytes_per_second,
    char* out_rate,
    size_t out_rate_size
);

AURA_RENDER_API AuraQtRenderBackendCaps aura_qt_hooks_backend_caps(void);

AURA_RENDER_API AuraQtRenderHooks* aura_qt_hooks_create(
    const AuraQtRenderCallbacks* callbacks,
    void* user_data
);

AURA_RENDER_API void aura_qt_hooks_destroy(AuraQtRenderHooks* hooks);

AURA_RENDER_API int aura_qt_hooks_render_frame(
    AuraQtRenderHooks* hooks,
    AuraQtRenderFrameInput input
);

AURA_RENDER_API const char* aura_qt_hooks_last_error(const AuraQtRenderHooks* hooks);

#ifdef __cplusplus
}
#endif
