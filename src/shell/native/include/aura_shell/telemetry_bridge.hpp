#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aura_shell/cockpit_types.hpp"

namespace aura::shell {

class ITelemetryBridge {
public:
    virtual ~ITelemetryBridge() = default;
    virtual bool available() const = 0;
    virtual std::optional<TelemetrySnapshot> collect_snapshot(std::string& error) = 0;
    virtual std::vector<ProcessSample> collect_top_processes(std::size_t max_samples, std::string& error) = 0;
};

class TelemetryBridge final : public ITelemetryBridge {
public:
    TelemetryBridge();
    ~TelemetryBridge() override;

    TelemetryBridge(const TelemetryBridge&) = delete;
    TelemetryBridge& operator=(const TelemetryBridge&) = delete;

    bool available() const override;
    std::optional<TelemetrySnapshot> collect_snapshot(std::string& error) override;
    std::vector<ProcessSample> collect_top_processes(std::size_t max_samples, std::string& error) override;

    std::string loaded_path() const;
    std::string load_error() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace aura::shell

