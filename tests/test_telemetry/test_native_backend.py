from __future__ import annotations

import ctypes
import pathlib
import sys
import unittest

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from telemetry import native_backend  # noqa: E402


class _FakeBindings:
    def __init__(self) -> None:
        self.system_status = native_backend.AURA_STATUS_OK
        self.process_status = native_backend.AURA_STATUS_OK
        self.disk_status = native_backend.AURA_STATUS_OK
        self.network_status = native_backend.AURA_STATUS_OK
        self.thermal_status = native_backend.AURA_STATUS_OK

    def _set_error(self, error_buffer: object, message: bytes) -> None:
        ctypes.memmove(error_buffer, message + b"\x00", len(message) + 1)

    def collect_system_snapshot(
        self,
        cpu_percent: object,
        memory_percent: object,
        error_buffer: object,
        _error_buffer_len: int,
    ) -> int:
        if self.system_status == native_backend.AURA_STATUS_OK:
            ctypes.cast(cpu_percent, ctypes.POINTER(ctypes.c_double))[0] = 17.5
            ctypes.cast(memory_percent, ctypes.POINTER(ctypes.c_double))[0] = 61.25
            return self.system_status
        self._set_error(error_buffer, b"system failed")
        return self.system_status

    def collect_processes(
        self,
        samples: object,
        max_samples: int,
        out_count: object,
        error_buffer: object,
        _error_buffer_len: int,
    ) -> int:
        if self.process_status != native_backend.AURA_STATUS_OK:
            self._set_error(error_buffer, b"processes failed")
            return self.process_status

        array = ctypes.cast(samples, ctypes.POINTER(native_backend._AuraProcessSample))
        write_count = 2 if max_samples >= 2 else 1

        array[0].pid = 10
        array[0].name = b"alpha"
        array[0].cpu_percent = 30.0
        array[0].memory_rss_bytes = 1000

        if write_count > 1:
            array[1].pid = 20
            array[1].name = b""
            array[1].cpu_percent = 35.0
            array[1].memory_rss_bytes = 2000

        ctypes.cast(out_count, ctypes.POINTER(ctypes.c_uint32))[0] = write_count
        return self.process_status

    def collect_disk_counters(
        self,
        counters: object,
        error_buffer: object,
        _error_buffer_len: int,
    ) -> int:
        if self.disk_status != native_backend.AURA_STATUS_OK:
            self._set_error(error_buffer, b"disk failed")
            return self.disk_status

        raw = ctypes.cast(counters, ctypes.POINTER(native_backend._AuraDiskCounters))
        raw[0].read_bytes = 111
        raw[0].write_bytes = 222
        raw[0].read_count = 10
        raw[0].write_count = 20
        return self.disk_status

    def collect_network_counters(
        self,
        counters: object,
        error_buffer: object,
        _error_buffer_len: int,
    ) -> int:
        if self.network_status != native_backend.AURA_STATUS_OK:
            self._set_error(error_buffer, b"network failed")
            return self.network_status

        raw = ctypes.cast(counters, ctypes.POINTER(native_backend._AuraNetworkCounters))
        raw[0].bytes_sent = 333
        raw[0].bytes_recv = 444
        raw[0].packets_sent = 30
        raw[0].packets_recv = 40
        return self.network_status

    def collect_thermal_readings(
        self,
        readings: object,
        max_samples: int,
        out_count: object,
        error_buffer: object,
        _error_buffer_len: int,
    ) -> int:
        if self.thermal_status != native_backend.AURA_STATUS_OK:
            self._set_error(error_buffer, b"thermal failed")
            return self.thermal_status

        if max_samples > 0:
            raw = ctypes.cast(readings, ctypes.POINTER(native_backend._AuraThermalReading))
            raw[0].label = b"CPU"
            raw[0].current_celsius = 70.5
            raw[0].high_celsius = 90.0
            raw[0].critical_celsius = 100.0
            raw[0].has_high = 1
            raw[0].has_critical = 1

        ctypes.cast(out_count, ctypes.POINTER(ctypes.c_uint32))[0] = 1
        return self.thermal_status


class NativeBackendTests(unittest.TestCase):
    def setUp(self) -> None:
        self._original_bindings = native_backend._bindings
        self._original_load_attempted = native_backend._load_attempted
        self._original_load_error = native_backend._load_error
        self._fake_bindings = _FakeBindings()
        native_backend._set_test_bindings(self._fake_bindings)

    def tearDown(self) -> None:
        with native_backend._bindings_lock:
            native_backend._bindings = self._original_bindings
            native_backend._load_attempted = self._original_load_attempted
            native_backend._load_error = self._original_load_error

    def test_collect_system_snapshot_success(self) -> None:
        snapshot = native_backend.collect_system_snapshot()
        assert snapshot is not None
        self.assertEqual(snapshot.cpu_percent, 17.5)
        self.assertEqual(snapshot.memory_percent, 61.25)

    def test_collect_system_snapshot_unavailable_returns_none(self) -> None:
        self._fake_bindings.system_status = native_backend.AURA_STATUS_UNAVAILABLE
        snapshot = native_backend.collect_system_snapshot()
        self.assertIsNone(snapshot)

    def test_collect_system_snapshot_error_raises(self) -> None:
        self._fake_bindings.system_status = native_backend.AURA_STATUS_ERROR
        with self.assertRaises(RuntimeError):
            native_backend.collect_system_snapshot()

    def test_collect_process_samples_success(self) -> None:
        samples = native_backend.collect_process_samples(limit=8)
        assert samples is not None
        self.assertEqual(len(samples), 2)
        self.assertEqual(samples[0].name, "alpha")
        self.assertEqual(samples[1].name, "pid-20")

    def test_collect_process_samples_requires_positive_limit(self) -> None:
        for invalid_limit in (0, -1, False):
            with self.subTest(invalid_limit=invalid_limit):
                with self.assertRaises(ValueError):
                    native_backend.collect_process_samples(limit=invalid_limit)

    def test_collect_disk_counters_success(self) -> None:
        counters = native_backend.collect_disk_counters()
        assert counters is not None
        self.assertEqual(counters.read_bytes, 111)
        self.assertEqual(counters.write_bytes, 222)

    def test_collect_network_counters_success(self) -> None:
        counters = native_backend.collect_network_counters()
        assert counters is not None
        self.assertEqual(counters.bytes_sent, 333)
        self.assertEqual(counters.bytes_recv, 444)

    def test_collect_thermal_readings_success(self) -> None:
        readings = native_backend.collect_thermal_readings(limit=8)
        assert readings is not None
        self.assertEqual(len(readings), 1)
        self.assertEqual(readings[0].label, "CPU")
        self.assertEqual(readings[0].high_celsius, 90.0)
        self.assertEqual(readings[0].critical_celsius, 100.0)

    def test_collect_thermal_readings_requires_positive_limit(self) -> None:
        for invalid_limit in (0, -1, False):
            with self.subTest(invalid_limit=invalid_limit):
                with self.assertRaises(ValueError):
                    native_backend.collect_thermal_readings(limit=invalid_limit)


if __name__ == "__main__":
    unittest.main()
