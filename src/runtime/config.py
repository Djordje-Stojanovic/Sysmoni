from __future__ import annotations

import math
import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Literal, Mapping

from runtime.store import DEFAULT_RETENTION_SECONDS

_APP_DIR_NAME = "Aura"
_DB_FILENAME = "telemetry.sqlite"


@dataclass(frozen=True)
class RuntimeConfig:
    db_path: str | None
    retention_seconds: float
    persistence_enabled: bool
    db_source: Literal["cli", "env", "auto", "disabled"]


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


def resolve_runtime_config(
    args: object,
    *,
    env: Mapping[str, str] | None = None,
    platform: str | None = None,
    home: Path | None = None,
) -> RuntimeConfig:
    env_map = os.environ if env is None else env
    persistence_enabled = not bool(getattr(args, "no_persist", False))

    cli_retention = getattr(args, "retention_seconds", None)
    if cli_retention is not None:
        retention_seconds = _require_positive_finite_retention(float(cli_retention))
    else:
        env_retention = _normalize_optional_path(env_map.get("AURA_RETENTION_SECONDS"))
        if env_retention is not None:
            retention_seconds = _parse_env_retention(env_retention)
        else:
            retention_seconds = DEFAULT_RETENTION_SECONDS

    if not persistence_enabled:
        return RuntimeConfig(
            db_path=None,
            retention_seconds=retention_seconds,
            persistence_enabled=False,
            db_source="disabled",
        )

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

    auto_db_path = resolve_default_db_path(env=env_map, platform=platform, home=home)
    return RuntimeConfig(
        db_path=auto_db_path,
        retention_seconds=retention_seconds,
        persistence_enabled=True,
        db_source="auto",
    )
