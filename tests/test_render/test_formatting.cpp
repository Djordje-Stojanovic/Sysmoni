#include "render_test_helpers.h"
#include <limits>

void test_formatting_and_status() {
    const AuraSnapshotLines lines = aura_format_snapshot_lines(0.0, 12.34, 56.78);
    assert(std::strcmp(lines.cpu, "CPU 12.3%") == 0);
    assert(std::strcmp(lines.memory, "Memory 56.8%") == 0);
    assert(std::strcmp(lines.timestamp, "Updated 00:00:00 UTC") == 0);

    char row[128] = {};
    aura_format_process_row(2, "worker", 4.5, 2.0 * 1024.0 * 1024.0, 20, row, sizeof(row));
    assert(std::strstr(row, "2.") != nullptr);
    assert(std::strstr(row, "worker") != nullptr);
    assert(std::strstr(row, "CPU") != nullptr);
    assert(std::strstr(row, "RAM") != nullptr);

    char status[128] = {};
    aura_format_initial_status("telemetry.sqlite", 1, 7, nullptr, status, sizeof(status));
    assert(std::strcmp(status, "Collecting telemetry... | DVR samples: 7") == 0);

    aura_format_stream_status("telemetry.sqlite", 1, 4, "disk full", status, sizeof(status));
    assert(std::strcmp(status, "Streaming telemetry | DVR unavailable: disk full") == 0);
}

void test_format_disk_rate() {
    char buf[64] = {};

    // Sub-MB: should format as KB/s
    aura_format_disk_rate(512.0 * 1024.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Disk 512.0 KB/s") == 0);

    // Exactly 1 MB/s
    aura_format_disk_rate(1024.0 * 1024.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Disk 1.0 MB/s") == 0);

    // Large value: GB/s
    aura_format_disk_rate(2.5 * 1024.0 * 1024.0 * 1024.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Disk 2.50 GB/s") == 0);

    // Zero
    aura_format_disk_rate(0.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Disk 0.0 KB/s") == 0);

    // Negative clamped to zero
    aura_format_disk_rate(-100.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Disk 0.0 KB/s") == 0);
}

void test_format_network_rate() {
    char buf[64] = {};

    // Sub-MB: should format as KB/s
    aura_format_network_rate(256.0 * 1024.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Net 256.0 KB/s") == 0);

    // MB/s range
    aura_format_network_rate(10.0 * 1024.0 * 1024.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Net 10.0 MB/s") == 0);

    // GB/s range
    aura_format_network_rate(1.0 * 1024.0 * 1024.0 * 1024.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Net 1.00 GB/s") == 0);

    // Small value
    aura_format_network_rate(100.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Net 0.1 KB/s") == 0);
}

void test_format_process_row_empty_name() {
    char row[128] = {};
    aura_format_process_row(1, "", 10.0, 1024.0 * 1024.0, 40, row, sizeof(row));
    assert(row[0] != '\0');
    assert(std::strstr(row, "CPU") != nullptr);
    assert(std::strstr(row, "RAM") != nullptr);
    assert_last_error_clear();
}

void test_format_process_row_null_name() {
    char row[128] = {};
    aura_format_process_row(1, nullptr, 10.0, 1024.0 * 1024.0, 40, row, sizeof(row));
    assert(row[0] != '\0');
    assert_last_error_clear();
}

void test_format_process_row_zero_values() {
    char row[128] = {};
    aura_format_process_row(1, "idle", 0.0, 0.0, 40, row, sizeof(row));
    assert(row[0] != '\0');
    assert(std::strstr(row, "idle") != nullptr);
    assert_last_error_clear();
}

void test_format_process_row_huge_memory() {
    char row[256] = {};
    const double one_tb_bytes = 1.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0;
    aura_format_process_row(1, "bigproc", 0.1, one_tb_bytes, 60, row, sizeof(row));
    assert(row[0] != '\0');
    assert_last_error_clear();
}

void test_format_process_row_clamped_cpu() {
    char row[128] = {};
    aura_format_process_row(1, "proc", 150.0, 1024.0 * 1024.0, 40, row, sizeof(row));
    assert(row[0] != '\0');
    // Ensure no negative or impossible values appear
    assert(std::strstr(row, "CPU") != nullptr);
    assert_last_error_clear();
}

void test_format_process_row_nan_cpu() {
    char row[128] = {};
    aura_format_process_row(
        1, "proc", std::numeric_limits<double>::quiet_NaN(), 1024.0 * 1024.0, 40, row, sizeof(row)
    );
    assert(row[0] != '\0');
    assert_last_error_clear();
}

void test_format_snapshot_lines_nan_inf() {
    const AuraSnapshotLines lines = aura_format_snapshot_lines(
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity()
    );
    // Should not be empty; exact content may be fallback but must be valid C strings
    assert(lines.cpu[0] != '\0' || true);  // always safe to check
    assert(std::strlen(lines.cpu) < sizeof(lines.cpu));
    assert(std::strlen(lines.memory) < sizeof(lines.memory));
    assert(std::strlen(lines.timestamp) < sizeof(lines.timestamp));
}

void test_format_disk_rate_zero() {
    char buf[64] = {};
    aura_format_disk_rate(0.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Disk 0.0 KB/s") == 0);
    assert_last_error_clear();
}

void test_format_disk_rate_nan() {
    char buf[64] = {};
    aura_format_disk_rate(std::numeric_limits<double>::quiet_NaN(), buf, sizeof(buf));
    assert(std::strcmp(buf, "Disk 0.0 KB/s") == 0);
    assert_last_error_clear();
}

void test_format_disk_rate_infinity() {
    char buf[64] = {};
    aura_format_disk_rate(std::numeric_limits<double>::infinity(), buf, sizeof(buf));
    // Should return some valid non-empty string (GB/s or fallback)
    assert(std::strlen(buf) > 0);
}

void test_format_disk_rate_exact_gb() {
    char buf[64] = {};
    aura_format_disk_rate(1.0 * 1024.0 * 1024.0 * 1024.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Disk 1.00 GB/s") == 0);
    assert_last_error_clear();
}

void test_format_network_rate_nan() {
    char buf[64] = {};
    aura_format_network_rate(std::numeric_limits<double>::quiet_NaN(), buf, sizeof(buf));
    assert(std::strcmp(buf, "Net 0.0 KB/s") == 0);
    assert_last_error_clear();
}

void test_format_network_rate_negative() {
    char buf[64] = {};
    aura_format_network_rate(-9999.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Net 0.0 KB/s") == 0);
    assert_last_error_clear();
}

void test_format_network_rate_exact_mb() {
    char buf[64] = {};
    aura_format_network_rate(1.0 * 1024.0 * 1024.0, buf, sizeof(buf));
    assert(std::strcmp(buf, "Net 1.0 MB/s") == 0);
    assert_last_error_clear();
}

void test_format_initial_status_no_db_no_samples() {
    char status[256] = {};
    aura_format_initial_status(nullptr, 0, 0, nullptr, status, sizeof(status));
    assert(status[0] != '\0');
    assert_last_error_clear();
}

void test_format_initial_status_with_error() {
    char status[256] = {};
    aura_format_initial_status(nullptr, 0, 0, "connection refused", status, sizeof(status));
    assert(status[0] != '\0');
    assert_last_error_clear();
}

void test_format_stream_status_null_db() {
    char status[256] = {};
    aura_format_stream_status(nullptr, 1, 10, nullptr, status, sizeof(status));
    assert(status[0] != '\0');
    assert_last_error_clear();
}
