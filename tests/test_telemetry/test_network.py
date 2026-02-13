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

from telemetry import network  # noqa: E402
from telemetry.network import NetworkSnapshot, collect_network_snapshot  # noqa: E402


class _NetIOStub:
    """Stub for psutil.net_io_counters return value."""

    def __init__(
        self,
        bytes_sent: int = 0,
        bytes_recv: int = 0,
        packets_sent: int = 0,
        packets_recv: int = 0,
    ) -> None:
        self.bytes_sent = bytes_sent
        self.bytes_recv = bytes_recv
        self.packets_sent = packets_sent
        self.packets_recv = packets_recv


class _PsutilNetStub:
    """Minimal psutil stub for network collection."""

    def __init__(self, counters_sequence: list[_NetIOStub]) -> None:
        self._counters_sequence = list(counters_sequence)
        self._call_index = 0
        self._lock = threading.Lock()

    def net_io_counters(self, pernic: bool = False) -> _NetIOStub:
        with self._lock:
            idx = self._call_index
            self._call_index += 1
        if idx < len(self._counters_sequence):
            return self._counters_sequence[idx]
        return self._counters_sequence[-1]


class CollectNetworkSnapshotTests(unittest.TestCase):
    def setUp(self) -> None:
        self._original_psutil = network.psutil
        self._original_prev = network._prev_counters
        network._reset_network_state()

    def tearDown(self) -> None:
        network.psutil = self._original_psutil
        network._reset_network_state()
        with network._prev_counters_lock:
            network._prev_counters = self._original_prev

    def test_first_call_returns_zero_rates_with_correct_totals(self) -> None:
        stub = _PsutilNetStub([
            _NetIOStub(bytes_sent=1000, bytes_recv=2000, packets_sent=10, packets_recv=20),
        ])
        network.psutil = stub

        snap = collect_network_snapshot(now=lambda: 100.0)

        self.assertEqual(snap.timestamp, 100.0)
        self.assertEqual(snap.bytes_sent_per_sec, 0.0)
        self.assertEqual(snap.bytes_recv_per_sec, 0.0)
        self.assertEqual(snap.packets_sent_per_sec, 0.0)
        self.assertEqual(snap.packets_recv_per_sec, 0.0)
        self.assertEqual(snap.total_bytes_sent, 1000)
        self.assertEqual(snap.total_bytes_recv, 2000)

    def test_second_call_computes_correct_rates(self) -> None:
        stub = _PsutilNetStub([
            _NetIOStub(bytes_sent=1000, bytes_recv=2000, packets_sent=10, packets_recv=20),
            _NetIOStub(bytes_sent=3000, bytes_recv=4000, packets_sent=30, packets_recv=40),
        ])
        network.psutil = stub
        times = iter([100.0, 102.0])

        collect_network_snapshot(now=lambda: next(times))
        snap = collect_network_snapshot(now=lambda: next(times))

        self.assertEqual(snap.timestamp, 102.0)
        self.assertAlmostEqual(snap.bytes_sent_per_sec, 1000.0)
        self.assertAlmostEqual(snap.bytes_recv_per_sec, 1000.0)
        self.assertAlmostEqual(snap.packets_sent_per_sec, 10.0)
        self.assertAlmostEqual(snap.packets_recv_per_sec, 10.0)
        self.assertEqual(snap.total_bytes_sent, 3000)
        self.assertEqual(snap.total_bytes_recv, 4000)

    def test_counter_reset_returns_zero_rates(self) -> None:
        stub = _PsutilNetStub([
            _NetIOStub(bytes_sent=5000, bytes_recv=6000, packets_sent=50, packets_recv=60),
            _NetIOStub(bytes_sent=100, bytes_recv=200, packets_sent=1, packets_recv=2),
        ])
        network.psutil = stub
        times = iter([100.0, 101.0])

        collect_network_snapshot(now=lambda: next(times))
        snap = collect_network_snapshot(now=lambda: next(times))

        self.assertEqual(snap.bytes_sent_per_sec, 0.0)
        self.assertEqual(snap.bytes_recv_per_sec, 0.0)
        self.assertEqual(snap.packets_sent_per_sec, 0.0)
        self.assertEqual(snap.packets_recv_per_sec, 0.0)

    def test_zero_elapsed_returns_zero_rates(self) -> None:
        stub = _PsutilNetStub([
            _NetIOStub(bytes_sent=1000, bytes_recv=2000, packets_sent=10, packets_recv=20),
            _NetIOStub(bytes_sent=2000, bytes_recv=3000, packets_sent=20, packets_recv=30),
        ])
        network.psutil = stub

        collect_network_snapshot(now=lambda: 100.0)
        snap = collect_network_snapshot(now=lambda: 100.0)

        self.assertEqual(snap.bytes_sent_per_sec, 0.0)
        self.assertEqual(snap.bytes_recv_per_sec, 0.0)
        self.assertEqual(snap.packets_sent_per_sec, 0.0)
        self.assertEqual(snap.packets_recv_per_sec, 0.0)

    def test_raises_runtime_error_when_psutil_missing(self) -> None:
        network.psutil = None
        with self.assertRaises(RuntimeError):
            collect_network_snapshot(now=lambda: 1.0)

    def test_custom_now_callable_used_for_timestamp(self) -> None:
        stub = _PsutilNetStub([
            _NetIOStub(bytes_sent=0, bytes_recv=0, packets_sent=0, packets_recv=0),
        ])
        network.psutil = stub

        snap = collect_network_snapshot(now=lambda: 42.5)

        self.assertEqual(snap.timestamp, 42.5)

    def test_thread_safety_no_exceptions(self) -> None:
        worker_count = 8
        stub = _PsutilNetStub([
            _NetIOStub(bytes_sent=i * 1000, bytes_recv=i * 2000, packets_sent=i * 10, packets_recv=i * 20)
            for i in range(worker_count + 2)
        ])
        network.psutil = stub

        errors: list[Exception] = []
        barrier = threading.Barrier(worker_count)
        call_count = iter(range(worker_count + 2))

        def _worker() -> None:
            try:
                barrier.wait()
                collect_network_snapshot(now=lambda: float(next(call_count)))
            except Exception as exc:
                errors.append(exc)

        threads = [threading.Thread(target=_worker) for _ in range(worker_count)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        self.assertEqual(errors, [])

    def test_cumulative_totals_reflect_raw_counters(self) -> None:
        stub = _PsutilNetStub([
            _NetIOStub(bytes_sent=100, bytes_recv=200, packets_sent=1, packets_recv=2),
            _NetIOStub(bytes_sent=500, bytes_recv=800, packets_sent=5, packets_recv=8),
        ])
        network.psutil = stub
        times = iter([1.0, 2.0])

        collect_network_snapshot(now=lambda: next(times))
        snap = collect_network_snapshot(now=lambda: next(times))

        self.assertEqual(snap.total_bytes_sent, 500)
        self.assertEqual(snap.total_bytes_recv, 800)


class NetworkSnapshotValidationTests(unittest.TestCase):
    def test_normalizes_numeric_fields(self) -> None:
        snap = NetworkSnapshot(
            timestamp=1,
            bytes_sent_per_sec=10,
            bytes_recv_per_sec=20,
            packets_sent_per_sec=3,
            packets_recv_per_sec=4,
            total_bytes_sent=100,
            total_bytes_recv=200,
        )
        self.assertIsInstance(snap.timestamp, float)
        self.assertIsInstance(snap.bytes_sent_per_sec, float)
        self.assertIsInstance(snap.bytes_recv_per_sec, float)
        self.assertIsInstance(snap.packets_sent_per_sec, float)
        self.assertIsInstance(snap.packets_recv_per_sec, float)
        self.assertIsInstance(snap.total_bytes_sent, int)
        self.assertIsInstance(snap.total_bytes_recv, int)

    def test_rejects_non_finite_timestamp(self) -> None:
        for bad in (math.nan, math.inf, -math.inf):
            with self.subTest(bad=bad):
                with self.assertRaises(ValueError):
                    NetworkSnapshot(
                        timestamp=bad,
                        bytes_sent_per_sec=0.0,
                        bytes_recv_per_sec=0.0,
                        packets_sent_per_sec=0.0,
                        packets_recv_per_sec=0.0,
                        total_bytes_sent=0,
                        total_bytes_recv=0,
                    )

    def test_rejects_non_finite_rate_fields(self) -> None:
        for field in ("bytes_sent_per_sec", "bytes_recv_per_sec", "packets_sent_per_sec", "packets_recv_per_sec"):
            for bad in (math.nan, math.inf):
                with self.subTest(field=field, bad=bad):
                    kwargs = dict(
                        timestamp=1.0,
                        bytes_sent_per_sec=0.0,
                        bytes_recv_per_sec=0.0,
                        packets_sent_per_sec=0.0,
                        packets_recv_per_sec=0.0,
                        total_bytes_sent=0,
                        total_bytes_recv=0,
                    )
                    kwargs[field] = bad
                    with self.assertRaises(ValueError):
                        NetworkSnapshot(**kwargs)

    def test_rejects_negative_rate_fields(self) -> None:
        for field in ("bytes_sent_per_sec", "bytes_recv_per_sec", "packets_sent_per_sec", "packets_recv_per_sec"):
            with self.subTest(field=field):
                kwargs = dict(
                    timestamp=1.0,
                    bytes_sent_per_sec=0.0,
                    bytes_recv_per_sec=0.0,
                    packets_sent_per_sec=0.0,
                    packets_recv_per_sec=0.0,
                    total_bytes_sent=0,
                    total_bytes_recv=0,
                )
                kwargs[field] = -1.0
                with self.assertRaises(ValueError):
                    NetworkSnapshot(**kwargs)

    def test_rejects_negative_totals(self) -> None:
        for field in ("total_bytes_sent", "total_bytes_recv"):
            with self.subTest(field=field):
                kwargs = dict(
                    timestamp=1.0,
                    bytes_sent_per_sec=0.0,
                    bytes_recv_per_sec=0.0,
                    packets_sent_per_sec=0.0,
                    packets_recv_per_sec=0.0,
                    total_bytes_sent=0,
                    total_bytes_recv=0,
                )
                kwargs[field] = -1
                with self.assertRaises(ValueError):
                    NetworkSnapshot(**kwargs)

    def test_rejects_boolean_totals(self) -> None:
        for field in ("total_bytes_sent", "total_bytes_recv"):
            with self.subTest(field=field):
                kwargs = dict(
                    timestamp=1.0,
                    bytes_sent_per_sec=0.0,
                    bytes_recv_per_sec=0.0,
                    packets_sent_per_sec=0.0,
                    packets_recv_per_sec=0.0,
                    total_bytes_sent=0,
                    total_bytes_recv=0,
                )
                kwargs[field] = True
                with self.assertRaises(ValueError):
                    NetworkSnapshot(**kwargs)

    def test_to_dict_returns_correct_structure(self) -> None:
        snap = NetworkSnapshot(
            timestamp=1.0,
            bytes_sent_per_sec=100.5,
            bytes_recv_per_sec=200.5,
            packets_sent_per_sec=10.0,
            packets_recv_per_sec=20.0,
            total_bytes_sent=5000,
            total_bytes_recv=10000,
        )
        d = snap.to_dict()
        self.assertEqual(d, {
            "timestamp": 1.0,
            "bytes_sent_per_sec": 100.5,
            "bytes_recv_per_sec": 200.5,
            "packets_sent_per_sec": 10.0,
            "packets_recv_per_sec": 20.0,
            "total_bytes_sent": 5000,
            "total_bytes_recv": 10000,
        })


if __name__ == "__main__":
    unittest.main()
