#include "platform_internal.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace aura::platform {
namespace {

std::string Trim(std::string value) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string StripQuotes(const std::string& value) {
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::optional<std::string> NormalizeOptionalPath(const char* raw) {
    if (raw == nullptr) {
        return std::nullopt;
    }
    const std::string trimmed = Trim(raw);
    if (trimmed.empty()) {
        return std::nullopt;
    }
    return trimmed;
}

std::optional<std::string> ReadEnvOptional(const char* key) {
    if (key == nullptr) {
        return std::nullopt;
    }
    const char* value = std::getenv(key);
    return NormalizeOptionalPath(value);
}

std::optional<double> ParsePositiveFiniteOptional(const std::string& value, const char* source_name) {
    try {
        const double parsed = std::stod(value);
        ValidatePositiveFinite(parsed, source_name);
        return parsed;
    } catch (const std::exception&) {
        throw std::runtime_error(std::string(source_name) + " must be a finite number greater than 0.");
    }
}

std::filesystem::path ResolveWindowsBaseDataPath() {
    if (const auto appdata = ReadEnvOptional("APPDATA")) {
        return std::filesystem::path(*appdata);
    }
    if (const auto localappdata = ReadEnvOptional("LOCALAPPDATA")) {
        return std::filesystem::path(*localappdata);
    }
    if (const auto home = ReadEnvOptional("USERPROFILE")) {
        return std::filesystem::path(*home) / "AppData" / "Roaming";
    }
    return std::filesystem::current_path();
}

std::filesystem::path ResolveDefaultDbPath() {
    return ResolveWindowsBaseDataPath() / "Aura" / "telemetry.sqlite";
}

std::filesystem::path ResolveDefaultConfigPath() {
    return ResolveWindowsBaseDataPath() / "Aura" / "aura.toml";
}

struct FileConfig {
    std::optional<std::string> db_path;
    std::optional<double> retention_seconds;
};

FileConfig LoadFileConfig(const std::optional<std::string>& config_path_override) {
    const std::filesystem::path config_path = config_path_override.has_value()
        ? std::filesystem::path(*config_path_override)
        : ResolveDefaultConfigPath();

    std::ifstream input(config_path);
    if (!input.is_open()) {
        return {};
    }

    FileConfig out;
    bool in_persistence = false;
    std::string line;
    while (std::getline(input, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed.starts_with('#')) {
            continue;
        }
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            in_persistence = trimmed == "[persistence]";
            continue;
        }
        if (!in_persistence) {
            continue;
        }

        const std::size_t eq = trimmed.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = Trim(trimmed.substr(0, eq));
        const std::string value = Trim(trimmed.substr(eq + 1));

        if (key == "db_path") {
            const std::string parsed = StripQuotes(value);
            if (!parsed.empty()) {
                out.db_path = parsed;
            }
        } else if (key == "retention_seconds") {
            out.retention_seconds = ParsePositiveFiniteOptional(value, "retention_seconds");
        }
    }

    return out;
}

} // namespace

void ValidatePositiveFinite(const double value, const char* field_name) {
    if (!std::isfinite(value) || value <= 0.0) {
        throw std::runtime_error(std::string(field_name) + " must be a finite number greater than 0.");
    }
}

void ValidateFinite(const double value, const char* field_name) {
    if (!std::isfinite(value)) {
        throw std::runtime_error(std::string(field_name) + " must be a finite number.");
    }
}

RuntimeConfig ResolveRuntimeConfig(const ConfigRequest& request) {
    const FileConfig file_cfg = LoadFileConfig(request.config_path_override);

    RuntimeConfig out;
    out.persistence_enabled = !request.no_persist;

    if (request.cli_retention_seconds.has_value()) {
        ValidatePositiveFinite(*request.cli_retention_seconds, "retention_seconds");
        out.retention_seconds = *request.cli_retention_seconds;
    } else if (const auto env_retention = ReadEnvOptional("AURA_RETENTION_SECONDS")) {
        out.retention_seconds = ParsePositiveFiniteOptional(*env_retention, "AURA_RETENTION_SECONDS").value();
    } else if (file_cfg.retention_seconds.has_value()) {
        out.retention_seconds = *file_cfg.retention_seconds;
    } else {
        out.retention_seconds = kDefaultRetentionSeconds;
    }

    if (!out.persistence_enabled) {
        out.db_source = DbSource::Disabled;
        out.db_path.clear();
        return out;
    }

    if (request.cli_db_path.has_value() && !request.cli_db_path->empty()) {
        out.db_source = DbSource::Cli;
        out.db_path = *request.cli_db_path;
        return out;
    }

    if (const auto env_db_path = ReadEnvOptional("AURA_DB_PATH")) {
        out.db_source = DbSource::Env;
        out.db_path = *env_db_path;
        return out;
    }

    if (file_cfg.db_path.has_value() && !file_cfg.db_path->empty()) {
        out.db_source = DbSource::Config;
        out.db_path = *file_cfg.db_path;
        return out;
    }

    out.db_source = DbSource::Auto;
    out.db_path = ResolveDefaultDbPath().string();
    return out;
}

} // namespace aura::platform
