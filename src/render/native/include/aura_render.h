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

#ifdef __cplusplus
}
#endif
