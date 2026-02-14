#include "platform_internal.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace aura::platform {
namespace {

void EnsureParentDirectory(const std::string& path) {
    if (path == ":memory:") {
        return;
    }
    const std::filesystem::path fs_path(path);
    const std::filesystem::path parent = fs_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

bool IsLegacySqliteFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    char header[16] = {};
    input.read(header, sizeof(header));
    if (input.gcount() < static_cast<std::streamsize>(sizeof(header))) {
        return false;
    }

    static constexpr char kSqliteMagic[16] = {
        'S', 'Q', 'L', 'i', 't', 'e', ' ', 'f',
        'o', 'r', 'm', 'a', 't', ' ', '3', '\0'
    };
    return std::memcmp(header, kSqliteMagic, sizeof(kSqliteMagic)) == 0;
}

Snapshot ParseSnapshotLine(const std::string& line) {
    std::istringstream input(line);
    std::string ts_raw;
    std::string cpu_raw;
    std::string mem_raw;

    if (!std::getline(input, ts_raw, ',')) {
        throw std::runtime_error("Malformed snapshot line: missing timestamp");
    }
    if (!std::getline(input, cpu_raw, ',')) {
        throw std::runtime_error("Malformed snapshot line: missing cpu_percent");
    }
    if (!std::getline(input, mem_raw)) {
        throw std::runtime_error("Malformed snapshot line: missing memory_percent");
    }

    Snapshot out;
    out.timestamp = std::stod(ts_raw);
    out.cpu_percent = std::stod(cpu_raw);
    out.memory_percent = std::stod(mem_raw);
    ValidateSnapshot(out);
    return out;
}

std::string SerializeSnapshotLine(const Snapshot& snapshot) {
    std::ostringstream output;
    output.precision(17);
    output << snapshot.timestamp << ',' << snapshot.cpu_percent << ',' << snapshot.memory_percent;
    return output.str();
}

class FileBackedStore final : public TelemetryStore {
  public:
    FileBackedStore(std::string db_path, const double retention_seconds)
        : db_path_(std::move(db_path)), retention_seconds_(retention_seconds) {
        ValidatePositiveFinite(retention_seconds_, "retention_seconds");
        EnsureParentDirectory(db_path_);
        if (db_path_ != ":memory:") {
            if (IsLegacySqliteFile(db_path_)) {
                const std::filesystem::path source(db_path_);
                const std::filesystem::path legacy_path = source.string() + ".legacy.sqlite";
                std::error_code rename_error;
                std::filesystem::rename(source, legacy_path, rename_error);
                if (rename_error) {
                    std::filesystem::remove(source, rename_error);
                }
            }
            LoadFromDisk();
            PruneExpiredLocked();
            RewriteAllLocked();
        }
    }

    void Append(const Snapshot& snapshot) override {
        ValidateSnapshot(snapshot);

        std::lock_guard<std::mutex> lock(mu_);
        snapshots_.push_back(snapshot);
        PruneExpiredLocked();
        if (db_path_ != ":memory:") {
            RewriteAllLocked();
        }
    }

    int Count() override {
        std::lock_guard<std::mutex> lock(mu_);
        PruneExpiredLocked();
        if (db_path_ != ":memory:") {
            RewriteAllLocked();
        }
        return static_cast<int>(snapshots_.size());
    }

    std::vector<Snapshot> Latest(const int limit) override {
        if (limit <= 0) {
            throw std::runtime_error("limit must be an integer greater than 0.");
        }

        std::lock_guard<std::mutex> lock(mu_);
        PruneExpiredLocked();
        if (db_path_ != ":memory:") {
            RewriteAllLocked();
        }

        const int start = std::max<int>(0, static_cast<int>(snapshots_.size()) - limit);
        return std::vector<Snapshot>(snapshots_.begin() + start, snapshots_.end());
    }

    std::vector<Snapshot> Between(
        const std::optional<double>& start_timestamp,
        const std::optional<double>& end_timestamp
    ) override {
        if (start_timestamp.has_value()) {
            ValidateFinite(*start_timestamp, "start_timestamp");
        }
        if (end_timestamp.has_value()) {
            ValidateFinite(*end_timestamp, "end_timestamp");
        }
        if (start_timestamp.has_value() && end_timestamp.has_value() && *start_timestamp > *end_timestamp) {
            throw std::runtime_error("start_timestamp must be less than or equal to end_timestamp.");
        }

        std::lock_guard<std::mutex> lock(mu_);
        PruneExpiredLocked();
        if (db_path_ != ":memory:") {
            RewriteAllLocked();
        }

        std::vector<Snapshot> out;
        out.reserve(snapshots_.size());
        for (const Snapshot& snapshot : snapshots_) {
            if (start_timestamp.has_value() && snapshot.timestamp < *start_timestamp) {
                continue;
            }
            if (end_timestamp.has_value() && snapshot.timestamp > *end_timestamp) {
                continue;
            }
            out.push_back(snapshot);
        }
        return out;
    }

  private:
    void LoadFromDisk() {
        std::ifstream input(db_path_);
        if (!input.is_open()) {
            return;
        }

        std::vector<Snapshot> loaded;
        std::string line;
        int parse_failures = 0;
        while (std::getline(input, line)) {
            if (line.empty()) {
                continue;
            }
            try {
                loaded.push_back(ParseSnapshotLine(line));
            } catch (const std::exception&) {
                parse_failures += 1;
            }
        }
        if (parse_failures > 0 && loaded.empty()) {
            // Existing file is incompatible with current native format.
            // Start clean instead of crashing startup.
            snapshots_.clear();
            return;
        }
        snapshots_ = std::move(loaded);
        std::sort(
            snapshots_.begin(),
            snapshots_.end(),
            [](const Snapshot& a, const Snapshot& b) {
                if (a.timestamp == b.timestamp) {
                    if (a.cpu_percent == b.cpu_percent) {
                        return a.memory_percent < b.memory_percent;
                    }
                    return a.cpu_percent < b.cpu_percent;
                }
                return a.timestamp < b.timestamp;
            }
        );
    }

    void PruneExpiredLocked() {
        const double cutoff = NowUnixSeconds() - retention_seconds_;
        snapshots_.erase(
            std::remove_if(
                snapshots_.begin(),
                snapshots_.end(),
                [cutoff](const Snapshot& snapshot) { return snapshot.timestamp < cutoff; }
            ),
            snapshots_.end()
        );
    }

    void RewriteAllLocked() {
        if (db_path_ == ":memory:") {
            return;
        }

        std::ofstream output(db_path_, std::ios::trunc);
        if (!output.is_open()) {
            throw std::runtime_error("Unable to write telemetry store at: " + db_path_);
        }
        for (const Snapshot& snapshot : snapshots_) {
            output << SerializeSnapshotLine(snapshot) << '\n';
        }
    }

    std::string db_path_;
    double retention_seconds_;
    std::mutex mu_;
    std::vector<Snapshot> snapshots_;
};

} // namespace

void ValidateSnapshot(const Snapshot& snapshot) {
    ValidateFinite(snapshot.timestamp, "timestamp");
    ValidateFinite(snapshot.cpu_percent, "cpu_percent");
    ValidateFinite(snapshot.memory_percent, "memory_percent");

    if (snapshot.cpu_percent < 0.0 || snapshot.cpu_percent > 100.0) {
        throw std::runtime_error("cpu_percent must be between 0 and 100.");
    }
    if (snapshot.memory_percent < 0.0 || snapshot.memory_percent > 100.0) {
        throw std::runtime_error("memory_percent must be between 0 and 100.");
    }
}

std::unique_ptr<TelemetryStore> OpenStore(const std::string& db_path, const double retention_seconds) {
    if (db_path.empty()) {
        throw std::runtime_error("db_path cannot be empty when persistence is enabled.");
    }
    return std::make_unique<FileBackedStore>(db_path, retention_seconds);
}

} // namespace aura::platform
