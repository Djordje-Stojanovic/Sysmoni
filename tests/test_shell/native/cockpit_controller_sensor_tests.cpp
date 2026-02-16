#include "cockpit_test_fakes.hpp"

#include "aura_shell/cockpit_controller.hpp"

#include <cmath>
#include <memory>
#include <string>

bool test_falls_back_to_live_when_dvr_unavailable() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();
    timeline->backend_available = false;

    aura::shell::CockpitController::Config config;
    config.db_path = "C:/tmp/aura.db";

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        config
    );
    static_cast<void>(controller.tick(1.0, 1700000010.0));
    const aura::shell::CockpitUiState state = controller.tick(1.0, 1700000011.0);

    bool ok = true;
    ok &= expect_true(state.timeline_source == aura::shell::TimelineSource::Live, "fallback-live: source");
    ok &= expect_true(state.timeline_points.size() >= 2U, "fallback-live: points");
    ok &= expect_true(contains(state.timeline_line, "timeline=live"), "fallback-live: line");
    return ok;
}

bool test_live_ring_respects_capacity() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto* telemetry_ptr = telemetry.get();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();

    aura::shell::CockpitController::Config config;
    config.prefer_dvr_timeline = false;
    config.timeline_live_capacity = 3U;
    config.timeline_window_seconds = 1000.0;

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        config
    );

    aura::shell::CockpitUiState state;
    for (int i = 0; i < 5; ++i) {
        telemetry_ptr->next_snapshot = aura::shell::TelemetrySnapshot{
            10.0 + static_cast<double>(i),
            20.0 + static_cast<double>(i),
        };
        state = controller.tick(1.0, 1700000100.0 + static_cast<double>(i));
    }

    bool ok = true;
    ok &= expect_true(state.timeline_source == aura::shell::TimelineSource::Live, "capacity: source");
    ok &= expect_true(state.timeline_points.size() == 3U, "capacity: size");
    ok &= expect_true(std::fabs(state.timeline_points.front().timestamp - 1700000102.0) < 0.0001, "capacity: front timestamp");
    ok &= expect_true(state.timeline_anomaly_count >= 0, "capacity: anomaly count present");
    return ok;
}

bool test_anomaly_count_detects_spikes() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto* telemetry_ptr = telemetry.get();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();

    aura::shell::CockpitController::Config config;
    config.prefer_dvr_timeline = false;
    config.timeline_live_capacity = 8U;

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        config
    );

    static_cast<void>(controller.tick(1.0, 1700000200.0));
    telemetry_ptr->next_snapshot = aura::shell::TelemetrySnapshot{92.0, 91.0};
    const aura::shell::CockpitUiState state = controller.tick(1.0, 1700000201.0);

    bool ok = true;
    ok &= expect_true(state.timeline_source == aura::shell::TimelineSource::Live, "anomaly: live source");
    ok &= expect_true(state.timeline_anomaly_count >= 1, "anomaly: count spike");
    ok &= expect_true(contains(state.timeline_line, "anomalies="), "anomaly: line contains anomalies");
    return ok;
}

bool test_per_core_cpu_flows_through() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    aura::shell::PerCoreCpuState per_core;
    per_core.core_count = 4;
    per_core.core_percents = {25.0, 50.0, 75.0, 100.0};
    telemetry->next_per_core = per_core;
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();

    aura::shell::CockpitController controller(
        std::move(telemetry), std::move(render), std::move(timeline), {}
    );
    const auto state = controller.tick(1.0, 1700001000.0);

    bool ok = true;
    ok &= expect_true(state.per_core_cpu.core_count == 4, "per-core: count");
    ok &= expect_true(state.per_core_cpu.core_percents.size() == 4, "per-core: vector size");
    ok &= expect_true(std::fabs(state.per_core_cpu.core_percents[2] - 75.0) < 0.01, "per-core: value");
    return ok;
}

bool test_gpu_unavailable_default() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    // next_gpu left as nullopt -- GPU not available
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();

    aura::shell::CockpitController controller(
        std::move(telemetry), std::move(render), std::move(timeline), {}
    );
    const auto state = controller.tick(1.0, 1700001001.0);

    bool ok = true;
    ok &= expect_true(!state.gpu.available, "gpu-unavail: not available");
    ok &= expect_true(state.gpu.gpu_percent == 0.0, "gpu-unavail: zero percent");
    return ok;
}

bool test_disk_network_rate_flows_through() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    aura::shell::DiskIoState disk;
    disk.read_bytes_per_sec = 1000000.0;
    disk.write_bytes_per_sec = 500000.0;
    telemetry->next_disk = disk;
    aura::shell::NetworkIoState net;
    net.recv_bytes_per_sec = 2000000.0;
    net.sent_bytes_per_sec = 100000.0;
    telemetry->next_network = net;
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();

    aura::shell::CockpitController controller(
        std::move(telemetry), std::move(render), std::move(timeline), {}
    );
    const auto state = controller.tick(1.0, 1700001002.0);

    bool ok = true;
    ok &= expect_true(std::fabs(state.disk_io.read_bytes_per_sec - 1000000.0) < 0.01, "disk: read rate");
    ok &= expect_true(std::fabs(state.network_io.recv_bytes_per_sec - 2000000.0) < 0.01, "net: recv rate");
    return ok;
}

bool test_tiered_polling_gpu_every_2_ticks() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto* telemetry_ptr = telemetry.get();
    aura::shell::GpuState gpu;
    gpu.available = true;
    gpu.gpu_percent = 42.0;
    telemetry_ptr->next_gpu = gpu;
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();

    aura::shell::CockpitController controller(
        std::move(telemetry), std::move(render), std::move(timeline), {}
    );

    // Tick 1: GPU should be collected (first tick)
    const auto state1 = controller.tick(1.0, 1700002000.0);
    bool ok = true;
    ok &= expect_true(state1.gpu.available, "tiered-gpu: available tick 1");
    ok &= expect_true(std::fabs(state1.gpu.gpu_percent - 42.0) < 0.01, "tiered-gpu: value tick 1");

    // Tick 2: change GPU value -- tick_count_=2 (even), should collect
    gpu.gpu_percent = 88.0;
    telemetry_ptr->next_gpu = gpu;
    const auto state2 = controller.tick(1.0, 1700002001.0);
    ok &= expect_true(std::fabs(state2.gpu.gpu_percent - 88.0) < 0.01, "tiered-gpu: value tick 2");

    // Tick 3: change GPU value -- tick_count_=3 (odd), should use cached
    gpu.gpu_percent = 99.0;
    telemetry_ptr->next_gpu = gpu;
    const auto state3 = controller.tick(1.0, 1700002002.0);
    ok &= expect_true(std::fabs(state3.gpu.gpu_percent - 88.0) < 0.01, "tiered-gpu: cached tick 3");

    return ok;
}

bool test_graceful_degradation_new_sensors() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    // All extended sensors left as nullopt
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();

    aura::shell::CockpitController controller(
        std::move(telemetry), std::move(render), std::move(timeline), {}
    );
    const auto state = controller.tick(1.0, 1700003000.0);

    bool ok = true;
    ok &= expect_true(state.telemetry_available, "graceful: telemetry still available");
    ok &= expect_true(!state.gpu.available, "graceful: gpu default");
    ok &= expect_true(state.per_core_cpu.core_count == 0, "graceful: no cores");
    ok &= expect_true(!state.thermal.available, "graceful: no thermal");
    ok &= expect_true(state.disk_io.read_bytes_per_sec == 0.0, "graceful: zero disk");
    ok &= expect_true(state.network_io.recv_bytes_per_sec == 0.0, "graceful: zero net");
    return ok;
}

bool test_thermal_every_5_ticks() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto* telemetry_ptr = telemetry.get();
    aura::shell::ThermalState thermal;
    thermal.available = true;
    thermal.hottest_celsius = 65.0;
    aura::shell::ThermalSensorReading reading;
    reading.label = "CPU Package";
    reading.current_celsius = 65.0;
    thermal.sensors.push_back(reading);
    telemetry_ptr->next_thermal = thermal;
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();

    aura::shell::CockpitController controller(
        std::move(telemetry), std::move(render), std::move(timeline), {}
    );

    // Tick 1: should collect thermal (first tick)
    const auto state1 = controller.tick(1.0, 1700004000.0);
    bool ok = true;
    ok &= expect_true(state1.thermal.available, "thermal-tier: available tick 1");

    // Ticks 2-4: change thermal -- should use cached (not divisible by 5)
    thermal.hottest_celsius = 99.0;
    telemetry_ptr->next_thermal = thermal;
    aura::shell::CockpitUiState state_mid;
    for (int i = 2; i <= 4; ++i) {
        state_mid = controller.tick(1.0, 1700004000.0 + i);
    }
    ok &= expect_true(std::fabs(state_mid.thermal.hottest_celsius - 65.0) < 0.01, "thermal-tier: cached ticks 2-4");

    // Tick 5: should collect thermal (tick_count_ % 5 == 0)
    const auto state5 = controller.tick(1.0, 1700004005.0);
    ok &= expect_true(std::fabs(state5.thermal.hottest_celsius - 99.0) < 0.01, "thermal-tier: updated tick 5");
    return ok;
}
