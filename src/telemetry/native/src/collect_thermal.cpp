#include "telemetry_abi.h"
#include "telemetry_utils.h"

extern "C" int aura_collect_thermal_readings(
    aura_thermal_reading* readings,
    uint32_t max_samples,
    uint32_t* out_count,
    char* error_buffer,
    size_t error_buffer_len
) {
    (void)readings;
    (void)max_samples;
    if (out_count != nullptr) {
        *out_count = 0;
    }
    write_error(
        error_buffer,
        error_buffer_len,
        "Thermal backend is currently unavailable in native collector."
    );
    return AURA_STATUS_UNAVAILABLE;
}
