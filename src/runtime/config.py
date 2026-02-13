from __future__ import annotations

import math
import os
import sys
import tomllib
import warnings
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Literal, Mapping

from runtime.store import DEFAULT_RETENTION_SECONDS

_APP_DIR_NAME = "Aura"
_DB_FILENAME = "telemetry.sqlite"
_CONFIG_FILENAME = "aura.toml"


@dataclass(frozen=True)
class RuntimeConfig:
    db_path: str | None
    retention_seconds: float
    persistence_enabled: bool
    db_source: Literal["cli", "env", "config", "auto", "disabled"]


@dataclass(frozen=True)
class _TomlFileConfig:
    db_path: str | None = None
    retention_seconds: float | None = None


def _normalize_optional_path(value: str | None) -> str | None:
    if value is None:
        return None
    normalized = value.strip()
    return normalized or None


def _require_positive_finite_retention(value: float) -> float:
    retention_seconds = float(value)
    if not math.isfinite(retention_seconds) or retention_seconds <= 0:
        raise ValueError("retention_seconds must be a finite number greater than 0.")
    return retention_seconds


def _parse_env_retention(value: str) -> float:
    try:
        parsed = _require_positive_finite_retention(float(value))
    except (TypeError, ValueError, OverflowError) as exc:
        raise RuntimeError(
            "AURA_RETENTION_SECONDS must be a finite number greater than 0."
        ) from exc
    return parsed


def resolve_default_db_path(
    *,
    env: Mapping[str, str] | None = None,
    platform: str | None = None,
    home: Path | None = None,
) -> str:
    env_map = os.environ if env is None else env
    platform_name = sys.platform if platform is None else platform
    home_dir = Path.home() if home is None else home

    if platform_name.startswith("win"):
        app_data = _normalize_optional_path(env_map.get("APPDATA"))
        if app_data is not None:
            base_dir = Path(app_data)
        else:
            local_app_data = _normalize_optional_path(env_map.get("LOCALAPPDATA"))
            if local_app_data is not None:
                base_dir = Path(local_app_data)
            else:
                base_dir = home_dir / "AppData" / "Roaming"
    elif platform_name == "darwin":
        base_dir = home_dir / "Library" / "Application Support"
    else:
        xdg_data_home = _normalize_optional_path(env_map.get("XDG_DATA_HOME"))
        if xdg_data_home is not None:
            base_dir = Path(xdg_data_home)
        else:
            base_dir = home_dir / ".local" / "share"

    return str(base_dir / _APP_DIR_NAME / _DB_FILENAME)


def resolve_config_file_path(
    *,
    env: Mapping[str, str] | None = None,
    platform: str | None = None,
    home: Path | None = None,
) -> Path:
    env_map = os.environ if env is None else env
    platform_name = sys.platform if platform is None else platform
    home_dir = Path.home() if home is None else home

    if platform_name.startswith("win"):
        app_data = _normalize_optional_path(env_map.get("APPDATA"))
        if app_data is not None:
            base_dir = Path(app_data)
        else:
            local_app_data = _normalize_optional_path(env_map.get("LOCALAPPDATA"))
            if local_app_data is not None:
                base_dir = Path(local_app_data)
            else:
                base_dir = home_dir / "AppData" / "Roaming"
    elif platform_name == "darwin":
        base_dir = home_dir / "Library" / "Application Support"
    else:
        xdg_config_home = _normalize_optional_path(env_map.get("XDG_CONFIG_HOME"))
        if xdg_config_home is not None:
            base_dir = Path(xdg_config_home)
        else:
            base_dir = home_dir / ".config"

    return base_dir / _APP_DIR_NAME / _CONFIG_FILENAME


def _load_toml_config(
    config_path: Path, *, warn: bool = True
) -> _TomlFileConfig | None:
    try:
        raw = config_path.read_bytes()
    except FileNotFoundError:
        return None

    try:
        data: dict[str, Any] = tomllib.loads(raw.decode())
    except (tomllib.TOMLDecodeError, UnicodeDecodeError) as exc:
        if warn:
            warnings.warn(
                f"Failed to parse config file {config_path}: {exc}",
                stacklevel=2,
            )
        return None

    persistence: dict[str, Any] = data.get("persistence", {})
    if not isinstance(persistence, dict):
        if warn:
            warnings.warn(
                f"Invalid [persistence] section in {config_path}: expected table",
                stacklevel=2,
            )
        return _TomlFileConfig()

    db_path: str | None = None
    raw_db = persistence.get("db_path")
    if raw_db is not None:
        if isinstance(raw_db, str):
            db_path = _normalize_optional_path(raw_db)
        elif warn:
            warnings.warn(
                f"Invalid db_path in {config_path}: expected string",
                stacklevel=2,
            )

    retention: float | None = None
    raw_ret = persistence.get("retention_seconds")
    if raw_ret is not None:
        try:
            retention = _require_positive_finite_retention(float(raw_ret))
        except (TypeError, ValueError, OverflowError):
            if warn:
                warnings.warn(
                    f"Invalid retention_seconds in {config_path}: "
                    "must be a finite number greater than 0",
                    stacklevel=2,
                )

    return _TomlFileConfig(db_path=db_path, retention_seconds=retention)


def resolve_runtime_config(
    args: object,
    *,
    env: Mapping[str, str] | None = None,
    platform: str | None = None,
    home: Path | None = None,
    config_file: Path | None = None,
) -> RuntimeConfig:
    env_map = os.environ if env is None else env
    persistence_enabled = not bool(getattr(args, "no_persist", False))

    # Load TOML config file (once)
    if config_file is None:
        config_file = resolve_config_file_path(
            env=env_map, platform=platform, home=home
        )
    toml_cfg = _load_toml_config(config_file)

    # Retention: CLI > env > config file > default
    cli_retention = getattr(args, "retention_seconds", None)
    if cli_retention is not None:
        retention_seconds = _require_positive_finite_retention(float(cli_retention))
    else:
        env_retention = _normalize_optional_path(env_map.get("AURA_RETENTION_SECONDS"))
        if env_retention is not None:
            retention_seconds = _parse_env_retention(env_retention)
        elif toml_cfg is not None and toml_cfg.retention_seconds is not None:
            retention_seconds = toml_cfg.retention_seconds
        else:
            retention_seconds = DEFAULT_RETENTION_SECONDS

    if not persistence_enabled:
        return RuntimeConfig(
            db_path=None,
            retention_seconds=retention_seconds,
            persistence_enabled=False,
            db_source="disabled",
        )

    # DB path: CLI > env > config file > auto
    cli_db_path = _normalize_optional_path(getattr(args, "db_path", None))
    if cli_db_path is not None:
        return RuntimeConfig(
            db_path=cli_db_path,
            retention_seconds=retention_seconds,
            persistence_enabled=True,
            db_source="cli",
        )

    env_db_path = _normalize_optional_path(env_map.get("AURA_DB_PATH"))
    if env_db_path is not None:
        return RuntimeConfig(
            db_path=env_db_path,
            retention_seconds=retention_seconds,
            persistence_enabled=True,
            db_source="env",
        )

    if toml_cfg is not None and toml_cfg.db_path is not None:
        return RuntimeConfig(
            db_path=toml_cfg.db_path,
            retention_seconds=retention_seconds,
            persistence_enabled=True,
            db_source="config",
        )

    auto_db_path = resolve_default_db_path(env=env_map, platform=platform, home=home)
    return RuntimeConfig(
        db_path=auto_db_path,
        retention_seconds=retention_seconds,
        persistence_enabled=True,
        db_source="auto",
    )
