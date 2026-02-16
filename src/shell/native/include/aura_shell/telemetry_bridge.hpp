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
    virtual std::optional<PerCoreCpuState> collect_per_core_cpu(std::string& error) = 0;
    virtual std::optional<GpuState> collect_gpu(std::string& error) = 0;
    virtual std::optional<DiskIoState> collect_disk_io(std::string& error) = 0;
    virtual std::optional<NetworkIoState> collect_network_io(std::string& error) = 0;
    virtual std::optional<ThermalState> collect_thermal(std::string& error) = 0;
    virtual std::vector<ProcessSample> collect_process_details(
        std::size_t max_results, std::uint8_t sort_column,
        bool sort_descending, std::string& error) = 0;
    virtual bool terminate_process(std::uint32_t pid, std::string& error) = 0;
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
    std::optional<PerCoreCpuState> collect_per_core_cpu(std::string& error) override;
    std::optional<GpuState> collect_gpu(std::string& error) override;
    std::optional<DiskIoState> collect_disk_io(std::string& error) override;
    std::optional<NetworkIoState> collect_network_io(std::string& error) override;
    std::optional<ThermalState> collect_thermal(std::string& error) override;
    std::vector<ProcessSample> collect_process_details(
        std::size_t max_results, std::uint8_t sort_column,
        bool sort_descending, std::string& error) override;
    bool terminate_process(std::uint32_t pid, std::string& error) override;

    std::string loaded_path() const;
    std::string load_error() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace aura::shell

