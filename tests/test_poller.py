from __future__ import annotations

import math
import pathlib
import sys
import threading
import time
import unittest
from typing import Callable


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from core import poller  # noqa: E402
from core.types import SystemSnapshot  # noqa: E402


class _PsutilStub:
    def __init__(
        self,
        cpu_percent: float = 12.5,
        memory_percent: float = 41.0,
        cpu_delay_seconds: float = 0.0,
    ) -> None:
        self._cpu_percent = cpu_percent
        self._memory_percent = memory_percent
        self._cpu_delay_seconds = cpu_delay_seconds
        self._cpu_intervals_lock = threading.Lock()
        self.cpu_intervals: list[float | None] = []

    def cpu_percent(self, interval: float | None = None) -> float:
        if self._cpu_delay_seconds:
            time.sleep(self._cpu_delay_seconds)
        with self._cpu_intervals_lock:
            self.cpu_intervals.append(interval)
        return self._cpu_percent

    def virtual_memory(self) -> object:
        class _Memory:
            percent = self._memory_percent

        return _Memory()


class CollectSnapshotTests(unittest.TestCase):
    def test_collect_snapshot_uses_nonzero_interval_by_default(self) -> None:
        stub = _PsutilStub(cpu_percent=33.3, memory_percent=66.6)
        original = poller.psutil
        original_primed = poller._cpu_percent_primed
        poller.psutil = stub
        poller._cpu_percent_primed = False
        try:
            snapshot = poller.collect_snapshot(now=lambda: 123.0)
        finally:
            poller.psutil = original
            poller._cpu_percent_primed = original_primed

        self.assertEqual(snapshot.timestamp, 123.0)
        self.assertEqual(snapshot.cpu_percent, 33.3)
        self.assertEqual(snapshot.memory_percent, 66.6)
        self.assertEqual(stub.cpu_intervals, [0.1])

    def test_collect_snapshot_uses_nonblocking_interval_after_first_sample(self) -> None:
        stub = _PsutilStub(cpu_percent=7.5, memory_percent=21.0)
        original = poller.psutil
        original_primed = poller._cpu_percent_primed
        poller.psutil = stub
        poller._cpu_percent_primed = False
        try:
            poller.collect_snapshot(now=lambda: 1.0)
            snapshot = poller.collect_snapshot(now=lambda: 2.0)
        finally:
            poller.psutil = original
            poller._cpu_percent_primed = original_primed

        self.assertEqual(snapshot.timestamp, 2.0)
        self.assertEqual(snapshot.cpu_percent, 7.5)
        self.assertEqual(snapshot.memory_percent, 21.0)
        self.assertEqual(stub.cpu_intervals, [0.1, None])

    def test_collect_snapshot_raises_when_psutil_missing(self) -> None:
        original = poller.psutil
        original_primed = poller._cpu_percent_primed
        poller.psutil = None
        poller._cpu_percent_primed = False
        try:
            with self.assertRaises(RuntimeError):
                poller.collect_snapshot(now=lambda: 1.0)
        finally:
            poller.psutil = original
            poller._cpu_percent_primed = original_primed

    def test_collect_snapshot_primes_cpu_once_under_concurrency(self) -> None:
        worker_count = 8
        stub = _PsutilStub(cpu_percent=5.0, memory_percent=10.0, cpu_delay_seconds=0.02)
        original = poller.psutil
        original_primed = poller._cpu_percent_primed
        poller.psutil = stub
        poller._cpu_percent_primed = False

        errors: list[Exception] = []
        barrier = threading.Barrier(worker_count)

        def _worker() -> None:
            try:
                barrier.wait()
                poller.collect_snapshot(now=lambda: 1.0)
            except Exception as exc:  # pragma: no cover - error path assertion below
                errors.append(exc)

        threads = [threading.Thread(target=_worker) for _ in range(worker_count)]
        try:
            for thread in threads:
                thread.start()
            for thread in threads:
                thread.join()
        finally:
            poller.psutil = original
            poller._cpu_percent_primed = original_primed

        self.assertEqual(errors, [])
        self.assertEqual(stub.cpu_intervals.count(0.1), 1)
        self.assertEqual(stub.cpu_intervals.count(None), worker_count - 1)


class RunPollingLoopTests(unittest.TestCase):
    def test_run_polling_loop_requires_positive_interval(self) -> None:
        with self.assertRaises(ValueError):
            poller.run_polling_loop(
                0,
                lambda _snapshot: None,
                stop_event=threading.Event(),
            )

    def test_run_polling_loop_requires_finite_interval(self) -> None:
        for invalid_interval in (math.nan, math.inf, -math.inf):
            with self.subTest(invalid_interval=invalid_interval):
                with self.assertRaises(ValueError):
                    poller.run_polling_loop(
                        invalid_interval,
                        lambda _snapshot: None,
                        stop_event=threading.Event(),
                    )

    def test_run_polling_loop_stops_when_stop_event_is_set(self) -> None:
        produced = [
            SystemSnapshot(timestamp=1.0, cpu_percent=10.0, memory_percent=20.0),
            SystemSnapshot(timestamp=2.0, cpu_percent=11.0, memory_percent=21.0),
        ]
        observed: list[SystemSnapshot] = []
        stop_event = threading.Event()
        monotonic_values = iter([10.0, 11.1, 11.1, 12.2])

        def _monotonic() -> float:
            return next(monotonic_values)

        def _collect() -> SystemSnapshot:
            return produced[len(observed)]

        def _on_snapshot(snapshot: SystemSnapshot) -> None:
            observed.append(snapshot)
            if len(observed) >= len(produced):
                stop_event.set()

        emitted = poller.run_polling_loop(
            1.0,
            _on_snapshot,
            stop_event=stop_event,
            collect=_collect,
            monotonic=_monotonic,
        )

        self.assertEqual(emitted, len(produced))
        self.assertEqual(observed, produced)


class PollSnapshotsTests(unittest.TestCase):
    def test_poll_snapshots_requires_positive_interval(self) -> None:
        with self.assertRaises(ValueError):
            poller.poll_snapshots(lambda _snapshot: None, interval_seconds=0)

    def test_poll_snapshots_requires_finite_interval(self) -> None:
        for invalid_interval in (math.nan, math.inf, -math.inf):
            with self.subTest(invalid_interval=invalid_interval):
                with self.assertRaises(ValueError):
                    poller.poll_snapshots(
                        lambda _snapshot: None,
                        interval_seconds=invalid_interval,
                    )

    def test_poll_snapshots_stops_without_sampling_when_requested(self) -> None:
        original_collect_snapshot = poller.collect_snapshot

        def _unexpected_collect_snapshot(now: Callable[[], float] | None = None) -> SystemSnapshot:
            raise AssertionError("collect_snapshot should not be called when stop is requested.")

        poller.collect_snapshot = _unexpected_collect_snapshot
        observed: list[SystemSnapshot] = []
        sleep_calls: list[float] = []
        try:
            emitted = poller.poll_snapshots(
                observed.append,
                should_stop=lambda: True,
                sleep=sleep_calls.append,
            )
        finally:
            poller.collect_snapshot = original_collect_snapshot

        self.assertEqual(emitted, 0)
        self.assertEqual(observed, [])
        self.assertEqual(sleep_calls, [])

    def test_poll_snapshots_uses_remaining_time_for_sleep(self) -> None:
        produced = [
            SystemSnapshot(timestamp=1.0, cpu_percent=10.0, memory_percent=20.0),
            SystemSnapshot(timestamp=2.0, cpu_percent=11.0, memory_percent=21.0),
        ]
        monotonic_values = iter([10.0, 10.2, 11.0, 11.6])

        def _monotonic() -> float:
            return next(monotonic_values)

        observed: list[SystemSnapshot] = []
        sleep_calls: list[float] = []
        original_collect_snapshot = poller.collect_snapshot

        def _collect_snapshot(now: Callable[[], float] | None = None) -> SystemSnapshot:
            return produced[len(observed)]

        def _should_stop() -> bool:
            return len(observed) >= len(produced)

        poller.collect_snapshot = _collect_snapshot
        try:
            emitted = poller.poll_snapshots(
                observed.append,
                interval_seconds=1.0,
                should_stop=_should_stop,
                sleep=sleep_calls.append,
                monotonic=_monotonic,
            )
        finally:
            poller.collect_snapshot = original_collect_snapshot

        self.assertEqual(emitted, len(produced))
        self.assertEqual(observed, produced)
        self.assertEqual(len(sleep_calls), 1)
        self.assertAlmostEqual(sleep_calls[0], 0.8)


if __name__ == "__main__":
    unittest.main()
