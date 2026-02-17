#pragma once

// ---------------------------------------------------------------------------
// Forward declarations for all 47 platform test functions.
// Each is defined in one of the four translation units:
//   test_platform_config.cpp  (5 config tests)
//   test_platform_store.cpp   (6 store tests)
//   test_platform_dvr.cpp     (30 DVR tests)
//   test_platform_native.cpp  (6 remaining tests)
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

// DVR tests — basic LTTB correctness
void TestLttbDownsample();
void TestLttbInputSmallerThanTarget();
void TestLttbInputEqualsTarget();
void TestLttbTargetTwo();
void TestLttbPreservesAllFields();

// DVR tests — multi-field LTTB behavior
void TestLttbMemorySpikePreserved();
void TestLttbDiskReadSpikePreserved();
void TestLttbDiskWriteSpikePreserved();
void TestLttbMultipleFieldSpikesAtDifferentPoints();
void TestLttbConstantCpuVaryingMemory();
void TestLttbAllFieldsConstant();
void TestLttbCpuOnlyVaryingMatchesOriginal();
void TestLttbNormalizationAcrossFieldScales();

// DVR tests — edge cases and boundary conditions
void TestLttbTargetThree();
void TestLttbLargeInput();
void TestLttbExtremeFieldValues();
void TestLttbNearZeroFieldRange();
void TestLttbTimestampsMonotonicallyIncreasing();
void TestLttbOutputIsSubsetOfInput();

// DVR tests — C ABI safety
void TestLttbAbiNullInputWithPositiveCount();
void TestLttbAbiNullOutput();
void TestLttbAbiNullOutCount();
void TestLttbAbiCapacityTooSmall();
void TestLttbAbiNegativeInputCount();
void TestLttbAbiTargetLessThanTwo();
void TestLttbAbiNullError();

// DVR tests — QueryTimeline integration
void TestQueryTimeline();
void TestQueryTimelineEmptyRange();
void TestQueryTimelineAbiNullStore();
void TestQueryTimelineResolutionOne();

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
