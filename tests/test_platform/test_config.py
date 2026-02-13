from __future__ import annotations

import argparse
import pathlib
import sys
import tempfile
import unittest
import warnings

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from runtime import store as runtime_store  # noqa: E402
from runtime.config import (  # noqa: E402
    resolve_config_file_path,
    resolve_default_db_path,
    resolve_runtime_config,
)


class RuntimeConfigTests(unittest.TestCase):
    def test_resolve_default_db_path_uses_windows_appdata(self) -> None:
        path = resolve_default_db_path(
            env={"APPDATA": r"C:\Users\dev\AppData\Roaming"},
            platform="win32",
            home=pathlib.Path(r"C:\Users\dev"),
        )
        self.assertEqual(path, r"C:\Users\dev\AppData\Roaming\Aura\telemetry.sqlite")

    def test_resolve_default_db_path_uses_windows_localappdata_fallback(self) -> None:
        path = resolve_default_db_path(
            env={"APPDATA": "  ", "LOCALAPPDATA": r"C:\Users\dev\AppData\Local"},
            platform="win32",
            home=pathlib.Path(r"C:\Users\dev"),
        )
        self.assertEqual(path, r"C:\Users\dev\AppData\Local\Aura\telemetry.sqlite")

    def test_resolve_default_db_path_uses_macos_app_support(self) -> None:
        path = resolve_default_db_path(
            env={},
            platform="darwin",
            home=pathlib.Path("/Users/dev"),
        )
        self.assertEqual(
            path.replace("\\", "/"),
            "/Users/dev/Library/Application Support/Aura/telemetry.sqlite",
        )

    def test_resolve_default_db_path_uses_xdg_data_home(self) -> None:
        path = resolve_default_db_path(
            env={"XDG_DATA_HOME": "/tmp/xdg-data"},
            platform="linux",
            home=pathlib.Path("/home/dev"),
        )
        self.assertEqual(path.replace("\\", "/"), "/tmp/xdg-data/Aura/telemetry.sqlite")

    def test_resolve_runtime_config_uses_cli_then_env_then_auto_precedence(self) -> None:
        args = argparse.Namespace(
            no_persist=False,
            db_path=" cli.sqlite ",
            retention_seconds=None,
        )
        config = resolve_runtime_config(
            args,
            env={"AURA_DB_PATH": "env.sqlite"},
            platform="linux",
            home=pathlib.Path("/home/dev"),
            config_file=pathlib.Path("/nonexistent/aura.toml"),
        )
        self.assertTrue(config.persistence_enabled)
        self.assertEqual(config.db_source, "cli")
        self.assertEqual(config.db_path, "cli.sqlite")

        args = argparse.Namespace(
            no_persist=False,
            db_path=None,
            retention_seconds=None,
        )
        config = resolve_runtime_config(
            args,
            env={"AURA_DB_PATH": " env.sqlite "},
            platform="linux",
            home=pathlib.Path("/home/dev"),
            config_file=pathlib.Path("/nonexistent/aura.toml"),
        )
        self.assertEqual(config.db_source, "env")
        self.assertEqual(config.db_path, "env.sqlite")

        config = resolve_runtime_config(
            args,
            env={},
            platform="linux",
            home=pathlib.Path("/home/dev"),
            config_file=pathlib.Path("/nonexistent/aura.toml"),
        )
        self.assertEqual(config.db_source, "auto")
        self.assertEqual(
            str(config.db_path).replace("\\", "/"),
            "/home/dev/.local/share/Aura/telemetry.sqlite",
        )

    def test_resolve_runtime_config_disables_persistence_with_no_persist(self) -> None:
        args = argparse.Namespace(
            no_persist=True,
            db_path="cli.sqlite",
            retention_seconds=None,
        )
        config = resolve_runtime_config(
            args,
            env={"AURA_DB_PATH": "env.sqlite"},
            platform="linux",
            home=pathlib.Path("/home/dev"),
            config_file=pathlib.Path("/nonexistent/aura.toml"),
        )
        self.assertFalse(config.persistence_enabled)
        self.assertEqual(config.db_source, "disabled")
        self.assertIsNone(config.db_path)

    def test_resolve_runtime_config_retention_uses_cli_then_env_then_default(self) -> None:
        args = argparse.Namespace(
            no_persist=False,
            db_path=None,
            retention_seconds=120.0,
        )
        config = resolve_runtime_config(
            args,
            env={"AURA_RETENTION_SECONDS": "60"},
            config_file=pathlib.Path("/nonexistent/aura.toml"),
        )
        self.assertEqual(config.retention_seconds, 120.0)

        args = argparse.Namespace(
            no_persist=False,
            db_path=None,
            retention_seconds=None,
        )
        config = resolve_runtime_config(
            args,
            env={"AURA_RETENTION_SECONDS": " 60 "},
            config_file=pathlib.Path("/nonexistent/aura.toml"),
        )
        self.assertEqual(config.retention_seconds, 60.0)

        config = resolve_runtime_config(
            args,
            env={},
            config_file=pathlib.Path("/nonexistent/aura.toml"),
        )
        self.assertEqual(config.retention_seconds, runtime_store.DEFAULT_RETENTION_SECONDS)

    def test_resolve_runtime_config_rejects_invalid_env_retention(self) -> None:
        args = argparse.Namespace(
            no_persist=False,
            db_path=None,
            retention_seconds=None,
        )
        with self.assertRaises(RuntimeError):
            resolve_runtime_config(
                args,
                env={"AURA_RETENTION_SECONDS": "nan"},
                config_file=pathlib.Path("/nonexistent/aura.toml"),
            )
        with self.assertRaises(RuntimeError):
            resolve_runtime_config(
                args,
                env={"AURA_RETENTION_SECONDS": "0"},
                config_file=pathlib.Path("/nonexistent/aura.toml"),
            )


class ConfigFilePathTests(unittest.TestCase):
    def test_windows_uses_appdata(self) -> None:
        path = resolve_config_file_path(
            env={"APPDATA": r"C:\Users\dev\AppData\Roaming"},
            platform="win32",
            home=pathlib.Path(r"C:\Users\dev"),
        )
        self.assertEqual(str(path), r"C:\Users\dev\AppData\Roaming\Aura\aura.toml")

    def test_macos_uses_app_support(self) -> None:
        path = resolve_config_file_path(
            env={},
            platform="darwin",
            home=pathlib.Path("/Users/dev"),
        )
        self.assertEqual(
            str(path).replace("\\", "/"),
            "/Users/dev/Library/Application Support/Aura/aura.toml",
        )

    def test_linux_uses_xdg_config_home(self) -> None:
        path = resolve_config_file_path(
            env={"XDG_CONFIG_HOME": "/tmp/xdg-config"},
            platform="linux",
            home=pathlib.Path("/home/dev"),
        )
        self.assertEqual(
            str(path).replace("\\", "/"),
            "/tmp/xdg-config/Aura/aura.toml",
        )

    def test_linux_falls_back_to_dot_config(self) -> None:
        path = resolve_config_file_path(
            env={},
            platform="linux",
            home=pathlib.Path("/home/dev"),
        )
        self.assertEqual(
            str(path).replace("\\", "/"),
            "/home/dev/.config/Aura/aura.toml",
        )


class TomlLoadingTests(unittest.TestCase):
    def _make_args(self) -> argparse.Namespace:
        return argparse.Namespace(
            no_persist=False,
            db_path=None,
            retention_seconds=None,
        )

    def test_missing_file_returns_none_silently(self) -> None:
        with warnings.catch_warnings(record=True) as caught:
            warnings.simplefilter("always")
            config = resolve_runtime_config(
                self._make_args(),
                env={},
                platform="linux",
                home=pathlib.Path("/home/dev"),
                config_file=pathlib.Path("/nonexistent/path/aura.toml"),
            )
        self.assertEqual(config.db_source, "auto")
        self.assertEqual(len(caught), 0)

    def test_parse_error_warns_and_returns_none(self) -> None:
        with tempfile.NamedTemporaryFile(suffix=".toml", delete=False) as f:
            f.write(b"[invalid toml ===")
            tmp_path = pathlib.Path(f.name)
        try:
            with warnings.catch_warnings(record=True) as caught:
                warnings.simplefilter("always")
                config = resolve_runtime_config(
                    self._make_args(),
                    env={},
                    platform="linux",
                    home=pathlib.Path("/home/dev"),
                    config_file=tmp_path,
                )
            self.assertEqual(config.db_source, "auto")
            self.assertTrue(any("Failed to parse" in str(w.message) for w in caught))
        finally:
            tmp_path.unlink()

    def test_full_persistence_section(self) -> None:
        toml_content = b'[persistence]\ndb_path = "/custom/db.sqlite"\nretention_seconds = 7200.0\n'
        with tempfile.NamedTemporaryFile(suffix=".toml", delete=False) as f:
            f.write(toml_content)
            tmp_path = pathlib.Path(f.name)
        try:
            config = resolve_runtime_config(
                self._make_args(),
                env={},
                platform="linux",
                home=pathlib.Path("/home/dev"),
                config_file=tmp_path,
            )
            self.assertEqual(config.db_source, "config")
            self.assertEqual(config.db_path, "/custom/db.sqlite")
            self.assertEqual(config.retention_seconds, 7200.0)
        finally:
            tmp_path.unlink()

    def test_partial_section_only_db_path(self) -> None:
        toml_content = b'[persistence]\ndb_path = "/custom/db.sqlite"\n'
        with tempfile.NamedTemporaryFile(suffix=".toml", delete=False) as f:
            f.write(toml_content)
            tmp_path = pathlib.Path(f.name)
        try:
            config = resolve_runtime_config(
                self._make_args(),
                env={},
                platform="linux",
                home=pathlib.Path("/home/dev"),
                config_file=tmp_path,
            )
            self.assertEqual(config.db_source, "config")
            self.assertEqual(config.db_path, "/custom/db.sqlite")
            self.assertEqual(config.retention_seconds, runtime_store.DEFAULT_RETENTION_SECONDS)
        finally:
            tmp_path.unlink()

    def test_partial_section_only_retention(self) -> None:
        toml_content = b"[persistence]\nretention_seconds = 3600.0\n"
        with tempfile.NamedTemporaryFile(suffix=".toml", delete=False) as f:
            f.write(toml_content)
            tmp_path = pathlib.Path(f.name)
        try:
            config = resolve_runtime_config(
                self._make_args(),
                env={},
                platform="linux",
                home=pathlib.Path("/home/dev"),
                config_file=tmp_path,
            )
            # No db_path in config, so falls through to auto
            self.assertEqual(config.db_source, "auto")
            self.assertEqual(config.retention_seconds, 3600.0)
        finally:
            tmp_path.unlink()

    def test_invalid_retention_drops_field_keeps_db_path(self) -> None:
        toml_content = b'[persistence]\ndb_path = "/custom/db.sqlite"\nretention_seconds = -1.0\n'
        with tempfile.NamedTemporaryFile(suffix=".toml", delete=False) as f:
            f.write(toml_content)
            tmp_path = pathlib.Path(f.name)
        try:
            with warnings.catch_warnings(record=True) as caught:
                warnings.simplefilter("always")
                config = resolve_runtime_config(
                    self._make_args(),
                    env={},
                    platform="linux",
                    home=pathlib.Path("/home/dev"),
                    config_file=tmp_path,
                )
            self.assertEqual(config.db_source, "config")
            self.assertEqual(config.db_path, "/custom/db.sqlite")
            self.assertEqual(config.retention_seconds, runtime_store.DEFAULT_RETENTION_SECONDS)
            self.assertTrue(any("Invalid retention_seconds" in str(w.message) for w in caught))
        finally:
            tmp_path.unlink()


class ConfigFilePriorityTests(unittest.TestCase):
    def _make_args(self, **kwargs: object) -> argparse.Namespace:
        defaults = {"no_persist": False, "db_path": None, "retention_seconds": None}
        defaults.update(kwargs)
        return argparse.Namespace(**defaults)

    def test_config_file_db_path_used_when_cli_and_env_absent(self) -> None:
        toml_content = b'[persistence]\ndb_path = "/from/config.sqlite"\n'
        with tempfile.NamedTemporaryFile(suffix=".toml", delete=False) as f:
            f.write(toml_content)
            tmp_path = pathlib.Path(f.name)
        try:
            config = resolve_runtime_config(
                self._make_args(),
                env={},
                platform="linux",
                home=pathlib.Path("/home/dev"),
                config_file=tmp_path,
            )
            self.assertEqual(config.db_source, "config")
            self.assertEqual(config.db_path, "/from/config.sqlite")
        finally:
            tmp_path.unlink()

    def test_env_overrides_config_file_db_path(self) -> None:
        toml_content = b'[persistence]\ndb_path = "/from/config.sqlite"\n'
        with tempfile.NamedTemporaryFile(suffix=".toml", delete=False) as f:
            f.write(toml_content)
            tmp_path = pathlib.Path(f.name)
        try:
            config = resolve_runtime_config(
                self._make_args(),
                env={"AURA_DB_PATH": "/from/env.sqlite"},
                platform="linux",
                home=pathlib.Path("/home/dev"),
                config_file=tmp_path,
            )
            self.assertEqual(config.db_source, "env")
            self.assertEqual(config.db_path, "/from/env.sqlite")
        finally:
            tmp_path.unlink()

    def test_config_file_retention_used_when_cli_and_env_absent(self) -> None:
        toml_content = b"[persistence]\nretention_seconds = 9999.0\n"
        with tempfile.NamedTemporaryFile(suffix=".toml", delete=False) as f:
            f.write(toml_content)
            tmp_path = pathlib.Path(f.name)
        try:
            config = resolve_runtime_config(
                self._make_args(),
                env={},
                platform="linux",
                home=pathlib.Path("/home/dev"),
                config_file=tmp_path,
            )
            self.assertEqual(config.retention_seconds, 9999.0)
        finally:
            tmp_path.unlink()

    def test_no_persist_still_returns_disabled_with_config_file(self) -> None:
        toml_content = b'[persistence]\ndb_path = "/from/config.sqlite"\nretention_seconds = 9999.0\n'
        with tempfile.NamedTemporaryFile(suffix=".toml", delete=False) as f:
            f.write(toml_content)
            tmp_path = pathlib.Path(f.name)
        try:
            config = resolve_runtime_config(
                self._make_args(no_persist=True),
                env={},
                platform="linux",
                home=pathlib.Path("/home/dev"),
                config_file=tmp_path,
            )
            self.assertFalse(config.persistence_enabled)
            self.assertEqual(config.db_source, "disabled")
            self.assertIsNone(config.db_path)
            # retention from config file is still used even when persistence disabled
            self.assertEqual(config.retention_seconds, 9999.0)
        finally:
            tmp_path.unlink()


if __name__ == "__main__":
    unittest.main()
