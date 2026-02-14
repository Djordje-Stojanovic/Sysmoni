#pragma once

#include <memory>
#include <string>
#include <vector>

#include "aura_shell/cockpit_types.hpp"

namespace aura::shell {

class ITimelineBridge {
public:
    virtual ~ITimelineBridge() = default;
    virtual bool available() const = 0;
    virtual std::vector<TimelinePoint> query_recent(
        const std::string& db_path,
        double end_timestamp,
        double window_seconds,
        int resolution,
        std::string& error
    ) = 0;
};

class TimelineBridge final : public ITimelineBridge {
public:
    TimelineBridge();
    ~TimelineBridge() override;

    TimelineBridge(const TimelineBridge&) = delete;
    TimelineBridge& operator=(const TimelineBridge&) = delete;

    bool available() const override;
    std::vector<TimelinePoint> query_recent(
        const std::string& db_path,
        double end_timestamp,
        double window_seconds,
        int resolution,
        std::string& error
    ) override;

    std::string loaded_path() const;
    std::string load_error() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace aura::shell

