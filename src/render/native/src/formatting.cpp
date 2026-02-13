#include "render_native/formatting.hpp"

#include "render_native/math.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace aura::render_native {

std::string truncate_process_name(const std::string& name, int max_chars) {
    if (max_chars <= 0) {
        return "";
    }
    if (static_cast<int>(name.size()) <= max_chars) {
        return name;
    }
    if (max_chars <= 3) {
        return name.substr(0, static_cast<std::size_t>(max_chars));
    }
    return name.substr(0, static_cast<std::size_t>(max_chars - 3)) + "...";
}

namespace {

std::tm utc_tm_from_epoch(double timestamp) {
    const std::time_t raw = static_cast<std::time_t>(timestamp);
    std::tm utc_tm{};
#if defined(_WIN32)
    gmtime_s(&utc_tm, &raw);
#else
    gmtime_r(&raw, &utc_tm);
#endif
    return utc_tm;
}

}  // namespace

SnapshotLines format_snapshot_lines(double timestamp, double cpu_percent, double memory_percent) {
    const double safe_cpu = sanitize_percent(cpu_percent);
    const double safe_memory = sanitize_percent(memory_percent);
    const std::tm utc_tm = utc_tm_from_epoch(timestamp);

    std::ostringstream ts;
    ts << std::setfill('0')
       << std::setw(2) << utc_tm.tm_hour << ":"
       << std::setw(2) << utc_tm.tm_min << ":"
       << std::setw(2) << utc_tm.tm_sec << " UTC";

    std::ostringstream cpu;
    cpu << "CPU " << std::fixed << std::setprecision(1) << safe_cpu << "%";

    std::ostringstream memory;
    memory << "Memory " << std::fixed << std::setprecision(1) << safe_memory << "%";

    return SnapshotLines{
        cpu.str(),
        memory.str(),
        "Updated " + ts.str(),
    };
}

std::string format_process_row(
    int rank,
    const std::string& name,
    double cpu_percent,
    double memory_rss_bytes,
    int max_chars
) {
    const double memory_bytes = sanitize_non_negative(memory_rss_bytes);
    const double memory_mb = memory_bytes / (1024.0 * 1024.0);
    const double safe_cpu = sanitize_percent(cpu_percent);
    const std::string trimmed_name = truncate_process_name(name, max_chars);

    std::ostringstream out;
    out << std::setw(2) << rank
        << ". "
        << std::left << std::setw(kDefaultProcessNameMaxChars) << trimmed_name
        << std::right
        << "  CPU " << std::setw(5) << std::fixed << std::setprecision(1) << safe_cpu
        << "%  RAM " << std::setw(7) << std::fixed << std::setprecision(1) << memory_mb
        << " MB";
    return out.str();
}

}  // namespace aura::render_native
