#include "test_platform_helpers.hpp"
#include "test_platform_tests.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <string>

// ---------------------------------------------------------------------------
// Helper: create a snapshot with controlled values
// ---------------------------------------------------------------------------

static aura_snapshot_t MakeSnapshot(double timestamp, double cpu, double mem,
                                     double disk_r = 0.0, double disk_w = 0.0,
                                     double net_r = 0.0, double net_s = 0.0) {
    aura_snapshot_t snap{};
    snap.timestamp = timestamp;
    snap.cpu_percent = cpu;
    snap.memory_percent = mem;
    snap.disk_read_bps = disk_r;
    snap.disk_write_bps = disk_w;
    snap.net_recv_bps = net_r;
    snap.net_sent_bps = net_s;
    return snap;
}

// Helper: create a rule with common defaults
static aura_alert_rule_t MakeRule(int id, int metric, int comparator,
                                   double threshold, double sustained = 0.0,
                                   double cooldown = 0.0, const char* name = "test") {
    aura_alert_rule_t rule{};
    rule.id = id;
    rule.metric = metric;
    rule.comparator = comparator;
    rule.threshold = threshold;
    rule.sustained_seconds = sustained;
    rule.cooldown_seconds = cooldown;
#ifdef _WIN32
    strncpy_s(rule.name, sizeof(rule.name), name, _TRUNCATE);
#else
    std::strncpy(rule.name, name, sizeof(rule.name) - 1);
    rule.name[sizeof(rule.name) - 1] = '\0';
#endif
    return rule;
}

// ---------------------------------------------------------------------------
// 1. Lifecycle tests (3)
// ---------------------------------------------------------------------------

void TestAlertEngineCreateDestroy() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;

    int rc = aura_alert_engine_create(&engine, &error);
    ExpectEq(rc, AURA_OK, "create: should succeed");
    ExpectTrue(engine != nullptr, "create: engine should be non-null");

    rc = aura_alert_engine_destroy(engine);
    ExpectEq(rc, AURA_OK, "destroy: should succeed");
}

void TestAlertEngineDestroyNull() {
    int rc = aura_alert_engine_destroy(nullptr);
    ExpectEq(rc, AURA_OK, "destroy null: should succeed gracefully");
}

void TestAlertEngineCreateNullOut() {
    aura_error_t error{};
    int rc = aura_alert_engine_create(nullptr, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "create null out: should fail");
}

// ---------------------------------------------------------------------------
// 2. Rule management tests (7)
// ---------------------------------------------------------------------------

void TestAlertAddRule() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    int rc = aura_alert_engine_create(&engine, &error);
    ExpectEq(rc, AURA_OK, "add rule: create engine");

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 5.0, 10.0, "CPU high");
    rc = aura_alert_engine_add_rule(engine, &rule, &error);
    ExpectEq(rc, AURA_OK, "add rule: should succeed");

    int count = 0;
    rc = aura_alert_engine_rule_count(engine, &count, &error);
    ExpectEq(rc, AURA_OK, "add rule: count query");
    ExpectEq(count, 1, "add rule: count should be 1");

    aura_alert_engine_destroy(engine);
}

void TestAlertAddRuleDuplicateId() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    int rc = aura_alert_engine_add_rule(engine, &rule, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "duplicate id: should fail");

    aura_alert_engine_destroy(engine);
}

void TestAlertRemoveRule() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    int rc = aura_alert_engine_remove_rule(engine, 1, &error);
    ExpectEq(rc, AURA_OK, "remove rule: should succeed");

    int count = 0;
    aura_alert_engine_rule_count(engine, &count, &error);
    ExpectEq(count, 0, "remove rule: count should be 0");

    aura_alert_engine_destroy(engine);
}

void TestAlertRemoveRuleNotFound() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    int rc = aura_alert_engine_remove_rule(engine, 999, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "remove not found: should fail");

    aura_alert_engine_destroy(engine);
}

void TestAlertGetRule() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(42, AURA_METRIC_MEMORY_PERCENT, AURA_COMPARATOR_BELOW, 20.0, 3.0, 5.0, "Mem low");
    aura_alert_engine_add_rule(engine, &rule, &error);

    aura_alert_rule_t out{};
    int rc = aura_alert_engine_get_rule(engine, 42, &out, &error);
    ExpectEq(rc, AURA_OK, "get rule: should succeed");
    ExpectEq(out.id, 42, "get rule: id");
    ExpectEq(out.metric, AURA_METRIC_MEMORY_PERCENT, "get rule: metric");
    ExpectEq(out.comparator, AURA_COMPARATOR_BELOW, "get rule: comparator");
    ExpectNear(out.threshold, 20.0, 1e-9, "get rule: threshold");
    ExpectNear(out.sustained_seconds, 3.0, 1e-9, "get rule: sustained");
    ExpectNear(out.cooldown_seconds, 5.0, 1e-9, "get rule: cooldown");
    ExpectTrue(std::strncmp(out.name, "Mem low", 7) == 0, "get rule: name");

    aura_alert_engine_destroy(engine);
}

void TestAlertGetRuleNotFound() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t out{};
    int rc = aura_alert_engine_get_rule(engine, 999, &out, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "get rule not found: should fail");

    aura_alert_engine_destroy(engine);
}

void TestAlertAddRuleValidation() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);
    int rc;

    // id = 0
    aura_alert_rule_t rule = MakeRule(0, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0);
    rc = aura_alert_engine_add_rule(engine, &rule, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "validation: id=0 rejected");

    // id = -1
    rule = MakeRule(-1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0);
    rc = aura_alert_engine_add_rule(engine, &rule, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "validation: id=-1 rejected");

    // invalid metric (6)
    rule = MakeRule(1, 6, AURA_COMPARATOR_ABOVE, 90.0);
    rc = aura_alert_engine_add_rule(engine, &rule, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "validation: metric=6 rejected");

    // invalid comparator (2)
    rule = MakeRule(2, AURA_METRIC_CPU_PERCENT, 2, 90.0);
    rc = aura_alert_engine_add_rule(engine, &rule, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "validation: comparator=2 rejected");

    // NaN threshold
    rule = MakeRule(3, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, std::numeric_limits<double>::quiet_NaN());
    rc = aura_alert_engine_add_rule(engine, &rule, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "validation: NaN threshold rejected");

    // negative sustained
    rule = MakeRule(4, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, -1.0);
    rc = aura_alert_engine_add_rule(engine, &rule, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "validation: negative sustained rejected");

    // negative cooldown
    rule = MakeRule(5, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0, -1.0);
    rc = aura_alert_engine_add_rule(engine, &rule, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "validation: negative cooldown rejected");

    aura_alert_engine_destroy(engine);
}

// ---------------------------------------------------------------------------
// 3. Evaluation basics (5)
// ---------------------------------------------------------------------------

void TestAlertEvalBelowThresholdIdle() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    aura_snapshot_t snap = MakeSnapshot(100.0, 50.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_IDLE, "below threshold: should stay IDLE");

    aura_alert_engine_destroy(engine);
}

void TestAlertEvalInstantTrigger() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    aura_snapshot_t snap = MakeSnapshot(100.0, 95.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_TRIGGERED, "instant trigger: should be TRIGGERED");
    ExpectNear(status.triggered_at, 100.0, 1e-9, "instant trigger: triggered_at");

    aura_alert_engine_destroy(engine);
}

void TestAlertEvalPending() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 5.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    aura_snapshot_t snap = MakeSnapshot(100.0, 95.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_PENDING, "pending: should be PENDING, not TRIGGERED");

    aura_alert_engine_destroy(engine);
}

void TestAlertEvalBelowComparator() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_MEMORY_PERCENT, AURA_COMPARATOR_BELOW, 20.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    // Value below threshold -> triggers
    aura_snapshot_t snap = MakeSnapshot(100.0, 50.0, 10.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_TRIGGERED, "below comparator: should trigger when value < threshold");

    // Value above threshold -> clears
    snap = MakeSnapshot(101.0, 50.0, 50.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_IDLE, "below comparator: should clear when value > threshold");

    aura_alert_engine_destroy(engine);
}

void TestAlertEvalNullPointers() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_snapshot_t snap = MakeSnapshot(100.0, 50.0, 40.0);

    int rc = aura_alert_engine_evaluate(nullptr, &snap, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null engine: should fail");

    rc = aura_alert_engine_evaluate(engine, nullptr, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null snapshot: should fail");

    aura_alert_engine_destroy(engine);
}

// ---------------------------------------------------------------------------
// 4. State transitions (8)
// ---------------------------------------------------------------------------

void TestAlertFullCycle() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 5.0, 10.0);
    aura_alert_engine_add_rule(engine, &rule, &error);
    aura_alert_status_t status{};

    // Phase 1: condition met -> PENDING
    aura_snapshot_t snap = MakeSnapshot(100.0, 95.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_PENDING, "full cycle: phase 1 PENDING");

    // Phase 2: sustained elapsed -> TRIGGERED
    snap = MakeSnapshot(106.0, 95.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_TRIGGERED, "full cycle: phase 2 TRIGGERED");
    ExpectNear(status.triggered_at, 106.0, 1e-9, "full cycle: triggered_at");

    // Phase 3: condition clears -> COOLDOWN
    snap = MakeSnapshot(107.0, 50.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_COOLDOWN, "full cycle: phase 3 COOLDOWN");

    // Phase 4: cooldown elapsed -> IDLE
    snap = MakeSnapshot(118.0, 50.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_IDLE, "full cycle: phase 4 IDLE");

    aura_alert_engine_destroy(engine);
}

void TestAlertPendingClearsEarly() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 5.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    // Condition met -> PENDING
    aura_snapshot_t snap = MakeSnapshot(100.0, 95.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_PENDING, "pending clears early: was PENDING");

    // Condition clears before sustained period -> IDLE
    snap = MakeSnapshot(102.0, 50.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_IDLE, "pending clears early: back to IDLE");

    aura_alert_engine_destroy(engine);
}

void TestAlertTriggeredStays() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    aura_snapshot_t snap = MakeSnapshot(100.0, 95.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_TRIGGERED, "stays triggered: initial");

    // Higher value -> still triggered, peak updates
    snap = MakeSnapshot(101.0, 98.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_TRIGGERED, "stays triggered: still triggered");
    ExpectNear(status.peak_value, 98.0, 1e-9, "stays triggered: peak should be 98");

    aura_alert_engine_destroy(engine);
}

void TestAlertCooldownIgnoresCondition() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0, 10.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    // Trigger
    aura_snapshot_t snap = MakeSnapshot(100.0, 95.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    // Clear -> COOLDOWN
    snap = MakeSnapshot(101.0, 50.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_COOLDOWN, "cooldown ignores: in COOLDOWN");

    // Condition met during cooldown -> should NOT re-trigger
    snap = MakeSnapshot(105.0, 95.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_COOLDOWN, "cooldown ignores: still in COOLDOWN");

    aura_alert_engine_destroy(engine);
}

void TestAlertZeroCooldown() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    // Trigger
    aura_snapshot_t snap = MakeSnapshot(100.0, 95.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    // Clear -> should go directly to IDLE (no cooldown)
    snap = MakeSnapshot(101.0, 50.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_IDLE, "zero cooldown: direct to IDLE");

    aura_alert_engine_destroy(engine);
}

void TestAlertZeroSustained() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    // First evaluation above threshold -> immediate trigger
    aura_snapshot_t snap = MakeSnapshot(100.0, 91.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_TRIGGERED, "zero sustained: instant trigger");

    aura_alert_engine_destroy(engine);
}

void TestAlertAcknowledgeTriggered() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0, 10.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    // Trigger
    aura_snapshot_t snap = MakeSnapshot(100.0, 95.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    // Acknowledge
    int rc = aura_alert_engine_acknowledge(engine, 1, &error);
    ExpectEq(rc, AURA_OK, "acknowledge: should succeed");

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.acknowledged, 1, "acknowledge: acknowledged flag set");

    aura_alert_engine_destroy(engine);
}

void TestAlertAcknowledgeNonTriggered() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    // Try to acknowledge while IDLE
    int rc = aura_alert_engine_acknowledge(engine, 1, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "acknowledge non-triggered: should fail");

    aura_alert_engine_destroy(engine);
}

// ---------------------------------------------------------------------------
// 5. Value tracking (4)
// ---------------------------------------------------------------------------

void TestAlertPeakAbove() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 50.0, 0.0, 60.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    aura_snapshot_t snap1 = MakeSnapshot(100.0, 60.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap1, &error);
    aura_snapshot_t snap2 = MakeSnapshot(101.0, 80.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap2, &error);
    aura_snapshot_t snap3 = MakeSnapshot(102.0, 70.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap3, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectNear(status.peak_value, 80.0, 1e-9, "peak above: max should be 80");

    aura_alert_engine_destroy(engine);
}

void TestAlertPeakBelow() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_MEMORY_PERCENT, AURA_COMPARATOR_BELOW, 50.0, 0.0, 60.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    aura_snapshot_t snap1 = MakeSnapshot(100.0, 50.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap1, &error);
    aura_snapshot_t snap2 = MakeSnapshot(101.0, 50.0, 20.0);
    aura_alert_engine_evaluate(engine, &snap2, &error);
    aura_snapshot_t snap3 = MakeSnapshot(102.0, 50.0, 30.0);
    aura_alert_engine_evaluate(engine, &snap3, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectNear(status.peak_value, 20.0, 1e-9, "peak below: min should be 20");

    aura_alert_engine_destroy(engine);
}

void TestAlertLastValue() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    aura_snapshot_t snap = MakeSnapshot(100.0, 50.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectNear(status.last_value, 50.0, 1e-9, "last value: first eval");

    snap = MakeSnapshot(101.0, 75.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectNear(status.last_value, 75.0, 1e-9, "last value: second eval");

    aura_alert_engine_destroy(engine);
}

void TestAlertDuration() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 50.0, 0.0, 60.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    // Trigger at t=100
    aura_snapshot_t snap = MakeSnapshot(100.0, 60.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    // Check at t=105
    snap = MakeSnapshot(105.0, 60.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectNear(status.duration, 5.0, 1e-9, "duration: should be 5 seconds");

    // Check at t=110
    snap = MakeSnapshot(110.0, 60.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectNear(status.duration, 10.0, 1e-9, "duration: should be 10 seconds");

    aura_alert_engine_destroy(engine);
}

// ---------------------------------------------------------------------------
// 6. Metric extraction (6)
// ---------------------------------------------------------------------------

void TestAlertMetricCpu() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 50.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    // Only cpu is above threshold; everything else is 0
    aura_snapshot_t snap = MakeSnapshot(100.0, 60.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_TRIGGERED, "metric cpu: triggered");

    aura_alert_engine_destroy(engine);
}

void TestAlertMetricMemory() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_MEMORY_PERCENT, AURA_COMPARATOR_ABOVE, 50.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    aura_snapshot_t snap = MakeSnapshot(100.0, 0.0, 60.0, 0.0, 0.0, 0.0, 0.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_TRIGGERED, "metric memory: triggered");

    aura_alert_engine_destroy(engine);
}

void TestAlertMetricDiskRead() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_DISK_READ_BPS, AURA_COMPARATOR_ABOVE, 1000.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    aura_snapshot_t snap = MakeSnapshot(100.0, 0.0, 0.0, 5000.0, 0.0, 0.0, 0.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_TRIGGERED, "metric disk_read: triggered");

    aura_alert_engine_destroy(engine);
}

void TestAlertMetricDiskWrite() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_DISK_WRITE_BPS, AURA_COMPARATOR_ABOVE, 1000.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    aura_snapshot_t snap = MakeSnapshot(100.0, 0.0, 0.0, 0.0, 5000.0, 0.0, 0.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_TRIGGERED, "metric disk_write: triggered");

    aura_alert_engine_destroy(engine);
}

void TestAlertMetricNetRecv() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_NET_RECV_BPS, AURA_COMPARATOR_ABOVE, 1000.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    aura_snapshot_t snap = MakeSnapshot(100.0, 0.0, 0.0, 0.0, 0.0, 5000.0, 0.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_TRIGGERED, "metric net_recv: triggered");

    aura_alert_engine_destroy(engine);
}

void TestAlertMetricNetSent() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_NET_SENT_BPS, AURA_COMPARATOR_ABOVE, 1000.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    aura_snapshot_t snap = MakeSnapshot(100.0, 0.0, 0.0, 0.0, 0.0, 0.0, 5000.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_TRIGGERED, "metric net_sent: triggered");

    aura_alert_engine_destroy(engine);
}

// ---------------------------------------------------------------------------
// 7. Edge cases (6)
// ---------------------------------------------------------------------------

void TestAlertNaNThresholdRejected() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE,
                                       std::numeric_limits<double>::quiet_NaN());
    int rc = aura_alert_engine_add_rule(engine, &rule, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "NaN threshold: rejected");

    aura_alert_engine_destroy(engine);
}

void TestAlertExactThresholdNoTrigger() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    // Exactly at threshold -> should NOT trigger (strict >)
    aura_snapshot_t snap = MakeSnapshot(100.0, 90.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_IDLE, "exact threshold: should stay IDLE");

    // Also test BELOW
    aura_alert_rule_t rule2 = MakeRule(2, AURA_METRIC_MEMORY_PERCENT, AURA_COMPARATOR_BELOW, 40.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule2, &error);

    snap = MakeSnapshot(101.0, 90.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_engine_get_status(engine, 2, &status, &error);
    ExpectEq(status.state, AURA_ALERT_IDLE, "exact threshold below: should stay IDLE");

    aura_alert_engine_destroy(engine);
}

void TestAlertZeroBothTimers() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);
    aura_alert_status_t status{};

    // Trigger instantly
    aura_snapshot_t snap = MakeSnapshot(100.0, 95.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_TRIGGERED, "zero both: triggered");

    // Clear instantly (no cooldown)
    snap = MakeSnapshot(101.0, 50.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_IDLE, "zero both: back to IDLE");

    aura_alert_engine_destroy(engine);
}

void TestAlertMultipleRulesIndependent() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t r1 = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0);
    aura_alert_rule_t r2 = MakeRule(2, AURA_METRIC_MEMORY_PERCENT, AURA_COMPARATOR_ABOVE, 80.0, 0.0);
    aura_alert_rule_t r3 = MakeRule(3, AURA_METRIC_DISK_READ_BPS, AURA_COMPARATOR_ABOVE, 1000.0, 0.0);
    aura_alert_engine_add_rule(engine, &r1, &error);
    aura_alert_engine_add_rule(engine, &r2, &error);
    aura_alert_engine_add_rule(engine, &r3, &error);

    // Only CPU is above threshold
    aura_snapshot_t snap = MakeSnapshot(100.0, 95.0, 50.0, 500.0, 0.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_TRIGGERED, "multiple rules: cpu triggered");

    aura_alert_engine_get_status(engine, 2, &status, &error);
    ExpectEq(status.state, AURA_ALERT_IDLE, "multiple rules: mem idle");

    aura_alert_engine_get_status(engine, 3, &status, &error);
    ExpectEq(status.state, AURA_ALERT_IDLE, "multiple rules: disk idle");

    aura_alert_engine_destroy(engine);
}

void TestAlertNaNSnapshotFieldSafe() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    // NaN in cpu field -> condition never met (IEEE 754: NaN > 90 is false)
    aura_snapshot_t snap = MakeSnapshot(100.0, std::numeric_limits<double>::quiet_NaN(), 40.0);
    int rc = aura_alert_engine_evaluate(engine, &snap, &error);
    ExpectEq(rc, AURA_OK, "NaN snapshot: evaluate should succeed");

    aura_alert_status_t status{};
    aura_alert_engine_get_status(engine, 1, &status, &error);
    ExpectEq(status.state, AURA_ALERT_IDLE, "NaN snapshot: should stay IDLE");

    aura_alert_engine_destroy(engine);
}

void TestAlertInfThresholdRejected() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE,
                                       std::numeric_limits<double>::infinity());
    int rc = aura_alert_engine_add_rule(engine, &rule, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "+Inf threshold: rejected");

    rule = MakeRule(2, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE,
                    -std::numeric_limits<double>::infinity());
    rc = aura_alert_engine_add_rule(engine, &rule, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "-Inf threshold: rejected");

    aura_alert_engine_destroy(engine);
}

// ---------------------------------------------------------------------------
// 8. History (4)
// ---------------------------------------------------------------------------

void TestAlertHistoryRecords() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    // Trigger
    aura_snapshot_t snap = MakeSnapshot(100.0, 95.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    // Clear
    snap = MakeSnapshot(101.0, 50.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_event_t events[10]{};
    int count = 0;
    int rc = aura_alert_engine_get_history(engine, events, 10, &count, &error);
    ExpectEq(rc, AURA_OK, "history records: get_history");
    ExpectEq(count, 2, "history records: should have 2 events");

    // First event: triggered
    ExpectEq(events[0].rule_id, 1, "history records: event 0 rule_id");
    ExpectEq(events[0].event_type, 0, "history records: event 0 type = triggered");
    ExpectNear(events[0].timestamp, 100.0, 1e-9, "history records: event 0 timestamp");

    // Second event: cleared
    ExpectEq(events[1].rule_id, 1, "history records: event 1 rule_id");
    ExpectEq(events[1].event_type, 1, "history records: event 1 type = cleared");
    ExpectNear(events[1].timestamp, 101.0, 1e-9, "history records: event 1 timestamp");

    aura_alert_engine_destroy(engine);
}

void TestAlertHistoryClear() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    aura_snapshot_t snap = MakeSnapshot(100.0, 95.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);
    snap = MakeSnapshot(101.0, 50.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    int rc = aura_alert_engine_clear_history(engine, &error);
    ExpectEq(rc, AURA_OK, "history clear: should succeed");

    int count = 0;
    rc = aura_alert_engine_get_history(engine, nullptr, 0, &count, &error);
    ExpectEq(rc, AURA_OK, "history clear: count query");
    ExpectEq(count, 0, "history clear: should be empty");

    aura_alert_engine_destroy(engine);
}

void TestAlertHistoryCapacity() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 50.0, 0.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    // Generate > 1000 events (each trigger+clear = 2 events)
    double t = 100.0;
    for (int i = 0; i < 600; ++i) {
        aura_snapshot_t snap = MakeSnapshot(t, 60.0, 40.0);
        aura_alert_engine_evaluate(engine, &snap, &error);
        t += 1.0;
        snap = MakeSnapshot(t, 40.0, 40.0);
        aura_alert_engine_evaluate(engine, &snap, &error);
        t += 1.0;
    }

    // Should have capped at 1000
    int count = 0;
    aura_alert_engine_get_history(engine, nullptr, 0, &count, &error);
    ExpectTrue(count <= 1000, "history capacity: should not exceed 1000");
    ExpectTrue(count > 0, "history capacity: should have some events");

    aura_alert_engine_destroy(engine);
}

void TestAlertHistoryNullBuffer() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    aura_snapshot_t snap = MakeSnapshot(100.0, 95.0, 40.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    // Query count with capacity=0 and null buffer
    int count = 0;
    int rc = aura_alert_engine_get_history(engine, nullptr, 0, &count, &error);
    ExpectEq(rc, AURA_OK, "history null buffer: should succeed");
    ExpectEq(count, 1, "history null buffer: should report 1 event");

    aura_alert_engine_destroy(engine);
}

// ---------------------------------------------------------------------------
// 9. Active alerts query (2)
// ---------------------------------------------------------------------------

void TestAlertGetActiveEmpty() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0);
    aura_alert_engine_add_rule(engine, &rule, &error);

    aura_alert_status_t statuses[5]{};
    int count = 0;
    int rc = aura_alert_engine_get_active(engine, statuses, 5, &count, &error);
    ExpectEq(rc, AURA_OK, "get active empty: should succeed");
    ExpectEq(count, 0, "get active empty: count should be 0");

    aura_alert_engine_destroy(engine);
}

void TestAlertGetActiveMultiple() {
    aura_error_t error{};
    aura_alert_engine_t* engine = nullptr;
    aura_alert_engine_create(&engine, &error);

    aura_alert_rule_t r1 = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0, 60.0);
    aura_alert_rule_t r2 = MakeRule(2, AURA_METRIC_MEMORY_PERCENT, AURA_COMPARATOR_ABOVE, 80.0, 0.0, 60.0);
    aura_alert_rule_t r3 = MakeRule(3, AURA_METRIC_DISK_READ_BPS, AURA_COMPARATOR_ABOVE, 1000.0, 0.0, 60.0);
    aura_alert_engine_add_rule(engine, &r1, &error);
    aura_alert_engine_add_rule(engine, &r2, &error);
    aura_alert_engine_add_rule(engine, &r3, &error);

    // Trigger rules 1 and 2 (CPU=95 > 90, mem=85 > 80, disk=500 < 1000)
    aura_snapshot_t snap = MakeSnapshot(100.0, 95.0, 85.0, 500.0, 0.0);
    aura_alert_engine_evaluate(engine, &snap, &error);

    aura_alert_status_t statuses[5]{};
    int count = 0;
    int rc = aura_alert_engine_get_active(engine, statuses, 5, &count, &error);
    ExpectEq(rc, AURA_OK, "get active multiple: should succeed");
    ExpectEq(count, 2, "get active multiple: 2 active alerts");

    aura_alert_engine_destroy(engine);
}

// ---------------------------------------------------------------------------
// 10. C ABI safety (2)
// ---------------------------------------------------------------------------

void TestAlertAbiNullEngine() {
    aura_error_t error{};
    int rc;

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0);
    rc = aura_alert_engine_add_rule(nullptr, &rule, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null engine: add_rule");

    rc = aura_alert_engine_remove_rule(nullptr, 1, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null engine: remove_rule");

    aura_alert_rule_t out_rule{};
    rc = aura_alert_engine_get_rule(nullptr, 1, &out_rule, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null engine: get_rule");

    int count = 0;
    rc = aura_alert_engine_rule_count(nullptr, &count, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null engine: rule_count");

    aura_snapshot_t snap = MakeSnapshot(100.0, 50.0, 40.0);
    rc = aura_alert_engine_evaluate(nullptr, &snap, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null engine: evaluate");

    aura_alert_status_t status{};
    rc = aura_alert_engine_get_status(nullptr, 1, &status, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null engine: get_status");

    aura_alert_status_t statuses[5]{};
    rc = aura_alert_engine_get_active(nullptr, statuses, 5, &count, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null engine: get_active");

    rc = aura_alert_engine_acknowledge(nullptr, 1, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null engine: acknowledge");

    aura_alert_event_t events[5]{};
    rc = aura_alert_engine_get_history(nullptr, events, 5, &count, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null engine: get_history");

    rc = aura_alert_engine_clear_history(nullptr, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null engine: clear_history");
}

void TestAlertAbiNullErrorSafe() {
    // Valid call with null error pointer should succeed
    aura_alert_engine_t* engine = nullptr;
    int rc = aura_alert_engine_create(&engine, nullptr);
    ExpectEq(rc, AURA_OK, "null error safe: create succeeds");

    aura_alert_rule_t rule = MakeRule(1, AURA_METRIC_CPU_PERCENT, AURA_COMPARATOR_ABOVE, 90.0, 0.0);
    rc = aura_alert_engine_add_rule(engine, &rule, nullptr);
    ExpectEq(rc, AURA_OK, "null error safe: add_rule succeeds");

    aura_snapshot_t snap = MakeSnapshot(100.0, 95.0, 40.0);
    rc = aura_alert_engine_evaluate(engine, &snap, nullptr);
    ExpectEq(rc, AURA_OK, "null error safe: evaluate succeeds");

    // Invalid call with null error pointer should return error code without crash
    rc = aura_alert_engine_evaluate(nullptr, &snap, nullptr);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null error safe: invalid call returns error");

    aura_alert_engine_destroy(engine);
}
