#include "aura_platform.h"
#include "alert_engine.hpp"
#include "c_api_helpers.hpp"

#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <vector>

using aura::platform::AlertComparator;
using aura::platform::AlertEngine;
using aura::platform::AlertEvent;
using aura::platform::AlertEventType;
using aura::platform::AlertMetric;
using aura::platform::AlertRule;
using aura::platform::AlertState;
using aura::platform::AlertStatus;

namespace {

struct AuraAlertEngine {
    explicit AuraAlertEngine(std::unique_ptr<AlertEngine> engine_in)
        : engine(std::move(engine_in)) {}

    std::unique_ptr<AlertEngine> engine;
};

// ---------------------------------------------------------------------------
// ABI conversion helpers
// ---------------------------------------------------------------------------

AlertRule ToInternalRule(const aura_alert_rule_t& abi) {
    AlertRule rule;
    rule.id = abi.id;
    rule.metric = static_cast<AlertMetric>(abi.metric);
    rule.comparator = static_cast<AlertComparator>(abi.comparator);
    rule.threshold = abi.threshold;
    rule.sustained_seconds = abi.sustained_seconds;
    rule.cooldown_seconds = abi.cooldown_seconds;
    rule.name = std::string(abi.name, strnlen(abi.name, sizeof(abi.name)));
    return rule;
}

aura_alert_rule_t ToAbiRule(const AlertRule& rule) {
    aura_alert_rule_t out{};
    out.id = rule.id;
    out.metric = static_cast<int>(rule.metric);
    out.comparator = static_cast<int>(rule.comparator);
    out.threshold = rule.threshold;
    out.sustained_seconds = rule.sustained_seconds;
    out.cooldown_seconds = rule.cooldown_seconds;
    std::memset(out.name, 0, sizeof(out.name));
#ifdef _WIN32
    strncpy_s(out.name, sizeof(out.name), rule.name.c_str(), _TRUNCATE);
#else
    std::strncpy(out.name, rule.name.c_str(), sizeof(out.name) - 1);
    out.name[sizeof(out.name) - 1] = '\0';
#endif
    return out;
}

aura_alert_status_t ToAbiStatus(const AlertStatus& status) {
    aura_alert_status_t out{};
    out.rule_id = status.rule_id;
    out.state = static_cast<int>(status.state);
    out.last_value = status.last_value;
    out.peak_value = status.peak_value;
    out.triggered_at = status.triggered_at;
    out.duration = status.duration;
    out.acknowledged = status.acknowledged ? 1 : 0;
    return out;
}

aura_alert_event_t ToAbiEvent(const AlertEvent& event) {
    aura_alert_event_t out{};
    out.rule_id = event.rule_id;
    out.event_type = static_cast<int>(event.event_type);
    out.timestamp = event.timestamp;
    out.value = event.value;
    return out;
}

} // namespace

extern "C" {

AURA_PLATFORM_EXPORT int aura_alert_engine_create(
    aura_alert_engine_t** out_engine,
    aura_error_t* out_error
) {
    if (out_engine == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_engine must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto wrapped = std::make_unique<AuraAlertEngine>(
            std::make_unique<AlertEngine>()
        );
        *out_engine = reinterpret_cast<aura_alert_engine_t*>(wrapped.release());
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_alert_engine_destroy(
    aura_alert_engine_t* engine
) {
    if (engine == nullptr) {
        return AURA_OK;
    }

    auto* typed = reinterpret_cast<AuraAlertEngine*>(engine);
    delete typed;
    return AURA_OK;
}

AURA_PLATFORM_EXPORT int aura_alert_engine_add_rule(
    aura_alert_engine_t* engine,
    const aura_alert_rule_t* rule,
    aura_error_t* out_error
) {
    if (engine == nullptr || rule == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "engine and rule must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed = reinterpret_cast<AuraAlertEngine*>(engine);
        typed->engine->AddRule(ToInternalRule(*rule));
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::invalid_argument& exc) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, exc.what());
        return AURA_ERR_INVALID_ARGUMENT;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_alert_engine_remove_rule(
    aura_alert_engine_t* engine,
    int rule_id,
    aura_error_t* out_error
) {
    if (engine == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "engine must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed = reinterpret_cast<AuraAlertEngine*>(engine);
        typed->engine->RemoveRule(rule_id);
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::invalid_argument& exc) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, exc.what());
        return AURA_ERR_INVALID_ARGUMENT;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_alert_engine_get_rule(
    aura_alert_engine_t* engine,
    int rule_id,
    aura_alert_rule_t* out_rule,
    aura_error_t* out_error
) {
    if (engine == nullptr || out_rule == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "engine and out_rule must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed = reinterpret_cast<AuraAlertEngine*>(engine);
        const AlertRule rule = typed->engine->GetRule(rule_id);
        *out_rule = ToAbiRule(rule);
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::invalid_argument& exc) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, exc.what());
        return AURA_ERR_INVALID_ARGUMENT;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_alert_engine_rule_count(
    aura_alert_engine_t* engine,
    int* out_count,
    aura_error_t* out_error
) {
    if (engine == nullptr || out_count == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "engine and out_count must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed = reinterpret_cast<AuraAlertEngine*>(engine);
        *out_count = typed->engine->RuleCount();
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_alert_engine_evaluate(
    aura_alert_engine_t* engine,
    const aura_snapshot_t* snapshot,
    aura_error_t* out_error
) {
    if (engine == nullptr || snapshot == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "engine and snapshot must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed = reinterpret_cast<AuraAlertEngine*>(engine);
        typed->engine->Evaluate(ToInternalSnapshot(*snapshot));
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_alert_engine_get_status(
    aura_alert_engine_t* engine,
    int rule_id,
    aura_alert_status_t* out_status,
    aura_error_t* out_error
) {
    if (engine == nullptr || out_status == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "engine and out_status must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed = reinterpret_cast<AuraAlertEngine*>(engine);
        const AlertStatus status = typed->engine->GetStatus(rule_id);
        *out_status = ToAbiStatus(status);
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::invalid_argument& exc) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, exc.what());
        return AURA_ERR_INVALID_ARGUMENT;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_alert_engine_get_active(
    aura_alert_engine_t* engine,
    aura_alert_status_t* out_statuses,
    int out_capacity,
    int* out_count,
    aura_error_t* out_error
) {
    if (engine == nullptr || out_count == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "engine and out_count must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    if (out_capacity < 0) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_capacity must be >= 0.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed = reinterpret_cast<AuraAlertEngine*>(engine);
        const std::vector<AlertStatus> active = typed->engine->GetActiveAlerts();

        if (static_cast<int>(active.size()) > out_capacity) {
            SetError(out_error, AURA_ERR_CAPACITY, "Output buffer capacity is too small.");
            return AURA_ERR_CAPACITY;
        }

        if (!active.empty() && out_statuses == nullptr) {
            SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_statuses must not be null when results are present.");
            return AURA_ERR_INVALID_ARGUMENT;
        }

        for (std::size_t i = 0; i < active.size(); ++i) {
            out_statuses[i] = ToAbiStatus(active[i]);
        }
        *out_count = static_cast<int>(active.size());
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_alert_engine_acknowledge(
    aura_alert_engine_t* engine,
    int rule_id,
    aura_error_t* out_error
) {
    if (engine == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "engine must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed = reinterpret_cast<AuraAlertEngine*>(engine);
        typed->engine->Acknowledge(rule_id);
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::invalid_argument& exc) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, exc.what());
        return AURA_ERR_INVALID_ARGUMENT;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_alert_engine_get_history(
    aura_alert_engine_t* engine,
    aura_alert_event_t* out_events,
    int out_capacity,
    int* out_count,
    aura_error_t* out_error
) {
    if (engine == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "engine must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed = reinterpret_cast<AuraAlertEngine*>(engine);
        const std::vector<AlertEvent> history = typed->engine->GetHistory();

        // Allow querying count with capacity=0 and null buffer
        if (out_count != nullptr && out_capacity == 0) {
            *out_count = static_cast<int>(history.size());
            ClearError(out_error);
            return AURA_OK;
        }

        if (out_count == nullptr) {
            SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_count must not be null.");
            return AURA_ERR_INVALID_ARGUMENT;
        }

        if (out_capacity < 0) {
            SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_capacity must be >= 0.");
            return AURA_ERR_INVALID_ARGUMENT;
        }

        if (static_cast<int>(history.size()) > out_capacity) {
            SetError(out_error, AURA_ERR_CAPACITY, "Output buffer capacity is too small.");
            return AURA_ERR_CAPACITY;
        }

        if (!history.empty() && out_events == nullptr) {
            SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_events must not be null when results are present.");
            return AURA_ERR_INVALID_ARGUMENT;
        }

        for (std::size_t i = 0; i < history.size(); ++i) {
            out_events[i] = ToAbiEvent(history[i]);
        }
        *out_count = static_cast<int>(history.size());
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_alert_engine_clear_history(
    aura_alert_engine_t* engine,
    aura_error_t* out_error
) {
    if (engine == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "engine must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed = reinterpret_cast<AuraAlertEngine*>(engine);
        typed->engine->ClearHistory();
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

} // extern "C"
