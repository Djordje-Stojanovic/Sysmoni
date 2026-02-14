#include "aura_platform.h"

#include <cmath>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Fail(const std::string& message) {
    std::cerr << "FAILED: " << message << std::endl;
    std::exit(1);
}

void ExpectTrue(const bool condition, const std::string& message) {
    if (!condition) {
        Fail(message);
    }
}

void ExpectEq(const int actual, const int expected, const std::string& message) {
    if (actual != expected) {
        Fail(message + " (actual=" + std::to_string(actual) + ", expected=" + std::to_string(expected) + ")");
    }
}

void ExpectNear(const double actual, const double expected, const double tolerance, const std::string& message) {
    if (std::fabs(actual - expected) > tolerance) {
        Fail(message + " (actual=" + std::to_string(actual) + ", expected=" + std::to_string(expected) + ")");
    }
}

double NowSeconds() {
    using clock = std::chrono::system_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

void TestConfigNoPersist() {
    aura_config_request_t request{};
    request.no_persist = 1;

    aura_runtime_config_t config{};
    aura_error_t error{};
    const int rc = aura_config_resolve(&request, &config, &error);
    ExpectEq(rc, AURA_OK, "aura_config_resolve should succeed");
    ExpectEq(config.persistence_enabled, 0, "persistence should be disabled");
    ExpectEq(config.db_source, AURA_DB_SOURCE_DISABLED, "db source should be disabled");
}

void TestStoreMemoryAppendLatestBetween() {
    aura_error_t error{};
    aura_store_t* store = nullptr;

    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "aura_store_open :memory: should succeed");
    ExpectTrue(store != nullptr, "store pointer should be set");

    const double base = NowSeconds();
    aura_snapshot_t first{base, 10.0, 20.0};
    aura_snapshot_t second{base + 1.0, 11.0, 21.0};
    aura_snapshot_t third{base + 2.0, 12.0, 22.0};

    rc = aura_store_append(store, &first, &error);
    ExpectEq(rc, AURA_OK, "append first should succeed");
    rc = aura_store_append(store, &second, &error);
    ExpectEq(rc, AURA_OK, "append second should succeed");
    rc = aura_store_append(store, &third, &error);
    ExpectEq(rc, AURA_OK, "append third should succeed");

    int count = 0;
    rc = aura_store_count(store, &count, &error);
    ExpectEq(rc, AURA_OK, "count should succeed");
    ExpectEq(count, 3, "count should equal three");

    aura_snapshot_t latest[2]{};
    int out_count = 0;
    rc = aura_store_latest(store, 2, latest, 2, &out_count, &error);
    ExpectEq(rc, AURA_OK, "latest should succeed");
    ExpectEq(out_count, 2, "latest count should be two");
    ExpectNear(latest[0].timestamp, base + 1.0, 1e-9, "latest[0] timestamp");
    ExpectNear(latest[1].timestamp, base + 2.0, 1e-9, "latest[1] timestamp");

    aura_snapshot_t range[3]{};
    out_count = 0;
    rc = aura_store_between(store, 1, base + 0.5, 1, base + 2.0, range, 3, &out_count, &error);
    ExpectEq(rc, AURA_OK, "between should succeed");
    ExpectEq(out_count, 2, "between count should be two");
    ExpectNear(range[0].timestamp, base + 1.0, 1e-9, "range[0] timestamp");
    ExpectNear(range[1].timestamp, base + 2.0, 1e-9, "range[1] timestamp");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "store close should succeed");
}

void TestLttbDownsample() {
    std::vector<aura_snapshot_t> input;
    input.reserve(10);
    for (int i = 0; i < 10; ++i) {
        input.push_back(aura_snapshot_t{100.0 + static_cast<double>(i), static_cast<double>(i), 50.0});
    }

    aura_snapshot_t output[4]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(),
        static_cast<int>(input.size()),
        4,
        output,
        4,
        &out_count,
        &error
    );

    ExpectEq(rc, AURA_OK, "downsample should succeed");
    ExpectEq(out_count, 4, "downsample output count");
    ExpectNear(output[0].timestamp, 100.0, 1e-9, "first timestamp preserved");
    ExpectNear(output[3].timestamp, 109.0, 1e-9, "last timestamp preserved");
}

void TestQueryTimeline() {
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "store open for query timeline");

    const double base = NowSeconds() - 100.0;
    for (int i = 0; i < 50; ++i) {
        aura_snapshot_t snapshot{
            base + static_cast<double>(i),
            static_cast<double>(i % 15),
            40.0,
        };
        rc = aura_store_append(store, &snapshot, &error);
        ExpectEq(rc, AURA_OK, "append in query timeline test");
    }

    aura_snapshot_t output[10]{};
    int out_count = 0;
    rc = aura_dvr_query_timeline(
        store,
        1,
        base + 10.0,
        1,
        base + 40.0,
        10,
        output,
        10,
        &out_count,
        &error
    );
    ExpectEq(rc, AURA_OK, "query timeline should succeed");
    ExpectEq(out_count, 10, "query timeline count");
    ExpectTrue(output[0].timestamp >= base + 10.0, "query timeline lower bound");
    ExpectTrue(output[out_count - 1].timestamp <= base + 40.0, "query timeline upper bound");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "store close query timeline");
}

} // namespace

int main() {
    TestConfigNoPersist();
    TestStoreMemoryAppendLatestBetween();
    TestLttbDownsample();
    TestQueryTimeline();

    std::cout << "platform_native_tests: PASS" << std::endl;
    return 0;
}
