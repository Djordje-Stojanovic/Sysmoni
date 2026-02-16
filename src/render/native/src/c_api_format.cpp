#include "c_api_internal.hpp"

using namespace aura::render::detail;

extern "C" {

void aura_blend_hex_color(
    const char* start,
    const char* end,
    double ratio,
    char* out_hex,
    size_t out_hex_size
) {
    const bool ok = call_void_with_error("aura_blend_hex_color", [&]() {
        const std::string value = aura::render_native::blend_hex_color(
            std::string(start == nullptr ? "" : start),
            std::string(end == nullptr ? "" : end),
            ratio
        );
        write_c_string(value, out_hex, out_hex_size);
    });
    if (!ok) {
        write_c_chars(kFallbackHexColor, out_hex, out_hex_size);
    }
}

AuraSnapshotLines aura_format_snapshot_lines(
    double timestamp,
    double cpu_percent,
    double memory_percent
) {
    return call_with_fallback<AuraSnapshotLines>(
        "aura_format_snapshot_lines",
        fallback_snapshot_lines(),
        [&]() {
            const aura::render_native::SnapshotLines lines =
                aura::render_native::format_snapshot_lines(timestamp, cpu_percent, memory_percent);
            AuraSnapshotLines out{};
            write_c_string(lines.cpu, out.cpu, sizeof(out.cpu));
            write_c_string(lines.memory, out.memory, sizeof(out.memory));
            write_c_string(lines.timestamp, out.timestamp, sizeof(out.timestamp));
            return out;
        }
    );
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
    const bool ok = call_void_with_error("aura_format_process_row", [&]() {
        const std::string value = aura::render_native::format_process_row(
            rank,
            std::string(name == nullptr ? "" : name),
            cpu_percent,
            memory_rss_bytes,
            max_chars
        );
        write_c_string(value, out_row, out_row_size);
    });
    if (!ok) {
        write_c_chars(kFallbackProcessRow, out_row, out_row_size);
    }
}

void aura_format_initial_status(
    const char* db_path,
    int sample_count_is_set,
    int sample_count,
    const char* error,
    char* out_status,
    size_t out_status_size
) {
    const bool ok = call_void_with_error("aura_format_initial_status", [&]() {
        const std::optional<int> sample_opt =
            sample_count_is_set != 0 ? std::optional<int>(sample_count) : std::nullopt;
        const std::string value = aura::render_native::format_initial_status(
            optional_string_from_nullable(db_path),
            sample_opt,
            optional_string_from_nullable(error)
        );
        write_c_string(value, out_status, out_status_size);
    });
    if (!ok) {
        write_c_chars(kFallbackInitialStatus, out_status, out_status_size);
    }
}

void aura_format_stream_status(
    const char* db_path,
    int sample_count_is_set,
    int sample_count,
    const char* error,
    char* out_status,
    size_t out_status_size
) {
    const bool ok = call_void_with_error("aura_format_stream_status", [&]() {
        const std::optional<int> sample_opt =
            sample_count_is_set != 0 ? std::optional<int>(sample_count) : std::nullopt;
        const std::string value = aura::render_native::format_stream_status(
            optional_string_from_nullable(db_path),
            sample_opt,
            optional_string_from_nullable(error)
        );
        write_c_string(value, out_status, out_status_size);
    });
    if (!ok) {
        write_c_chars(kFallbackStreamStatus, out_status, out_status_size);
    }
}

void aura_format_disk_rate(
    double bytes_per_second,
    char* out_rate,
    size_t out_rate_size
) {
    const bool ok = call_void_with_error("aura_format_disk_rate", [&]() {
        const std::string value = aura::render_native::format_disk_rate(bytes_per_second);
        write_c_string(value, out_rate, out_rate_size);
    });
    if (!ok) {
        write_c_chars("Disk 0.0 KB/s", out_rate, out_rate_size);
    }
}

void aura_format_network_rate(
    double bytes_per_second,
    char* out_rate,
    size_t out_rate_size
) {
    const bool ok = call_void_with_error("aura_format_network_rate", [&]() {
        const std::string value = aura::render_native::format_network_rate(bytes_per_second);
        write_c_string(value, out_rate, out_rate_size);
    });
    if (!ok) {
        write_c_chars("Net 0.0 KB/s", out_rate, out_rate_size);
    }
}

AuraQtRenderBackendCaps aura_qt_hooks_backend_caps(void) {
    const AuraQtRenderBackendCaps fallback_caps{0, 1, kDefaultTargetFps};
    return call_with_fallback<AuraQtRenderBackendCaps>(
        "aura_qt_hooks_backend_caps",
        fallback_caps,
        [&]() {
            const aura::render_native::QtRenderBackendCaps caps = aura::render_native::qt_backend_caps();
            return AuraQtRenderBackendCaps{
                caps.available ? 1 : 0,
                caps.supports_callbacks ? 1 : 0,
                caps.preferred_fps,
            };
        }
    );
}

AuraQtRenderHooks* aura_qt_hooks_create(const AuraQtRenderCallbacks* callbacks, void* user_data) {
    if (callbacks == nullptr) {
        set_last_error("aura_qt_hooks_create", "callbacks cannot be null");
        return nullptr;
    }

    aura::render_native::QtRenderCallbacks internal_callbacks = to_internal(*callbacks);
    if (!aura::render_native::qt_callbacks_complete(internal_callbacks)) {
        set_last_error("aura_qt_hooks_create", "callbacks are incomplete");
        return nullptr;
    }

    try {
        AuraQtRenderHooks* hooks = new AuraQtRenderHooks(std::move(internal_callbacks), user_data);
        clear_last_error();
        return hooks;
    } catch (const std::exception& ex) {
        set_last_error_exception("aura_qt_hooks_create", ex);
        return nullptr;
    } catch (...) {
        set_last_error_unknown("aura_qt_hooks_create");
        return nullptr;
    }
}

void aura_qt_hooks_destroy(AuraQtRenderHooks* hooks) {
    (void)call_void_with_error("aura_qt_hooks_destroy", [&]() {
        delete hooks;
    });
}

int aura_qt_hooks_render_frame(AuraQtRenderHooks* hooks, AuraQtRenderFrameInput input) {
    if (hooks == nullptr) {
        set_last_error("aura_qt_hooks_render_frame", "invalid render hooks handle");
        return 0;
    }
    try {
        if (hooks->hooks.render_frame(to_internal(input))) {
            clear_last_error();
            return 1;
        }
        const std::string hook_error = hooks->hooks.last_error();
        if (hook_error.empty()) {
            set_last_error("aura_qt_hooks_render_frame", "render frame failed");
        } else {
            set_last_error("aura_qt_hooks_render_frame", hook_error);
        }
        return 0;
    } catch (const std::exception& ex) {
        set_last_error_exception("aura_qt_hooks_render_frame", ex);
        return 0;
    } catch (...) {
        set_last_error_unknown("aura_qt_hooks_render_frame");
        return 0;
    }
}

const char* aura_qt_hooks_last_error(const AuraQtRenderHooks* hooks) {
    if (hooks == nullptr) {
        set_last_error("aura_qt_hooks_last_error", "invalid render hooks handle");
        return "invalid render hooks handle";
    }
    try {
        clear_last_error();
        return hooks->hooks.last_error().c_str();
    } catch (const std::exception& ex) {
        set_last_error_exception("aura_qt_hooks_last_error", ex);
        return "callback invocation failed: unable to read hook error";
    } catch (...) {
        set_last_error_unknown("aura_qt_hooks_last_error");
        return "callback invocation failed: unable to read hook error";
    }
}

}  // extern "C"
