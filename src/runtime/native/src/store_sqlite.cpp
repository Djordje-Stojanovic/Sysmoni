#include "platform_internal.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

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

bool FileStartsWithSqliteMagic(const std::string& path) {
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

bool FileExistsAndNonEmpty(const std::string& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return false;
    }
    const auto size = std::filesystem::file_size(path, ec);
    return !ec && size > 0;
}

std::filesystem::path TempStorePath(const std::filesystem::path& db_path) {
    std::filesystem::path temp_path = db_path;
    temp_path += ".tmp";
    return temp_path;
}

void RemoveFileBestEffort(const std::filesystem::path& path) {
    std::error_code remove_error;
    std::filesystem::remove(path, remove_error);
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

    Snapshot out;

    if (!std::getline(input, mem_raw, ',')) {
        throw std::runtime_error("Malformed snapshot line: missing memory_percent");
    }

    out.timestamp = std::stod(ts_raw);
    out.cpu_percent = std::stod(cpu_raw);
    out.memory_percent = std::stod(mem_raw);

    std::string disk_read_raw;
    std::string disk_write_raw;
    if (std::getline(input, disk_read_raw, ',') && std::getline(input, disk_write_raw, ',')) {
        out.disk_read_bps = std::stod(disk_read_raw);
        out.disk_write_bps = std::stod(disk_write_raw);

        std::string net_recv_raw;
        std::string net_sent_raw;
        if (std::getline(input, net_recv_raw, ',') && std::getline(input, net_sent_raw)) {
            out.net_recv_bps = std::stod(net_recv_raw);
            out.net_sent_bps = std::stod(net_sent_raw);
        }
    }

    ValidateSnapshot(out);
    return out;
}

void SqliteCheck(sqlite3* db, int rc, const char* context) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        const std::string msg = std::string(context) + ": " +
            (db ? sqlite3_errmsg(db) : "null db handle");
        throw std::runtime_error(msg);
    }
}

void ExecPragma(sqlite3* db, const char* sql) {
    char* err_msg = nullptr;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string msg = std::string("pragma failed: ") + sql;
        if (err_msg) {
            msg += " — ";
            msg += err_msg;
            sqlite3_free(err_msg);
        }
        throw std::runtime_error(msg);
    }
}

sqlite3_stmt* PrepareStatement(sqlite3* db, const char* sql) {
    sqlite3_stmt* stmt = nullptr;
    const int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    SqliteCheck(db, rc, sql);
    return stmt;
}

class SqliteStore final : public TelemetryStore {
  public:
    SqliteStore(std::string db_path, const double retention_seconds)
        : db_path_(std::move(db_path)), retention_seconds_(retention_seconds) {
        ValidatePositiveFinite(retention_seconds_, "retention_seconds");
        EnsureParentDirectory(db_path_);

        if (db_path_ != ":memory:") {
            RecoverPendingTempFile();
            MigrateCsvToSqlite();
        }

        OpenAndInitialize();
        if (!db_ && db_path_ != ":memory:") {
            // Corrupt SQLite file — remove and retry with fresh DB
            std::error_code ec;
            std::filesystem::remove(db_path_, ec);
            // Also remove WAL/SHM leftovers
            std::filesystem::remove(db_path_ + "-wal", ec);
            std::filesystem::remove(db_path_ + "-shm", ec);
            OpenAndInitialize();
        }
        if (!db_) {
            throw std::runtime_error("Failed to open SQLite store at: " + db_path_);
        }
    }

    ~SqliteStore() override {
        if (stmt_insert_) sqlite3_finalize(stmt_insert_);
        if (stmt_latest_) sqlite3_finalize(stmt_latest_);
        if (stmt_count_) sqlite3_finalize(stmt_count_);
        if (stmt_between_) sqlite3_finalize(stmt_between_);
        if (stmt_prune_) sqlite3_finalize(stmt_prune_);
        if (db_) sqlite3_close(db_);
    }

    void Append(const Snapshot& snapshot) override {
        ValidateSnapshot(snapshot);

        std::lock_guard<std::mutex> lock(mu_);

        sqlite3_reset(stmt_insert_);
        sqlite3_bind_double(stmt_insert_, 1, snapshot.timestamp);
        sqlite3_bind_double(stmt_insert_, 2, snapshot.cpu_percent);
        sqlite3_bind_double(stmt_insert_, 3, snapshot.memory_percent);
        sqlite3_bind_double(stmt_insert_, 4, snapshot.disk_read_bps);
        sqlite3_bind_double(stmt_insert_, 5, snapshot.disk_write_bps);
        sqlite3_bind_double(stmt_insert_, 6, snapshot.net_recv_bps);
        sqlite3_bind_double(stmt_insert_, 7, snapshot.net_sent_bps);

        const int rc = sqlite3_step(stmt_insert_);
        SqliteCheck(db_, rc, "insert snapshot");

        ++append_counter_;
        if (append_counter_ >= 100) {
            PruneLocked();
            append_counter_ = 0;
        }
    }

    int Count() override {
        std::lock_guard<std::mutex> lock(mu_);

        sqlite3_reset(stmt_count_);
        const int rc = sqlite3_step(stmt_count_);
        if (rc != SQLITE_ROW) {
            SqliteCheck(db_, rc, "count snapshots");
            return 0;
        }
        return sqlite3_column_int(stmt_count_, 0);
    }

    std::vector<Snapshot> Latest(const int limit) override {
        if (limit <= 0) {
            throw std::runtime_error("limit must be an integer greater than 0.");
        }

        std::lock_guard<std::mutex> lock(mu_);

        sqlite3_reset(stmt_latest_);
        sqlite3_bind_int(stmt_latest_, 1, limit);

        std::vector<Snapshot> results;
        while (true) {
            const int rc = sqlite3_step(stmt_latest_);
            if (rc == SQLITE_DONE) break;
            if (rc != SQLITE_ROW) {
                SqliteCheck(db_, rc, "latest snapshots");
                break;
            }
            Snapshot s;
            s.timestamp = sqlite3_column_double(stmt_latest_, 0);
            s.cpu_percent = sqlite3_column_double(stmt_latest_, 1);
            s.memory_percent = sqlite3_column_double(stmt_latest_, 2);
            s.disk_read_bps = sqlite3_column_double(stmt_latest_, 3);
            s.disk_write_bps = sqlite3_column_double(stmt_latest_, 4);
            s.net_recv_bps = sqlite3_column_double(stmt_latest_, 5);
            s.net_sent_bps = sqlite3_column_double(stmt_latest_, 6);
            results.push_back(s);
        }

        // Results come back DESC, reverse to ASC
        std::reverse(results.begin(), results.end());
        return results;
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

        if (start_timestamp.has_value() && end_timestamp.has_value()) {
            sqlite3_reset(stmt_between_);
            sqlite3_bind_double(stmt_between_, 1, *start_timestamp);
            sqlite3_bind_double(stmt_between_, 2, *end_timestamp);

            std::vector<Snapshot> results;
            while (true) {
                const int rc = sqlite3_step(stmt_between_);
                if (rc == SQLITE_DONE) break;
                if (rc != SQLITE_ROW) {
                    SqliteCheck(db_, rc, "between snapshots");
                    break;
                }
                Snapshot s;
                s.timestamp = sqlite3_column_double(stmt_between_, 0);
                s.cpu_percent = sqlite3_column_double(stmt_between_, 1);
                s.memory_percent = sqlite3_column_double(stmt_between_, 2);
                s.disk_read_bps = sqlite3_column_double(stmt_between_, 3);
                s.disk_write_bps = sqlite3_column_double(stmt_between_, 4);
                s.net_recv_bps = sqlite3_column_double(stmt_between_, 5);
                s.net_sent_bps = sqlite3_column_double(stmt_between_, 6);
                results.push_back(s);
            }
            return results;
        }

        // Fallback: fetch all and filter in-memory for open-ended ranges
        const double lo = start_timestamp.value_or(-1e308);
        const double hi = end_timestamp.value_or(1e308);

        sqlite3_reset(stmt_between_);
        sqlite3_bind_double(stmt_between_, 1, lo);
        sqlite3_bind_double(stmt_between_, 2, hi);

        std::vector<Snapshot> results;
        while (true) {
            const int rc = sqlite3_step(stmt_between_);
            if (rc == SQLITE_DONE) break;
            if (rc != SQLITE_ROW) {
                SqliteCheck(db_, rc, "between snapshots (open)");
                break;
            }
            Snapshot s;
            s.timestamp = sqlite3_column_double(stmt_between_, 0);
            s.cpu_percent = sqlite3_column_double(stmt_between_, 1);
            s.memory_percent = sqlite3_column_double(stmt_between_, 2);
            s.disk_read_bps = sqlite3_column_double(stmt_between_, 3);
            s.disk_write_bps = sqlite3_column_double(stmt_between_, 4);
            s.net_recv_bps = sqlite3_column_double(stmt_between_, 5);
            s.net_sent_bps = sqlite3_column_double(stmt_between_, 6);
            results.push_back(s);
        }
        return results;
    }

  private:
    void OpenAndInitialize() {
        const int rc = sqlite3_open(db_path_.c_str(), &db_);
        if (rc != SQLITE_OK) {
            if (db_) { sqlite3_close(db_); db_ = nullptr; }
            return;
        }

        // Try to set pragmas and create schema. If this fails on a corrupt
        // file, close the handle and signal failure via db_=nullptr.
        char* err_msg = nullptr;
        auto try_exec = [&](const char* sql) -> bool {
            int exec_rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg);
            if (exec_rc != SQLITE_OK) {
                if (err_msg) sqlite3_free(err_msg);
                err_msg = nullptr;
                return false;
            }
            return true;
        };

        if (!try_exec("PRAGMA journal_mode=WAL;") ||
            !try_exec("PRAGMA synchronous=NORMAL;") ||
            !try_exec("PRAGMA temp_store=MEMORY;") ||
            !try_exec("PRAGMA cache_size=-4096;") ||
            !try_exec("CREATE TABLE IF NOT EXISTS snapshots ("
                "ts REAL NOT NULL,"
                "cpu REAL NOT NULL,"
                "mem REAL NOT NULL,"
                "disk_r REAL NOT NULL DEFAULT 0,"
                "disk_w REAL NOT NULL DEFAULT 0,"
                "net_r REAL NOT NULL DEFAULT 0,"
                "net_w REAL NOT NULL DEFAULT 0"
                ");") ||
            !try_exec("CREATE INDEX IF NOT EXISTS idx_snapshots_ts ON snapshots(ts);")) {
            sqlite3_close(db_);
            db_ = nullptr;
            return;
        }

        // Migrate existing databases: add net columns if missing.
        // Errors are expected and ignored (column may already exist).
        sqlite3_exec(db_, "ALTER TABLE snapshots ADD COLUMN net_r REAL NOT NULL DEFAULT 0;",
                     nullptr, nullptr, nullptr);
        sqlite3_exec(db_, "ALTER TABLE snapshots ADD COLUMN net_w REAL NOT NULL DEFAULT 0;",
                     nullptr, nullptr, nullptr);

        stmt_insert_ = PrepareStatement(db_,
            "INSERT INTO snapshots(ts,cpu,mem,disk_r,disk_w,net_r,net_w) VALUES(?,?,?,?,?,?,?)");
        stmt_latest_ = PrepareStatement(db_,
            "SELECT ts,cpu,mem,disk_r,disk_w,net_r,net_w FROM snapshots ORDER BY ts DESC LIMIT ?");
        stmt_count_ = PrepareStatement(db_,
            "SELECT COUNT(*) FROM snapshots");
        stmt_between_ = PrepareStatement(db_,
            "SELECT ts,cpu,mem,disk_r,disk_w,net_r,net_w FROM snapshots WHERE ts BETWEEN ? AND ? ORDER BY ts");
        stmt_prune_ = PrepareStatement(db_,
            "DELETE FROM snapshots WHERE ts < ?");
    }

    void RecoverPendingTempFile() {
        const std::filesystem::path db_path(db_path_);
        const std::filesystem::path temp_path = TempStorePath(db_path);

        std::error_code ec;
        if (!std::filesystem::exists(temp_path, ec) || ec) {
            return;
        }

        if (std::filesystem::exists(db_path, ec) && !ec) {
            RemoveFileBestEffort(temp_path);
            return;
        }

        // Main missing, temp exists — this was a CSV-era crash recovery.
        // Rename temp to main so MigrateCsvToSqlite picks it up.
#ifdef _WIN32
        const auto src = temp_path.native();
        const auto dst = db_path.native();
        if (!MoveFileExW(src.c_str(), dst.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            RemoveFileBestEffort(temp_path);
        }
#else
        std::filesystem::rename(temp_path, db_path, ec);
        if (ec) {
            RemoveFileBestEffort(temp_path);
        }
#endif
    }

    void MigrateCsvToSqlite() {
        if (!FileExistsAndNonEmpty(db_path_)) {
            return;
        }
        if (FileStartsWithSqliteMagic(db_path_)) {
            return; // Already SQLite — nothing to do.
        }

        // It's a CSV file (or legacy data). Read all lines.
        std::vector<Snapshot> loaded;
        {
            std::ifstream input(db_path_);
            if (!input.is_open()) {
                return;
            }
            std::string line;
            while (std::getline(input, line)) {
                if (line.empty()) continue;
                try {
                    loaded.push_back(ParseSnapshotLine(line));
                } catch (const std::exception&) {
                    // Skip corrupt lines
                }
            }
        }

        // Rename old CSV to .csv.bak
        const std::filesystem::path csv_bak = db_path_ + ".csv.bak";
        std::error_code ec;
        std::filesystem::rename(db_path_, csv_bak, ec);
        if (ec) {
            // If rename fails, just remove the old file
            std::filesystem::remove(db_path_, ec);
        }

        // Now open a fresh SQLite DB at db_path_, insert all rows, close it.
        // The constructor will reopen it normally.
        sqlite3* mig_db = nullptr;
        int rc = sqlite3_open(db_path_.c_str(), &mig_db);
        if (rc != SQLITE_OK) {
            if (mig_db) sqlite3_close(mig_db);
            return; // Migration failed — constructor will create fresh DB
        }

        sqlite3_exec(mig_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(mig_db, "CREATE TABLE IF NOT EXISTS snapshots ("
            "ts REAL NOT NULL,"
            "cpu REAL NOT NULL,"
            "mem REAL NOT NULL,"
            "disk_r REAL NOT NULL DEFAULT 0,"
            "disk_w REAL NOT NULL DEFAULT 0,"
            "net_r REAL NOT NULL DEFAULT 0,"
            "net_w REAL NOT NULL DEFAULT 0"
            ");", nullptr, nullptr, nullptr);
        sqlite3_exec(mig_db, "CREATE INDEX IF NOT EXISTS idx_snapshots_ts ON snapshots(ts);",
            nullptr, nullptr, nullptr);

        if (!loaded.empty()) {
            sqlite3_exec(mig_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
            sqlite3_stmt* ins = PrepareStatement(mig_db,
                "INSERT INTO snapshots(ts,cpu,mem,disk_r,disk_w,net_r,net_w) VALUES(?,?,?,?,?,?,?)");
            for (const auto& s : loaded) {
                sqlite3_reset(ins);
                sqlite3_bind_double(ins, 1, s.timestamp);
                sqlite3_bind_double(ins, 2, s.cpu_percent);
                sqlite3_bind_double(ins, 3, s.memory_percent);
                sqlite3_bind_double(ins, 4, s.disk_read_bps);
                sqlite3_bind_double(ins, 5, s.disk_write_bps);
                sqlite3_bind_double(ins, 6, s.net_recv_bps);
                sqlite3_bind_double(ins, 7, s.net_sent_bps);
                sqlite3_step(ins);
            }
            sqlite3_finalize(ins);
            sqlite3_exec(mig_db, "COMMIT;", nullptr, nullptr, nullptr);
        }

        sqlite3_close(mig_db);
    }

    void PruneLocked() {
        const double cutoff = NowUnixSeconds() - retention_seconds_;
        sqlite3_reset(stmt_prune_);
        sqlite3_bind_double(stmt_prune_, 1, cutoff);
        const int rc = sqlite3_step(stmt_prune_);
        SqliteCheck(db_, rc, "prune snapshots");
    }

    std::string db_path_;
    double retention_seconds_;
    std::mutex mu_;
    sqlite3* db_ = nullptr;
    sqlite3_stmt* stmt_insert_ = nullptr;
    sqlite3_stmt* stmt_latest_ = nullptr;
    sqlite3_stmt* stmt_count_ = nullptr;
    sqlite3_stmt* stmt_between_ = nullptr;
    sqlite3_stmt* stmt_prune_ = nullptr;
    int append_counter_ = 0;
};

} // namespace

void ValidateSnapshot(const Snapshot& snapshot) {
    ValidateFinite(snapshot.timestamp, "timestamp");
    ValidateFinite(snapshot.cpu_percent, "cpu_percent");
    ValidateFinite(snapshot.memory_percent, "memory_percent");
    ValidateFinite(snapshot.disk_read_bps, "disk_read_bps");
    ValidateFinite(snapshot.disk_write_bps, "disk_write_bps");
    ValidateFinite(snapshot.net_recv_bps, "net_recv_bps");
    ValidateFinite(snapshot.net_sent_bps, "net_sent_bps");

    if (snapshot.cpu_percent < 0.0 || snapshot.cpu_percent > 100.0) {
        throw std::runtime_error("cpu_percent must be between 0 and 100.");
    }
    if (snapshot.memory_percent < 0.0 || snapshot.memory_percent > 100.0) {
        throw std::runtime_error("memory_percent must be between 0 and 100.");
    }
    if (snapshot.disk_read_bps < 0.0) {
        throw std::runtime_error("disk_read_bps must be non-negative.");
    }
    if (snapshot.disk_write_bps < 0.0) {
        throw std::runtime_error("disk_write_bps must be non-negative.");
    }
    if (snapshot.net_recv_bps < 0.0) {
        throw std::runtime_error("net_recv_bps must be non-negative.");
    }
    if (snapshot.net_sent_bps < 0.0) {
        throw std::runtime_error("net_sent_bps must be non-negative.");
    }
}

std::unique_ptr<TelemetryStore> OpenStore(const std::string& db_path, const double retention_seconds) {
    if (db_path.empty()) {
        throw std::runtime_error("db_path cannot be empty when persistence is enabled.");
    }
    return std::make_unique<SqliteStore>(db_path, retention_seconds);
}

} // namespace aura::platform
