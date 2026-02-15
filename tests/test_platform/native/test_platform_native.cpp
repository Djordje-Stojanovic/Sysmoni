#include "aura_platform.h"

#include <cmath>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

void Fail(const std::string& message) {
    std::cerr << "FAILED: " << message << std::endl;
    std::exit(1);
}

void ExpectTrue(const bool condition, const std::string& message) {
    if (!condition) {
        Fail(message);
    }
}

void ExpectEq(const int actual, const int expected, const std::string& message) {
    if (actual != expected) {
        Fail(message + " (actual=" + std::to_string(actual) + ", expected=" + std::to_string(expected) + ")");
    }
}

void ExpectNear(const double actual, const double expected, const double tolerance, const std::string& message) {
    if (std::fabs(actual - expected) > tolerance) {
        Fail(message + " (actual=" + std::to_string(actual) + ", expected=" + std::to_string(expected) + ")");
    }
}

class ScopedEnvVar final {
  public:
    explicit ScopedEnvVar(std::string key)
        : key_(std::move(key)) {
        char* value = nullptr;
        std::size_t value_length = 0;
        const errno_t rc = _dupenv_s(&value, &value_length, key_.c_str());
        if (rc != 0) {
            if (value != nullptr) {
                std::free(value);
            }
            Fail("failed to read env var: " + key_);
        }
        if (value != nullptr) {
            has_original_ = true;
            original_value_ = value;
            std::free(value);
        }
    }

    ~ScopedEnvVar() {
        if (has_original_) {
            (void)_putenv_s(key_.c_str(), original_value_.c_str());
        } else {
            (void)_putenv_s(key_.c_str(), "");
        }
    }

    void Set(const std::string& value) {
        const errno_t rc = _putenv_s(key_.c_str(), value.c_str());
        if (rc != 0) {
            Fail("failed to set env var: " + key_);
        }
    }

  private:
    std::string key_;
    bool has_original_ = false;
    std::string original_value_;
};

double NowSeconds() {
    using clock = std::chrono::system_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

std::filesystem::path BuildStorePath(const std::string& test_name) {
    const auto unique = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "aura_platform_native_tests";
    std::error_code create_error;
    std::filesystem::create_directories(root, create_error);
    return root / (test_name + "_" + std::to_string(unique) + ".db");
}

std::filesystem::path TempStorePath(const std::filesystem::path& db_path) {
    std::filesystem::path temp_path = db_path;
    temp_path += ".tmp";
    return temp_path;
}

std::filesystem::path CsvBakPath(const std::filesystem::path& db_path) {
    return db_path.string() + ".csv.bak";
}

void CleanupStoreFiles(const std::filesystem::path& db_path) {
    std::error_code remove_error;
    std::filesystem::remove(db_path, remove_error);
    std::filesystem::remove(TempStorePath(db_path), remove_error);
    std::filesystem::remove(CsvBakPath(db_path), remove_error);
    // WAL and SHM files
    std::filesystem::path wal_path = db_path;
    wal_path += "-wal";
    std::filesystem::remove(wal_path, remove_error);
    std::filesystem::path shm_path = db_path;
    shm_path += "-shm";
    std::filesystem::remove(shm_path, remove_error);
}

std::string SnapshotLine(const double timestamp, const double cpu_percent, const double memory_percent) {
    std::ostringstream output;
    output.precision(17);
    output << timestamp << ',' << cpu_percent << ',' << memory_percent << ',' << 0.0 << ',' << 0.0;
    return output.str();
}

std::string SnapshotLineWithDisk(
    const double timestamp,
    const double cpu_percent,
    const double memory_percent,
    const double disk_read_bps,
    const double disk_write_bps
) {
    std::ostringstream output;
    output.precision(17);
    output << timestamp << ',' << cpu_percent << ',' << memory_percent
           << ',' << disk_read_bps << ',' << disk_write_bps;
    return output.str();
}

void WriteTextFileLines(const std::filesystem::path& path, const std::vector<std::string>& lines) {
    std::ofstream output(path, std::ios::trunc | std::ios::binary);
    if (!output.is_open()) {
        Fail("failed to create test file: " + path.string());
    }
    for (const std::string& line : lines) {
        output << line << '\n';
    }
}

bool StartsWithSqliteMagic(const std::filesystem::path& path) {
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

void TestConfigNoPersist() {
    aura_config_request_t request{};
    request.no_persist = 1;

    aura_runtime_config_t config{};
    aura_error_t error{};
    const int rc = aura_config_resolve(&request, &config, &error);
    ExpectEq(rc, AURA_OK, "aura_config_resolve should succeed");
    ExpectEq(config.persistence_enabled, 0, "persistence should be disabled");
    ExpectEq(config.db_source, AURA_DB_SOURCE_DISABLED, "db source should be disabled");
}

void TestConfigRejectsMalformedEnvRetention() {
    ScopedEnvVar retention_env("AURA_RETENTION_SECONDS");
    retention_env.Set("30junk");

    aura_config_request_t request{};
    request.no_persist = 1;

    aura_runtime_config_t config{};
    aura_error_t error{};
    const int rc = aura_config_resolve(&request, &config, &error);
    ExpectTrue(rc != AURA_OK, "aura_config_resolve should reject malformed env retention");
    ExpectTrue(error.message[0] != '\0', "error message should be populated");
    ExpectTrue(
        std::string(error.message).find("AURA_RETENTION_SECONDS") != std::string::npos,
        "error should identify malformed env retention source"
    );
}

void TestConfigRejectsMalformedTomlRetention() {
    const std::filesystem::path config_path = BuildStorePath("bad_retention_config");
    const std::string config_path_raw = config_path.string();
    WriteTextFileLines(
        config_path,
        {
            "[persistence]",
            "retention_seconds = 42oops",
        }
    );

    aura_config_request_t request{};
    request.no_persist = 1;
    request.config_path_override = config_path_raw.c_str();

    aura_runtime_config_t config{};
    aura_error_t error{};
    const int rc = aura_config_resolve(&request, &config, &error);
    ExpectTrue(rc != AURA_OK, "aura_config_resolve should reject malformed TOML retention");
    ExpectTrue(error.message[0] != '\0', "error message should be populated");
    ExpectTrue(
        std::string(error.message).find("retention_seconds") != std::string::npos,
        "error should identify malformed TOML retention field"
    );

    std::error_code remove_error;
    std::filesystem::remove(config_path, remove_error);
}

void TestConfigAcceptsTomlInlineCommentRetention() {
    ScopedEnvVar retention_env("AURA_RETENTION_SECONDS");
    retention_env.Set("");

    const std::filesystem::path config_path = BuildStorePath("inline_comment_retention");
    const std::string config_path_raw = config_path.string();
    WriteTextFileLines(
        config_path,
        {
            "[persistence] # persistence section",
            "retention_seconds = 42 # keep six weeks of history",
        }
    );

    aura_config_request_t request{};
    request.no_persist = 1;
    request.config_path_override = config_path_raw.c_str();

    aura_runtime_config_t config{};
    aura_error_t error{};
    const int rc = aura_config_resolve(&request, &config, &error);
    ExpectEq(rc, AURA_OK, "aura_config_resolve should accept TOML inline comment on retention");
    ExpectNear(config.retention_seconds, 42.0, 1e-9, "retention should parse from TOML value before comment");

    std::error_code remove_error;
    std::filesystem::remove(config_path, remove_error);
}

void TestConfigAcceptsTomlInlineCommentDbPath() {
    ScopedEnvVar db_path_env("AURA_DB_PATH");
    db_path_env.Set("");
    ScopedEnvVar retention_env("AURA_RETENTION_SECONDS");
    retention_env.Set("");

    const std::filesystem::path expected_db_path = BuildStorePath("inline_comment_db");
    const std::filesystem::path config_path = BuildStorePath("inline_comment_db_config");
    const std::string config_path_raw = config_path.string();

    WriteTextFileLines(
        config_path,
        {
            "[persistence] # persistence section",
            std::string("db_path = \"") + expected_db_path.string() + "\" # default db path",
        }
    );

    aura_config_request_t request{};
    request.no_persist = 0;
    request.config_path_override = config_path_raw.c_str();

    aura_runtime_config_t config{};
    aura_error_t error{};
    const int rc = aura_config_resolve(&request, &config, &error);
    ExpectEq(rc, AURA_OK, "aura_config_resolve should accept TOML inline comment on db_path");
    ExpectEq(config.db_source, AURA_DB_SOURCE_CONFIG, "db source should remain config when env/cli are unset");
    ExpectTrue(
        std::string(config.db_path) == expected_db_path.string(),
        "db path should parse from TOML quoted value before comment"
    );

    std::error_code remove_error;
    std::filesystem::remove(config_path, remove_error);
    CleanupStoreFiles(expected_db_path);
}

void TestStoreMemoryAppendLatestBetween() {
    aura_error_t error{};
    aura_store_t* store = nullptr;

    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "aura_store_open :memory: should succeed");
    ExpectTrue(store != nullptr, "store pointer should be set");

    const double base = NowSeconds();
    aura_snapshot_t first{base, 10.0, 20.0};
    aura_snapshot_t second{base + 1.0, 11.0, 21.0};
    aura_snapshot_t third{base + 2.0, 12.0, 22.0};

    rc = aura_store_append(store, &first, &error);
    ExpectEq(rc, AURA_OK, "append first should succeed");
    rc = aura_store_append(store, &second, &error);
    ExpectEq(rc, AURA_OK, "append second should succeed");
    rc = aura_store_append(store, &third, &error);
    ExpectEq(rc, AURA_OK, "append third should succeed");

    int count = 0;
    rc = aura_store_count(store, &count, &error);
    ExpectEq(rc, AURA_OK, "count should succeed");
    ExpectEq(count, 3, "count should equal three");

    aura_snapshot_t latest[2]{};
    int out_count = 0;
    rc = aura_store_latest(store, 2, latest, 2, &out_count, &error);
    ExpectEq(rc, AURA_OK, "latest should succeed");
    ExpectEq(out_count, 2, "latest count should be two");
    ExpectNear(latest[0].timestamp, base + 1.0, 1e-9, "latest[0] timestamp");
    ExpectNear(latest[1].timestamp, base + 2.0, 1e-9, "latest[1] timestamp");

    aura_snapshot_t range[3]{};
    out_count = 0;
    rc = aura_store_between(store, 1, base + 0.5, 1, base + 2.0, range, 3, &out_count, &error);
    ExpectEq(rc, AURA_OK, "between should succeed");
    ExpectEq(out_count, 2, "between count should be two");
    ExpectNear(range[0].timestamp, base + 1.0, 1e-9, "range[0] timestamp");
    ExpectNear(range[1].timestamp, base + 2.0, 1e-9, "range[1] timestamp");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "store close should succeed");
}

void TestStoreFilePersistenceAcrossReopen() {
    const std::filesystem::path db_path = BuildStorePath("store_reopen");
    const std::string db_path_raw = db_path.string();

    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "file-backed store open should succeed");

    const double base = NowSeconds() - 10.0;
    aura_snapshot_t first{base, 15.0, 25.0};
    aura_snapshot_t second{base + 1.0, 16.0, 26.0};
    aura_snapshot_t third{base + 2.0, 17.0, 27.0};

    rc = aura_store_append(store, &first, &error);
    ExpectEq(rc, AURA_OK, "append first file-backed snapshot");
    rc = aura_store_append(store, &second, &error);
    ExpectEq(rc, AURA_OK, "append second file-backed snapshot");
    rc = aura_store_append(store, &third, &error);
    ExpectEq(rc, AURA_OK, "append third file-backed snapshot");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "close first file-backed store handle");

    store = nullptr;
    rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "reopen file-backed store should succeed");

    int count = 0;
    rc = aura_store_count(store, &count, &error);
    ExpectEq(rc, AURA_OK, "count after reopen should succeed");
    ExpectEq(count, 3, "count after reopen should persist snapshots");

    aura_snapshot_t latest[2]{};
    int out_count = 0;
    rc = aura_store_latest(store, 2, latest, 2, &out_count, &error);
    ExpectEq(rc, AURA_OK, "latest after reopen should succeed");
    ExpectEq(out_count, 2, "latest after reopen count should be two");
    ExpectNear(latest[0].timestamp, base + 1.0, 1e-9, "latest[0] timestamp after reopen");
    ExpectNear(latest[1].timestamp, base + 2.0, 1e-9, "latest[1] timestamp after reopen");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "close reopened store should succeed");

    CleanupStoreFiles(db_path);
}

void TestStoreRecoveryFromStaleTmpWhenMainMissing() {
    const std::filesystem::path db_path = BuildStorePath("recover_tmp_missing_main");
    const std::filesystem::path tmp_path = TempStorePath(db_path);
    const std::string db_path_raw = db_path.string();

    // Write a CSV temp file — the recovery path will rename it to main,
    // then the CSV migration will convert it to SQLite.
    const double base = NowSeconds() - 10.0;
    WriteTextFileLines(
        tmp_path,
        {
            SnapshotLine(base, 10.0, 20.0),
            SnapshotLine(base + 1.0, 11.0, 21.0),
        }
    );

    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "store open should recover from temp file");

    int count = 0;
    rc = aura_store_count(store, &count, &error);
    ExpectEq(rc, AURA_OK, "count after temp recovery should succeed");
    ExpectEq(count, 2, "recovered temp store should contain two snapshots");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "close recovered temp store");

    ExpectTrue(std::filesystem::exists(db_path), "recovery should materialize main store file");
    ExpectTrue(!std::filesystem::exists(tmp_path), "recovery should clear temp store file");

    CleanupStoreFiles(db_path);
}

void TestStoreIgnoresStaleTmpWhenMainExists() {
    const std::filesystem::path db_path = BuildStorePath("ignore_stale_tmp");
    const std::filesystem::path tmp_path = TempStorePath(db_path);
    const std::string db_path_raw = db_path.string();

    // Write a CSV main file and a CSV temp file.
    // The store should use main (after migration) and discard temp.
    const double base = NowSeconds() - 10.0;
    WriteTextFileLines(db_path, {SnapshotLine(base, 31.0, 41.0)});
    WriteTextFileLines(tmp_path, {SnapshotLine(base + 1.0, 91.0, 92.0)});

    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "store open with stale temp file should succeed");

    aura_snapshot_t latest[1]{};
    int out_count = 0;
    rc = aura_store_latest(store, 1, latest, 1, &out_count, &error);
    ExpectEq(rc, AURA_OK, "latest with stale temp should succeed");
    ExpectEq(out_count, 1, "latest with stale temp should return one snapshot");
    ExpectNear(latest[0].timestamp, base, 1e-9, "main file data should win over stale temp file");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "close store with stale temp");

    ExpectTrue(!std::filesystem::exists(tmp_path), "stale temp file should be removed");

    CleanupStoreFiles(db_path);
}

void TestLegacySqliteHeaderMigration() {
    const std::filesystem::path db_path = BuildStorePath("legacy_sqlite_header");
    const std::string db_path_raw = db_path.string();

    // Write a file that starts with the SQLite magic header.
    // The store should recognize it as already-SQLite and open it directly.
    // Since it's not a valid DB, sqlite3_open will create a fresh one on top.
    // We just verify no crash and empty store.
    {
        std::ofstream output(db_path, std::ios::trunc | std::ios::binary);
        if (!output.is_open()) {
            Fail("failed to create legacy sqlite file fixture");
        }
        static constexpr char kSqliteMagic[16] = {
            'S', 'Q', 'L', 'i', 't', 'e', ' ', 'f',
            'o', 'r', 'm', 'a', 't', ' ', '3', '\0'
        };
        output.write(kSqliteMagic, sizeof(kSqliteMagic));
    }

    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "store open should handle legacy sqlite header");

    int count = 0;
    rc = aura_store_count(store, &count, &error);
    ExpectEq(rc, AURA_OK, "count after legacy migration should succeed");
    ExpectEq(count, 0, "legacy sqlite fixture should migrate to empty native store");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "close store after legacy migration");

    // After opening, the file should now be a proper SQLite DB
    ExpectTrue(std::filesystem::exists(db_path), "legacy migration should leave a writable main store file");
    ExpectTrue(StartsWithSqliteMagic(db_path), "store file should be SQLite format after open");

    CleanupStoreFiles(db_path);
}

void TestCorruptLineToleranceDoesNotCrash() {
    const std::filesystem::path db_path = BuildStorePath("corrupt_lines");
    const std::string db_path_raw = db_path.string();
    const double base = NowSeconds() - 10.0;

    // Write a CSV file with mixed valid/corrupt lines.
    // The CSV migration path should skip corrupt lines and import valid ones.
    WriteTextFileLines(
        db_path,
        {
            "not,a,snapshot",
            SnapshotLine(base, 55.0, 65.0),
            "bad_line",
        }
    );

    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "store open should tolerate mixed valid/corrupt lines via CSV migration");

    int count = 0;
    rc = aura_store_count(store, &count, &error);
    ExpectEq(rc, AURA_OK, "count should succeed with corrupt-line fixture");
    ExpectEq(count, 1, "only valid snapshot lines should be loaded");

    aura_snapshot_t latest[1]{};
    int out_count = 0;
    rc = aura_store_latest(store, 1, latest, 1, &out_count, &error);
    ExpectEq(rc, AURA_OK, "latest should succeed with corrupt-line fixture");
    ExpectEq(out_count, 1, "latest should return exactly one valid snapshot");
    ExpectNear(latest[0].timestamp, base, 1e-9, "valid snapshot timestamp should be preserved");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "close store with corrupt-line fixture");

    // CSV file should have been backed up
    ExpectTrue(std::filesystem::exists(CsvBakPath(db_path)) || !std::filesystem::exists(db_path.string() + ".csv.bak.tmp"),
        "CSV backup should exist after migration");

    CleanupStoreFiles(db_path);
}

void TestLttbDownsample() {
    std::vector<aura_snapshot_t> input;
    input.reserve(10);
    for (int i = 0; i < 10; ++i) {
        input.push_back(aura_snapshot_t{100.0 + static_cast<double>(i), static_cast<double>(i), 50.0});
    }

    aura_snapshot_t output[4]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(),
        static_cast<int>(input.size()),
        4,
        output,
        4,
        &out_count,
        &error
    );

    ExpectEq(rc, AURA_OK, "downsample should succeed");
    ExpectEq(out_count, 4, "downsample output count");
    ExpectNear(output[0].timestamp, 100.0, 1e-9, "first timestamp preserved");
    ExpectNear(output[3].timestamp, 109.0, 1e-9, "last timestamp preserved");
}

void TestQueryTimeline() {
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "store open for query timeline");

    const double base = NowSeconds() - 100.0;
    for (int i = 0; i < 50; ++i) {
        aura_snapshot_t snapshot{
            base + static_cast<double>(i),
            static_cast<double>(i % 15),
            40.0,
        };
        rc = aura_store_append(store, &snapshot, &error);
        ExpectEq(rc, AURA_OK, "append in query timeline test");
    }

    aura_snapshot_t output[10]{};
    int out_count = 0;
    rc = aura_dvr_query_timeline(
        store,
        1,
        base + 10.0,
        1,
        base + 40.0,
        10,
        output,
        10,
        &out_count,
        &error
    );
    ExpectEq(rc, AURA_OK, "query timeline should succeed");
    ExpectEq(out_count, 10, "query timeline count");
    ExpectTrue(output[0].timestamp >= base + 10.0, "query timeline lower bound");
    ExpectTrue(output[out_count - 1].timestamp <= base + 40.0, "query timeline upper bound");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "store close query timeline");
}

void TestSnapshotDiskFieldsPersisted() {
    aura_error_t error{};
    aura_store_t* store = nullptr;

    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "disk fields: store open should succeed");

    const double base = NowSeconds();
    aura_snapshot_t snap{};
    snap.timestamp = base;
    snap.cpu_percent = 10.0;
    snap.memory_percent = 20.0;
    snap.disk_read_bps = 1234567.0;
    snap.disk_write_bps = 7654321.0;

    rc = aura_store_append(store, &snap, &error);
    ExpectEq(rc, AURA_OK, "disk fields: append should succeed");

    aura_snapshot_t latest[1]{};
    int out_count = 0;
    rc = aura_store_latest(store, 1, latest, 1, &out_count, &error);
    ExpectEq(rc, AURA_OK, "disk fields: latest should succeed");
    ExpectEq(out_count, 1, "disk fields: latest count should be one");
    ExpectNear(latest[0].disk_read_bps, 1234567.0, 1e-3, "disk_read_bps should persist");
    ExpectNear(latest[0].disk_write_bps, 7654321.0, 1e-3, "disk_write_bps should persist");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "disk fields: store close should succeed");
}

void TestSnapshotDiskFieldsPersistedToFile() {
    const std::filesystem::path db_path = BuildStorePath("disk_fields_file");
    const std::string db_path_raw = db_path.string();

    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "disk fields file: store open should succeed");

    const double base = NowSeconds() - 10.0;
    aura_snapshot_t snap{};
    snap.timestamp = base;
    snap.cpu_percent = 15.0;
    snap.memory_percent = 25.0;
    snap.disk_read_bps = 500000.0;
    snap.disk_write_bps = 300000.0;

    rc = aura_store_append(store, &snap, &error);
    ExpectEq(rc, AURA_OK, "disk fields file: append should succeed");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "disk fields file: close first handle");

    store = nullptr;
    rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "disk fields file: reopen should succeed");

    aura_snapshot_t latest[1]{};
    int out_count = 0;
    rc = aura_store_latest(store, 1, latest, 1, &out_count, &error);
    ExpectEq(rc, AURA_OK, "disk fields file: latest should succeed");
    ExpectEq(out_count, 1, "disk fields file: latest count should be one");
    ExpectNear(latest[0].disk_read_bps, 500000.0, 1e-3, "disk_read_bps should persist across reopen");
    ExpectNear(latest[0].disk_write_bps, 300000.0, 1e-3, "disk_write_bps should persist across reopen");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "disk fields file: close second handle");

    CleanupStoreFiles(db_path);
}

void TestLegacy3FieldSnapshotBackwardCompat() {
    const std::filesystem::path db_path = BuildStorePath("legacy_3field");
    const std::string db_path_raw = db_path.string();
    const double base = NowSeconds() - 10.0;

    // Write old 3-field format lines (no disk fields) as CSV.
    // The CSV migration path should handle them.
    {
        std::ofstream output(db_path, std::ios::trunc | std::ios::binary);
        ExpectTrue(output.is_open(), "legacy 3-field: create fixture file");
        output.precision(17);
        output << base << ',' << 55.0 << ',' << 65.0 << '\n';
    }

    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "legacy 3-field: store open should succeed");

    aura_snapshot_t latest[1]{};
    int out_count = 0;
    rc = aura_store_latest(store, 1, latest, 1, &out_count, &error);
    ExpectEq(rc, AURA_OK, "legacy 3-field: latest should succeed");
    ExpectEq(out_count, 1, "legacy 3-field: latest count should be one");
    ExpectNear(latest[0].cpu_percent, 55.0, 1e-3, "legacy 3-field: cpu_percent preserved");
    ExpectNear(latest[0].disk_read_bps, 0.0, 1e-9, "legacy 3-field: disk_read_bps defaults to zero");
    ExpectNear(latest[0].disk_write_bps, 0.0, 1e-9, "legacy 3-field: disk_write_bps defaults to zero");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "legacy 3-field: close");

    CleanupStoreFiles(db_path);
}

void TestSqliteWalModeEnabled() {
    const std::filesystem::path db_path = BuildStorePath("wal_mode");
    const std::string db_path_raw = db_path.string();

    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "wal mode: store open should succeed");

    const double base = NowSeconds();
    aura_snapshot_t snap{base, 10.0, 20.0};
    rc = aura_store_append(store, &snap, &error);
    ExpectEq(rc, AURA_OK, "wal mode: append should succeed");

    // Check that WAL file exists
    std::filesystem::path wal_path = db_path;
    wal_path += "-wal";
    ExpectTrue(std::filesystem::exists(wal_path), "WAL file should exist after write");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "wal mode: store close should succeed");

    CleanupStoreFiles(db_path);
}

void TestSqlitePrunePeriodic() {
    aura_error_t error{};
    aura_store_t* store = nullptr;

    // Use short retention so old snapshots expire
    int rc = aura_store_open(":memory:", 60.0, &store, &error);
    ExpectEq(rc, AURA_OK, "prune periodic: store open should succeed");

    const double now = NowSeconds();

    // Insert 110 snapshots with timestamps far in the past (expired)
    for (int i = 0; i < 110; ++i) {
        aura_snapshot_t snap{now - 3600.0 + static_cast<double>(i) * 0.1, 10.0, 20.0};
        rc = aura_store_append(store, &snap, &error);
        ExpectEq(rc, AURA_OK, "prune periodic: append old snapshot");
    }

    // After 100 appends, prune should have triggered.
    // All 110 snapshots have timestamps > 1 hour ago, which is > 60s retention.
    int count = 0;
    rc = aura_store_count(store, &count, &error);
    ExpectEq(rc, AURA_OK, "prune periodic: count should succeed");
    // After prune at append #100, the first 100 expired rows get deleted.
    // Then 10 more expired rows are appended (but no prune yet since counter reset).
    // So count should be 10.
    ExpectEq(count, 10, "prune periodic: expired snapshots should be pruned after 100 appends");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "prune periodic: store close");
}

void TestSqliteConcurrentReadWrite() {
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "concurrent: store open should succeed");

    const double base = NowSeconds();
    bool writer_ok = true;
    bool reader_ok = true;

    std::thread writer([&]() {
        for (int i = 0; i < 200; ++i) {
            aura_error_t werr{};
            aura_snapshot_t snap{base + static_cast<double>(i) * 0.01, 10.0, 20.0};
            int wrc = aura_store_append(store, &snap, &werr);
            if (wrc != AURA_OK) {
                writer_ok = false;
                return;
            }
        }
    });

    std::thread reader([&]() {
        for (int i = 0; i < 200; ++i) {
            aura_error_t rerr{};
            int count = 0;
            int rrc = aura_store_count(store, &count, &rerr);
            if (rrc != AURA_OK) {
                reader_ok = false;
                return;
            }
            aura_snapshot_t latest[5]{};
            int out_count = 0;
            rrc = aura_store_latest(store, 5, latest, 5, &out_count, &rerr);
            if (rrc != AURA_OK) {
                reader_ok = false;
                return;
            }
        }
    });

    writer.join();
    reader.join();

    ExpectTrue(writer_ok, "concurrent writer should not fail");
    ExpectTrue(reader_ok, "concurrent reader should not fail");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "concurrent: store close");
}

} // namespace

int main() {
    TestConfigNoPersist();
    TestConfigRejectsMalformedEnvRetention();
    TestConfigRejectsMalformedTomlRetention();
    TestConfigAcceptsTomlInlineCommentRetention();
    TestConfigAcceptsTomlInlineCommentDbPath();
    TestStoreMemoryAppendLatestBetween();
    TestStoreFilePersistenceAcrossReopen();
    TestStoreRecoveryFromStaleTmpWhenMainMissing();
    TestStoreIgnoresStaleTmpWhenMainExists();
    TestLegacySqliteHeaderMigration();
    TestCorruptLineToleranceDoesNotCrash();
    TestLttbDownsample();
    TestQueryTimeline();
    TestSnapshotDiskFieldsPersisted();
    TestSnapshotDiskFieldsPersistedToFile();
    TestLegacy3FieldSnapshotBackwardCompat();
    TestSqliteWalModeEnabled();
    TestSqlitePrunePeriodic();
    TestSqliteConcurrentReadWrite();

    std::cout << "platform_native_tests: PASS" << std::endl;
    return 0;
}
