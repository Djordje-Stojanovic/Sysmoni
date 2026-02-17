#include "render_test_helpers.h"
#include <limits>

namespace {

AuraRenderStyleTokensInput make_input(double cpu, double mem, double elapsed = 0.016) {
    AuraRenderStyleTokensInput input{};
    input.previous_phase = 0.0;
    input.cpu_percent = cpu;
    input.memory_percent = mem;
    input.elapsed_since_last_frame = elapsed;
    input.pulse_hz = 0.5;
    input.target_fps = 60;
    input.max_catchup_frames = 4;
    return input;
}

// Two identical calls ensure slope=0 on the second (combined_load unchanged).
// Returns the second call's result.
AuraRenderStyleTokens tokens_with_zero_slope(double cpu, double mem) {
    const auto input = make_input(cpu, mem);
    (void)aura_compute_style_tokens(input);
    return aura_compute_style_tokens(input);
}

// Baseline call sets thread-local trend state, then test call computes slope.
// With elapsed=1.0: slope = 0.5*(test_cpu+test_mem) - 0.5*(base_cpu+base_mem).
// For symmetric values (cpu==mem): slope = test_val - base_val.
AuraRenderStyleTokens tokens_with_slope(
    double base_cpu, double base_mem,
    double test_cpu, double test_mem
) {
    (void)aura_compute_style_tokens(make_input(base_cpu, base_mem));
    return aura_compute_style_tokens(make_input(test_cpu, test_mem, 1.0));
}

}  // namespace

// ---------------------------------------------------------------------------
// Severity level: load-only boundaries (slope forced to 0)
// ---------------------------------------------------------------------------

void test_severity_level_load_boundaries() {
    // load = max(cpu, mem).  With slope=0, severity depends only on load.
    // Thresholds: 0→sev0, 50→sev1, 75→sev2, 92→sev3

    // load=0 → severity 0
    {
        const auto t = tokens_with_zero_slope(0.0, 0.0);
        assert(t.severity_level == 0);
    }

    // load=49 → severity 0 (just below 50)
    {
        const auto t = tokens_with_zero_slope(49.0, 0.0);
        assert(t.severity_level == 0);
    }

    // load=50 → severity 1 (threshold: load >= 50)
    {
        const auto t = tokens_with_zero_slope(50.0, 0.0);
        assert(t.severity_level == 1);
    }

    // load=74 → severity 1 (below 75)
    {
        const auto t = tokens_with_zero_slope(74.0, 0.0);
        assert(t.severity_level == 1);
    }

    // load=75 → severity 2 (threshold: load >= 75)
    {
        const auto t = tokens_with_zero_slope(75.0, 0.0);
        assert(t.severity_level == 2);
    }

    // load=91 → severity 2 (below 92)
    {
        const auto t = tokens_with_zero_slope(91.0, 0.0);
        assert(t.severity_level == 2);
    }

    // load=92 → severity 3 (threshold: load >= 92)
    {
        const auto t = tokens_with_zero_slope(92.0, 0.0);
        assert(t.severity_level == 3);
    }

    // load=100 → severity 3
    {
        const auto t = tokens_with_zero_slope(100.0, 0.0);
        assert(t.severity_level == 3);
    }
}

// ---------------------------------------------------------------------------
// Severity: compound conditions (slope pushes severity up)
// ---------------------------------------------------------------------------

void test_severity_level_slope_compounds() {
    // Baseline 80 → test 89, elapsed=1.0 → slope=9, load=89
    // Rule: load>=85 && slope>=8 → severity 3
    {
        const auto t = tokens_with_slope(80.0, 80.0, 89.0, 89.0);
        assert(t.severity_level == 3);
    }

    // Baseline 60 → test 68, elapsed=1.0 → slope=8, load=68
    // Rule: load>=65 && slope>=6 → severity 2
    {
        const auto t = tokens_with_slope(60.0, 60.0, 68.0, 68.0);
        assert(t.severity_level == 2);
    }

    // Baseline 40 → test 45, elapsed=1.0 → slope=5, load=45
    // Rule: slope>=4 → severity 1 (load 45 < 50)
    {
        const auto t = tokens_with_slope(40.0, 40.0, 45.0, 45.0);
        assert(t.severity_level == 1);
    }
}

// ---------------------------------------------------------------------------
// Severity uses max(cpu, memory), not just CPU
// ---------------------------------------------------------------------------

void test_severity_memory_drives_load() {
    // cpu=10, mem=95 → load = max(10, 95) = 95 → severity 3
    const auto t = tokens_with_zero_slope(10.0, 95.0);
    assert(t.severity_level == 3);
}

// ---------------------------------------------------------------------------
// Motion scale: severity-based array lookup with slope penalty
// ---------------------------------------------------------------------------

void test_motion_scale_per_severity() {
    // With slope=0, motion_scale = kSeverityScale[severity]:
    //   severity 0 → 1.00,  severity 1 → 0.92,
    //   severity 2 → 0.80,  severity 3 → 0.68

    {
        const auto t = tokens_with_zero_slope(10.0, 10.0);
        assert(t.severity_level == 0);
        assert(std::fabs(t.motion_scale - 1.00) < 0.01);
    }
    {
        const auto t = tokens_with_zero_slope(55.0, 0.0);
        assert(t.severity_level == 1);
        assert(std::fabs(t.motion_scale - 0.92) < 0.01);
    }
    {
        const auto t = tokens_with_zero_slope(80.0, 0.0);
        assert(t.severity_level == 2);
        assert(std::fabs(t.motion_scale - 0.80) < 0.01);
    }
    {
        const auto t = tokens_with_zero_slope(95.0, 0.0);
        assert(t.severity_level == 3);
        assert(std::fabs(t.motion_scale - 0.68) < 0.01);
    }

    // motion_scale always in [0.60, 1.00] across the full load range
    for (double load = 0.0; load <= 100.0; load += 5.0) {
        const auto t = tokens_with_zero_slope(load, 0.0);
        assert(t.motion_scale >= 0.60);
        assert(t.motion_scale <= 1.00);
    }
}

// ---------------------------------------------------------------------------
// Motion scale: positive slope reduces motion_scale below base
// ---------------------------------------------------------------------------

void test_motion_scale_slope_penalty() {
    // Establish base: severity 3 with slope=0 → motion_scale ≈ 0.68
    const auto base = tokens_with_zero_slope(95.0, 95.0);
    assert(base.severity_level == 3);
    const double base_motion = base.motion_scale;

    // With slope: baseline 90, test 95, elapsed=1.0 → slope=5
    // Penalty = clamp(5/100) * 0.15 = 0.0075
    // motion_scale = 0.68 - 0.0075 = 0.6725
    const auto with_slope = tokens_with_slope(90.0, 90.0, 95.0, 95.0);
    assert(with_slope.severity_level == 3);
    assert(with_slope.motion_scale < base_motion);
    assert(with_slope.motion_scale >= 0.60);
}

// ---------------------------------------------------------------------------
// Quality hint: downgrade signal at high load + high severity
// ---------------------------------------------------------------------------

void test_quality_hint_thresholds() {
    // severity 0 → quality_hint = 0
    {
        const auto t = tokens_with_zero_slope(10.0, 10.0);
        assert(t.severity_level == 0);
        assert(t.quality_hint == 0);
    }

    // severity 1 → quality_hint = 0
    {
        const auto t = tokens_with_zero_slope(55.0, 0.0);
        assert(t.severity_level == 1);
        assert(t.quality_hint == 0);
    }

    // severity 2, load=80 (< 82) → quality_hint = 0
    {
        const auto t = tokens_with_zero_slope(80.0, 0.0);
        assert(t.severity_level == 2);
        assert(t.quality_hint == 0);
    }

    // severity 2, load=82 (>= 82) → quality_hint = 1
    {
        const auto t = tokens_with_zero_slope(82.0, 0.0);
        assert(t.severity_level == 2);
        assert(t.quality_hint == 1);
    }

    // severity 3 → quality_hint = 1
    {
        const auto t = tokens_with_zero_slope(95.0, 0.0);
        assert(t.severity_level == 3);
        assert(t.quality_hint == 1);
    }
}

// ---------------------------------------------------------------------------
// Timeline anomaly alpha: low load vs high load
// ---------------------------------------------------------------------------

void test_timeline_anomaly_alpha_range() {
    // Low load (10%) → anomaly_alpha near minimum 0.05
    {
        const auto t = tokens_with_zero_slope(10.0, 10.0);
        assert(t.timeline_anomaly_alpha >= 0.0 && t.timeline_anomaly_alpha <= 1.0);
        assert(t.timeline_anomaly_alpha < 0.15);
    }

    // High load (95%) → anomaly_alpha significantly above base
    {
        const auto t = tokens_with_zero_slope(95.0, 95.0);
        assert(t.timeline_anomaly_alpha >= 0.0 && t.timeline_anomaly_alpha <= 1.0);
        assert(t.timeline_anomaly_alpha > 0.50);
    }
}

// ---------------------------------------------------------------------------
// Timeline anomaly alpha increases with load
// ---------------------------------------------------------------------------

void test_timeline_anomaly_alpha_monotonic_with_load() {
    // Anomaly alpha should be non-decreasing as load increases.
    // (Monotonic because both load_score and severity_boost increase with load.)
    double prev_alpha = 0.0;
    const double loads[] = {20.0, 50.0, 60.0, 75.0, 95.0};
    for (const double load : loads) {
        const auto t = tokens_with_zero_slope(load, load);
        assert(t.timeline_anomaly_alpha >= prev_alpha - 1e-9);
        prev_alpha = t.timeline_anomaly_alpha;
    }
}

// ---------------------------------------------------------------------------
// All NaN/Inf inputs: no crash, values in range
// ---------------------------------------------------------------------------

void test_severity_pipeline_all_nan() {
    // Warm up with zeros to set a clean baseline
    (void)aura_compute_style_tokens(make_input(0.0, 0.0));

    AuraRenderStyleTokensInput bad{};
    bad.previous_phase = std::numeric_limits<double>::quiet_NaN();
    bad.cpu_percent = std::numeric_limits<double>::quiet_NaN();
    bad.memory_percent = std::numeric_limits<double>::infinity();
    bad.elapsed_since_last_frame = std::numeric_limits<double>::quiet_NaN();
    bad.pulse_hz = -1.0;
    bad.target_fps = 0;
    bad.max_catchup_frames = -1;

    const auto t = aura_compute_style_tokens(bad);

    // All fields must be finite and within their documented ranges
    assert(t.severity_level >= 0 && t.severity_level <= 3);
    assert(std::isfinite(t.motion_scale));
    assert(t.motion_scale >= 0.60 && t.motion_scale <= 1.00);
    assert(t.quality_hint == 0 || t.quality_hint == 1);
    assert(std::isfinite(t.timeline_anomaly_alpha));
    assert(t.timeline_anomaly_alpha >= 0.0 && t.timeline_anomaly_alpha <= 1.0);

    // General range check for all style token fields
    assert_style_tokens_ranges(t, 60);
}
