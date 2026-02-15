#pragma once

#include <cmath>
#include <iostream>
#include <string>

#include "telemetry_engine.h"

inline bool nearly_equal(double left, double right, double tolerance = 1e-6) {
    return std::fabs(left - right) <= tolerance;
}

inline bool process_ranks_before(
    const aura_process_sample& left,
    const aura_process_sample& right
) {
    if (left.cpu_percent != right.cpu_percent)
        return left.cpu_percent > right.cpu_percent;
    if (left.memory_rss_bytes != right.memory_rss_bytes)
        return left.memory_rss_bytes > right.memory_rss_bytes;
    return left.pid < right.pid;
}

inline int expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }
    return 0;
}
