#include "test_platform_helpers.hpp"
#include "test_platform_tests.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Snapshot field tests
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Legacy compat test
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// SQLite tests
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Concurrency test
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// main — calls all 66 tests
// ---------------------------------------------------------------------------

int main() {
    // Config tests (5)
    TestConfigNoPersist();
    TestConfigRejectsMalformedEnvRetention();
    TestConfigRejectsMalformedTomlRetention();
    TestConfigAcceptsTomlInlineCommentRetention();
    TestConfigAcceptsTomlInlineCommentDbPath();

    // Store tests (6)
    TestStoreMemoryAppendLatestBetween();
    TestStoreFilePersistenceAcrossReopen();
    TestStoreRecoveryFromStaleTmpWhenMainMissing();
    TestStoreIgnoresStaleTmpWhenMainExists();
    TestLegacySqliteHeaderMigration();
    TestCorruptLineToleranceDoesNotCrash();

    // DVR tests — basic LTTB correctness (5)
    TestLttbDownsample();
    TestLttbInputSmallerThanTarget();
    TestLttbInputEqualsTarget();
    TestLttbTargetTwo();
    TestLttbPreservesAllFields();

    // DVR tests — multi-field LTTB behavior (8)
    TestLttbMemorySpikePreserved();
    TestLttbDiskReadSpikePreserved();
    TestLttbDiskWriteSpikePreserved();
    TestLttbMultipleFieldSpikesAtDifferentPoints();
    TestLttbConstantCpuVaryingMemory();
    TestLttbAllFieldsConstant();
    TestLttbCpuOnlyVaryingMatchesOriginal();
    TestLttbNormalizationAcrossFieldScales();

    // DVR tests — edge cases and boundary conditions (6)
    TestLttbTargetThree();
    TestLttbLargeInput();
    TestLttbExtremeFieldValues();
    TestLttbNearZeroFieldRange();
    TestLttbTimestampsMonotonicallyIncreasing();
    TestLttbOutputIsSubsetOfInput();

    // DVR tests — C ABI safety (7)
    TestLttbAbiNullInputWithPositiveCount();
    TestLttbAbiNullOutput();
    TestLttbAbiNullOutCount();
    TestLttbAbiCapacityTooSmall();
    TestLttbAbiNegativeInputCount();
    TestLttbAbiTargetLessThanTwo();
    TestLttbAbiNullError();

    // DVR tests — QueryTimeline integration (3)
    TestQueryTimeline();
    TestQueryTimelineEmptyRange();
    TestQueryTimelineAbiNullStore();
    TestQueryTimelineResolutionOne();

    // Snapshot field tests (2)
    TestSnapshotDiskFieldsPersisted();
    TestSnapshotDiskFieldsPersistedToFile();

    // Legacy compat test (1)
    TestLegacy3FieldSnapshotBackwardCompat();

    // SQLite tests (2)
    TestSqliteWalModeEnabled();
    TestSqlitePrunePeriodic();

    // Concurrency test (1)
    TestSqliteConcurrentReadWrite();

    // Network field store persistence tests (4)
    TestSnapshotNetFieldsPersisted();
    TestSnapshotNetFieldsPersistedToFile();
    TestSnapshotNetFieldsDefaultToZero();
    TestSnapshotNetFieldsInBetweenQuery();

    // Schema migration tests (5)
    TestSchemaMigrationAddsNetColumns();
    TestSchemaMigrationIdempotent();
    TestCsvMigration7FieldLines();
    TestCsvMigration5FieldLinesNetDefaultZero();
    TestCsvMigrationMixed5And7FieldLines();

    // DVR tests — LTTB with network fields (5)
    TestLttbNetRecvSpikePreserved();
    TestLttbNetSentSpikePreserved();
    TestLttbPreservesAllFieldsIncludingNet();
    TestLttbNetFieldsZeroRangeSkipped();
    TestLttbSixFieldNormalizationNetVsCpu();

    // Validation tests — network fields (3)
    TestValidateSnapshotRejectsNegativeNetRecv();
    TestValidateSnapshotRejectsNegativeNetSent();
    TestValidateSnapshotRejectsInfiniteNetRecv();

    // C API round-trip tests — network fields (2)
    TestAbiSnapshotNetFieldsRoundTrip();
    TestAbiSnapshotNetFieldsZeroInit();

    std::cout << "platform_native_tests: PASS" << std::endl;
    return 0;
}
