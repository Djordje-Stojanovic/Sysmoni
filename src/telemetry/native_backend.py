from __future__ import annotations

import ctypes
import math
import os
import platform
import threading
from dataclasses import dataclass
from pathlib import Path
from typing import Protocol


AURA_STATUS_OK = 0
AURA_STATUS_UNAVAILABLE = 1
AURA_STATUS_ERROR = 2

_DEFAULT_MAX_NATIVE_SAMPLES = 256
_ERROR_BUFFER_BYTES = 512


class _BindingsProtocol(Protocol):
    def collect_system_snapshot(
        self,
        cpu_percent: object,
        memory_percent: object,
        error_buffer: object,
        error_buffer_len: int,
    ) -> int: ...

    def collect_processes(
        self,
        samples: object,
        max_samples: int,
        out_count: object,
        error_buffer: object,
        error_buffer_len: int,
    ) -> int: ...

    def collect_disk_counters(
        self,
        counters: object,
        error_buffer: object,
        error_buffer_len: int,
    ) -> int: ...

    def collect_network_counters(
        self,
        counters: object,
        error_buffer: object,
        error_buffer_len: int,
    ) -> int: ...

    def collect_thermal_readings(
        self,
        samples: object,
        max_samples: int,
        out_count: object,
        error_buffer: object,
        error_buffer_len: int,
    ) -> int: ...


class _AuraProcessSample(ctypes.Structure):
    _fields_ = [
        ("pid", ctypes.c_uint32),
        ("name", ctypes.c_char * 260),
        ("cpu_percent", ctypes.c_double),
        ("memory_rss_bytes", ctypes.c_uint64),
    ]


class _AuraDiskCounters(ctypes.Structure):
    _fields_ = [
        ("read_bytes", ctypes.c_uint64),
        ("write_bytes", ctypes.c_uint64),
        ("read_count", ctypes.c_uint64),
        ("write_count", ctypes.c_uint64),
    ]


class _AuraNetworkCounters(ctypes.Structure):
    _fields_ = [
        ("bytes_sent", ctypes.c_uint64),
        ("bytes_recv", ctypes.c_uint64),
        ("packets_sent", ctypes.c_uint64),
        ("packets_recv", ctypes.c_uint64),
    ]


class _AuraThermalReading(ctypes.Structure):
    _fields_ = [
        ("label", ctypes.c_char * 128),
        ("current_celsius", ctypes.c_double),
        ("high_celsius", ctypes.c_double),
        ("critical_celsius", ctypes.c_double),
        ("has_high", ctypes.c_uint8),
        ("has_critical", ctypes.c_uint8),
    ]


@dataclass(slots=True, frozen=True)
class NativeSystemSnapshot:
    cpu_percent: float
    memory_percent: float


@dataclass(slots=True, frozen=True)
class NativeProcessSample:
    pid: int
    name: str
    cpu_percent: float
    memory_rss_bytes: int


@dataclass(slots=True, frozen=True)
class NativeDiskCounters:
    read_bytes: int
    write_bytes: int
    read_count: int
    write_count: int


@dataclass(slots=True, frozen=True)
class NativeNetworkCounters:
    bytes_sent: int
    bytes_recv: int
    packets_sent: int
    packets_recv: int


@dataclass(slots=True, frozen=True)
class NativeThermalReading:
    label: str
    current_celsius: float
    high_celsius: float | None
    critical_celsius: float | None


class _NativeBindings:
    def __init__(self, library: ctypes.WinDLL) -> None:
        self._library = library

        self.collect_system_snapshot = library.aura_collect_system_snapshot
        self.collect_system_snapshot.argtypes = [
            ctypes.POINTER(ctypes.c_double),
            ctypes.POINTER(ctypes.c_double),
            ctypes.c_void_p,
            ctypes.c_size_t,
        ]
        self.collect_system_snapshot.restype = ctypes.c_int

        self.collect_processes = library.aura_collect_processes
        self.collect_processes.argtypes = [
            ctypes.POINTER(_AuraProcessSample),
            ctypes.c_uint32,
            ctypes.POINTER(ctypes.c_uint32),
            ctypes.c_void_p,
            ctypes.c_size_t,
        ]
        self.collect_processes.restype = ctypes.c_int

        self.collect_disk_counters = library.aura_collect_disk_counters
        self.collect_disk_counters.argtypes = [
            ctypes.POINTER(_AuraDiskCounters),
            ctypes.c_void_p,
            ctypes.c_size_t,
        ]
        self.collect_disk_counters.restype = ctypes.c_int

        self.collect_network_counters = library.aura_collect_network_counters
        self.collect_network_counters.argtypes = [
            ctypes.POINTER(_AuraNetworkCounters),
            ctypes.c_void_p,
            ctypes.c_size_t,
        ]
        self.collect_network_counters.restype = ctypes.c_int

        self.collect_thermal_readings = library.aura_collect_thermal_readings
        self.collect_thermal_readings.argtypes = [
            ctypes.POINTER(_AuraThermalReading),
            ctypes.c_uint32,
            ctypes.POINTER(ctypes.c_uint32),
            ctypes.c_void_p,
            ctypes.c_size_t,
        ]
        self.collect_thermal_readings.restype = ctypes.c_int


_bindings_lock = threading.Lock()
_bindings: _BindingsProtocol | None = None
_load_attempted = False
_load_error: str | None = None


def _native_library_path() -> Path:
    configured_path = os.getenv("AURA_TELEMETRY_NATIVE_LIB")
    if configured_path:
        return Path(configured_path).expanduser()
    return Path(__file__).resolve().parent / "native" / "bin" / "aura_telemetry_native.dll"


def _decode_error(error_buffer: ctypes.Array[ctypes.c_char]) -> str:
    raw = bytes(error_buffer).split(b"\x00", 1)[0].strip()
    if not raw:
        return ""
    return raw.decode("utf-8", errors="replace")


def _format_error(operation: str, status: int, error_buffer: ctypes.Array[ctypes.c_char]) -> RuntimeError:
    native_error = _decode_error(error_buffer)
    message = native_error or "native backend returned an unknown error"
    return RuntimeError(f"{operation} failed with status={status}: {message}")


def _load_bindings() -> _BindingsProtocol | None:
    global _bindings
    global _load_attempted
    global _load_error

    with _bindings_lock:
        if _load_attempted:
            return _bindings

        _load_attempted = True

        if platform.system() != "Windows":
            _load_error = "native telemetry backend is only available on Windows."
            return None

        native_library_path = _native_library_path()
        if not native_library_path.exists():
            _load_error = f"native telemetry library not found: {native_library_path}"
            return None

        try:
            library = ctypes.WinDLL(str(native_library_path))
        except OSError as exc:
            _load_error = f"failed to load native telemetry library: {exc}"
            return None

        _bindings = _NativeBindings(library)
        _load_error = None
        return _bindings


def _get_bindings() -> _BindingsProtocol | None:
    return _load_bindings()


def _require_positive_limit(limit: int) -> int:
    if isinstance(limit, bool):
        raise ValueError("limit must be an integer greater than 0.")

    normalized_limit = int(limit)
    if normalized_limit <= 0:
        raise ValueError("limit must be an integer greater than 0.")
    return normalized_limit


def _decode_label(raw_bytes: bytes, fallback: str) -> str:
    decoded = raw_bytes.split(b"\x00", 1)[0].decode("utf-8", errors="replace").strip()
    return decoded or fallback


def _normalize_finite(value: float) -> float:
    normalized = float(value)
    if not math.isfinite(normalized):
        raise ValueError("native telemetry returned a non-finite value.")
    return normalized


def native_backend_available() -> bool:
    return _get_bindings() is not None


def native_backend_load_error() -> str | None:
    _get_bindings()
    return _load_error


def collect_system_snapshot() -> NativeSystemSnapshot | None:
    bindings = _get_bindings()
    if bindings is None:
        return None

    cpu_percent = ctypes.c_double()
    memory_percent = ctypes.c_double()
    error_buffer = ctypes.create_string_buffer(_ERROR_BUFFER_BYTES)
    status = int(
        bindings.collect_system_snapshot(
            ctypes.byref(cpu_percent),
            ctypes.byref(memory_percent),
            error_buffer,
            len(error_buffer),
        )
    )
    if status == AURA_STATUS_OK:
        return NativeSystemSnapshot(
            cpu_percent=_normalize_finite(cpu_percent.value),
            memory_percent=_normalize_finite(memory_percent.value),
        )
    if status == AURA_STATUS_UNAVAILABLE:
        return None
    raise _format_error("collect_system_snapshot", status, error_buffer)


def collect_process_samples(*, limit: int = 20) -> list[NativeProcessSample] | None:
    normalized_limit = _require_positive_limit(limit)
    bindings = _get_bindings()
    if bindings is None:
        return None

    max_samples = min(normalized_limit, _DEFAULT_MAX_NATIVE_SAMPLES)
    native_samples = (_AuraProcessSample * max_samples)()
    out_count = ctypes.c_uint32()
    error_buffer = ctypes.create_string_buffer(_ERROR_BUFFER_BYTES)
    status = int(
        bindings.collect_processes(
            native_samples,
            max_samples,
            ctypes.byref(out_count),
            error_buffer,
            len(error_buffer),
        )
    )
    if status == AURA_STATUS_UNAVAILABLE:
        return None
    if status != AURA_STATUS_OK:
        raise _format_error("collect_process_samples", status, error_buffer)

    count = min(int(out_count.value), max_samples)
    samples: list[NativeProcessSample] = []
    for idx in range(count):
        native_sample = native_samples[idx]
        samples.append(
            NativeProcessSample(
                pid=int(native_sample.pid),
                name=_decode_label(bytes(native_sample.name), f"pid-{int(native_sample.pid)}"),
                cpu_percent=_normalize_finite(native_sample.cpu_percent),
                memory_rss_bytes=int(native_sample.memory_rss_bytes),
            )
        )
    return samples


def collect_disk_counters() -> NativeDiskCounters | None:
    bindings = _get_bindings()
    if bindings is None:
        return None

    counters = _AuraDiskCounters()
    error_buffer = ctypes.create_string_buffer(_ERROR_BUFFER_BYTES)
    status = int(
        bindings.collect_disk_counters(
            ctypes.byref(counters),
            error_buffer,
            len(error_buffer),
        )
    )
    if status == AURA_STATUS_UNAVAILABLE:
        return None
    if status != AURA_STATUS_OK:
        raise _format_error("collect_disk_counters", status, error_buffer)

    return NativeDiskCounters(
        read_bytes=int(counters.read_bytes),
        write_bytes=int(counters.write_bytes),
        read_count=int(counters.read_count),
        write_count=int(counters.write_count),
    )


def collect_network_counters() -> NativeNetworkCounters | None:
    bindings = _get_bindings()
    if bindings is None:
        return None

    counters = _AuraNetworkCounters()
    error_buffer = ctypes.create_string_buffer(_ERROR_BUFFER_BYTES)
    status = int(
        bindings.collect_network_counters(
            ctypes.byref(counters),
            error_buffer,
            len(error_buffer),
        )
    )
    if status == AURA_STATUS_UNAVAILABLE:
        return None
    if status != AURA_STATUS_OK:
        raise _format_error("collect_network_counters", status, error_buffer)

    return NativeNetworkCounters(
        bytes_sent=int(counters.bytes_sent),
        bytes_recv=int(counters.bytes_recv),
        packets_sent=int(counters.packets_sent),
        packets_recv=int(counters.packets_recv),
    )


def collect_thermal_readings(
    *, limit: int = _DEFAULT_MAX_NATIVE_SAMPLES
) -> list[NativeThermalReading] | None:
    normalized_limit = _require_positive_limit(limit)
    bindings = _get_bindings()
    if bindings is None:
        return None

    max_samples = min(normalized_limit, _DEFAULT_MAX_NATIVE_SAMPLES)
    native_samples = (_AuraThermalReading * max_samples)()
    out_count = ctypes.c_uint32()
    error_buffer = ctypes.create_string_buffer(_ERROR_BUFFER_BYTES)
    status = int(
        bindings.collect_thermal_readings(
            native_samples,
            max_samples,
            ctypes.byref(out_count),
            error_buffer,
            len(error_buffer),
        )
    )
    if status == AURA_STATUS_UNAVAILABLE:
        return None
    if status != AURA_STATUS_OK:
        raise _format_error("collect_thermal_readings", status, error_buffer)

    count = min(int(out_count.value), max_samples)
    readings: list[NativeThermalReading] = []
    for idx in range(count):
        native_reading = native_samples[idx]
        readings.append(
            NativeThermalReading(
                label=_decode_label(bytes(native_reading.label), f"sensor-{idx}"),
                current_celsius=_normalize_finite(native_reading.current_celsius),
                high_celsius=(
                    _normalize_finite(native_reading.high_celsius)
                    if int(native_reading.has_high)
                    else None
                ),
                critical_celsius=(
                    _normalize_finite(native_reading.critical_celsius)
                    if int(native_reading.has_critical)
                    else None
                ),
            )
        )
    return readings


def _set_test_bindings(bindings: _BindingsProtocol | None) -> None:
    global _bindings
    global _load_attempted
    global _load_error
    with _bindings_lock:
        _bindings = bindings
        _load_attempted = bindings is not None
        _load_error = None
