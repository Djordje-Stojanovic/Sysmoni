#include "platform_internal.hpp"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <stdexcept>

#define NOMINMAX
#include <windows.h>

namespace aura::platform {
namespace {

unsigned long long FileTimeToUInt64(const FILETIME& value) {
    ULARGE_INTEGER out;
    out.LowPart = value.dwLowDateTime;
    out.HighPart = value.dwHighDateTime;
    return out.QuadPart;
}

class CpuSampler {
  public:
    double SamplePercent() {
        FILETIME idle_time;
        FILETIME kernel_time;
        FILETIME user_time;
        if (!GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
            return 0.0;
        }

        const unsigned long long idle = FileTimeToUInt64(idle_time);
        const unsigned long long kernel = FileTimeToUInt64(kernel_time);
        const unsigned long long user = FileTimeToUInt64(user_time);

        std::lock_guard<std::mutex> lock(mu_);
        if (!has_previous_) {
            previous_idle_ = idle;
            previous_kernel_ = kernel;
            previous_user_ = user;
            has_previous_ = true;
            return 0.0;
        }

        const unsigned long long idle_delta = idle - previous_idle_;
        const unsigned long long kernel_delta = kernel - previous_kernel_;
        const unsigned long long user_delta = user - previous_user_;
        const unsigned long long total = kernel_delta + user_delta;

        previous_idle_ = idle;
        previous_kernel_ = kernel;
        previous_user_ = user;

        if (total == 0) {
            return 0.0;
        }

        const double active = static_cast<double>(total - idle_delta);
        const double percent = (active / static_cast<double>(total)) * 100.0;
        return std::clamp(percent, 0.0, 100.0);
    }

  private:
    std::mutex mu_;
    bool has_previous_ = false;
    unsigned long long previous_idle_ = 0;
    unsigned long long previous_kernel_ = 0;
    unsigned long long previous_user_ = 0;
};

CpuSampler& SharedCpuSampler() {
    static CpuSampler sampler;
    return sampler;
}

} // namespace

double NowUnixSeconds() {
    const auto now = std::chrono::system_clock::now();
    const auto epoch = now.time_since_epoch();
    return std::chrono::duration<double>(epoch).count();
}

Snapshot CollectSystemSnapshot() {
    MEMORYSTATUSEX memory_info;
    memory_info.dwLength = sizeof(MEMORYSTATUSEX);
    if (!GlobalMemoryStatusEx(&memory_info)) {
        throw std::runtime_error("GlobalMemoryStatusEx failed.");
    }

    Snapshot out;
    out.timestamp = NowUnixSeconds();
    out.cpu_percent = SharedCpuSampler().SamplePercent();
    out.memory_percent = static_cast<double>(memory_info.dwMemoryLoad);
    ValidateSnapshot(out);
    return out;
}

} // namespace aura::platform
