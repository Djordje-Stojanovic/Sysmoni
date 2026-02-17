#include "alert_engine.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace aura::platform {

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

static bool IsFinite(double v) {
    return std::isfinite(v);
}

static void ValidateRule(const AlertRule& rule) {
    if (rule.id <= 0) {
        throw std::invalid_argument("rule id must be > 0");
    }
    if (static_cast<int>(rule.metric) < 0 || static_cast<int>(rule.metric) > 5) {
        throw std::invalid_argument("invalid alert metric");
    }
    if (static_cast<int>(rule.comparator) < 0 || static_cast<int>(rule.comparator) > 1) {
        throw std::invalid_argument("invalid alert comparator");
    }
    if (!IsFinite(rule.threshold)) {
        throw std::invalid_argument("threshold must be finite");
    }
    if (!IsFinite(rule.sustained_seconds) || rule.sustained_seconds < 0.0) {
        throw std::invalid_argument("sustained_seconds must be >= 0 and finite");
    }
    if (!IsFinite(rule.cooldown_seconds) || rule.cooldown_seconds < 0.0) {
        throw std::invalid_argument("cooldown_seconds must be >= 0 and finite");
    }
}

// ---------------------------------------------------------------------------
// Metric extraction
// ---------------------------------------------------------------------------

double AlertEngine::ExtractMetric(const Snapshot& snapshot, AlertMetric metric) {
    switch (metric) {
        case AlertMetric::CpuPercent:    return snapshot.cpu_percent;
        case AlertMetric::MemoryPercent: return snapshot.memory_percent;
        case AlertMetric::DiskReadBps:   return snapshot.disk_read_bps;
        case AlertMetric::DiskWriteBps:  return snapshot.disk_write_bps;
        case AlertMetric::NetRecvBps:    return snapshot.net_recv_bps;
        case AlertMetric::NetSentBps:    return snapshot.net_sent_bps;
    }
    throw std::runtime_error("unknown alert metric");
}

// ---------------------------------------------------------------------------
// Condition evaluation
// ---------------------------------------------------------------------------

bool AlertEngine::ConditionMet(double value, AlertComparator comparator, double threshold) {
    // NaN comparisons always return false per IEEE 754, which is the safe behavior.
    switch (comparator) {
        case AlertComparator::Above: return value > threshold;
        case AlertComparator::Below: return value < threshold;
    }
    throw std::runtime_error("unknown alert comparator");
}

// ---------------------------------------------------------------------------
// Peak value tracking
// ---------------------------------------------------------------------------

void AlertEngine::UpdatePeakValue(RuleState& rs, double value) const {
    if (rs.rule.comparator == AlertComparator::Below) {
        rs.peak_value = std::min(rs.peak_value, value);
    } else {
        rs.peak_value = std::max(rs.peak_value, value);
    }
}

// ---------------------------------------------------------------------------
// History management
// ---------------------------------------------------------------------------

void AlertEngine::RecordEvent(int rule_id, AlertEventType type, double timestamp, double value) {
    AlertEvent event;
    event.rule_id = rule_id;
    event.event_type = type;
    event.timestamp = timestamp;
    event.value = value;

    if (static_cast<int>(history_.size()) >= kMaxHistoryEvents) {
        history_.erase(history_.begin());
    }
    history_.push_back(event);
}

// ---------------------------------------------------------------------------
// Rule management
// ---------------------------------------------------------------------------

void AlertEngine::AddRule(const AlertRule& rule) {
    std::lock_guard<std::mutex> lock(mu_);

    ValidateRule(rule);

    if (rules_.find(rule.id) != rules_.end()) {
        throw std::invalid_argument("duplicate rule id: " + std::to_string(rule.id));
    }

    RuleState rs;
    rs.rule = rule;
    rules_.emplace(rule.id, std::move(rs));
}

void AlertEngine::RemoveRule(int rule_id) {
    std::lock_guard<std::mutex> lock(mu_);

    auto it = rules_.find(rule_id);
    if (it == rules_.end()) {
        throw std::invalid_argument("rule not found: " + std::to_string(rule_id));
    }
    rules_.erase(it);
}

AlertRule AlertEngine::GetRule(int rule_id) const {
    std::lock_guard<std::mutex> lock(mu_);

    auto it = rules_.find(rule_id);
    if (it == rules_.end()) {
        throw std::invalid_argument("rule not found: " + std::to_string(rule_id));
    }
    return it->second.rule;
}

int AlertEngine::RuleCount() const {
    std::lock_guard<std::mutex> lock(mu_);
    return static_cast<int>(rules_.size());
}

// ---------------------------------------------------------------------------
// Evaluation — core state machine
// ---------------------------------------------------------------------------

void AlertEngine::Evaluate(const Snapshot& snapshot) {
    std::lock_guard<std::mutex> lock(mu_);

    const double now = snapshot.timestamp;

    for (auto& [rule_id, rs] : rules_) {
        const double value = ExtractMetric(snapshot, rs.rule.metric);
        rs.last_value = value;
        rs.last_eval_time = now;

        const bool condition = ConditionMet(value, rs.rule.comparator, rs.rule.threshold);

        switch (rs.state) {
            case AlertState::Idle: {
                if (condition) {
                    if (rs.rule.sustained_seconds <= 0.0) {
                        // Instant trigger
                        rs.state = AlertState::Triggered;
                        rs.triggered_at = now;
                        rs.peak_value = value;
                        rs.acknowledged = false;
                        RecordEvent(rule_id, AlertEventType::Triggered, now, value);
                    } else {
                        rs.state = AlertState::Pending;
                        rs.pending_since = now;
                        rs.peak_value = value;
                    }
                }
                break;
            }

            case AlertState::Pending: {
                if (condition) {
                    UpdatePeakValue(rs, value);
                    const double elapsed = std::max(0.0, now - rs.pending_since);
                    if (elapsed >= rs.rule.sustained_seconds) {
                        rs.state = AlertState::Triggered;
                        rs.triggered_at = now;
                        rs.acknowledged = false;
                        RecordEvent(rule_id, AlertEventType::Triggered, now, value);
                    }
                } else {
                    // Condition cleared before sustained period
                    rs.state = AlertState::Idle;
                    rs.pending_since = 0.0;
                    rs.peak_value = 0.0;
                }
                break;
            }

            case AlertState::Triggered: {
                if (condition) {
                    UpdatePeakValue(rs, value);
                } else {
                    // Condition cleared
                    if (rs.rule.cooldown_seconds <= 0.0) {
                        rs.state = AlertState::Idle;
                        RecordEvent(rule_id, AlertEventType::Cleared, now, value);
                        rs.triggered_at = 0.0;
                        rs.peak_value = 0.0;
                        rs.acknowledged = false;
                    } else {
                        rs.state = AlertState::Cooldown;
                        rs.cooldown_since = now;
                        RecordEvent(rule_id, AlertEventType::Cleared, now, value);
                    }
                }
                break;
            }

            case AlertState::Cooldown: {
                const double cooldown_elapsed = std::max(0.0, now - rs.cooldown_since);
                if (cooldown_elapsed >= rs.rule.cooldown_seconds) {
                    rs.state = AlertState::Idle;
                    rs.triggered_at = 0.0;
                    rs.peak_value = 0.0;
                    rs.acknowledged = false;
                }
                // Ignore condition during cooldown to prevent flapping
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Status queries
// ---------------------------------------------------------------------------

AlertStatus AlertEngine::GetStatus(int rule_id) const {
    std::lock_guard<std::mutex> lock(mu_);

    auto it = rules_.find(rule_id);
    if (it == rules_.end()) {
        throw std::invalid_argument("rule not found: " + std::to_string(rule_id));
    }

    const RuleState& rs = it->second;

    AlertStatus status;
    status.rule_id = rule_id;
    status.state = rs.state;
    status.last_value = rs.last_value;
    status.peak_value = rs.peak_value;
    status.triggered_at = rs.triggered_at;
    status.acknowledged = rs.acknowledged;

    if (rs.state == AlertState::Triggered && rs.triggered_at > 0.0) {
        status.duration = std::max(0.0, rs.last_eval_time - rs.triggered_at);
    } else {
        status.duration = 0.0;
    }

    return status;
}

std::vector<AlertStatus> AlertEngine::GetActiveAlerts() const {
    std::lock_guard<std::mutex> lock(mu_);

    std::vector<AlertStatus> active;
    for (const auto& [rule_id, rs] : rules_) {
        if (rs.state == AlertState::Triggered) {
            AlertStatus status;
            status.rule_id = rule_id;
            status.state = rs.state;
            status.last_value = rs.last_value;
            status.peak_value = rs.peak_value;
            status.triggered_at = rs.triggered_at;
            status.acknowledged = rs.acknowledged;
            if (rs.triggered_at > 0.0) {
                status.duration = std::max(0.0, rs.last_eval_time - rs.triggered_at);
            } else {
                status.duration = 0.0;
            }
            active.push_back(status);
        }
    }
    return active;
}

// ---------------------------------------------------------------------------
// Acknowledgment
// ---------------------------------------------------------------------------

void AlertEngine::Acknowledge(int rule_id) {
    std::lock_guard<std::mutex> lock(mu_);

    auto it = rules_.find(rule_id);
    if (it == rules_.end()) {
        throw std::invalid_argument("rule not found: " + std::to_string(rule_id));
    }

    RuleState& rs = it->second;
    if (rs.state != AlertState::Triggered) {
        throw std::invalid_argument("can only acknowledge TRIGGERED alerts");
    }

    rs.acknowledged = true;
    RecordEvent(rule_id, AlertEventType::Acknowledged, rs.last_eval_time, rs.last_value);
}

// ---------------------------------------------------------------------------
// History
// ---------------------------------------------------------------------------

std::vector<AlertEvent> AlertEngine::GetHistory() const {
    std::lock_guard<std::mutex> lock(mu_);
    return history_;
}

void AlertEngine::ClearHistory() {
    std::lock_guard<std::mutex> lock(mu_);
    history_.clear();
}

} // namespace aura::platform
