#pragma once

#include "aura_platform.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Assertion utilities
// ---------------------------------------------------------------------------

inline void Fail(const std::string& message) {
    std::cerr << "FAILED: " << message << std::endl;
    std::exit(1);
}

inline void ExpectTrue(const bool condition, const std::string& message) {
    if (!condition) {
        Fail(message);
    }
}

inline void ExpectEq(const int actual, const int expected, const std::string& message) {
    if (actual != expected) {
        Fail(message + " (actual=" + std::to_string(actual) + ", expected=" + std::to_string(expected) + ")");
    }
}

inline void ExpectNear(const double actual, const double expected, const double tolerance, const std::string& message) {
    if (std::fabs(actual - expected) > tolerance) {
        Fail(message + " (actual=" + std::to_string(actual) + ", expected=" + std::to_string(expected) + ")");
    }
}

// ---------------------------------------------------------------------------
// ScopedEnvVar — RAII env-var override
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Fixture helpers
// ---------------------------------------------------------------------------

inline double NowSeconds() {
    using clock = std::chrono::system_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

inline std::filesystem::path BuildStorePath(const std::string& test_name) {
    const auto unique = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "aura_platform_native_tests";
    std::error_code create_error;
    std::filesystem::create_directories(root, create_error);
    return root / (test_name + "_" + std::to_string(unique) + ".db");
}

inline std::filesystem::path TempStorePath(const std::filesystem::path& db_path) {
    std::filesystem::path temp_path = db_path;
    temp_path += ".tmp";
    return temp_path;
}

inline std::filesystem::path CsvBakPath(const std::filesystem::path& db_path) {
    return db_path.string() + ".csv.bak";
}

inline void CleanupStoreFiles(const std::filesystem::path& db_path) {
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

inline std::string SnapshotLine(const double timestamp, const double cpu_percent, const double memory_percent) {
    std::ostringstream output;
    output.precision(17);
    output << timestamp << ',' << cpu_percent << ',' << memory_percent << ',' << 0.0 << ',' << 0.0;
    return output.str();
}

inline std::string SnapshotLineWithDisk(
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

inline std::string SnapshotLineWithNet(
    const double timestamp,
    const double cpu_percent,
    const double memory_percent,
    const double disk_read_bps,
    const double disk_write_bps,
    const double net_recv_bps,
    const double net_sent_bps
) {
    std::ostringstream output;
    output.precision(17);
    output << timestamp << ',' << cpu_percent << ',' << memory_percent
           << ',' << disk_read_bps << ',' << disk_write_bps
           << ',' << net_recv_bps << ',' << net_sent_bps;
    return output.str();
}

inline void WriteTextFileLines(const std::filesystem::path& path, const std::vector<std::string>& lines) {
    std::ofstream output(path, std::ios::trunc | std::ios::binary);
    if (!output.is_open()) {
        Fail("failed to create test file: " + path.string());
    }
    for (const std::string& line : lines) {
        output << line << '\n';
    }
}

inline bool StartsWithSqliteMagic(const std::filesystem::path& path) {
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
