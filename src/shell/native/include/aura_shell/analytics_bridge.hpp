#pragma once

#include "aura_shell/cockpit_types.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aura::shell {

struct TrendResult {
    TrendDirection direction{TrendDirection::Stable};
    double slope{0.0};
    double r_squared{0.0};
};

class IAnalyticsBridge {
public:
    virtual ~IAnalyticsBridge() = default;

    virtual bool available() const = 0;
    virtual bool smoother_available() const = 0;
    virtual bool alerts_available() const = 0;
    virtual bool health_available() const = 0;
    virtual bool trend_available() const = 0;
    virtual bool dvr_stats_available() const = 0;

    virtual std::optional<HealthScoreState> compute_health_score(
        const AnalyticsSnapshot& snapshot, std::string& error) = 0;

    virtual std::optional<TrendResult> detect_trend(
        const std::vector<AnalyticsSnapshot>& snapshots,
        int metric, double sensitivity, std::string& error) = 0;

    virtual std::optional<AnalyticsSnapshot> smooth(
        const AnalyticsSnapshot& snapshot, std::string& error) = 0;

    virtual bool reset_smoother(std::string& error) = 0;

    virtual bool evaluate_alerts(const AnalyticsSnapshot& snapshot, std::string& error) = 0;

    virtual std::vector<ActiveAlert> get_active_alerts(std::string& error) = 0;

    virtual bool acknowledge_alert(int rule_id, std::string& error) = 0;

    virtual std::optional<DvrStatsResult> compute_dvr_stats(
        void* store_handle, double start, double end, std::string& error) = 0;

    virtual bool export_json(void* store_handle, double start, double end,
        bool include_stats, const std::string& path, std::string& error) = 0;

    virtual bool export_csv(void* store_handle, double start, double end,
        const std::string& path, std::string& error) = 0;
};

class AnalyticsBridge final : public IAnalyticsBridge {
public:
    AnalyticsBridge();
    ~AnalyticsBridge() override;

    AnalyticsBridge(const AnalyticsBridge&) = delete;
    AnalyticsBridge& operator=(const AnalyticsBridge&) = delete;

    bool available() const override;
    bool smoother_available() const override;
    bool alerts_available() const override;
    bool health_available() const override;
    bool trend_available() const override;
    bool dvr_stats_available() const override;

    std::optional<HealthScoreState> compute_health_score(
        const AnalyticsSnapshot& snapshot, std::string& error) override;

    std::optional<TrendResult> detect_trend(
        const std::vector<AnalyticsSnapshot>& snapshots,
        int metric, double sensitivity, std::string& error) override;

    std::optional<AnalyticsSnapshot> smooth(
        const AnalyticsSnapshot& snapshot, std::string& error) override;

    bool reset_smoother(std::string& error) override;

    bool evaluate_alerts(const AnalyticsSnapshot& snapshot, std::string& error) override;

    std::vector<ActiveAlert> get_active_alerts(std::string& error) override;

    bool acknowledge_alert(int rule_id, std::string& error) override;

    std::optional<DvrStatsResult> compute_dvr_stats(
        void* store_handle, double start, double end, std::string& error) override;

    bool export_json(void* store_handle, double start, double end,
        bool include_stats, const std::string& path, std::string& error) override;

    bool export_csv(void* store_handle, double start, double end,
        const std::string& path, std::string& error) override;

    std::string loaded_path() const;
    std::string load_error() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace aura::shell
