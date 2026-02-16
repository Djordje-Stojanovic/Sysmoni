#include "telemetry_engine.h"
#include "telemetry_engine_internal.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace aura::telemetry {

bool TelemetryEngine::CollectThermalSnapshot(
    double timestamp_seconds,
    ThermalSnapshot* out_snapshot,
    std::string* error_message
) const noexcept {
    if (out_snapshot == nullptr || !detail::is_finite(timestamp_seconds)) {
        if (error_message != nullptr) {
            *error_message = "CollectThermalSnapshot requires finite timestamp and output.";
        }
        return false;
    }

    out_snapshot->timestamp_seconds = timestamp_seconds;
    out_snapshot->readings.clear();
    out_snapshot->hottest_celsius.reset();

    if (collectors_.collect_thermal_readings == nullptr) {
        detail::clear_error(error_message);
        return true;
    }

    std::array<aura_thermal_reading, kMaxThermalReadings> raw_readings{};
    uint32_t out_count = 0;
    std::array<char, 512> error_buffer{};
    const int status = collectors_.collect_thermal_readings(
        raw_readings.data(),
        static_cast<uint32_t>(raw_readings.size()),
        &out_count,
        error_buffer.data(),
        error_buffer.size()
    );

    if (status != AURA_STATUS_OK) {
        detail::clear_error(error_message);
        return true;
    }

    const uint32_t bounded_count =
        std::min(out_count, static_cast<uint32_t>(raw_readings.size()));
    out_snapshot->readings.reserve(bounded_count);
    for (uint32_t i = 0; i < bounded_count; ++i) {
        const aura_thermal_reading& raw = raw_readings[static_cast<size_t>(i)];
        if (!detail::is_finite(raw.current_celsius)) {
            continue;
        }
        if (raw.current_celsius < detail::kCelsiusMin || raw.current_celsius > detail::kCelsiusMax) {
            continue;
        }

        ThermalReading reading{};
        reading.label = detail::decode_fixed_utf8(raw.label, sizeof(raw.label));
        if (reading.label.empty()) {
            reading.label = "sensor-" + std::to_string(i);
        }
        reading.current_celsius = raw.current_celsius;

        if (raw.has_high != 0 && detail::is_finite(raw.high_celsius) && raw.high_celsius >= detail::kCelsiusMin &&
            raw.high_celsius <= detail::kCelsiusOptionalMax) {
            reading.high_celsius = raw.high_celsius;
        }
        if (raw.has_critical != 0 && detail::is_finite(raw.critical_celsius) &&
            raw.critical_celsius >= detail::kCelsiusMin &&
            raw.critical_celsius <= detail::kCelsiusOptionalMax) {
            reading.critical_celsius = raw.critical_celsius;
        }

        out_snapshot->readings.push_back(std::move(reading));
    }

    if (!out_snapshot->readings.empty()) {
        double hottest = -std::numeric_limits<double>::infinity();
        for (const ThermalReading& reading : out_snapshot->readings) {
            hottest = std::max(hottest, reading.current_celsius);
        }
        if (detail::is_finite(hottest)) {
            out_snapshot->hottest_celsius = hottest;
        }
    }

    detail::clear_error(error_message);
    return true;
}

bool TelemetryEngine::CollectPerCoreCpu(
    double timestamp_seconds,
    PerCoreCpuSnapshot* out_snapshot,
    std::string* error_message
) const {
    if (out_snapshot == nullptr) {
        if (error_message != nullptr) {
            *error_message = "CollectPerCoreCpu requires out_snapshot.";
        }
        return false;
    }
    if (!detail::is_finite(timestamp_seconds)) {
        if (error_message != nullptr) {
            *error_message = "CollectPerCoreCpu requires finite timestamp.";
        }
        return false;
    }

    out_snapshot->timestamp_seconds = timestamp_seconds;
    out_snapshot->core_percents.clear();

    if (collectors_.collect_per_core_cpu == nullptr) {
        detail::clear_error(error_message);
        return true;
    }

    std::array<double, kMaxCores> raw_percents{};
    uint32_t out_count = 0;
    std::array<char, 512> error_buffer{};
    const int status = collectors_.collect_per_core_cpu(
        raw_percents.data(),
        static_cast<uint32_t>(raw_percents.size()),
        &out_count,
        error_buffer.data(),
        error_buffer.size()
    );

    if (status != AURA_STATUS_OK) {
        detail::clear_error(error_message);
        return true;
    }

    const uint32_t bounded_count = std::min(out_count, static_cast<uint32_t>(raw_percents.size()));
    out_snapshot->core_percents.reserve(bounded_count);
    for (uint32_t i = 0; i < bounded_count; ++i) {
        out_snapshot->core_percents.push_back(detail::clamp_percent(raw_percents[i]));
    }

    detail::clear_error(error_message);
    return true;
}

bool TelemetryEngine::CollectGpuSnapshot(
    double timestamp_seconds,
    GpuSnapshot* out_snapshot,
    std::string* error_message
) const noexcept {
    if (out_snapshot == nullptr || !detail::is_finite(timestamp_seconds)) {
        if (error_message != nullptr) {
            *error_message = "CollectGpuSnapshot requires finite timestamp and output.";
        }
        return false;
    }

    out_snapshot->timestamp_seconds = timestamp_seconds;
    out_snapshot->available = false;
    out_snapshot->gpu_percent = 0.0;
    out_snapshot->vram_percent = 0.0;
    out_snapshot->vram_used_bytes = 0;
    out_snapshot->vram_total_bytes = 0;

    if (collectors_.collect_gpu_utilization == nullptr) {
        detail::clear_error(error_message);
        return true;
    }

    aura_gpu_utilization raw{};
    std::array<char, 512> error_buffer{};
    const int status = collectors_.collect_gpu_utilization(
        &raw,
        error_buffer.data(),
        error_buffer.size()
    );

    if (status != AURA_STATUS_OK) {
        detail::clear_error(error_message);
        return true;
    }

    out_snapshot->available = true;
    out_snapshot->gpu_percent = detail::clamp_percent(raw.gpu_percent);
    out_snapshot->vram_percent = detail::clamp_percent(raw.vram_percent);
    out_snapshot->vram_used_bytes = raw.vram_used_bytes;
    out_snapshot->vram_total_bytes = raw.vram_total_bytes;
    detail::clear_error(error_message);
    return true;
}

}  // namespace aura::telemetry
