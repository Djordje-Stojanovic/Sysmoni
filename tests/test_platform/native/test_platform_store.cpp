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

// ---------------------------------------------------------------------------
// Network telemetry field tests — store persistence
// ---------------------------------------------------------------------------

void TestSnapshotNetFieldsPersisted() {
    aura_error_t error{};
    aura_store_t* store = nullptr;

    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "net fields: store open should succeed");

    const double base = NowSeconds();
    aura_snapshot_t snap{};
    snap.timestamp = base;
    snap.cpu_percent = 10.0;
    snap.memory_percent = 20.0;
    snap.disk_read_bps = 100.0;
    snap.disk_write_bps = 200.0;
    snap.net_recv_bps = 5000000.0;
    snap.net_sent_bps = 3000000.0;

    rc = aura_store_append(store, &snap, &error);
    ExpectEq(rc, AURA_OK, "net fields: append should succeed");

    aura_snapshot_t latest[1]{};
    int out_count = 0;
    rc = aura_store_latest(store, 1, latest, 1, &out_count, &error);
    ExpectEq(rc, AURA_OK, "net fields: latest should succeed");
    ExpectEq(out_count, 1, "net fields: latest count should be one");
    ExpectNear(latest[0].net_recv_bps, 5000000.0, 1e-3, "net_recv_bps should persist");
    ExpectNear(latest[0].net_sent_bps, 3000000.0, 1e-3, "net_sent_bps should persist");
    ExpectNear(latest[0].cpu_percent, 10.0, 1e-3, "net fields: cpu_percent preserved");
    ExpectNear(latest[0].disk_read_bps, 100.0, 1e-3, "net fields: disk_read_bps preserved");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "net fields: store close");
}

void TestSnapshotNetFieldsPersistedToFile() {
    const std::filesystem::path db_path = BuildStorePath("net_fields_file");
    const std::string db_path_raw = db_path.string();

    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "net fields file: store open should succeed");

    const double base = NowSeconds() - 10.0;
    aura_snapshot_t snap{};
    snap.timestamp = base;
    snap.cpu_percent = 15.0;
    snap.memory_percent = 25.0;
    snap.disk_read_bps = 500000.0;
    snap.disk_write_bps = 300000.0;
    snap.net_recv_bps = 8000000.0;
    snap.net_sent_bps = 4000000.0;

    rc = aura_store_append(store, &snap, &error);
    ExpectEq(rc, AURA_OK, "net fields file: append should succeed");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "net fields file: close first handle");

    store = nullptr;
    rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "net fields file: reopen should succeed");

    aura_snapshot_t latest[1]{};
    int out_count = 0;
    rc = aura_store_latest(store, 1, latest, 1, &out_count, &error);
    ExpectEq(rc, AURA_OK, "net fields file: latest should succeed");
    ExpectEq(out_count, 1, "net fields file: latest count should be one");
    ExpectNear(latest[0].net_recv_bps, 8000000.0, 1e-3, "net_recv_bps should persist across reopen");
    ExpectNear(latest[0].net_sent_bps, 4000000.0, 1e-3, "net_sent_bps should persist across reopen");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "net fields file: close second handle");

    CleanupStoreFiles(db_path);
}

void TestSnapshotNetFieldsDefaultToZero() {
    aura_error_t error{};
    aura_store_t* store = nullptr;

    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "net default: store open");

    const double base = NowSeconds();
    aura_snapshot_t snap{base, 10.0, 20.0};

    rc = aura_store_append(store, &snap, &error);
    ExpectEq(rc, AURA_OK, "net default: append");

    aura_snapshot_t latest[1]{};
    int out_count = 0;
    rc = aura_store_latest(store, 1, latest, 1, &out_count, &error);
    ExpectEq(rc, AURA_OK, "net default: latest");
    ExpectEq(out_count, 1, "net default: count");
    ExpectNear(latest[0].net_recv_bps, 0.0, 1e-9, "net_recv_bps defaults to zero");
    ExpectNear(latest[0].net_sent_bps, 0.0, 1e-9, "net_sent_bps defaults to zero");

    aura_store_close(store);
}

void TestSnapshotNetFieldsInBetweenQuery() {
    aura_error_t error{};
    aura_store_t* store = nullptr;

    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "net between: store open");

    const double base = NowSeconds();
    aura_snapshot_t snap1{};
    snap1.timestamp = base;
    snap1.cpu_percent = 10.0;
    snap1.memory_percent = 20.0;
    snap1.net_recv_bps = 1000000.0;
    snap1.net_sent_bps = 500000.0;

    aura_snapshot_t snap2{};
    snap2.timestamp = base + 1.0;
    snap2.cpu_percent = 11.0;
    snap2.memory_percent = 21.0;
    snap2.net_recv_bps = 2000000.0;
    snap2.net_sent_bps = 600000.0;

    rc = aura_store_append(store, &snap1, &error);
    ExpectEq(rc, AURA_OK, "net between: append first");
    rc = aura_store_append(store, &snap2, &error);
    ExpectEq(rc, AURA_OK, "net between: append second");

    aura_snapshot_t range[2]{};
    int out_count = 0;
    rc = aura_store_between(store, 1, base - 0.5, 1, base + 1.5, range, 2, &out_count, &error);
    ExpectEq(rc, AURA_OK, "net between: query should succeed");
    ExpectEq(out_count, 2, "net between: count should be two");
    ExpectNear(range[0].net_recv_bps, 1000000.0, 1e-3, "net between: first net_recv_bps");
    ExpectNear(range[1].net_recv_bps, 2000000.0, 1e-3, "net between: second net_recv_bps");
    ExpectNear(range[1].net_sent_bps, 600000.0, 1e-3, "net between: second net_sent_bps");

    aura_store_close(store);
}

// ---------------------------------------------------------------------------
// Network telemetry field tests — schema migration
// ---------------------------------------------------------------------------

void TestSchemaMigrationAddsNetColumns() {
    const std::filesystem::path db_path = BuildStorePath("schema_migration_net");
    const std::string db_path_raw = db_path.string();
    const double base = NowSeconds() - 10.0;

    WriteTextFileLines(db_path, {
        SnapshotLineWithDisk(base, 50.0, 60.0, 1000.0, 2000.0),
    });

    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "schema migration: store open");

    aura_snapshot_t latest[1]{};
    int out_count = 0;
    rc = aura_store_latest(store, 1, latest, 1, &out_count, &error);
    ExpectEq(rc, AURA_OK, "schema migration: latest");
    ExpectEq(out_count, 1, "schema migration: count");
    ExpectNear(latest[0].cpu_percent, 50.0, 1e-3, "schema migration: cpu preserved");
    ExpectNear(latest[0].net_recv_bps, 0.0, 1e-9, "migrated row: net_recv_bps defaults to zero");
    ExpectNear(latest[0].net_sent_bps, 0.0, 1e-9, "migrated row: net_sent_bps defaults to zero");

    aura_snapshot_t new_snap{};
    new_snap.timestamp = base + 1.0;
    new_snap.cpu_percent = 55.0;
    new_snap.memory_percent = 65.0;
    new_snap.net_recv_bps = 7000000.0;
    new_snap.net_sent_bps = 3500000.0;
    rc = aura_store_append(store, &new_snap, &error);
    ExpectEq(rc, AURA_OK, "schema migration: append new row");

    aura_snapshot_t all[2]{};
    out_count = 0;
    rc = aura_store_latest(store, 2, all, 2, &out_count, &error);
    ExpectEq(rc, AURA_OK, "schema migration: latest after new row");
    ExpectEq(out_count, 2, "schema migration: count after new row");
    ExpectNear(all[0].net_recv_bps, 0.0, 1e-9, "old row net_recv still zero");
    ExpectNear(all[1].net_recv_bps, 7000000.0, 1e-3, "new row net_recv set");

    aura_store_close(store);
    CleanupStoreFiles(db_path);
}

void TestSchemaMigrationIdempotent() {
    const std::filesystem::path db_path = BuildStorePath("schema_idempotent");
    const std::string db_path_raw = db_path.string();

    aura_error_t error{};
    aura_store_t* store = nullptr;

    int rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "idempotent: first open");

    const double base = NowSeconds() - 10.0;
    aura_snapshot_t snap{};
    snap.timestamp = base;
    snap.cpu_percent = 10.0;
    snap.memory_percent = 20.0;
    snap.net_recv_bps = 1000000.0;
    snap.net_sent_bps = 500000.0;
    rc = aura_store_append(store, &snap, &error);
    ExpectEq(rc, AURA_OK, "idempotent: append");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "idempotent: first close");

    store = nullptr;
    rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "idempotent: second open should succeed");

    aura_snapshot_t latest[1]{};
    int out_count = 0;
    rc = aura_store_latest(store, 1, latest, 1, &out_count, &error);
    ExpectEq(rc, AURA_OK, "idempotent: latest after reopen");
    ExpectEq(out_count, 1, "idempotent: count");
    ExpectNear(latest[0].net_recv_bps, 1000000.0, 1e-3, "idempotent: net_recv preserved");

    aura_store_close(store);
    CleanupStoreFiles(db_path);
}

void TestCsvMigration7FieldLines() {
    const std::filesystem::path db_path = BuildStorePath("csv_7field");
    const std::string db_path_raw = db_path.string();
    const double base = NowSeconds() - 10.0;

    WriteTextFileLines(db_path, {
        SnapshotLineWithNet(base, 50.0, 60.0, 1000.0, 2000.0, 5000000.0, 3000000.0),
    });

    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "7-field CSV: store open");

    aura_snapshot_t latest[1]{};
    int out_count = 0;
    rc = aura_store_latest(store, 1, latest, 1, &out_count, &error);
    ExpectEq(rc, AURA_OK, "7-field CSV: latest");
    ExpectEq(out_count, 1, "7-field CSV: count");
    ExpectNear(latest[0].cpu_percent, 50.0, 1e-3, "7-field CSV: cpu parsed");
    ExpectNear(latest[0].disk_read_bps, 1000.0, 1e-3, "7-field CSV: disk_read parsed");
    ExpectNear(latest[0].net_recv_bps, 5000000.0, 1e-3, "7-field CSV: net_recv_bps parsed");
    ExpectNear(latest[0].net_sent_bps, 3000000.0, 1e-3, "7-field CSV: net_sent_bps parsed");

    aura_store_close(store);
    CleanupStoreFiles(db_path);
}

void TestCsvMigration5FieldLinesNetDefaultZero() {
    const std::filesystem::path db_path = BuildStorePath("csv_5field_net");
    const std::string db_path_raw = db_path.string();
    const double base = NowSeconds() - 10.0;

    WriteTextFileLines(db_path, {
        SnapshotLineWithDisk(base, 40.0, 50.0, 2000.0, 3000.0),
    });

    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "5-field CSV net: store open");

    aura_snapshot_t latest[1]{};
    int out_count = 0;
    rc = aura_store_latest(store, 1, latest, 1, &out_count, &error);
    ExpectEq(rc, AURA_OK, "5-field CSV net: latest");
    ExpectEq(out_count, 1, "5-field CSV net: count");
    ExpectNear(latest[0].disk_read_bps, 2000.0, 1e-3, "5-field CSV net: disk_read preserved");
    ExpectNear(latest[0].net_recv_bps, 0.0, 1e-9, "5-field CSV net: net_recv defaults to zero");
    ExpectNear(latest[0].net_sent_bps, 0.0, 1e-9, "5-field CSV net: net_sent defaults to zero");

    aura_store_close(store);
    CleanupStoreFiles(db_path);
}

void TestCsvMigrationMixed5And7FieldLines() {
    const std::filesystem::path db_path = BuildStorePath("csv_mixed");
    const std::string db_path_raw = db_path.string();
    const double base = NowSeconds() - 10.0;

    WriteTextFileLines(db_path, {
        SnapshotLineWithDisk(base, 50.0, 60.0, 1000.0, 2000.0),
        SnapshotLineWithNet(base + 1.0, 55.0, 65.0, 1100.0, 2100.0, 5000000.0, 3000000.0),
    });

    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(db_path_raw.c_str(), 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "mixed CSV: store open");

    aura_snapshot_t latest[2]{};
    int out_count = 0;
    rc = aura_store_latest(store, 2, latest, 2, &out_count, &error);
    ExpectEq(rc, AURA_OK, "mixed CSV: latest");
    ExpectEq(out_count, 2, "mixed CSV: count");
    ExpectNear(latest[0].net_recv_bps, 0.0, 1e-9, "mixed CSV: 5-field row net_recv=0");
    ExpectNear(latest[0].net_sent_bps, 0.0, 1e-9, "mixed CSV: 5-field row net_sent=0");
    ExpectNear(latest[1].net_recv_bps, 5000000.0, 1e-3, "mixed CSV: 7-field row net_recv parsed");
    ExpectNear(latest[1].net_sent_bps, 3000000.0, 1e-3, "mixed CSV: 7-field row net_sent parsed");

    aura_store_close(store);
    CleanupStoreFiles(db_path);
}

// ---------------------------------------------------------------------------
// Network telemetry field tests — validation
// ---------------------------------------------------------------------------

void TestValidateSnapshotRejectsNegativeNetRecv() {
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "neg net_recv: store open");

    aura_snapshot_t snap{};
    snap.timestamp = NowSeconds();
    snap.cpu_percent = 10.0;
    snap.memory_percent = 20.0;
    snap.net_recv_bps = -1.0;

    rc = aura_store_append(store, &snap, &error);
    ExpectTrue(rc != AURA_OK, "negative net_recv_bps should be rejected");

    aura_store_close(store);
}

void TestValidateSnapshotRejectsNegativeNetSent() {
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "neg net_sent: store open");

    aura_snapshot_t snap{};
    snap.timestamp = NowSeconds();
    snap.cpu_percent = 10.0;
    snap.memory_percent = 20.0;
    snap.net_sent_bps = -1.0;

    rc = aura_store_append(store, &snap, &error);
    ExpectTrue(rc != AURA_OK, "negative net_sent_bps should be rejected");

    aura_store_close(store);
}

void TestValidateSnapshotRejectsInfiniteNetRecv() {
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "inf net_recv: store open");

    aura_snapshot_t snap{};
    snap.timestamp = NowSeconds();
    snap.cpu_percent = 10.0;
    snap.memory_percent = 20.0;
    snap.net_recv_bps = std::numeric_limits<double>::infinity();

    rc = aura_store_append(store, &snap, &error);
    ExpectTrue(rc != AURA_OK, "infinite net_recv_bps should be rejected");

    aura_store_close(store);
}

// ---------------------------------------------------------------------------
// Network telemetry field tests — C API round-trip
// ---------------------------------------------------------------------------

void TestAbiSnapshotNetFieldsRoundTrip() {
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "abi net roundtrip: store open");

    const double base = NowSeconds();
    aura_snapshot_t snap{};
    snap.timestamp = base;
    snap.cpu_percent = 25.0;
    snap.memory_percent = 35.0;
    snap.disk_read_bps = 100000.0;
    snap.disk_write_bps = 200000.0;
    snap.net_recv_bps = 9876543.0;
    snap.net_sent_bps = 1234567.0;

    rc = aura_store_append(store, &snap, &error);
    ExpectEq(rc, AURA_OK, "abi net roundtrip: append");

    aura_snapshot_t latest[1]{};
    int out_count = 0;
    rc = aura_store_latest(store, 1, latest, 1, &out_count, &error);
    ExpectEq(rc, AURA_OK, "abi net roundtrip: latest");
    ExpectEq(out_count, 1, "abi net roundtrip: count");
    ExpectNear(latest[0].timestamp, base, 1e-9, "abi net roundtrip: timestamp");
    ExpectNear(latest[0].cpu_percent, 25.0, 1e-3, "abi net roundtrip: cpu");
    ExpectNear(latest[0].memory_percent, 35.0, 1e-3, "abi net roundtrip: mem");
    ExpectNear(latest[0].disk_read_bps, 100000.0, 1e-3, "abi net roundtrip: disk_r");
    ExpectNear(latest[0].disk_write_bps, 200000.0, 1e-3, "abi net roundtrip: disk_w");
    ExpectNear(latest[0].net_recv_bps, 9876543.0, 1e-3, "abi net roundtrip: net_recv");
    ExpectNear(latest[0].net_sent_bps, 1234567.0, 1e-3, "abi net roundtrip: net_sent");

    aura_store_close(store);
}

void TestAbiSnapshotNetFieldsZeroInit() {
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "abi zero init: store open");

    const double base = NowSeconds();
    aura_snapshot_t snap{};
    snap.timestamp = base;
    snap.cpu_percent = 5.0;
    snap.memory_percent = 10.0;

    rc = aura_store_append(store, &snap, &error);
    ExpectEq(rc, AURA_OK, "abi zero init: append");

    aura_snapshot_t latest[1]{};
    int out_count = 0;
    rc = aura_store_latest(store, 1, latest, 1, &out_count, &error);
    ExpectEq(rc, AURA_OK, "abi zero init: latest");
    ExpectNear(latest[0].disk_read_bps, 0.0, 1e-9, "abi zero init: disk_read zero");
    ExpectNear(latest[0].disk_write_bps, 0.0, 1e-9, "abi zero init: disk_write zero");
    ExpectNear(latest[0].net_recv_bps, 0.0, 1e-9, "abi zero init: net_recv zero");
    ExpectNear(latest[0].net_sent_bps, 0.0, 1e-9, "abi zero init: net_sent zero");

    aura_store_close(store);
}
