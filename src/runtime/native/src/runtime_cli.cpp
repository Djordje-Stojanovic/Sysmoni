#include "aura_platform.h"
#include "platform_internal.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#define NOMINMAX
#include <windows.h>

namespace {

std::atomic<bool> g_stop_requested = false;

BOOL WINAPI CtrlHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        g_stop_requested = true;
        return TRUE;
    }
    return FALSE;
}

struct CliOptions {
    bool output_json = false;
    bool watch = false;
    double interval_seconds = 1.0;
    std::optional<int> count;
    std::optional<double> retention_seconds;
    bool no_persist = false;
    std::optional<int> latest;
    std::optional<double> since;
    std::optional<double> until;
    std::optional<std::string> db_path;
    std::optional<std::string> config_path;
};

void RequirePositiveFinite(const double value, const char* field_name) {
    if (!std::isfinite(value) || value <= 0.0) {
        throw std::runtime_error(std::string(field_name) + " must be a finite number greater than 0.");
    }
}

int ParsePositiveInt(const std::string& raw, const char* field_name) {
    const int parsed = std::stoi(raw);
    if (parsed <= 0) {
        throw std::runtime_error(std::string(field_name) + " must be an integer greater than 0.");
    }
    return parsed;
}

double ParseFiniteDouble(const std::string& raw, const char* field_name) {
    const double parsed = std::stod(raw);
    if (!std::isfinite(parsed)) {
        throw std::runtime_error(std::string(field_name) + " must be a finite number.");
    }
    return parsed;
}

void PrintUsage() {
    std::cout
        << "Aura native platform runtime (Windows-first)\n"
        << "Usage: aura [options]\n"
        << "  --json\n"
        << "  --watch\n"
        << "  --interval <seconds>\n"
        << "  --count <n>\n"
        << "  --retention-seconds <seconds>\n"
        << "  --no-persist\n"
        << "  --latest <n>\n"
        << "  --since <timestamp>\n"
        << "  --until <timestamp>\n"
        << "  --db-path <path>\n"
        << "  --config-path <path>\n"
        << "  --help\n";
}

CliOptions ParseArgs(const int argc, char** argv) {
    CliOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        auto require_value = [&](const char* flag_name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string("Missing value for ") + flag_name + ".");
            }
            ++i;
            return std::string(argv[i]);
        };

        if (arg == "--help" || arg == "-h") {
            PrintUsage();
            std::exit(0);
        }
        if (arg == "--json") {
            options.output_json = true;
        } else if (arg == "--watch") {
            options.watch = true;
        } else if (arg == "--interval") {
            const double parsed = ParseFiniteDouble(require_value("--interval"), "interval");
            RequirePositiveFinite(parsed, "interval");
            options.interval_seconds = parsed;
        } else if (arg == "--count") {
            options.count = ParsePositiveInt(require_value("--count"), "count");
        } else if (arg == "--retention-seconds") {
            const double parsed = ParseFiniteDouble(require_value("--retention-seconds"), "retention");
            RequirePositiveFinite(parsed, "retention");
            options.retention_seconds = parsed;
        } else if (arg == "--no-persist") {
            options.no_persist = true;
        } else if (arg == "--latest") {
            options.latest = ParsePositiveInt(require_value("--latest"), "count");
        } else if (arg == "--since") {
            options.since = ParseFiniteDouble(require_value("--since"), "timestamp");
        } else if (arg == "--until") {
            options.until = ParseFiniteDouble(require_value("--until"), "timestamp");
        } else if (arg == "--db-path") {
            options.db_path = require_value("--db-path");
        } else if (arg == "--config-path") {
            options.config_path = require_value("--config-path");
        } else {
            throw std::runtime_error("Unknown flag: " + arg);
        }
    }

    if (options.count.has_value() && !options.watch) {
        throw std::runtime_error("--count requires --watch");
    }
    if (options.latest.has_value() && options.watch) {
        throw std::runtime_error("--latest cannot be used with --watch");
    }

    const bool has_range = options.since.has_value() || options.until.has_value();
    if (has_range && options.watch) {
        throw std::runtime_error("--since/--until cannot be used with --watch");
    }
    if (has_range && options.latest.has_value()) {
        throw std::runtime_error("--since/--until cannot be used with --latest");
    }
    if (options.latest.has_value() && options.no_persist) {
        throw std::runtime_error("--latest cannot be used with --no-persist");
    }
    if (has_range && options.no_persist) {
        throw std::runtime_error("--since/--until cannot be used with --no-persist");
    }
    if (options.since.has_value() && options.until.has_value() && *options.since > *options.until) {
        throw std::runtime_error("--since must be less than or equal to --until");
    }

    return options;
}

void PrintSnapshot(const aura::platform::Snapshot& snapshot, const bool output_json) {
    if (output_json) {
        std::cout
            << "{\"cpu_percent\": " << std::fixed << std::setprecision(1) << snapshot.cpu_percent
            << ", \"memory_percent\": " << std::fixed << std::setprecision(1) << snapshot.memory_percent
            << ", \"timestamp\": " << std::setprecision(3) << snapshot.timestamp
            << "}" << '\n';
        return;
    }

    std::cout
        << "cpu=" << std::fixed << std::setprecision(1) << snapshot.cpu_percent << "% "
        << "mem=" << std::fixed << std::setprecision(1) << snapshot.memory_percent << "% "
        << "ts=" << std::setprecision(3) << snapshot.timestamp
        << '\n';
}

std::vector<aura::platform::Snapshot> LoadSnapshots(
    aura_store_t* store,
    const std::optional<int>& latest,
    const std::optional<double>& since,
    const std::optional<double>& until
) {
    aura_error_t err{};
    constexpr int max_results = 100000;
    std::vector<aura_snapshot_t> output(static_cast<std::size_t>(max_results));
    int out_count = 0;

    if (latest.has_value()) {
        const int rc = aura_store_latest(
            store,
            *latest,
            output.data(),
            max_results,
            &out_count,
            &err
        );
        if (rc != AURA_OK) {
            throw std::runtime_error(err.message[0] != '\0' ? err.message : "aura_store_latest failed");
        }
    } else {
        const int rc = aura_store_between(
            store,
            since.has_value() ? 1 : 0,
            since.value_or(0.0),
            until.has_value() ? 1 : 0,
            until.value_or(0.0),
            output.data(),
            max_results,
            &out_count,
            &err
        );
        if (rc != AURA_OK) {
            throw std::runtime_error(err.message[0] != '\0' ? err.message : "aura_store_between failed");
        }
    }

    std::vector<aura::platform::Snapshot> snapshots;
    snapshots.reserve(static_cast<std::size_t>(out_count));
    for (int i = 0; i < out_count; ++i) {
        aura::platform::Snapshot next;
        next.timestamp = output[i].timestamp;
        next.cpu_percent = output[i].cpu_percent;
        next.memory_percent = output[i].memory_percent;
        snapshots.push_back(next);
    }
    return snapshots;
}

} // namespace

int main(int argc, char** argv) {
    try {
        SetConsoleCtrlHandler(CtrlHandler, TRUE);

        const CliOptions options = ParseArgs(argc, argv);

        aura_config_request_t config_request{};
        config_request.no_persist = options.no_persist ? 1 : 0;
        config_request.cli_db_path = options.db_path.has_value() ? options.db_path->c_str() : nullptr;
        config_request.has_cli_retention = options.retention_seconds.has_value() ? 1 : 0;
        config_request.cli_retention_seconds = options.retention_seconds.value_or(0.0);
        config_request.config_path_override = options.config_path.has_value() ? options.config_path->c_str() : nullptr;

        aura_runtime_config_t config{};
        aura_error_t err{};
        const int cfg_rc = aura_config_resolve(&config_request, &config, &err);
        if (cfg_rc != AURA_OK) {
            std::cerr << (err.message[0] != '\0' ? err.message : "Failed to resolve config") << '\n';
            return 2;
        }

        aura_store_t* store = nullptr;
        if (config.persistence_enabled != 0) {
            const int rc = aura_store_open(config.db_path, config.retention_seconds, &store, &err);
            if (rc != AURA_OK) {
                std::cerr << (err.message[0] != '\0' ? err.message : "Failed to open store") << '\n';
                return 2;
            }
        }

        const bool has_range = options.since.has_value() || options.until.has_value();
        if (options.latest.has_value() || has_range) {
            if (store == nullptr) {
                std::cerr << "Persistence store unavailable for read query." << '\n';
                return 2;
            }

            const auto snapshots = LoadSnapshots(store, options.latest, options.since, options.until);
            for (const auto& snapshot : snapshots) {
                PrintSnapshot(snapshot, options.output_json);
            }
            aura_store_close(store);
            return 0;
        }

        if (options.watch) {
            std::optional<int> remaining = options.count;
            while (!g_stop_requested.load()) {
                aura::platform::Snapshot snapshot = aura::platform::CollectSystemSnapshot();
                if (store != nullptr) {
                    aura_snapshot_t raw{};
                    raw.timestamp = snapshot.timestamp;
                    raw.cpu_percent = snapshot.cpu_percent;
                    raw.memory_percent = snapshot.memory_percent;
                    const int rc = aura_store_append(store, &raw, &err);
                    if (rc != AURA_OK) {
                        std::cerr << "DVR persistence disabled: "
                                  << (err.message[0] != '\0' ? err.message : "store append failed")
                                  << '\n';
                        aura_store_close(store);
                        store = nullptr;
                    }
                }

                PrintSnapshot(snapshot, options.output_json);
                std::cout.flush();

                if (remaining.has_value()) {
                    *remaining -= 1;
                    if (*remaining <= 0) {
                        break;
                    }
                }

                const auto sleep_ms = static_cast<int>(options.interval_seconds * 1000.0);
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            }

            aura_store_close(store);
            return 0;
        }

        aura::platform::Snapshot snapshot = aura::platform::CollectSystemSnapshot();
        if (store != nullptr) {
            aura_snapshot_t raw{};
            raw.timestamp = snapshot.timestamp;
            raw.cpu_percent = snapshot.cpu_percent;
            raw.memory_percent = snapshot.memory_percent;
            const int rc = aura_store_append(store, &raw, &err);
            if (rc != AURA_OK) {
                std::cerr << "DVR persistence disabled: "
                          << (err.message[0] != '\0' ? err.message : "store append failed")
                          << '\n';
            }
        }

        PrintSnapshot(snapshot, options.output_json);
        aura_store_close(store);
        return 0;
    } catch (const std::exception& exc) {
        std::cerr << exc.what() << '\n';
        return 2;
    }
}
