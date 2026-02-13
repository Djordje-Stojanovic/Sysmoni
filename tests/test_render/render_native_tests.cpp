#include <cassert>
#include <cmath>
#include <cstring>

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

}  // namespace

int main() {
    test_metrics();
    test_animation();
    test_theme();
    test_formatting_and_status();
    test_widgets_backend();
    return 0;
}
