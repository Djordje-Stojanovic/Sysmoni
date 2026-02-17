#pragma once

// ---------------------------------------------------------------------------
// Forward declarations for all platform test functions.
// Each is defined in one of the six translation units:
//   test_platform_config.cpp  (5 config tests)
//   test_platform_store.cpp   (20 store tests)
//   test_platform_dvr.cpp     (35 DVR tests)
//   test_platform_native.cpp  (6 remaining tests)
//   test_dvr_stats.cpp        (35 stats engine tests)
//   test_dvr_export.cpp       (20 export tests)
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

// Network field store persistence tests (4)
void TestSnapshotNetFieldsPersisted();
void TestSnapshotNetFieldsPersistedToFile();
void TestSnapshotNetFieldsDefaultToZero();
void TestSnapshotNetFieldsInBetweenQuery();

// Schema migration tests (5)
void TestSchemaMigrationAddsNetColumns();
void TestSchemaMigrationIdempotent();
void TestCsvMigration7FieldLines();
void TestCsvMigration5FieldLinesNetDefaultZero();
void TestCsvMigrationMixed5And7FieldLines();

// DVR tests — LTTB with network fields (5)
void TestLttbNetRecvSpikePreserved();
void TestLttbNetSentSpikePreserved();
void TestLttbPreservesAllFieldsIncludingNet();
void TestLttbNetFieldsZeroRangeSkipped();
void TestLttbSixFieldNormalizationNetVsCpu();

// Validation tests — network fields (3)
void TestValidateSnapshotRejectsNegativeNetRecv();
void TestValidateSnapshotRejectsNegativeNetSent();
void TestValidateSnapshotRejectsInfiniteNetRecv();

// C API round-trip tests — network fields (2)
void TestAbiSnapshotNetFieldsRoundTrip();
void TestAbiSnapshotNetFieldsZeroInit();

// Alert engine tests (47)
void TestAlertEngineCreateDestroy();
void TestAlertEngineDestroyNull();
void TestAlertEngineCreateNullOut();
void TestAlertAddRule();
void TestAlertAddRuleDuplicateId();
void TestAlertRemoveRule();
void TestAlertRemoveRuleNotFound();
void TestAlertGetRule();
void TestAlertGetRuleNotFound();
void TestAlertAddRuleValidation();
void TestAlertEvalBelowThresholdIdle();
void TestAlertEvalInstantTrigger();
void TestAlertEvalPending();
void TestAlertEvalBelowComparator();
void TestAlertEvalNullPointers();
void TestAlertFullCycle();
void TestAlertPendingClearsEarly();
void TestAlertTriggeredStays();
void TestAlertCooldownIgnoresCondition();
void TestAlertZeroCooldown();
void TestAlertZeroSustained();
void TestAlertAcknowledgeTriggered();
void TestAlertAcknowledgeNonTriggered();
void TestAlertPeakAbove();
void TestAlertPeakBelow();
void TestAlertLastValue();
void TestAlertDuration();
void TestAlertMetricCpu();
void TestAlertMetricMemory();
void TestAlertMetricDiskRead();
void TestAlertMetricDiskWrite();
void TestAlertMetricNetRecv();
void TestAlertMetricNetSent();
void TestAlertNaNThresholdRejected();
void TestAlertExactThresholdNoTrigger();
void TestAlertZeroBothTimers();
void TestAlertMultipleRulesIndependent();
void TestAlertNaNSnapshotFieldSafe();
void TestAlertInfThresholdRejected();
void TestAlertHistoryRecords();
void TestAlertHistoryClear();
void TestAlertHistoryCapacity();
void TestAlertHistoryNullBuffer();
void TestAlertGetActiveEmpty();
void TestAlertGetActiveMultiple();
void TestAlertAbiNullEngine();
void TestAlertAbiNullErrorSafe();

// DVR Statistics Engine — basic correctness (8)
void TestStatsEmptyInput();
void TestStatsSingleSnapshot();
void TestStatsTwoSnapshots();
void TestStatsKnownLinearDistribution();
void TestStatsConstantValues();
void TestStatsTimestampRange();
void TestStatsCountField();
void TestStatsAllMetricsPopulated();

// DVR Statistics Engine — percentile correctness (7)
void TestStatsP50EvenCount();
void TestStatsP50OddCount();
void TestStatsP95LargeInput();
void TestStatsP99LargeInput();
void TestStatsPercentilesOrdered();
void TestStatsPercentilesWithinMinMax();
void TestStatsPercentilesThreeValues();

// DVR Statistics Engine — standard deviation (4)
void TestStatsStddevZeroForConstant();
void TestStatsStddevKnownValues();
void TestStatsStddevSingleValue();
void TestStatsStddevLargeSpread();

// DVR Statistics Engine — per-metric isolation (6)
void TestStatsCpuMetricIsolated();
void TestStatsMemoryMetricIsolated();
void TestStatsDiskReadMetricIsolated();
void TestStatsDiskWriteMetricIsolated();
void TestStatsNetRecvMetricIsolated();
void TestStatsNetSentMetricIsolated();

// DVR Statistics Engine — C ABI safety (6)
void TestStatsAbiNullSnapshots();
void TestStatsAbiNullOutStats();
void TestStatsAbiNullError();
void TestStatsAbiNegativeCount();
void TestStatsAbiZeroCount();
void TestStatsAbiNullErrorInvalid();

// DVR Statistics Engine — store integration (4)
void TestDvrComputeStatsFullRange();
void TestDvrComputeStatsSubRange();
void TestDvrComputeStatsEmptyRange();
void TestDvrComputeStatsNullStore();

// DVR Export — JSON (10)
void TestExportJsonBasic();
void TestExportJsonWithStats();
void TestExportJsonWithoutStats();
void TestExportJsonEmptyRange();
void TestExportJsonFieldValues();
void TestExportJsonVersionField();
void TestExportJsonTimePrecision();
void TestExportJsonNullStore();
void TestExportJsonNullFilePath();
void TestExportJsonLargeDataset();

// DVR Export — CSV (10)
void TestExportCsvBasic();
void TestExportCsvHeader();
void TestExportCsvFieldCount();
void TestExportCsvEmptyRange();
void TestExportCsvRoundTrip();
void TestExportCsvTimePrecision();
void TestExportCsvNullStore();
void TestExportCsvNullFilePath();
void TestExportCsvNullError();
void TestExportCsvLargeDataset();
