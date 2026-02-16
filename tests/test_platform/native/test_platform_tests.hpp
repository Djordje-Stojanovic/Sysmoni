#pragma once

// ---------------------------------------------------------------------------
// Forward declarations for all 19 platform test functions.
// Each is defined in one of the three translation units:
//   test_platform_config.cpp  (5 config tests)
//   test_platform_store.cpp   (6 store tests)
//   test_platform_native.cpp  (8 remaining tests)
// ---------------------------------------------------------------------------

// Config tests
void TestConfigNoPersist();
void TestConfigRejectsMalformedEnvRetention();
void TestConfigRejectsMalformedTomlRetention();
void TestConfigAcceptsTomlInlineCommentRetention();
void TestConfigAcceptsTomlInlineCommentDbPath();

// Store tests
void TestStoreMemoryAppendLatestBetween();
void TestStoreFilePersistenceAcrossReopen();
void TestStoreRecoveryFromStaleTmpWhenMainMissing();
void TestStoreIgnoresStaleTmpWhenMainExists();
void TestLegacySqliteHeaderMigration();
void TestCorruptLineToleranceDoesNotCrash();

// DVR tests
void TestLttbDownsample();
void TestQueryTimeline();

// Snapshot field tests
void TestSnapshotDiskFieldsPersisted();
void TestSnapshotDiskFieldsPersistedToFile();

// Legacy compat test
void TestLegacy3FieldSnapshotBackwardCompat();

// SQLite tests
void TestSqliteWalModeEnabled();
void TestSqlitePrunePeriodic();

// Concurrency test
void TestSqliteConcurrentReadWrite();
