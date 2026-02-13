from __future__ import annotations

import argparse
import pathlib
import sys
import unittest


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from runtime import store as runtime_store  # noqa: E402
from runtime.config import resolve_default_db_path, resolve_runtime_config  # noqa: E402


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
        )
        self.assertEqual(config.db_source, "env")
        self.assertEqual(config.db_path, "env.sqlite")

        config = resolve_runtime_config(
            args,
            env={},
            platform="linux",
            home=pathlib.Path("/home/dev"),
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
        config = resolve_runtime_config(args, env={"AURA_RETENTION_SECONDS": "60"})
        self.assertEqual(config.retention_seconds, 120.0)

        args = argparse.Namespace(
            no_persist=False,
            db_path=None,
            retention_seconds=None,
        )
        config = resolve_runtime_config(args, env={"AURA_RETENTION_SECONDS": " 60 "})
        self.assertEqual(config.retention_seconds, 60.0)

        config = resolve_runtime_config(args, env={})
        self.assertEqual(config.retention_seconds, runtime_store.DEFAULT_RETENTION_SECONDS)

    def test_resolve_runtime_config_rejects_invalid_env_retention(self) -> None:
        args = argparse.Namespace(
            no_persist=False,
            db_path=None,
            retention_seconds=None,
        )
        with self.assertRaises(RuntimeError):
            resolve_runtime_config(args, env={"AURA_RETENTION_SECONDS": "nan"})
        with self.assertRaises(RuntimeError):
            resolve_runtime_config(args, env={"AURA_RETENTION_SECONDS": "0"})


if __name__ == "__main__":
    unittest.main()
