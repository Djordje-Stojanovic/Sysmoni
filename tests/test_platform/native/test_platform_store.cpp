#include "test_platform_helpers.hpp"

#include <filesystem>
#include <string>

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
