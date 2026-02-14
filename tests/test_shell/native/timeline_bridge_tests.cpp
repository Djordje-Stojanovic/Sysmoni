#include "aura_shell/timeline_bridge.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace {

bool expect_true(const bool condition, const std::string& name) {
    if (!condition) {
        std::cerr << "FAILED: " << name << '\n';
        return false;
    }
    return true;
}

bool contains(const std::string& text, const std::string& pattern) {
    return text.find(pattern) != std::string::npos;
}

bool test_rejects_empty_db_path() {
    aura::shell::TimelineBridge bridge;
    std::string error;
    const auto points = bridge.query_recent("", 1700000000.0, 300.0, 64, error);
    bool ok = true;
    ok &= expect_true(points.empty(), "empty-path: no points");
    ok &= expect_true(contains(error, "db_path"), "empty-path: error message");
    return ok;
}

bool test_rejects_non_positive_window() {
    aura::shell::TimelineBridge bridge;
    std::string error;
    const auto points = bridge.query_recent("C:/tmp/aura.db", 1700000000.0, 0.0, 64, error);
    bool ok = true;
    ok &= expect_true(points.empty(), "window: no points");
    ok &= expect_true(contains(error, "window_seconds"), "window: error message");
    return ok;
}

bool test_rejects_non_finite_end_timestamp() {
    aura::shell::TimelineBridge bridge;
    std::string error;
    const auto points = bridge.query_recent(
        "C:/tmp/aura.db",
        std::numeric_limits<double>::quiet_NaN(),
        300.0,
        64,
        error
    );
    bool ok = true;
    ok &= expect_true(points.empty(), "timestamp: no points");
    ok &= expect_true(contains(error, "end_timestamp"), "timestamp: error message");
    return ok;
}

bool test_rejects_resolution_under_min() {
    aura::shell::TimelineBridge bridge;
    std::string error;
    const auto points = bridge.query_recent("C:/tmp/aura.db", 1700000000.0, 300.0, 1, error);
    bool ok = true;
    ok &= expect_true(points.empty(), "resolution: no points");
    ok &= expect_true(contains(error, "resolution"), "resolution: error message");
    return ok;
}

}  // namespace

int main() {
    int failures = 0;

    if (!test_rejects_empty_db_path()) {
        ++failures;
    }
    if (!test_rejects_non_positive_window()) {
        ++failures;
    }
    if (!test_rejects_non_finite_end_timestamp()) {
        ++failures;
    }
    if (!test_rejects_resolution_under_min()) {
        ++failures;
    }

    if (failures == 0) {
        std::cout << "All timeline bridge tests passed." << '\n';
        return 0;
    }
    std::cerr << failures << " timeline bridge tests failed." << '\n';
    return 1;
}
