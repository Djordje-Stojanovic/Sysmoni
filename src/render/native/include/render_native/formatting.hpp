#pragma once

#include <string>

namespace aura::render_native {

constexpr int kDefaultProcessNameMaxChars = 20;

struct SnapshotLines {
    std::string cpu;
    std::string memory;
    std::string timestamp;
};

std::string truncate_process_name(const std::string& name, int max_chars = kDefaultProcessNameMaxChars);

SnapshotLines format_snapshot_lines(double timestamp, double cpu_percent, double memory_percent);

std::string format_process_row(
    int rank,
    const std::string& name,
    double cpu_percent,
    double memory_rss_bytes,
    int max_chars = kDefaultProcessNameMaxChars
);

std::string format_disk_rate(double bytes_per_second);
std::string format_network_rate(double bytes_per_second);

}  // namespace aura::render_native
