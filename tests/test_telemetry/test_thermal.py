from __future__ import annotations

import math
import pathlib
import sys
import threading
import types
import unittest

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from telemetry import thermal  # noqa: E402
from telemetry.thermal import (  # noqa: E402
    ThermalReading,
    ThermalSnapshot,
    collect_thermal_snapshot,
)


# ---------------------------------------------------------------------------
# Stubs
# ---------------------------------------------------------------------------

class _SensorEntry:
    """Stub for psutil sensor entry (named tuple-like)."""

    def __init__(
        self,
        label: str = "",
        current: float = 50.0,
        high: float | None = 90.0,
        critical: float | None = 100.0,
    ) -> None:
        self.label = label
        self.current = current
        self.high = high
        self.critical = critical


class _PsutilThermalStub:
    """Minimal psutil stub for thermal collection."""

    def __init__(self, groups: dict[str, list[_SensorEntry]] | None = None) -> None:
        self._groups = groups

    def sensors_temperatures(self) -> dict[str, list[_SensorEntry]] | None:
        return self._groups


class _PsutilThermalRaiseStub:
    """psutil stub that raises on sensors_temperatures."""

    def sensors_temperatures(self) -> None:
        raise OSError("sensor read failed")


# ---------------------------------------------------------------------------
# ThermalReading validation
# ---------------------------------------------------------------------------

class ThermalReadingValidationTests(unittest.TestCase):
    def test_valid_reading(self) -> None:
        r = ThermalReading(label="cpu", current_celsius=65.0, high_celsius=90.0, critical_celsius=100.0)
        self.assertEqual(r.label, "cpu")
        self.assertEqual(r.current_celsius, 65.0)
        self.assertEqual(r.high_celsius, 90.0)
        self.assertEqual(r.critical_celsius, 100.0)

    def test_label_stripped(self) -> None:
        r = ThermalReading(label="  gpu  ", current_celsius=40.0, high_celsius=None, critical_celsius=None)
        self.assertEqual(r.label, "gpu")

    def test_label_empty_rejected(self) -> None:
        with self.assertRaises(ValueError):
            ThermalReading(label="", current_celsius=40.0, high_celsius=None, critical_celsius=None)

    def test_label_whitespace_only_rejected(self) -> None:
        with self.assertRaises(ValueError):
            ThermalReading(label="   ", current_celsius=40.0, high_celsius=None, critical_celsius=None)

    def test_label_non_string_rejected(self) -> None:
        with self.assertRaises(TypeError):
            ThermalReading(label=123, current_celsius=40.0, high_celsius=None, critical_celsius=None)  # type: ignore[arg-type]

    def test_current_celsius_bool_rejected(self) -> None:
        with self.assertRaises(ValueError):
            ThermalReading(label="cpu", current_celsius=True, high_celsius=None, critical_celsius=None)  # type: ignore[arg-type]

    def test_current_celsius_nan_rejected(self) -> None:
        with self.assertRaises(ValueError):
            ThermalReading(label="cpu", current_celsius=math.nan, high_celsius=None, critical_celsius=None)

    def test_current_celsius_inf_rejected(self) -> None:
        with self.assertRaises(ValueError):
            ThermalReading(label="cpu", current_celsius=math.inf, high_celsius=None, critical_celsius=None)

    def test_current_celsius_below_range_rejected(self) -> None:
        with self.assertRaises(ValueError):
            ThermalReading(label="cpu", current_celsius=-31.0, high_celsius=None, critical_celsius=None)

    def test_current_celsius_above_range_rejected(self) -> None:
        with self.assertRaises(ValueError):
            ThermalReading(label="cpu", current_celsius=151.0, high_celsius=None, critical_celsius=None)

    def test_current_celsius_boundary_low(self) -> None:
        r = ThermalReading(label="cpu", current_celsius=-30.0, high_celsius=None, critical_celsius=None)
        self.assertEqual(r.current_celsius, -30.0)

    def test_current_celsius_boundary_high(self) -> None:
        r = ThermalReading(label="cpu", current_celsius=150.0, high_celsius=None, critical_celsius=None)
        self.assertEqual(r.current_celsius, 150.0)

    def test_high_celsius_none_accepted(self) -> None:
        r = ThermalReading(label="cpu", current_celsius=50.0, high_celsius=None, critical_celsius=None)
        self.assertIsNone(r.high_celsius)

    def test_high_celsius_bool_rejected(self) -> None:
        with self.assertRaises(ValueError):
            ThermalReading(label="cpu", current_celsius=50.0, high_celsius=True, critical_celsius=None)  # type: ignore[arg-type]

    def test_high_celsius_out_of_range_rejected(self) -> None:
        with self.assertRaises(ValueError):
            ThermalReading(label="cpu", current_celsius=50.0, high_celsius=251.0, critical_celsius=None)

    def test_critical_celsius_bool_rejected(self) -> None:
        with self.assertRaises(ValueError):
            ThermalReading(label="cpu", current_celsius=50.0, high_celsius=None, critical_celsius=False)  # type: ignore[arg-type]

    def test_critical_celsius_nan_rejected(self) -> None:
        with self.assertRaises(ValueError):
            ThermalReading(label="cpu", current_celsius=50.0, high_celsius=None, critical_celsius=math.nan)

    def test_current_celsius_int_normalized_to_float(self) -> None:
        r = ThermalReading(label="cpu", current_celsius=50, high_celsius=None, critical_celsius=None)
        self.assertIsInstance(r.current_celsius, float)
        self.assertEqual(r.current_celsius, 50.0)

    def test_high_celsius_boundary_250(self) -> None:
        r = ThermalReading(label="cpu", current_celsius=50.0, high_celsius=250.0, critical_celsius=None)
        self.assertEqual(r.high_celsius, 250.0)


# ---------------------------------------------------------------------------
# ThermalSnapshot validation
# ---------------------------------------------------------------------------

class ThermalSnapshotValidationTests(unittest.TestCase):
    def test_valid_snapshot(self) -> None:
        r = ThermalReading(label="cpu", current_celsius=65.0, high_celsius=None, critical_celsius=None)
        snap = ThermalSnapshot(timestamp=100.0, readings=(r,), hottest_celsius=65.0)
        self.assertEqual(snap.timestamp, 100.0)
        self.assertEqual(len(snap.readings), 1)
        self.assertEqual(snap.hottest_celsius, 65.0)

    def test_empty_readings(self) -> None:
        snap = ThermalSnapshot(timestamp=100.0, readings=(), hottest_celsius=None)
        self.assertEqual(snap.readings, ())
        self.assertIsNone(snap.hottest_celsius)

    def test_timestamp_bool_rejected(self) -> None:
        with self.assertRaises(ValueError):
            ThermalSnapshot(timestamp=True, readings=(), hottest_celsius=None)  # type: ignore[arg-type]

    def test_timestamp_nan_rejected(self) -> None:
        with self.assertRaises(ValueError):
            ThermalSnapshot(timestamp=math.nan, readings=(), hottest_celsius=None)

    def test_timestamp_inf_rejected(self) -> None:
        with self.assertRaises(ValueError):
            ThermalSnapshot(timestamp=math.inf, readings=(), hottest_celsius=None)

    def test_hottest_celsius_nan_rejected(self) -> None:
        with self.assertRaises(ValueError):
            ThermalSnapshot(timestamp=1.0, readings=(), hottest_celsius=math.nan)

    def test_hottest_celsius_none_accepted(self) -> None:
        snap = ThermalSnapshot(timestamp=1.0, readings=(), hottest_celsius=None)
        self.assertIsNone(snap.hottest_celsius)

    def test_to_dict(self) -> None:
        r = ThermalReading(label="cpu", current_celsius=65.0, high_celsius=90.0, critical_celsius=100.0)
        snap = ThermalSnapshot(timestamp=1.0, readings=(r,), hottest_celsius=65.0)
        d = snap.to_dict()
        self.assertEqual(d["timestamp"], 1.0)
        self.assertEqual(d["hottest_celsius"], 65.0)
        self.assertEqual(len(d["readings"]), 1)
        self.assertEqual(d["readings"][0]["label"], "cpu")

    def test_timestamp_int_normalized_to_float(self) -> None:
        snap = ThermalSnapshot(timestamp=1, readings=(), hottest_celsius=None)
        self.assertIsInstance(snap.timestamp, float)


# ---------------------------------------------------------------------------
# collect_thermal_snapshot — psutil path
# ---------------------------------------------------------------------------

class CollectThermalSnapshotPsutilTests(unittest.TestCase):
    def setUp(self) -> None:
        self._original_psutil = thermal.psutil

    def tearDown(self) -> None:
        thermal.psutil = self._original_psutil

    def test_single_sensor_group(self) -> None:
        stub = _PsutilThermalStub({
            "coretemp": [_SensorEntry(label="Core 0", current=55.0, high=85.0, critical=100.0)],
        })
        thermal.psutil = stub
        snap = collect_thermal_snapshot(now=lambda: 1.0)
        self.assertEqual(len(snap.readings), 1)
        self.assertEqual(snap.readings[0].label, "Core 0")
        self.assertEqual(snap.readings[0].current_celsius, 55.0)
        self.assertEqual(snap.hottest_celsius, 55.0)

    def test_multiple_groups(self) -> None:
        stub = _PsutilThermalStub({
            "coretemp": [
                _SensorEntry(label="Core 0", current=55.0, high=85.0, critical=100.0),
                _SensorEntry(label="Core 1", current=60.0, high=85.0, critical=100.0),
            ],
            "acpitz": [
                _SensorEntry(label="", current=45.0, high=None, critical=None),
            ],
        })
        thermal.psutil = stub
        snap = collect_thermal_snapshot(now=lambda: 2.0)
        self.assertEqual(len(snap.readings), 3)
        self.assertEqual(snap.hottest_celsius, 60.0)
        # entry with empty label should use group name
        labels = [r.label for r in snap.readings]
        self.assertIn("acpitz", labels)

    def test_out_of_range_entry_skipped(self) -> None:
        stub = _PsutilThermalStub({
            "coretemp": [
                _SensorEntry(label="good", current=55.0, high=None, critical=None),
                _SensorEntry(label="bad", current=200.0, high=None, critical=None),
            ],
        })
        thermal.psutil = stub
        snap = collect_thermal_snapshot(now=lambda: 1.0)
        self.assertEqual(len(snap.readings), 1)
        self.assertEqual(snap.readings[0].label, "good")

    def test_nan_entry_skipped(self) -> None:
        stub = _PsutilThermalStub({
            "coretemp": [
                _SensorEntry(label="good", current=55.0, high=None, critical=None),
                _SensorEntry(label="nan", current=math.nan, high=None, critical=None),
            ],
        })
        thermal.psutil = stub
        snap = collect_thermal_snapshot(now=lambda: 1.0)
        self.assertEqual(len(snap.readings), 1)

    def test_psutil_exception_returns_empty(self) -> None:
        stub = _PsutilThermalRaiseStub()
        thermal.psutil = stub
        snap = collect_thermal_snapshot(now=lambda: 1.0)
        self.assertEqual(len(snap.readings), 0)
        self.assertIsNone(snap.hottest_celsius)

    def test_psutil_returns_none(self) -> None:
        stub = _PsutilThermalStub(None)
        thermal.psutil = stub
        snap = collect_thermal_snapshot(now=lambda: 1.0)
        self.assertEqual(len(snap.readings), 0)

    def test_psutil_returns_empty_dict(self) -> None:
        stub = _PsutilThermalStub({})
        thermal.psutil = stub
        snap = collect_thermal_snapshot(now=lambda: 1.0)
        self.assertEqual(len(snap.readings), 0)

    def test_custom_now_used(self) -> None:
        stub = _PsutilThermalStub({
            "coretemp": [_SensorEntry(label="Core 0", current=50.0)],
        })
        thermal.psutil = stub
        snap = collect_thermal_snapshot(now=lambda: 42.5)
        self.assertEqual(snap.timestamp, 42.5)

    def test_bad_high_celsius_sanitized_to_none(self) -> None:
        stub = _PsutilThermalStub({
            "coretemp": [_SensorEntry(label="Core 0", current=50.0, high=math.nan, critical=None)],
        })
        thermal.psutil = stub
        snap = collect_thermal_snapshot(now=lambda: 1.0)
        self.assertEqual(len(snap.readings), 1)
        self.assertIsNone(snap.readings[0].high_celsius)

    def test_bad_critical_celsius_sanitized_to_none(self) -> None:
        stub = _PsutilThermalStub({
            "coretemp": [_SensorEntry(label="Core 0", current=50.0, high=None, critical=math.inf)],
        })
        thermal.psutil = stub
        snap = collect_thermal_snapshot(now=lambda: 1.0)
        self.assertEqual(len(snap.readings), 1)
        self.assertIsNone(snap.readings[0].critical_celsius)


# ---------------------------------------------------------------------------
# collect_thermal_snapshot — WMI path
# ---------------------------------------------------------------------------

class _WmiZoneStub:
    """Stub for a single WMI MSAcpi_ThermalZoneTemperature instance."""

    def __init__(self, current_temperature: int | float, instance_name: str = "") -> None:
        self.CurrentTemperature = current_temperature
        if instance_name:
            self.InstanceName = instance_name


class _WmiNamespaceStub:
    """Stub for wmi.WMI(namespace=...)."""

    def __init__(self, zones: list[_WmiZoneStub]) -> None:
        self._zones = zones

    def MSAcpi_ThermalZoneTemperature(self) -> list[_WmiZoneStub]:
        return self._zones


def _make_wmi_module(zones: list[_WmiZoneStub]) -> types.ModuleType:
    """Create a fake wmi module that returns the given zones."""
    mod = types.ModuleType("wmi")
    ns_stub = _WmiNamespaceStub(zones)
    mod.WMI = lambda namespace="": ns_stub  # type: ignore[attr-defined]
    return mod


class CollectThermalSnapshotWmiTests(unittest.TestCase):
    def setUp(self) -> None:
        self._original_psutil = thermal.psutil
        # Disable psutil path so WMI path is exercised
        thermal.psutil = None

    def tearDown(self) -> None:
        thermal.psutil = self._original_psutil
        sys.modules.pop("wmi", None)

    def test_wmi_decikelvin_conversion(self) -> None:
        # 3232 tenths-of-Kelvin = 323.2K = 50.05°C
        zones = [_WmiZoneStub(current_temperature=3232, instance_name="Zone0")]
        sys.modules["wmi"] = _make_wmi_module(zones)
        snap = collect_thermal_snapshot(now=lambda: 1.0)
        self.assertEqual(len(snap.readings), 1)
        self.assertAlmostEqual(snap.readings[0].current_celsius, 50.05, places=1)
        self.assertEqual(snap.readings[0].label, "Zone0")

    def test_wmi_multiple_zones(self) -> None:
        zones = [
            _WmiZoneStub(current_temperature=3232, instance_name="Zone0"),
            _WmiZoneStub(current_temperature=3332, instance_name="Zone1"),
        ]
        sys.modules["wmi"] = _make_wmi_module(zones)
        snap = collect_thermal_snapshot(now=lambda: 1.0)
        self.assertEqual(len(snap.readings), 2)
        self.assertEqual(snap.hottest_celsius, max(r.current_celsius for r in snap.readings))

    def test_wmi_out_of_range_skipped(self) -> None:
        # 5000 deciK = 226.85°C → above 150 range
        zones = [
            _WmiZoneStub(current_temperature=3232, instance_name="Good"),
            _WmiZoneStub(current_temperature=5000, instance_name="TooHot"),
        ]
        sys.modules["wmi"] = _make_wmi_module(zones)
        snap = collect_thermal_snapshot(now=lambda: 1.0)
        self.assertEqual(len(snap.readings), 1)
        self.assertEqual(snap.readings[0].label, "Good")

    def test_wmi_fallback_label_when_no_instance_name(self) -> None:
        zones = [_WmiZoneStub(current_temperature=3232)]
        sys.modules["wmi"] = _make_wmi_module(zones)
        snap = collect_thermal_snapshot(now=lambda: 1.0)
        self.assertEqual(snap.readings[0].label, "ThermalZone0")

    def test_wmi_import_error_returns_empty(self) -> None:
        # Ensure wmi is NOT importable
        sys.modules.pop("wmi", None)
        # Patch import to fail
        import builtins
        original_import = builtins.__import__

        def _fail_wmi(name: str, *args: object, **kwargs: object) -> object:
            if name == "wmi":
                raise ImportError("no wmi")
            return original_import(name, *args, **kwargs)

        builtins.__import__ = _fail_wmi  # type: ignore[assignment]
        try:
            snap = collect_thermal_snapshot(now=lambda: 1.0)
            self.assertEqual(len(snap.readings), 0)
        finally:
            builtins.__import__ = original_import  # type: ignore[assignment]

    def test_wmi_connection_error_returns_empty(self) -> None:
        mod = types.ModuleType("wmi")
        mod.WMI = lambda namespace="": (_ for _ in ()).throw(Exception("connection failed"))  # type: ignore[attr-defined]
        sys.modules["wmi"] = mod
        snap = collect_thermal_snapshot(now=lambda: 1.0)
        self.assertEqual(len(snap.readings), 0)


# ---------------------------------------------------------------------------
# Graceful degradation
# ---------------------------------------------------------------------------

class CollectThermalSnapshotDegradationTests(unittest.TestCase):
    def setUp(self) -> None:
        self._original_psutil = thermal.psutil

    def tearDown(self) -> None:
        thermal.psutil = self._original_psutil
        sys.modules.pop("wmi", None)

    def test_no_psutil_no_wmi_returns_empty_snapshot(self) -> None:
        thermal.psutil = None
        sys.modules.pop("wmi", None)
        import builtins
        original_import = builtins.__import__

        def _fail_wmi(name: str, *args: object, **kwargs: object) -> object:
            if name == "wmi":
                raise ImportError("no wmi")
            return original_import(name, *args, **kwargs)

        builtins.__import__ = _fail_wmi  # type: ignore[assignment]
        try:
            snap = collect_thermal_snapshot(now=lambda: 1.0)
            self.assertEqual(len(snap.readings), 0)
            self.assertIsNone(snap.hottest_celsius)
            self.assertEqual(snap.timestamp, 1.0)
        finally:
            builtins.__import__ = original_import  # type: ignore[assignment]

    def test_never_raises(self) -> None:
        """Even with completely broken backends, collect never raises."""
        thermal.psutil = None
        sys.modules.pop("wmi", None)
        import builtins
        original_import = builtins.__import__

        def _fail_wmi(name: str, *args: object, **kwargs: object) -> object:
            if name == "wmi":
                raise ImportError("no wmi")
            return original_import(name, *args, **kwargs)

        builtins.__import__ = _fail_wmi  # type: ignore[assignment]
        try:
            # Should not raise under any circumstances
            for _ in range(10):
                snap = collect_thermal_snapshot(now=lambda: 1.0)
                self.assertIsInstance(snap, ThermalSnapshot)
        finally:
            builtins.__import__ = original_import  # type: ignore[assignment]


# ---------------------------------------------------------------------------
# Thread safety
# ---------------------------------------------------------------------------

class ThermalThreadSafetyTests(unittest.TestCase):
    def setUp(self) -> None:
        self._original_psutil = thermal.psutil

    def tearDown(self) -> None:
        thermal.psutil = self._original_psutil

    def test_concurrent_collection_no_exceptions(self) -> None:
        worker_count = 8
        stub = _PsutilThermalStub({
            "coretemp": [_SensorEntry(label="Core 0", current=55.0, high=85.0, critical=100.0)],
        })
        thermal.psutil = stub

        errors: list[Exception] = []
        barrier = threading.Barrier(worker_count)

        def _worker() -> None:
            try:
                barrier.wait()
                snap = collect_thermal_snapshot(now=lambda: 1.0)
                assert isinstance(snap, ThermalSnapshot)
            except Exception as exc:
                errors.append(exc)

        threads = [threading.Thread(target=_worker) for _ in range(worker_count)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        self.assertEqual(errors, [])


if __name__ == "__main__":
    unittest.main()
