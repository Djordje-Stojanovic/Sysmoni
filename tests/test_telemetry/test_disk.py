from __future__ import annotations

import math
import pathlib
import sys
import threading
import unittest

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from telemetry import disk  # noqa: E402
from telemetry.disk import DiskSnapshot, collect_disk_snapshot  # noqa: E402


class _DiskIOStub:
    """Stub for psutil.disk_io_counters return value."""

    def __init__(
        self,
        read_bytes: int = 0,
        write_bytes: int = 0,
        read_count: int = 0,
        write_count: int = 0,
    ) -> None:
        self.read_bytes = read_bytes
        self.write_bytes = write_bytes
        self.read_count = read_count
        self.write_count = write_count


class _PsutilDiskStub:
    """Minimal psutil stub for disk collection."""

    def __init__(self, counters_sequence: list[_DiskIOStub | None]) -> None:
        self._counters_sequence = list(counters_sequence)
        self._call_index = 0
        self._lock = threading.Lock()

    def disk_io_counters(self, perdisk: bool = False) -> _DiskIOStub | None:
        with self._lock:
            idx = self._call_index
            self._call_index += 1
        if idx < len(self._counters_sequence):
            return self._counters_sequence[idx]
        return self._counters_sequence[-1]


class CollectDiskSnapshotTests(unittest.TestCase):
    def setUp(self) -> None:
        self._original_psutil = disk.psutil
        self._original_prev = disk._prev_disk_counters
        disk._reset_disk_state()

    def tearDown(self) -> None:
        disk.psutil = self._original_psutil
        disk._reset_disk_state()
        with disk._prev_disk_counters_lock:
            disk._prev_disk_counters = self._original_prev

    def test_first_call_returns_zero_rates_with_correct_totals(self) -> None:
        stub = _PsutilDiskStub([
            _DiskIOStub(read_bytes=1000, write_bytes=2000, read_count=10, write_count=20),
        ])
        disk.psutil = stub

        snap = collect_disk_snapshot(now=lambda: 100.0)

        self.assertEqual(snap.timestamp, 100.0)
        self.assertEqual(snap.read_bytes_per_sec, 0.0)
        self.assertEqual(snap.write_bytes_per_sec, 0.0)
        self.assertEqual(snap.read_ops_per_sec, 0.0)
        self.assertEqual(snap.write_ops_per_sec, 0.0)
        self.assertEqual(snap.total_read_bytes, 1000)
        self.assertEqual(snap.total_write_bytes, 2000)

    def test_second_call_computes_correct_rates(self) -> None:
        stub = _PsutilDiskStub([
            _DiskIOStub(read_bytes=1000, write_bytes=2000, read_count=10, write_count=20),
            _DiskIOStub(read_bytes=3000, write_bytes=4000, read_count=30, write_count=40),
        ])
        disk.psutil = stub
        times = iter([100.0, 102.0])

        collect_disk_snapshot(now=lambda: next(times))
        snap = collect_disk_snapshot(now=lambda: next(times))

        self.assertEqual(snap.timestamp, 102.0)
        self.assertAlmostEqual(snap.read_bytes_per_sec, 1000.0)
        self.assertAlmostEqual(snap.write_bytes_per_sec, 1000.0)
        self.assertAlmostEqual(snap.read_ops_per_sec, 10.0)
        self.assertAlmostEqual(snap.write_ops_per_sec, 10.0)
        self.assertEqual(snap.total_read_bytes, 3000)
        self.assertEqual(snap.total_write_bytes, 4000)

    def test_counter_reset_returns_zero_rates(self) -> None:
        stub = _PsutilDiskStub([
            _DiskIOStub(read_bytes=5000, write_bytes=6000, read_count=50, write_count=60),
            _DiskIOStub(read_bytes=100, write_bytes=200, read_count=1, write_count=2),
        ])
        disk.psutil = stub
        times = iter([100.0, 101.0])

        collect_disk_snapshot(now=lambda: next(times))
        snap = collect_disk_snapshot(now=lambda: next(times))

        self.assertEqual(snap.read_bytes_per_sec, 0.0)
        self.assertEqual(snap.write_bytes_per_sec, 0.0)
        self.assertEqual(snap.read_ops_per_sec, 0.0)
        self.assertEqual(snap.write_ops_per_sec, 0.0)

    def test_partial_counter_reset_returns_zero_rates(self) -> None:
        stub = _PsutilDiskStub([
            _DiskIOStub(read_bytes=5000, write_bytes=6000, read_count=50, write_count=60),
            _DiskIOStub(read_bytes=6000, write_bytes=7000, read_count=10, write_count=70),
        ])
        disk.psutil = stub
        times = iter([100.0, 101.0])

        collect_disk_snapshot(now=lambda: next(times))
        snap = collect_disk_snapshot(now=lambda: next(times))

        self.assertEqual(snap.read_bytes_per_sec, 0.0)
        self.assertEqual(snap.write_bytes_per_sec, 0.0)
        self.assertEqual(snap.read_ops_per_sec, 0.0)
        self.assertEqual(snap.write_ops_per_sec, 0.0)

    def test_zero_elapsed_returns_zero_rates(self) -> None:
        stub = _PsutilDiskStub([
            _DiskIOStub(read_bytes=1000, write_bytes=2000, read_count=10, write_count=20),
            _DiskIOStub(read_bytes=2000, write_bytes=3000, read_count=20, write_count=30),
        ])
        disk.psutil = stub

        collect_disk_snapshot(now=lambda: 100.0)
        snap = collect_disk_snapshot(now=lambda: 100.0)

        self.assertEqual(snap.read_bytes_per_sec, 0.0)
        self.assertEqual(snap.write_bytes_per_sec, 0.0)
        self.assertEqual(snap.read_ops_per_sec, 0.0)
        self.assertEqual(snap.write_ops_per_sec, 0.0)

    def test_raises_runtime_error_when_psutil_missing(self) -> None:
        disk.psutil = None
        with self.assertRaises(RuntimeError):
            collect_disk_snapshot(now=lambda: 1.0)

    def test_raises_runtime_error_when_disk_io_counters_returns_none(self) -> None:
        stub = _PsutilDiskStub([None])
        disk.psutil = stub
        with self.assertRaises(RuntimeError):
            collect_disk_snapshot(now=lambda: 1.0)

    def test_collect_disk_snapshot_prefers_native_backend_when_available(self) -> None:
        original_native_collect = disk.native_backend.collect_disk_counters
        original_psutil = disk.psutil
        disk.psutil = None
        native_values = iter(
            [
                disk.native_backend.NativeDiskCounters(
                    read_bytes=1000,
                    write_bytes=2000,
                    read_count=10,
                    write_count=20,
                ),
                disk.native_backend.NativeDiskCounters(
                    read_bytes=3000,
                    write_bytes=5000,
                    read_count=30,
                    write_count=50,
                ),
            ]
        )
        disk.native_backend.collect_disk_counters = lambda: next(native_values)
        times = iter([100.0, 102.0])
        try:
            first = collect_disk_snapshot(now=lambda: next(times))
            second = collect_disk_snapshot(now=lambda: next(times))
        finally:
            disk.native_backend.collect_disk_counters = original_native_collect
            disk.psutil = original_psutil

        self.assertEqual(first.read_bytes_per_sec, 0.0)
        self.assertEqual(second.read_bytes_per_sec, 1000.0)
        self.assertEqual(second.write_bytes_per_sec, 1500.0)
        self.assertEqual(second.read_ops_per_sec, 10.0)
        self.assertEqual(second.write_ops_per_sec, 15.0)

    def test_custom_now_callable_used_for_timestamp(self) -> None:
        stub = _PsutilDiskStub([
            _DiskIOStub(read_bytes=0, write_bytes=0, read_count=0, write_count=0),
        ])
        disk.psutil = stub

        snap = collect_disk_snapshot(now=lambda: 42.5)

        self.assertEqual(snap.timestamp, 42.5)

    def test_cumulative_totals_reflect_raw_counters(self) -> None:
        stub = _PsutilDiskStub([
            _DiskIOStub(read_bytes=100, write_bytes=200, read_count=1, write_count=2),
            _DiskIOStub(read_bytes=500, write_bytes=800, read_count=5, write_count=8),
        ])
        disk.psutil = stub
        times = iter([1.0, 2.0])

        collect_disk_snapshot(now=lambda: next(times))
        snap = collect_disk_snapshot(now=lambda: next(times))

        self.assertEqual(snap.total_read_bytes, 500)
        self.assertEqual(snap.total_write_bytes, 800)

    def test_thread_safety_no_exceptions(self) -> None:
        worker_count = 8
        stub = _PsutilDiskStub([
            _DiskIOStub(
                read_bytes=i * 1000,
                write_bytes=i * 2000,
                read_count=i * 10,
                write_count=i * 20,
            )
            for i in range(worker_count + 2)
        ])
        disk.psutil = stub

        errors: list[Exception] = []
        barrier = threading.Barrier(worker_count)
        call_count = iter(range(worker_count + 2))

        def _worker() -> None:
            try:
                barrier.wait()
                collect_disk_snapshot(now=lambda: float(next(call_count)))
            except Exception as exc:
                errors.append(exc)

        threads = [threading.Thread(target=_worker) for _ in range(worker_count)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        self.assertEqual(errors, [])


class DiskSnapshotValidationTests(unittest.TestCase):
    def test_normalizes_numeric_fields(self) -> None:
        snap = DiskSnapshot(
            timestamp=1,
            read_bytes_per_sec=10,
            write_bytes_per_sec=20,
            read_ops_per_sec=3,
            write_ops_per_sec=4,
            total_read_bytes=100,
            total_write_bytes=200,
        )
        self.assertIsInstance(snap.timestamp, float)
        self.assertIsInstance(snap.read_bytes_per_sec, float)
        self.assertIsInstance(snap.write_bytes_per_sec, float)
        self.assertIsInstance(snap.read_ops_per_sec, float)
        self.assertIsInstance(snap.write_ops_per_sec, float)
        self.assertIsInstance(snap.total_read_bytes, int)
        self.assertIsInstance(snap.total_write_bytes, int)

    def test_rejects_non_finite_timestamp(self) -> None:
        for bad in (math.nan, math.inf, -math.inf):
            with self.subTest(bad=bad):
                with self.assertRaises(ValueError):
                    DiskSnapshot(
                        timestamp=bad,
                        read_bytes_per_sec=0.0,
                        write_bytes_per_sec=0.0,
                        read_ops_per_sec=0.0,
                        write_ops_per_sec=0.0,
                        total_read_bytes=0,
                        total_write_bytes=0,
                    )

    def test_rejects_non_finite_rate_fields(self) -> None:
        for field in ("read_bytes_per_sec", "write_bytes_per_sec", "read_ops_per_sec", "write_ops_per_sec"):
            for bad in (math.nan, math.inf):
                with self.subTest(field=field, bad=bad):
                    kwargs = dict(
                        timestamp=1.0,
                        read_bytes_per_sec=0.0,
                        write_bytes_per_sec=0.0,
                        read_ops_per_sec=0.0,
                        write_ops_per_sec=0.0,
                        total_read_bytes=0,
                        total_write_bytes=0,
                    )
                    kwargs[field] = bad
                    with self.assertRaises(ValueError):
                        DiskSnapshot(**kwargs)

    def test_rejects_negative_rate_fields(self) -> None:
        for field in ("read_bytes_per_sec", "write_bytes_per_sec", "read_ops_per_sec", "write_ops_per_sec"):
            with self.subTest(field=field):
                kwargs = dict(
                    timestamp=1.0,
                    read_bytes_per_sec=0.0,
                    write_bytes_per_sec=0.0,
                    read_ops_per_sec=0.0,
                    write_ops_per_sec=0.0,
                    total_read_bytes=0,
                    total_write_bytes=0,
                )
                kwargs[field] = -1.0
                with self.assertRaises(ValueError):
                    DiskSnapshot(**kwargs)

    def test_rejects_negative_total_read_bytes(self) -> None:
        with self.assertRaises(ValueError):
            DiskSnapshot(
                timestamp=1.0,
                read_bytes_per_sec=0.0,
                write_bytes_per_sec=0.0,
                read_ops_per_sec=0.0,
                write_ops_per_sec=0.0,
                total_read_bytes=-1,
                total_write_bytes=0,
            )

    def test_rejects_negative_total_write_bytes(self) -> None:
        with self.assertRaises(ValueError):
            DiskSnapshot(
                timestamp=1.0,
                read_bytes_per_sec=0.0,
                write_bytes_per_sec=0.0,
                read_ops_per_sec=0.0,
                write_ops_per_sec=0.0,
                total_read_bytes=0,
                total_write_bytes=-1,
            )

    def test_rejects_boolean_total_read_bytes(self) -> None:
        with self.assertRaises(ValueError):
            DiskSnapshot(
                timestamp=1.0,
                read_bytes_per_sec=0.0,
                write_bytes_per_sec=0.0,
                read_ops_per_sec=0.0,
                write_ops_per_sec=0.0,
                total_read_bytes=True,
                total_write_bytes=0,
            )

    def test_rejects_boolean_total_write_bytes(self) -> None:
        with self.assertRaises(ValueError):
            DiskSnapshot(
                timestamp=1.0,
                read_bytes_per_sec=0.0,
                write_bytes_per_sec=0.0,
                read_ops_per_sec=0.0,
                write_ops_per_sec=0.0,
                total_read_bytes=0,
                total_write_bytes=True,
            )

    def test_accepts_all_zeros(self) -> None:
        snap = DiskSnapshot(
            timestamp=0.0,
            read_bytes_per_sec=0.0,
            write_bytes_per_sec=0.0,
            read_ops_per_sec=0.0,
            write_ops_per_sec=0.0,
            total_read_bytes=0,
            total_write_bytes=0,
        )
        self.assertEqual(snap.timestamp, 0.0)
        self.assertEqual(snap.read_bytes_per_sec, 0.0)
        self.assertEqual(snap.total_read_bytes, 0)

    def test_to_dict_returns_correct_structure(self) -> None:
        snap = DiskSnapshot(
            timestamp=1.0,
            read_bytes_per_sec=100.5,
            write_bytes_per_sec=200.5,
            read_ops_per_sec=10.0,
            write_ops_per_sec=20.0,
            total_read_bytes=5000,
            total_write_bytes=10000,
        )
        d = snap.to_dict()
        self.assertEqual(d, {
            "timestamp": 1.0,
            "read_bytes_per_sec": 100.5,
            "write_bytes_per_sec": 200.5,
            "read_ops_per_sec": 10.0,
            "write_ops_per_sec": 20.0,
            "total_read_bytes": 5000,
            "total_write_bytes": 10000,
        })


if __name__ == "__main__":
    unittest.main()
