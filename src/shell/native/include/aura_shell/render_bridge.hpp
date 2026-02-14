#pragma once

#include <memory>
#include <optional>
#include <string>

#include "aura_shell/cockpit_types.hpp"

namespace aura::shell {

class IRenderBridge {
public:
    virtual ~IRenderBridge() = default;
    virtual bool available() const = 0;
    virtual double sanitize_percent(double value) const = 0;

    virtual std::optional<FrameState> compose_frame(
        double previous_phase,
        double elapsed_since_last_frame,
        double cpu_percent,
        double memory_percent,
        std::string& error
    ) const = 0;

    virtual std::optional<RenderStyleTokens> compute_style_tokens(
        double previous_phase,
        double elapsed_since_last_frame,
        double cpu_percent,
        double memory_percent,
        std::string& error
    ) const = 0;

    virtual std::optional<SnapshotLines> format_snapshot_lines(
        double timestamp,
        double cpu_percent,
        double memory_percent,
        std::string& error
    ) const = 0;

    virtual std::optional<std::string> format_process_row(
        int rank,
        const std::string& name,
        double cpu_percent,
        double memory_rss_bytes,
        int max_chars,
        std::string& error
    ) const = 0;

    virtual std::optional<std::string> format_stream_status(
        const std::optional<std::string>& db_path,
        const std::optional<int>& sample_count,
        const std::optional<std::string>& stream_error,
        std::string& error
    ) const = 0;

    virtual std::string last_error_text() const = 0;
};

class RenderBridge final : public IRenderBridge {
public:
    RenderBridge();
    ~RenderBridge() override;

    RenderBridge(const RenderBridge&) = delete;
    RenderBridge& operator=(const RenderBridge&) = delete;

    bool available() const override;
    double sanitize_percent(double value) const override;

    std::optional<FrameState> compose_frame(
        double previous_phase,
        double elapsed_since_last_frame,
        double cpu_percent,
        double memory_percent,
        std::string& error
    ) const override;

    std::optional<SnapshotLines> format_snapshot_lines(
        double timestamp,
        double cpu_percent,
        double memory_percent,
        std::string& error
    ) const override;

    std::optional<RenderStyleTokens> compute_style_tokens(
        double previous_phase,
        double elapsed_since_last_frame,
        double cpu_percent,
        double memory_percent,
        std::string& error
    ) const override;

    std::optional<std::string> format_process_row(
        int rank,
        const std::string& name,
        double cpu_percent,
        double memory_rss_bytes,
        int max_chars,
        std::string& error
    ) const override;

    std::optional<std::string> format_stream_status(
        const std::optional<std::string>& db_path,
        const std::optional<int>& sample_count,
        const std::optional<std::string>& stream_error,
        std::string& error
    ) const override;

    std::string last_error_text() const override;

    std::string loaded_path() const;
    std::string load_error() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace aura::shell
