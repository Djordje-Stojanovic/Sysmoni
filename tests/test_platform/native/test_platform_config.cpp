#include "test_platform_helpers.hpp"

#include <filesystem>
#include <string>

void TestConfigNoPersist() {
    aura_config_request_t request{};
    request.no_persist = 1;

    aura_runtime_config_t config{};
    aura_error_t error{};
    const int rc = aura_config_resolve(&request, &config, &error);
    ExpectEq(rc, AURA_OK, "aura_config_resolve should succeed");
    ExpectEq(config.persistence_enabled, 0, "persistence should be disabled");
    ExpectEq(config.db_source, AURA_DB_SOURCE_DISABLED, "db source should be disabled");
}

void TestConfigRejectsMalformedEnvRetention() {
    ScopedEnvVar retention_env("AURA_RETENTION_SECONDS");
    retention_env.Set("30junk");

    aura_config_request_t request{};
    request.no_persist = 1;

    aura_runtime_config_t config{};
    aura_error_t error{};
    const int rc = aura_config_resolve(&request, &config, &error);
    ExpectTrue(rc != AURA_OK, "aura_config_resolve should reject malformed env retention");
    ExpectTrue(error.message[0] != '\0', "error message should be populated");
    ExpectTrue(
        std::string(error.message).find("AURA_RETENTION_SECONDS") != std::string::npos,
        "error should identify malformed env retention source"
    );
}

void TestConfigRejectsMalformedTomlRetention() {
    const std::filesystem::path config_path = BuildStorePath("bad_retention_config");
    const std::string config_path_raw = config_path.string();
    WriteTextFileLines(
        config_path,
        {
            "[persistence]",
            "retention_seconds = 42oops",
        }
    );

    aura_config_request_t request{};
    request.no_persist = 1;
    request.config_path_override = config_path_raw.c_str();

    aura_runtime_config_t config{};
    aura_error_t error{};
    const int rc = aura_config_resolve(&request, &config, &error);
    ExpectTrue(rc != AURA_OK, "aura_config_resolve should reject malformed TOML retention");
    ExpectTrue(error.message[0] != '\0', "error message should be populated");
    ExpectTrue(
        std::string(error.message).find("retention_seconds") != std::string::npos,
        "error should identify malformed TOML retention field"
    );

    std::error_code remove_error;
    std::filesystem::remove(config_path, remove_error);
}

void TestConfigAcceptsTomlInlineCommentRetention() {
    ScopedEnvVar retention_env("AURA_RETENTION_SECONDS");
    retention_env.Set("");

    const std::filesystem::path config_path = BuildStorePath("inline_comment_retention");
    const std::string config_path_raw = config_path.string();
    WriteTextFileLines(
        config_path,
        {
            "[persistence] # persistence section",
            "retention_seconds = 42 # keep six weeks of history",
        }
    );

    aura_config_request_t request{};
    request.no_persist = 1;
    request.config_path_override = config_path_raw.c_str();

    aura_runtime_config_t config{};
    aura_error_t error{};
    const int rc = aura_config_resolve(&request, &config, &error);
    ExpectEq(rc, AURA_OK, "aura_config_resolve should accept TOML inline comment on retention");
    ExpectNear(config.retention_seconds, 42.0, 1e-9, "retention should parse from TOML value before comment");

    std::error_code remove_error;
    std::filesystem::remove(config_path, remove_error);
}

void TestConfigAcceptsTomlInlineCommentDbPath() {
    ScopedEnvVar db_path_env("AURA_DB_PATH");
    db_path_env.Set("");
    ScopedEnvVar retention_env("AURA_RETENTION_SECONDS");
    retention_env.Set("");

    const std::filesystem::path expected_db_path = BuildStorePath("inline_comment_db");
    const std::filesystem::path config_path = BuildStorePath("inline_comment_db_config");
    const std::string config_path_raw = config_path.string();

    WriteTextFileLines(
        config_path,
        {
            "[persistence] # persistence section",
            std::string("db_path = \"") + expected_db_path.string() + "\" # default db path",
        }
    );

    aura_config_request_t request{};
    request.no_persist = 0;
    request.config_path_override = config_path_raw.c_str();

    aura_runtime_config_t config{};
    aura_error_t error{};
    const int rc = aura_config_resolve(&request, &config, &error);
    ExpectEq(rc, AURA_OK, "aura_config_resolve should accept TOML inline comment on db_path");
    ExpectEq(config.db_source, AURA_DB_SOURCE_CONFIG, "db source should remain config when env/cli are unset");
    ExpectTrue(
        std::string(config.db_path) == expected_db_path.string(),
        "db path should parse from TOML quoted value before comment"
    );

    std::error_code remove_error;
    std::filesystem::remove(config_path, remove_error);
    CleanupStoreFiles(expected_db_path);
}
