#include "test_fakes.h"
#include "test_helpers.h"

#include <string>

using aura::telemetry::NetworkSnapshot;
using aura::telemetry::TelemetryEngine;

int test_network_rate_computation() {
    g_network_status = AURA_STATUS_OK;
    g_network_index = 0;
    g_network_sequence = {
        aura_network_counters{1000, 2000, 10, 20},
        aura_network_counters{4000, 5000, 40, 80},
    };

    TelemetryEngine engine(make_collectors());
    NetworkSnapshot first{};
    NetworkSnapshot second{};
    std::string error;
    if (expect(engine.CollectNetworkSnapshot(100.0, &first, &error), "first network sample should succeed")) {
        return 1;
    }
    if (expect(engine.CollectNetworkSnapshot(103.0, &second, &error), "second network sample should succeed")) {
        return 1;
    }
    if (expect(nearly_equal(first.bytes_sent_per_sec, 0.0), "first network send rate should be zero")) {
        return 1;
    }
    if (expect(nearly_equal(second.bytes_sent_per_sec, 1000.0), "network send rate mismatch")) {
        return 1;
    }
    if (expect(nearly_equal(second.packets_recv_per_sec, 20.0), "network recv packet rate mismatch")) {
        return 1;
    }
    return 0;
}

int test_network_non_increasing_timestamp_keeps_baseline() {
    g_network_status = AURA_STATUS_OK;
    g_network_index = 0;
    g_network_sequence = {
        aura_network_counters{1000, 2000, 10, 20},
        aura_network_counters{2000, 3000, 20, 30},
        aura_network_counters{5000, 8000, 50, 80},
    };

    TelemetryEngine engine(make_collectors());
    NetworkSnapshot first{};
    NetworkSnapshot non_increasing{};
    NetworkSnapshot resumed{};
    std::string error;
    if (expect(engine.CollectNetworkSnapshot(200.0, &first, &error), "network baseline sample should succeed")) {
        return 1;
    }
    if (expect(
            engine.CollectNetworkSnapshot(200.0, &non_increasing, &error),
            "network non-increasing timestamp sample should succeed"
        )) {
        return 1;
    }
    if (expect(
            nearly_equal(non_increasing.bytes_sent_per_sec, 0.0),
            "non-increasing network send rate should stay zero"
        )) {
        return 1;
    }
    if (expect(
            engine.CollectNetworkSnapshot(204.0, &resumed, &error),
            "network resumed sample should succeed"
        )) {
        return 1;
    }
    if (expect(nearly_equal(resumed.bytes_sent_per_sec, 1000.0), "network resumed send rate mismatch")) {
        return 1;
    }
    if (expect(nearly_equal(resumed.bytes_recv_per_sec, 1500.0), "network resumed recv rate mismatch")) {
        return 1;
    }
    if (expect(nearly_equal(resumed.packets_sent_per_sec, 10.0), "network resumed send packets mismatch")) {
        return 1;
    }
    if (expect(nearly_equal(resumed.packets_recv_per_sec, 15.0), "network resumed recv packets mismatch")) {
        return 1;
    }
    return 0;
}

int test_network_unavailable_degrades_gracefully() {
    g_network_status = AURA_STATUS_UNAVAILABLE;
    g_network_index = 0;
    g_network_sequence = {aura_network_counters{700, 900, 11, 13}};

    TelemetryEngine engine(make_collectors());
    NetworkSnapshot unavailable{};
    std::string error;
    if (expect(
            engine.CollectNetworkSnapshot(100.0, &unavailable, &error),
            "network unavailable should degrade gracefully"
        )) {
        return 1;
    }
    if (expect(
            unavailable.total_bytes_sent == 0U,
            "network unavailable sent total should be zero"
        )) {
        return 1;
    }
    if (expect(
            unavailable.total_bytes_recv == 0U,
            "network unavailable recv total should be zero"
        )) {
        return 1;
    }
    if (expect(
            nearly_equal(unavailable.bytes_sent_per_sec, 0.0),
            "network unavailable send rate should be zero"
        )) {
        return 1;
    }
    if (expect(
            nearly_equal(unavailable.bytes_recv_per_sec, 0.0),
            "network unavailable recv rate should be zero"
        )) {
        return 1;
    }

    g_network_status = AURA_STATUS_OK;
    g_network_index = 0;
    g_network_sequence = {aura_network_counters{700, 900, 11, 13}};
    NetworkSnapshot recovered{};
    if (expect(
            engine.CollectNetworkSnapshot(101.0, &recovered, &error),
            "network collection should recover after unavailable"
        )) {
        return 1;
    }
    if (expect(
            nearly_equal(recovered.bytes_sent_per_sec, 0.0),
            "first recovered network sample should keep zero rate"
        )) {
        return 1;
    }
    if (expect(recovered.total_bytes_sent == 700U, "network recovered sent total mismatch")) {
        return 1;
    }
    return 0;
}

int test_network_error_still_fails() {
    g_network_status = AURA_STATUS_ERROR;
    g_network_index = 0;
    g_network_sequence.clear();

    TelemetryEngine engine(make_collectors());
    NetworkSnapshot snapshot{};
    std::string error;
    if (expect(
            !engine.CollectNetworkSnapshot(100.0, &snapshot, &error),
            "network error should fail"
        )) {
        return 1;
    }
    if (expect(
            error.find("collect_network_counters failed") != std::string::npos,
            "network error message should include collector failure context"
        )) {
        return 1;
    }
    return 0;
}

int test_network_error_message_clears_on_graceful_paths() {
    g_network_status = AURA_STATUS_ERROR;
    g_network_index = 0;
    g_network_sequence.clear();

    TelemetryEngine engine(make_collectors());
    NetworkSnapshot failed{};
    std::string error;
    if (expect(!engine.CollectNetworkSnapshot(100.0, &failed, &error), "network error should fail")) {
        return 1;
    }
    if (expect(!error.empty(), "network error should populate error message")) {
        return 1;
    }

    g_network_status = AURA_STATUS_UNAVAILABLE;
    g_network_sequence = {aura_network_counters{10, 20, 1, 2}};
    NetworkSnapshot unavailable{};
    if (expect(
            engine.CollectNetworkSnapshot(101.0, &unavailable, &error),
            "network unavailable should return success"
        )) {
        return 1;
    }
    if (expect(error.empty(), "network unavailable path should clear stale error")) {
        return 1;
    }

    g_network_status = AURA_STATUS_OK;
    g_network_index = 0;
    g_network_sequence = {aura_network_counters{50, 70, 5, 7}};
    NetworkSnapshot recovered{};
    if (expect(
            engine.CollectNetworkSnapshot(102.0, &recovered, &error),
            "network success should return true"
        )) {
        return 1;
    }
    if (expect(error.empty(), "network success path should keep error empty")) {
        return 1;
    }
    return 0;
}
