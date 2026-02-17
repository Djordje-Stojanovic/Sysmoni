#pragma once

#include "platform_internal.hpp"

#include <cmath>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace aura::platform {

enum class AlertMetric {
    CpuPercent = 0,
    MemoryPercent = 1,
    DiskReadBps = 2,
    DiskWriteBps = 3,
    NetRecvBps = 4,
    NetSentBps = 5,
};

enum class AlertComparator {
    Above = 0,
    Below = 1,
};

enum class AlertState {
    Idle = 0,
    Pending = 1,
    Triggered = 2,
    Cooldown = 3,
};

enum class AlertEventType {
    Triggered = 0,
    Cleared = 1,
    Acknowledged = 2,
};

struct AlertRule {
    int id = 0;
    AlertMetric metric = AlertMetric::CpuPercent;
    AlertComparator comparator = AlertComparator::Above;
    double threshold = 0.0;
    double sustained_seconds = 0.0;
    double cooldown_seconds = 0.0;
    std::string name;
};

struct AlertStatus {
    int rule_id = 0;
    AlertState state = AlertState::Idle;
    double last_value = 0.0;
    double peak_value = 0.0;
    double triggered_at = 0.0;
    double duration = 0.0;
    bool acknowledged = false;
};

struct AlertEvent {
    int rule_id = 0;
    AlertEventType event_type = AlertEventType::Triggered;
    double timestamp = 0.0;
    double value = 0.0;
};

class AlertEngine {
public:
    static constexpr int kMaxHistoryEvents = 1000;

    AlertEngine() = default;
    ~AlertEngine() = default;

    AlertEngine(const AlertEngine&) = delete;
    AlertEngine& operator=(const AlertEngine&) = delete;

    void AddRule(const AlertRule& rule);
    void RemoveRule(int rule_id);
    AlertRule GetRule(int rule_id) const;
    int RuleCount() const;

    void Evaluate(const Snapshot& snapshot);

    AlertStatus GetStatus(int rule_id) const;
    std::vector<AlertStatus> GetActiveAlerts() const;
    void Acknowledge(int rule_id);

    std::vector<AlertEvent> GetHistory() const;
    void ClearHistory();

private:
    struct RuleState {
        AlertRule rule;
        AlertState state = AlertState::Idle;
        double pending_since = 0.0;
        double triggered_at = 0.0;
        double cooldown_since = 0.0;
        double last_value = 0.0;
        double peak_value = 0.0;
        double last_eval_time = 0.0;
        bool acknowledged = false;
    };

    static double ExtractMetric(const Snapshot& snapshot, AlertMetric metric);
    static bool ConditionMet(double value, AlertComparator comparator, double threshold);
    void UpdatePeakValue(RuleState& rs, double value) const;
    void RecordEvent(int rule_id, AlertEventType type, double timestamp, double value);

    mutable std::mutex mu_;
    std::unordered_map<int, RuleState> rules_;
    std::vector<AlertEvent> history_;
};

} // namespace aura::platform
