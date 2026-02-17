#include "test_platform_helpers.hpp"
#include "test_platform_tests.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Export test helpers
// ---------------------------------------------------------------------------

namespace {

std::filesystem::path BuildExportPath(const std::string& test_name, const std::string& ext) {
    const auto unique = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "aura_export_tests";
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    return root / (test_name + "_" + std::to_string(unique) + ext);
}

std::string ReadFileContents(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        Fail("failed to read export file: " + path.string());
    }
    std::ostringstream oss;
    oss << input.rdbuf();
    return oss.str();
}

std::vector<std::string> ReadFileLines(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        Fail("failed to read export file: " + path.string());
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        // Remove trailing \r if present (binary mode writes \n, but just in case)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

void CleanupExportFile(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

aura_store_t* CreateStoreWithSnapshots(int count, double base_timestamp) {
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    if (rc != AURA_OK || store == nullptr) {
        Fail("CreateStoreWithSnapshots: store open failed");
    }

    for (int i = 0; i < count; ++i) {
        aura_snapshot_t snap{};
        snap.timestamp = base_timestamp + static_cast<double>(i);
        snap.cpu_percent = static_cast<double>(i * 2 % 101);
        snap.memory_percent = 50.0 + static_cast<double>(i % 30);
        snap.disk_read_bps = static_cast<double>(i) * 1000.0;
        snap.disk_write_bps = static_cast<double>(i) * 2000.0;
        snap.net_recv_bps = static_cast<double>(i) * 500.0;
        snap.net_sent_bps = static_cast<double>(i) * 300.0;
        rc = aura_store_append(store, &snap, &error);
        if (rc != AURA_OK) {
            Fail("CreateStoreWithSnapshots: append failed at i=" + std::to_string(i));
        }
    }
    return store;
}

} // namespace

// ---------------------------------------------------------------------------
// Category G: JSON Export
// ---------------------------------------------------------------------------

void TestExportJsonBasic() {
    const double base = NowSeconds() - 100.0;
    aura_store_t* store = CreateStoreWithSnapshots(5, base);
    const auto path = BuildExportPath("json_basic", ".json");

    aura_error_t error{};
    const int rc = aura_dvr_export_json(store, 0, 0.0, 0, 0.0, 0, path.string().c_str(), &error);
    ExpectEq(rc, AURA_OK, "json basic: export should succeed");

    const std::string contents = ReadFileContents(path);
    ExpectTrue(!contents.empty(), "json basic: file should not be empty");
    ExpectTrue(contents.front() == '{', "json basic: should start with {");
    ExpectTrue(contents.back() == '}', "json basic: should end with }");

    aura_store_close(store);
    CleanupExportFile(path);
}

void TestExportJsonWithStats() {
    const double base = NowSeconds() - 100.0;
    aura_store_t* store = CreateStoreWithSnapshots(10, base);
    const auto path = BuildExportPath("json_with_stats", ".json");

    aura_error_t error{};
    const int rc = aura_dvr_export_json(store, 0, 0.0, 0, 0.0, 1, path.string().c_str(), &error);
    ExpectEq(rc, AURA_OK, "json with stats: export should succeed");

    const std::string contents = ReadFileContents(path);
    ExpectTrue(contents.find("\"stats\"") != std::string::npos, "json with stats: stats section present");
    ExpectTrue(contents.find("\"cpu\"") != std::string::npos, "json with stats: cpu stats present");
    ExpectTrue(contents.find("\"avg\"") != std::string::npos, "json with stats: avg field present");
    ExpectTrue(contents.find("\"p95\"") != std::string::npos, "json with stats: p95 field present");

    aura_store_close(store);
    CleanupExportFile(path);
}

void TestExportJsonWithoutStats() {
    const double base = NowSeconds() - 100.0;
    aura_store_t* store = CreateStoreWithSnapshots(5, base);
    const auto path = BuildExportPath("json_no_stats", ".json");

    aura_error_t error{};
    const int rc = aura_dvr_export_json(store, 0, 0.0, 0, 0.0, 0, path.string().c_str(), &error);
    ExpectEq(rc, AURA_OK, "json no stats: export should succeed");

    const std::string contents = ReadFileContents(path);
    ExpectTrue(contents.find("\"stats\"") == std::string::npos, "json no stats: stats section absent");

    aura_store_close(store);
    CleanupExportFile(path);
}

void TestExportJsonEmptyRange() {
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "json empty: store open");

    const auto path = BuildExportPath("json_empty", ".json");
    rc = aura_dvr_export_json(store, 0, 0.0, 0, 0.0, 0, path.string().c_str(), &error);
    ExpectEq(rc, AURA_OK, "json empty: export should succeed");

    const std::string contents = ReadFileContents(path);
    ExpectTrue(contents.find("\"count\":0") != std::string::npos, "json empty: count = 0");
    ExpectTrue(contents.find("\"snapshots\":[]") != std::string::npos, "json empty: empty snapshots array");

    aura_store_close(store);
    CleanupExportFile(path);
}

void TestExportJsonFieldValues() {
    const double base = NowSeconds() - 100.0;
    aura_store_t* store = CreateStoreWithSnapshots(3, base);
    const auto path = BuildExportPath("json_fields", ".json");

    aura_error_t error{};
    const int rc = aura_dvr_export_json(store, 0, 0.0, 0, 0.0, 0, path.string().c_str(), &error);
    ExpectEq(rc, AURA_OK, "json fields: export should succeed");

    const std::string contents = ReadFileContents(path);
    ExpectTrue(contents.find("\"ts\"") != std::string::npos, "json fields: ts field present");
    ExpectTrue(contents.find("\"cpu\"") != std::string::npos, "json fields: cpu field present");
    ExpectTrue(contents.find("\"mem\"") != std::string::npos, "json fields: mem field present");
    ExpectTrue(contents.find("\"disk_r\"") != std::string::npos, "json fields: disk_r field present");
    ExpectTrue(contents.find("\"disk_w\"") != std::string::npos, "json fields: disk_w field present");
    ExpectTrue(contents.find("\"net_r\"") != std::string::npos, "json fields: net_r field present");
    ExpectTrue(contents.find("\"net_s\"") != std::string::npos, "json fields: net_s field present");

    aura_store_close(store);
    CleanupExportFile(path);
}

void TestExportJsonVersionField() {
    const double base = NowSeconds() - 100.0;
    aura_store_t* store = CreateStoreWithSnapshots(1, base);
    const auto path = BuildExportPath("json_version", ".json");

    aura_error_t error{};
    const int rc = aura_dvr_export_json(store, 0, 0.0, 0, 0.0, 0, path.string().c_str(), &error);
    ExpectEq(rc, AURA_OK, "json version: export should succeed");

    const std::string contents = ReadFileContents(path);
    ExpectTrue(contents.find("\"version\":1") != std::string::npos, "json version: version = 1");
    ExpectTrue(contents.find("\"format\":\"aura_dvr_export\"") != std::string::npos, "json version: format field");

    aura_store_close(store);
    CleanupExportFile(path);
}

void TestExportJsonTimePrecision() {
    const double base = 1700000000.123456;
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "json precision: store open");

    aura_snapshot_t snap{};
    snap.timestamp = base;
    snap.cpu_percent = 50.0;
    snap.memory_percent = 50.0;
    rc = aura_store_append(store, &snap, &error);
    ExpectEq(rc, AURA_OK, "json precision: append");

    const auto path = BuildExportPath("json_precision", ".json");
    rc = aura_dvr_export_json(store, 0, 0.0, 0, 0.0, 0, path.string().c_str(), &error);
    ExpectEq(rc, AURA_OK, "json precision: export should succeed");

    const std::string contents = ReadFileContents(path);
    // The timestamp should contain a decimal point with at least 6 digits
    ExpectTrue(contents.find("1700000000.") != std::string::npos, "json precision: timestamp has decimal");

    aura_store_close(store);
    CleanupExportFile(path);
}

void TestExportJsonNullStore() {
    const auto path = BuildExportPath("json_null_store", ".json");
    aura_error_t error{};
    const int rc = aura_dvr_export_json(nullptr, 0, 0.0, 0, 0.0, 0, path.string().c_str(), &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "json null store: error code");
    CleanupExportFile(path);
}

void TestExportJsonNullFilePath() {
    const double base = NowSeconds() - 100.0;
    aura_store_t* store = CreateStoreWithSnapshots(1, base);

    aura_error_t error{};
    const int rc = aura_dvr_export_json(store, 0, 0.0, 0, 0.0, 0, nullptr, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "json null path: error code");

    aura_store_close(store);
}

void TestExportJsonLargeDataset() {
    const double base = NowSeconds() - 2000.0;
    aura_store_t* store = CreateStoreWithSnapshots(1000, base);
    const auto path = BuildExportPath("json_large", ".json");

    aura_error_t error{};
    const int rc = aura_dvr_export_json(store, 0, 0.0, 0, 0.0, 1, path.string().c_str(), &error);
    ExpectEq(rc, AURA_OK, "json large: export should succeed");

    std::error_code ec;
    const auto file_size = std::filesystem::file_size(path, ec);
    ExpectTrue(!ec, "json large: file size readable");
    ExpectTrue(file_size > 1000, "json large: file has substantial content");

    const std::string contents = ReadFileContents(path);
    ExpectTrue(contents.find("\"count\":1000") != std::string::npos, "json large: count = 1000");

    aura_store_close(store);
    CleanupExportFile(path);
}

// ---------------------------------------------------------------------------
// Category H: CSV Export
// ---------------------------------------------------------------------------

void TestExportCsvBasic() {
    const double base = NowSeconds() - 100.0;
    aura_store_t* store = CreateStoreWithSnapshots(5, base);
    const auto path = BuildExportPath("csv_basic", ".csv");

    aura_error_t error{};
    const int rc = aura_dvr_export_csv(store, 0, 0.0, 0, 0.0, path.string().c_str(), &error);
    ExpectEq(rc, AURA_OK, "csv basic: export should succeed");

    const auto lines = ReadFileLines(path);
    // 1 header + 5 data lines
    ExpectEq(static_cast<int>(lines.size()), 6, "csv basic: header + 5 data lines");

    aura_store_close(store);
    CleanupExportFile(path);
}

void TestExportCsvHeader() {
    const double base = NowSeconds() - 100.0;
    aura_store_t* store = CreateStoreWithSnapshots(1, base);
    const auto path = BuildExportPath("csv_header", ".csv");

    aura_error_t error{};
    const int rc = aura_dvr_export_csv(store, 0, 0.0, 0, 0.0, path.string().c_str(), &error);
    ExpectEq(rc, AURA_OK, "csv header: export should succeed");

    const auto lines = ReadFileLines(path);
    ExpectTrue(!lines.empty(), "csv header: file should not be empty");
    ExpectTrue(lines[0] == "timestamp,cpu_percent,memory_percent,disk_read_bps,disk_write_bps,net_recv_bps,net_sent_bps",
               "csv header: header matches expected format");

    aura_store_close(store);
    CleanupExportFile(path);
}

void TestExportCsvFieldCount() {
    const double base = NowSeconds() - 100.0;
    aura_store_t* store = CreateStoreWithSnapshots(3, base);
    const auto path = BuildExportPath("csv_field_count", ".csv");

    aura_error_t error{};
    const int rc = aura_dvr_export_csv(store, 0, 0.0, 0, 0.0, path.string().c_str(), &error);
    ExpectEq(rc, AURA_OK, "csv field count: export should succeed");

    const auto lines = ReadFileLines(path);
    // Check each data line has exactly 7 fields (6 commas)
    for (std::size_t i = 1; i < lines.size(); ++i) {
        if (lines[i].empty()) continue;
        int comma_count = 0;
        for (char c : lines[i]) {
            if (c == ',') ++comma_count;
        }
        ExpectEq(comma_count, 6, "csv field count: line " + std::to_string(i) + " has 7 fields");
    }

    aura_store_close(store);
    CleanupExportFile(path);
}

void TestExportCsvEmptyRange() {
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "csv empty: store open");

    const auto path = BuildExportPath("csv_empty", ".csv");
    rc = aura_dvr_export_csv(store, 0, 0.0, 0, 0.0, path.string().c_str(), &error);
    ExpectEq(rc, AURA_OK, "csv empty: export should succeed");

    const auto lines = ReadFileLines(path);
    // Just the header line
    ExpectEq(static_cast<int>(lines.size()), 1, "csv empty: header only");

    aura_store_close(store);
    CleanupExportFile(path);
}

void TestExportCsvRoundTrip() {
    const double base = NowSeconds() - 100.0;
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "csv round trip: store open");

    // Insert known values
    aura_snapshot_t snap{};
    snap.timestamp = base;
    snap.cpu_percent = 42.5;
    snap.memory_percent = 67.3;
    snap.disk_read_bps = 12345.678;
    snap.disk_write_bps = 98765.432;
    snap.net_recv_bps = 55555.111;
    snap.net_sent_bps = 33333.222;
    rc = aura_store_append(store, &snap, &error);
    ExpectEq(rc, AURA_OK, "csv round trip: append");

    const auto path = BuildExportPath("csv_round_trip", ".csv");
    rc = aura_dvr_export_csv(store, 0, 0.0, 0, 0.0, path.string().c_str(), &error);
    ExpectEq(rc, AURA_OK, "csv round trip: export should succeed");

    // Read back and parse
    const auto lines = ReadFileLines(path);
    ExpectEq(static_cast<int>(lines.size()), 2, "csv round trip: header + 1 data line");

    // Parse the data line
    std::istringstream iss(lines[1]);
    std::string field;
    std::vector<double> values;
    while (std::getline(iss, field, ',')) {
        values.push_back(std::stod(field));
    }
    ExpectEq(static_cast<int>(values.size()), 7, "csv round trip: 7 fields parsed");

    ExpectNear(values[1], 42.5, 0.001, "csv round trip: cpu matches");
    ExpectNear(values[2], 67.3, 0.001, "csv round trip: memory matches");
    ExpectNear(values[3], 12345.678, 0.01, "csv round trip: disk_read matches");
    ExpectNear(values[4], 98765.432, 0.01, "csv round trip: disk_write matches");
    ExpectNear(values[5], 55555.111, 0.01, "csv round trip: net_recv matches");
    ExpectNear(values[6], 33333.222, 0.01, "csv round trip: net_sent matches");

    aura_store_close(store);
    CleanupExportFile(path);
}

void TestExportCsvTimePrecision() {
    const double base = 1700000000.123456;
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "csv precision: store open");

    aura_snapshot_t snap{};
    snap.timestamp = base;
    snap.cpu_percent = 50.0;
    snap.memory_percent = 50.0;
    rc = aura_store_append(store, &snap, &error);
    ExpectEq(rc, AURA_OK, "csv precision: append");

    const auto path = BuildExportPath("csv_precision", ".csv");
    rc = aura_dvr_export_csv(store, 0, 0.0, 0, 0.0, path.string().c_str(), &error);
    ExpectEq(rc, AURA_OK, "csv precision: export should succeed");

    const auto lines = ReadFileLines(path);
    ExpectTrue(lines.size() >= 2, "csv precision: has data line");
    // The timestamp should contain decimal digits
    ExpectTrue(lines[1].find("1700000000.") != std::string::npos, "csv precision: timestamp has decimals");

    aura_store_close(store);
    CleanupExportFile(path);
}

void TestExportCsvNullStore() {
    const auto path = BuildExportPath("csv_null_store", ".csv");
    aura_error_t error{};
    const int rc = aura_dvr_export_csv(nullptr, 0, 0.0, 0, 0.0, path.string().c_str(), &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "csv null store: error code");
    CleanupExportFile(path);
}

void TestExportCsvNullFilePath() {
    const double base = NowSeconds() - 100.0;
    aura_store_t* store = CreateStoreWithSnapshots(1, base);

    aura_error_t error{};
    const int rc = aura_dvr_export_csv(store, 0, 0.0, 0, 0.0, nullptr, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "csv null path: error code");

    aura_store_close(store);
}

void TestExportCsvNullError() {
    const double base = NowSeconds() - 100.0;
    aura_store_t* store = CreateStoreWithSnapshots(3, base);
    const auto path = BuildExportPath("csv_null_error", ".csv");

    const int rc = aura_dvr_export_csv(store, 0, 0.0, 0, 0.0, path.string().c_str(), nullptr);
    ExpectEq(rc, AURA_OK, "csv null error: should succeed without crash");

    const auto lines = ReadFileLines(path);
    ExpectEq(static_cast<int>(lines.size()), 4, "csv null error: header + 3 data lines");

    aura_store_close(store);
    CleanupExportFile(path);
}

void TestExportCsvLargeDataset() {
    const double base = NowSeconds() - 2000.0;
    aura_store_t* store = CreateStoreWithSnapshots(1000, base);
    const auto path = BuildExportPath("csv_large", ".csv");

    aura_error_t error{};
    const int rc = aura_dvr_export_csv(store, 0, 0.0, 0, 0.0, path.string().c_str(), &error);
    ExpectEq(rc, AURA_OK, "csv large: export should succeed");

    const auto lines = ReadFileLines(path);
    // 1 header + 1000 data lines
    ExpectEq(static_cast<int>(lines.size()), 1001, "csv large: header + 1000 data lines");

    aura_store_close(store);
    CleanupExportFile(path);
}
