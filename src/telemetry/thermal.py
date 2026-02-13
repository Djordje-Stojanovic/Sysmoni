from __future__ import annotations

import math
import threading
import time
from dataclasses import asdict, dataclass
from typing import Callable

from . import native_backend

try:
    import psutil
except ImportError:
    psutil = None  # type: ignore[assignment]

_CELSIUS_MIN = -30.0
_CELSIUS_MAX = 150.0
_CELSIUS_OPTIONAL_MAX = 250.0


def _validate_finite_float(value: object, name: str) -> float:
    if isinstance(value, bool):
        raise ValueError(f"{name} must be a finite number, got bool.")
    result = float(value)  # type: ignore[arg-type]
    if not math.isfinite(result):
        raise ValueError(f"{name} must be a finite number.")
    return result


def _normalize_optional_celsius(
    value: float | None, name: str, *, hi: float = _CELSIUS_OPTIONAL_MAX
) -> float | None:
    if value is None:
        return None
    v = _validate_finite_float(value, name)
    if v < _CELSIUS_MIN or v > hi:
        raise ValueError(f"{name} must be in [{_CELSIUS_MIN}, {hi}].")
    return v


@dataclass(slots=True, frozen=True)
class ThermalReading:
    """A single thermal sensor reading."""

    label: str
    current_celsius: float
    high_celsius: float | None
    critical_celsius: float | None

    def __post_init__(self) -> None:
        if not isinstance(self.label, str):
            raise TypeError("label must be a string.")
        label = self.label.strip()
        if not label:
            raise ValueError("label must not be empty.")
        object.__setattr__(self, "label", label)

        current = _validate_finite_float(self.current_celsius, "current_celsius")
        if current < _CELSIUS_MIN or current > _CELSIUS_MAX:
            raise ValueError(
                f"current_celsius must be in [{_CELSIUS_MIN}, {_CELSIUS_MAX}]."
            )
        object.__setattr__(self, "current_celsius", current)

        high = _normalize_optional_celsius(self.high_celsius, "high_celsius")
        object.__setattr__(self, "high_celsius", high)

        critical = _normalize_optional_celsius(self.critical_celsius, "critical_celsius")
        object.__setattr__(self, "critical_celsius", critical)


@dataclass(slots=True, frozen=True)
class ThermalSnapshot:
    """Point-in-time collection of all thermal readings."""

    timestamp: float
    readings: tuple[ThermalReading, ...]
    hottest_celsius: float | None

    def __post_init__(self) -> None:
        ts = _validate_finite_float(self.timestamp, "timestamp")
        object.__setattr__(self, "timestamp", ts)

        if self.hottest_celsius is not None:
            hottest = _validate_finite_float(self.hottest_celsius, "hottest_celsius")
            object.__setattr__(self, "hottest_celsius", hottest)

    def to_dict(self) -> dict:
        return asdict(self)


_thermal_collection_lock = threading.Lock()


def _try_collect_native() -> list[ThermalReading]:
    """Attempt to collect thermal readings via native backend."""
    try:
        native_readings = native_backend.collect_thermal_readings()
    except RuntimeError:
        return []

    if native_readings is None:
        return []

    readings: list[ThermalReading] = []
    for native_reading in native_readings:
        try:
            readings.append(
                ThermalReading(
                    label=native_reading.label,
                    current_celsius=native_reading.current_celsius,
                    high_celsius=native_reading.high_celsius,
                    critical_celsius=native_reading.critical_celsius,
                )
            )
        except (TypeError, ValueError):
            continue
    return readings


def _try_collect_psutil() -> list[ThermalReading]:
    """Attempt to collect thermal readings via psutil."""
    if psutil is None:
        return []
    temps_fn = getattr(psutil, "sensors_temperatures", None)
    if temps_fn is None:
        return []
    try:
        groups: dict | None = temps_fn()
    except Exception:
        return []
    if not groups:
        return []

    readings: list[ThermalReading] = []
    for group_name, entries in groups.items():
        for entry in entries:
            label = entry.label or group_name
            current = entry.current
            if not isinstance(current, (int, float)) or isinstance(current, bool):
                continue
            if not math.isfinite(current):
                continue
            if current < _CELSIUS_MIN or current > _CELSIUS_MAX:
                continue

            high: float | None = getattr(entry, "high", None)
            if high is not None:
                if isinstance(high, bool) or not isinstance(high, (int, float)):
                    high = None
                elif (
                    not math.isfinite(high)
                    or high < _CELSIUS_MIN
                    or high > _CELSIUS_OPTIONAL_MAX
                ):
                    high = None

            critical: float | None = getattr(entry, "critical", None)
            if critical is not None:
                if isinstance(critical, bool) or not isinstance(critical, (int, float)):
                    critical = None
                elif (
                    not math.isfinite(critical)
                    or critical < _CELSIUS_MIN
                    or critical > _CELSIUS_OPTIONAL_MAX
                ):
                    critical = None

            readings.append(
                ThermalReading(
                    label=label,
                    current_celsius=float(current),
                    high_celsius=float(high) if high is not None else None,
                    critical_celsius=float(critical) if critical is not None else None,
                )
            )
    return readings


def _try_collect_wmi() -> list[ThermalReading]:
    """Attempt to collect thermal readings via WMI on Windows."""
    try:
        import wmi as wmi_mod  # type: ignore[import-untyped]
    except Exception:
        return []

    try:
        w = wmi_mod.WMI(namespace="root\\wmi")
        zones = w.MSAcpi_ThermalZoneTemperature()
    except Exception:
        return []

    readings: list[ThermalReading] = []
    for idx, zone in enumerate(zones):
        try:
            raw_temp = zone.CurrentTemperature
        except Exception:
            continue
        if not isinstance(raw_temp, (int, float)) or isinstance(raw_temp, bool):
            continue
        celsius = (raw_temp / 10.0) - 273.15
        if not math.isfinite(celsius):
            continue
        if celsius < _CELSIUS_MIN or celsius > _CELSIUS_MAX:
            continue

        label = getattr(zone, "InstanceName", None) or f"ThermalZone{idx}"
        readings.append(
            ThermalReading(
                label=str(label),
                current_celsius=round(celsius, 2),
                high_celsius=None,
                critical_celsius=None,
            )
        )
    return readings


def collect_thermal_snapshot(
    now: Callable[[], float] | None = None,
) -> ThermalSnapshot:
    """Collect thermal readings from all available sources.

    Never raises and degrades gracefully to an empty snapshot.
    """
    current_time = time.time if now is None else now
    ts = float(current_time())

    with _thermal_collection_lock:
        readings = _try_collect_native()
        if not readings:
            readings = _try_collect_psutil()
        if not readings:
            readings = _try_collect_wmi()

    readings_tuple = tuple(readings)
    hottest: float | None = None
    if readings_tuple:
        hottest = max(r.current_celsius for r in readings_tuple)

    return ThermalSnapshot(
        timestamp=ts,
        readings=readings_tuple,
        hottest_celsius=hottest,
    )
