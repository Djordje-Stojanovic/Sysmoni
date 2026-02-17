#include "aura_shell/analytics_bridge.hpp"

#include "platform_dll_helpers.hpp"

#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "aura_platform.h"
#endif

namespace aura::shell {

namespace {
constexpr int kStatusOk = 0;
}  // namespace

struct AnalyticsBridge::Impl {
#ifdef _WIN32
    // Health score
    using HealthScoreComputeFn = int (*)(const aura_snapshot_t*, aura_health_score_t*, aura_error_t*);

    // Trend detection
    using TrendDetectFn = int (*)(const aura_snapshot_t*, int, int, double, aura_trend_result_t*, aura_error_t*);

    // EMA smoother
    using SmootherCreateFn = int (*)(double, aura_smoother_t**, aura_error_t*);
    using SmootherUpdateFn = int (*)(aura_smoother_t*, const aura_snapshot_t*, aura_snapshot_t*, aura_error_t*);
    using SmootherResetFn = int (*)(aura_smoother_t*, aura_error_t*);
    using SmootherDestroyFn = int (*)(aura_smoother_t*);

    // Alert engine
    using AlertEngineCreateFn = int (*)(aura_alert_engine_t**, aura_error_t*);
    using AlertEngineDestroyFn = int (*)(aura_alert_engine_t*);
    using AlertEngineAddRuleFn = int (*)(aura_alert_engine_t*, const aura_alert_rule_t*, aura_error_t*);
    using AlertEngineEvaluateFn = int (*)(aura_alert_engine_t*, const aura_snapshot_t*, aura_error_t*);
    using AlertEngineGetActiveFn = int (*)(aura_alert_engine_t*, aura_alert_status_t*, int, int*, aura_error_t*);
    using AlertEngineAcknowledgeFn = int (*)(aura_alert_engine_t*, int, aura_error_t*);

    // DVR stats & export
    using DvrComputeStatsFn = int (*)(aura_store_t*, int, double, int, double, aura_stats_result_t*, aura_error_t*);
    using DvrExportJsonFn = int (*)(aura_store_t*, int, double, int, double, int, const char*, aura_error_t*);
    using DvrExportCsvFn = int (*)(aura_store_t*, int, double, int, double, const char*, aura_error_t*);

    HMODULE module_handle{nullptr};

    // Function pointers
    HealthScoreComputeFn health_score_compute_fn{nullptr};
    TrendDetectFn trend_detect_fn{nullptr};
    SmootherCreateFn smoother_create_fn{nullptr};
    SmootherUpdateFn smoother_update_fn{nullptr};
    SmootherResetFn smoother_reset_fn{nullptr};
    SmootherDestroyFn smoother_destroy_fn{nullptr};
    AlertEngineCreateFn alert_engine_create_fn{nullptr};
    AlertEngineDestroyFn alert_engine_destroy_fn{nullptr};
    AlertEngineAddRuleFn alert_engine_add_rule_fn{nullptr};
    AlertEngineEvaluateFn alert_engine_evaluate_fn{nullptr};
    AlertEngineGetActiveFn alert_engine_get_active_fn{nullptr};
    AlertEngineAcknowledgeFn alert_engine_acknowledge_fn{nullptr};
    DvrComputeStatsFn dvr_compute_stats_fn{nullptr};
    DvrExportJsonFn dvr_export_json_fn{nullptr};
    DvrExportCsvFn dvr_export_csv_fn{nullptr};

    // Lazily created handles
    aura_smoother_t* smoother_handle{nullptr};
    aura_alert_engine_t* alert_engine_handle{nullptr};
    bool alert_engine_initialized{false};
#endif
    bool loaded{false};
    std::string loaded_path;
    std::string load_error;
};

AnalyticsBridge::AnalyticsBridge() : impl_(std::make_unique<Impl>()) {
#ifdef _WIN32
    using namespace detail;

    DWORD last_error_code = 0;
    for (const auto& candidate : runtime_library_candidates()) {
        const std::wstring path = candidate.wstring();
        HMODULE module = LoadLibraryW(path.c_str());
        if (module == nullptr) {
            last_error_code = GetLastError();
            continue;
        }

        // Health score
        auto* health_compute = reinterpret_cast<Impl::HealthScoreComputeFn>(
            GetProcAddress(module, "aura_health_score_compute"));

        // Trend detection
        auto* trend_detect = reinterpret_cast<Impl::TrendDetectFn>(
            GetProcAddress(module, "aura_trend_detect"));

        // At minimum we need health_score to consider the DLL useful for analytics.
        // Other subsystems are optional.
        if (health_compute == nullptr && trend_detect == nullptr) {
            last_error_code = GetLastError();
            FreeLibrary(module);
            continue;
        }

        impl_->module_handle = module;
        impl_->health_score_compute_fn = health_compute;
        impl_->trend_detect_fn = trend_detect;

        // EMA smoother (optional subsystem)
        impl_->smoother_create_fn = reinterpret_cast<Impl::SmootherCreateFn>(
            GetProcAddress(module, "aura_smoother_create"));
        impl_->smoother_update_fn = reinterpret_cast<Impl::SmootherUpdateFn>(
            GetProcAddress(module, "aura_smoother_update"));
        impl_->smoother_reset_fn = reinterpret_cast<Impl::SmootherResetFn>(
            GetProcAddress(module, "aura_smoother_reset"));
        impl_->smoother_destroy_fn = reinterpret_cast<Impl::SmootherDestroyFn>(
            GetProcAddress(module, "aura_smoother_destroy"));

        // Alert engine (optional subsystem)
        impl_->alert_engine_create_fn = reinterpret_cast<Impl::AlertEngineCreateFn>(
            GetProcAddress(module, "aura_alert_engine_create"));
        impl_->alert_engine_destroy_fn = reinterpret_cast<Impl::AlertEngineDestroyFn>(
            GetProcAddress(module, "aura_alert_engine_destroy"));
        impl_->alert_engine_add_rule_fn = reinterpret_cast<Impl::AlertEngineAddRuleFn>(
            GetProcAddress(module, "aura_alert_engine_add_rule"));
        impl_->alert_engine_evaluate_fn = reinterpret_cast<Impl::AlertEngineEvaluateFn>(
            GetProcAddress(module, "aura_alert_engine_evaluate"));
        impl_->alert_engine_get_active_fn = reinterpret_cast<Impl::AlertEngineGetActiveFn>(
            GetProcAddress(module, "aura_alert_engine_get_active"));
        impl_->alert_engine_acknowledge_fn = reinterpret_cast<Impl::AlertEngineAcknowledgeFn>(
            GetProcAddress(module, "aura_alert_engine_acknowledge"));

        // DVR stats & export (optional subsystem)
        impl_->dvr_compute_stats_fn = reinterpret_cast<Impl::DvrComputeStatsFn>(
            GetProcAddress(module, "aura_dvr_compute_stats"));
        impl_->dvr_export_json_fn = reinterpret_cast<Impl::DvrExportJsonFn>(
            GetProcAddress(module, "aura_dvr_export_json"));
        impl_->dvr_export_csv_fn = reinterpret_cast<Impl::DvrExportCsvFn>(
            GetProcAddress(module, "aura_dvr_export_csv"));

        impl_->loaded = true;
        impl_->loaded_path = narrow_from_wide(path);
        impl_->load_error.clear();
        return;
    }

    impl_->load_error = "Unable to load aura_platform.dll for analytics";
    const std::string suffix = format_windows_error(last_error_code);
    if (!suffix.empty()) {
        impl_->load_error += ": " + suffix;
    }
#else
    impl_->load_error = "Analytics bridge is only supported on Windows.";
#endif
}

AnalyticsBridge::~AnalyticsBridge() {
#ifdef _WIN32
    if (impl_ != nullptr) {
        if (impl_->smoother_handle != nullptr && impl_->smoother_destroy_fn != nullptr) {
            impl_->smoother_destroy_fn(impl_->smoother_handle);
            impl_->smoother_handle = nullptr;
        }
        if (impl_->alert_engine_handle != nullptr && impl_->alert_engine_destroy_fn != nullptr) {
            impl_->alert_engine_destroy_fn(impl_->alert_engine_handle);
            impl_->alert_engine_handle = nullptr;
        }
        if (impl_->module_handle != nullptr) {
            FreeLibrary(impl_->module_handle);
            impl_->module_handle = nullptr;
        }
    }
#endif
}

bool AnalyticsBridge::available() const {
    return impl_ != nullptr && impl_->loaded;
}

bool AnalyticsBridge::smoother_available() const {
#ifdef _WIN32
    return available() &&
           impl_->smoother_create_fn != nullptr &&
           impl_->smoother_update_fn != nullptr &&
           impl_->smoother_reset_fn != nullptr &&
           impl_->smoother_destroy_fn != nullptr;
#else
    return false;
#endif
}

bool AnalyticsBridge::alerts_available() const {
#ifdef _WIN32
    return available() &&
           impl_->alert_engine_create_fn != nullptr &&
           impl_->alert_engine_destroy_fn != nullptr &&
           impl_->alert_engine_add_rule_fn != nullptr &&
           impl_->alert_engine_evaluate_fn != nullptr &&
           impl_->alert_engine_get_active_fn != nullptr &&
           impl_->alert_engine_acknowledge_fn != nullptr;
#else
    return false;
#endif
}

bool AnalyticsBridge::health_available() const {
#ifdef _WIN32
    return available() && impl_->health_score_compute_fn != nullptr;
#else
    return false;
#endif
}

bool AnalyticsBridge::trend_available() const {
#ifdef _WIN32
    return available() && impl_->trend_detect_fn != nullptr;
#else
    return false;
#endif
}

bool AnalyticsBridge::dvr_stats_available() const {
#ifdef _WIN32
    return available() && impl_->dvr_compute_stats_fn != nullptr;
#else
    return false;
#endif
}

namespace {
#ifdef _WIN32
aura_snapshot_t to_c_snapshot(const AnalyticsSnapshot& s) {
    aura_snapshot_t c{};
    c.timestamp = s.timestamp;
    c.cpu_percent = s.cpu_percent;
    c.memory_percent = s.memory_percent;
    c.disk_read_bps = s.disk_read_bps;
    c.disk_write_bps = s.disk_write_bps;
    c.net_recv_bps = s.net_recv_bps;
    c.net_sent_bps = s.net_sent_bps;
    return c;
}

AnalyticsSnapshot from_c_snapshot(const aura_snapshot_t& c) {
    AnalyticsSnapshot s;
    s.timestamp = c.timestamp;
    s.cpu_percent = c.cpu_percent;
    s.memory_percent = c.memory_percent;
    s.disk_read_bps = c.disk_read_bps;
    s.disk_write_bps = c.disk_write_bps;
    s.net_recv_bps = c.net_recv_bps;
    s.net_sent_bps = c.net_sent_bps;
    return s;
}

MetricStats from_c_metric_stats(const aura_metric_stats_t& c) {
    MetricStats s;
    s.avg = c.avg;
    s.min_val = c.min;
    s.max_val = c.max;
    s.p50 = c.p50;
    s.p95 = c.p95;
    s.p99 = c.p99;
    s.stddev = c.stddev;
    return s;
}
#endif
}  // namespace

std::optional<HealthScoreState> AnalyticsBridge::compute_health_score(
    const AnalyticsSnapshot& snapshot, std::string& error
) {
    error.clear();
    if (!health_available()) {
        error = "Health score computation unavailable.";
        return std::nullopt;
    }

#ifdef _WIN32
    using namespace detail;

    aura_snapshot_t c_snap = to_c_snapshot(snapshot);
    aura_health_score_t score{};
    aura_error_t c_error{};
    const int status = impl_->health_score_compute_fn(&c_snap, &score, &c_error);
    if (status != kStatusOk) {
        error = aura_error_message(c_error, "Health score computation failed.");
        return std::nullopt;
    }

    HealthScoreState result;
    result.available = true;
    result.overall = score.overall;
    result.cpu = score.cpu_score;
    result.memory = score.memory_score;
    result.disk = score.disk_score;
    result.network = score.network_score;
    return result;
#else
    error = "Analytics bridge is only supported on Windows.";
    return std::nullopt;
#endif
}

std::optional<TrendResult> AnalyticsBridge::detect_trend(
    const std::vector<AnalyticsSnapshot>& snapshots,
    const int metric, const double sensitivity, std::string& error
) {
    error.clear();
    if (!trend_available()) {
        error = "Trend detection unavailable.";
        return std::nullopt;
    }

#ifdef _WIN32
    using namespace detail;

    std::vector<aura_snapshot_t> c_snaps;
    c_snaps.reserve(snapshots.size());
    for (const auto& s : snapshots) {
        c_snaps.push_back(to_c_snapshot(s));
    }

    aura_trend_result_t c_trend{};
    aura_error_t c_error{};
    const int status = impl_->trend_detect_fn(
        c_snaps.data(), static_cast<int>(c_snaps.size()),
        metric, sensitivity, &c_trend, &c_error);
    if (status != kStatusOk) {
        error = aura_error_message(c_error, "Trend detection failed.");
        return std::nullopt;
    }

    TrendResult result;
    result.direction = static_cast<TrendDirection>(c_trend.direction);
    result.slope = c_trend.slope;
    result.r_squared = c_trend.r_squared;
    return result;
#else
    error = "Analytics bridge is only supported on Windows.";
    return std::nullopt;
#endif
}

std::optional<AnalyticsSnapshot> AnalyticsBridge::smooth(
    const AnalyticsSnapshot& snapshot, std::string& error
) {
    error.clear();
    if (!smoother_available()) {
        error = "EMA smoother unavailable.";
        return std::nullopt;
    }

#ifdef _WIN32
    using namespace detail;

    // Lazy smoother creation (alpha=0.3)
    if (impl_->smoother_handle == nullptr) {
        aura_error_t c_error{};
        const int status = impl_->smoother_create_fn(0.3, &impl_->smoother_handle, &c_error);
        if (status != kStatusOk || impl_->smoother_handle == nullptr) {
            error = aura_error_message(c_error, "Failed to create EMA smoother.");
            return std::nullopt;
        }
    }

    aura_snapshot_t c_raw = to_c_snapshot(snapshot);
    aura_snapshot_t c_smoothed{};
    aura_error_t c_error{};
    const int status = impl_->smoother_update_fn(impl_->smoother_handle, &c_raw, &c_smoothed, &c_error);
    if (status != kStatusOk) {
        error = aura_error_message(c_error, "EMA smoother update failed.");
        return std::nullopt;
    }

    return from_c_snapshot(c_smoothed);
#else
    error = "Analytics bridge is only supported on Windows.";
    return std::nullopt;
#endif
}

bool AnalyticsBridge::reset_smoother(std::string& error) {
    error.clear();
    if (!smoother_available()) {
        error = "EMA smoother unavailable.";
        return false;
    }

#ifdef _WIN32
    using namespace detail;

    if (impl_->smoother_handle == nullptr) {
        return true;  // Nothing to reset
    }

    aura_error_t c_error{};
    const int status = impl_->smoother_reset_fn(impl_->smoother_handle, &c_error);
    if (status != kStatusOk) {
        error = aura_error_message(c_error, "EMA smoother reset failed.");
        return false;
    }
    return true;
#else
    error = "Analytics bridge is only supported on Windows.";
    return false;
#endif
}

bool AnalyticsBridge::evaluate_alerts(const AnalyticsSnapshot& snapshot, std::string& error) {
    error.clear();
    if (!alerts_available()) {
        error = "Alert engine unavailable.";
        return false;
    }

#ifdef _WIN32
    using namespace detail;

    // Lazy alert engine initialization
    if (!impl_->alert_engine_initialized && impl_->alert_engine_handle == nullptr) {
        aura_error_t init_error{};
        const int init_status = impl_->alert_engine_create_fn(&impl_->alert_engine_handle, &init_error);
        if (init_status == kStatusOk && impl_->alert_engine_handle != nullptr) {
            auto add_rule = [&](int id, int metric, double threshold, double sustained, double cooldown, const char* name) -> bool {
                aura_alert_rule_t rule{};
                rule.id = id;
                rule.metric = metric;
                rule.comparator = AURA_COMPARATOR_ABOVE;
                rule.threshold = threshold;
                rule.sustained_seconds = sustained;
                rule.cooldown_seconds = cooldown;
                std::strncpy(rule.name, name, sizeof(rule.name) - 1);
                aura_error_t rule_error{};
                return impl_->alert_engine_add_rule_fn(impl_->alert_engine_handle, &rule, &rule_error) == kStatusOk;
            };

            const bool cpu_ok = add_rule(1, AURA_METRIC_CPU_PERCENT,    90.0,  5.0, 30.0, "CPU High");
            const bool mem_ok = add_rule(2, AURA_METRIC_MEMORY_PERCENT,  85.0, 10.0, 30.0, "Memory High");
            add_rule(3, AURA_METRIC_DISK_READ_BPS,   400.0 * 1024 * 1024, 3.0, 15.0, "Disk Read High");
            add_rule(4, AURA_METRIC_NET_RECV_BPS,    80.0 * 1024 * 1024,  3.0, 15.0, "Network Recv High");
            // Mark initialized even if optional rules fail — engine still usable
            impl_->alert_engine_initialized = cpu_ok || mem_ok;
        }
    }
    if (impl_->alert_engine_handle == nullptr) {
        error = "Failed to initialize alert engine.";
        return false;
    }

    aura_snapshot_t c_snap = to_c_snapshot(snapshot);
    aura_error_t c_error{};
    const int status = impl_->alert_engine_evaluate_fn(impl_->alert_engine_handle, &c_snap, &c_error);
    if (status != kStatusOk) {
        error = aura_error_message(c_error, "Alert evaluation failed.");
        return false;
    }
    return true;
#else
    error = "Analytics bridge is only supported on Windows.";
    return false;
#endif
}

std::vector<ActiveAlert> AnalyticsBridge::get_active_alerts(std::string& error) {
    error.clear();
    if (!alerts_available()) {
        error = "Alert engine unavailable.";
        return {};
    }

#ifdef _WIN32
    using namespace detail;

    if (impl_->alert_engine_handle == nullptr) {
        return {};
    }

    constexpr int kMaxAlerts = 32;
    aura_alert_status_t statuses[kMaxAlerts]{};
    int count = 0;
    aura_error_t c_error{};
    const int status = impl_->alert_engine_get_active_fn(
        impl_->alert_engine_handle, statuses, kMaxAlerts, &count, &c_error);
    if (status != kStatusOk) {
        error = aura_error_message(c_error, "Failed to get active alerts.");
        return {};
    }

    std::vector<ActiveAlert> result;
    result.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        ActiveAlert alert;
        alert.rule_id = statuses[i].rule_id;
        alert.state = statuses[i].state;
        alert.last_value = statuses[i].last_value;
        alert.peak_value = statuses[i].peak_value;
        alert.duration = statuses[i].duration;
        alert.acknowledged = statuses[i].acknowledged != 0;
        result.push_back(alert);
    }
    return result;
#else
    error = "Analytics bridge is only supported on Windows.";
    return {};
#endif
}

bool AnalyticsBridge::acknowledge_alert(const int rule_id, std::string& error) {
    error.clear();
    if (!alerts_available()) {
        error = "Alert engine unavailable.";
        return false;
    }

#ifdef _WIN32
    using namespace detail;

    if (impl_->alert_engine_handle == nullptr) {
        error = "Alert engine not initialized.";
        return false;
    }

    aura_error_t c_error{};
    const int status = impl_->alert_engine_acknowledge_fn(impl_->alert_engine_handle, rule_id, &c_error);
    if (status != kStatusOk) {
        error = aura_error_message(c_error, "Failed to acknowledge alert.");
        return false;
    }
    return true;
#else
    error = "Analytics bridge is only supported on Windows.";
    return false;
#endif
}

std::optional<DvrStatsResult> AnalyticsBridge::compute_dvr_stats(
    void* store_handle, const double start, const double end, std::string& error
) {
    error.clear();
    if (!dvr_stats_available()) {
        error = "DVR stats computation unavailable.";
        return std::nullopt;
    }

#ifdef _WIN32
    using namespace detail;

    if (store_handle == nullptr) {
        error = "Store handle is null.";
        return std::nullopt;
    }

    auto* store = static_cast<aura_store_t*>(store_handle);
    const int has_start = (start > 0.0) ? 1 : 0;
    const int has_end = (end > 0.0) ? 1 : 0;

    aura_stats_result_t c_stats{};
    aura_error_t c_error{};
    const int status = impl_->dvr_compute_stats_fn(
        store, has_start, start, has_end, end, &c_stats, &c_error);
    if (status != kStatusOk) {
        error = aura_error_message(c_error, "DVR stats computation failed.");
        return std::nullopt;
    }

    DvrStatsResult result;
    result.count = c_stats.count;
    result.duration_seconds = c_stats.duration_seconds;
    result.cpu = from_c_metric_stats(c_stats.cpu);
    result.memory = from_c_metric_stats(c_stats.memory);
    result.disk_read = from_c_metric_stats(c_stats.disk_read);
    result.disk_write = from_c_metric_stats(c_stats.disk_write);
    result.net_recv = from_c_metric_stats(c_stats.net_recv);
    result.net_sent = from_c_metric_stats(c_stats.net_sent);
    return result;
#else
    error = "Analytics bridge is only supported on Windows.";
    return std::nullopt;
#endif
}

bool AnalyticsBridge::export_json(
    void* store_handle, const double start, const double end,
    const bool include_stats, const std::string& path, std::string& error
) {
    error.clear();
    if (!available()) {
        error = "Analytics bridge unavailable.";
        return false;
    }

#ifdef _WIN32
    using namespace detail;

    if (impl_->dvr_export_json_fn == nullptr) {
        error = "DVR JSON export function unavailable.";
        return false;
    }
    if (store_handle == nullptr) {
        error = "Store handle is null.";
        return false;
    }

    auto* store = static_cast<aura_store_t*>(store_handle);
    const int has_start = (start > 0.0) ? 1 : 0;
    const int has_end = (end > 0.0) ? 1 : 0;

    aura_error_t c_error{};
    const int status = impl_->dvr_export_json_fn(
        store, has_start, start, has_end, end,
        include_stats ? 1 : 0, path.c_str(), &c_error);
    if (status != kStatusOk) {
        error = aura_error_message(c_error, "DVR JSON export failed.");
        return false;
    }
    return true;
#else
    error = "Analytics bridge is only supported on Windows.";
    return false;
#endif
}

bool AnalyticsBridge::export_csv(
    void* store_handle, const double start, const double end,
    const std::string& path, std::string& error
) {
    error.clear();
    if (!available()) {
        error = "Analytics bridge unavailable.";
        return false;
    }

#ifdef _WIN32
    using namespace detail;

    if (impl_->dvr_export_csv_fn == nullptr) {
        error = "DVR CSV export function unavailable.";
        return false;
    }
    if (store_handle == nullptr) {
        error = "Store handle is null.";
        return false;
    }

    auto* store = static_cast<aura_store_t*>(store_handle);
    const int has_start = (start > 0.0) ? 1 : 0;
    const int has_end = (end > 0.0) ? 1 : 0;

    aura_error_t c_error{};
    const int status = impl_->dvr_export_csv_fn(
        store, has_start, start, has_end, end, path.c_str(), &c_error);
    if (status != kStatusOk) {
        error = aura_error_message(c_error, "DVR CSV export failed.");
        return false;
    }
    return true;
#else
    error = "Analytics bridge is only supported on Windows.";
    return false;
#endif
}

std::string AnalyticsBridge::loaded_path() const {
    return impl_ != nullptr ? impl_->loaded_path : std::string{};
}

std::string AnalyticsBridge::load_error() const {
    return impl_ != nullptr ? impl_->load_error : std::string{};
}

}  // namespace aura::shell
