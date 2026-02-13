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
from core.types import ProcessSample, SystemSnapshot  # noqa: E402


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


class _ProcessMemoryInfoStub:
    def __init__(self, rss: int) -> None:
        self.rss = rss


class _ProcessStub:
    def __init__(self, info: dict[str, object]) -> None:
        self.info = info


class _NoSuchProcessStub(Exception):
    pass


class _AccessDeniedStub(Exception):
    pass


class _ZombieProcessStub(Exception):
    pass


class _ProcessPsutilStub:
    NoSuchProcess = _NoSuchProcessStub
    AccessDenied = _AccessDeniedStub
    ZombieProcess = _ZombieProcessStub

    def __init__(self, processes: list[object]) -> None:
        self._processes = processes
        self.process_iter_attrs: list[list[str]] = []

    def process_iter(self, attrs: list[str]) -> list[object]:
        self.process_iter_attrs.append(attrs)
        return list(self._processes)


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


class CollectTopProcessesTests(unittest.TestCase):
    def setUp(self) -> None:
        with poller._process_name_cache_lock:
            poller._process_name_cache.clear()

    def tearDown(self) -> None:
        with poller._process_name_cache_lock:
            poller._process_name_cache.clear()

    def test_collect_top_processes_raises_when_psutil_missing(self) -> None:
        original_psutil = poller.psutil
        poller.psutil = None
        try:
            with self.assertRaises(RuntimeError):
                poller.collect_top_processes()
        finally:
            poller.psutil = original_psutil

    def test_collect_top_processes_requires_positive_limit(self) -> None:
        original_psutil = poller.psutil
        poller.psutil = _ProcessPsutilStub([])
        try:
            for invalid_limit in (0, -1, False):
                with self.subTest(invalid_limit=invalid_limit):
                    with self.assertRaises(ValueError):
                        poller.collect_top_processes(limit=invalid_limit)
        finally:
            poller.psutil = original_psutil

    def test_collect_top_processes_sorts_by_cpu_then_memory_and_applies_limit(self) -> None:
        stub = _ProcessPsutilStub(
            [
                _ProcessStub(
                    {
                        "pid": 1,
                        "name": "alpha",
                        "cpu_percent": 2.0,
                        "memory_info": _ProcessMemoryInfoStub(200),
                    }
                ),
                _ProcessStub(
                    {
                        "pid": 2,
                        "name": "beta",
                        "cpu_percent": 20.0,
                        "memory_info": _ProcessMemoryInfoStub(100),
                    }
                ),
                _ProcessStub(
                    {
                        "pid": 3,
                        "name": "",
                        "cpu_percent": 20.0,
                        "memory_info": _ProcessMemoryInfoStub(500),
                    }
                ),
                _ProcessStub(
                    {
                        "pid": 0,
                        "name": "invalid",
                        "cpu_percent": 99.0,
                        "memory_info": _ProcessMemoryInfoStub(999),
                    }
                ),
            ]
        )
        original_psutil = poller.psutil
        poller.psutil = stub
        try:
            samples = poller.collect_top_processes(limit=2)
        finally:
            poller.psutil = original_psutil

        self.assertEqual(
            stub.process_iter_attrs,
            [["pid", "name", "cpu_percent", "memory_info"]],
        )
        self.assertEqual(
            samples,
            [
                ProcessSample(
                    pid=3,
                    name="pid-3",
                    cpu_percent=20.0,
                    memory_rss_bytes=500,
                ),
                ProcessSample(
                    pid=2,
                    name="beta",
                    cpu_percent=20.0,
                    memory_rss_bytes=100,
                ),
            ],
        )

    def test_collect_top_processes_reuses_cached_name_when_name_is_missing(self) -> None:
        seeded_stub = _ProcessPsutilStub(
            [
                _ProcessStub(
                    {
                        "pid": 7,
                        "name": "worker",
                        "cpu_percent": 15.0,
                        "memory_info": _ProcessMemoryInfoStub(150),
                    }
                )
            ]
        )
        missing_name_stub = _ProcessPsutilStub(
            [
                _ProcessStub(
                    {
                        "pid": 7,
                        "name": "",
                        "cpu_percent": 16.0,
                        "memory_info": _ProcessMemoryInfoStub(160),
                    }
                )
            ]
        )
        original_psutil = poller.psutil
        poller.psutil = seeded_stub
        try:
            seeded_samples = poller.collect_top_processes(limit=5)
            poller.psutil = missing_name_stub
            cached_samples = poller.collect_top_processes(limit=5)
        finally:
            poller.psutil = original_psutil

        self.assertEqual(seeded_samples[0].name, "worker")
        self.assertEqual(cached_samples[0].name, "worker")

    def test_collect_top_processes_prunes_name_cache_for_unseen_pids(self) -> None:
        first_stub = _ProcessPsutilStub(
            [
                _ProcessStub(
                    {
                        "pid": 10,
                        "name": "alpha",
                        "cpu_percent": 20.0,
                        "memory_info": _ProcessMemoryInfoStub(200),
                    }
                )
            ]
        )
        second_stub = _ProcessPsutilStub(
            [
                _ProcessStub(
                    {
                        "pid": 11,
                        "name": "beta",
                        "cpu_percent": 30.0,
                        "memory_info": _ProcessMemoryInfoStub(300),
                    }
                )
            ]
        )
        original_psutil = poller.psutil
        poller.psutil = first_stub
        try:
            poller.collect_top_processes(limit=5)
            poller.psutil = second_stub
            poller.collect_top_processes(limit=5)
        finally:
            poller.psutil = original_psutil

        with poller._process_name_cache_lock:
            cached_names = dict(poller._process_name_cache)

        self.assertEqual(cached_names, {11: "beta"})

    def test_collect_top_processes_skips_recoverable_process_errors(self) -> None:
        class _DeniedProcess:
            @property
            def info(self) -> dict[str, object]:
                raise _AccessDeniedStub("permission denied")

        stub = _ProcessPsutilStub(
            [
                _DeniedProcess(),
                _ProcessStub(
                    {
                        "pid": 4,
                        "name": "worker",
                        "cpu_percent": 8.5,
                        "memory_info": _ProcessMemoryInfoStub(120),
                    }
                ),
                _ProcessStub(
                    {
                        "pid": 5,
                        "name": "broken",
                        "cpu_percent": math.nan,
                        "memory_info": _ProcessMemoryInfoStub(200),
                    }
                ),
            ]
        )
        original_psutil = poller.psutil
        poller.psutil = stub
        try:
            samples = poller.collect_top_processes(limit=10)
        finally:
            poller.psutil = original_psutil

        self.assertEqual(
            samples,
            [
                ProcessSample(
                    pid=4,
                    name="worker",
                    cpu_percent=8.5,
                    memory_rss_bytes=120,
                )
            ],
        )


class RunPollingLoopTests(unittest.TestCase):
    def test_run_polling_loop_rejects_boolean_interval(self) -> None:
        for invalid_interval in (True, False):
            with self.subTest(invalid_interval=invalid_interval):
                with self.assertRaises(ValueError):
                    poller.run_polling_loop(
                        invalid_interval,
                        lambda _snapshot: None,
                        stop_event=threading.Event(),
                    )

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
    def test_poll_snapshots_rejects_boolean_interval(self) -> None:
        for invalid_interval in (True, False):
            with self.subTest(invalid_interval=invalid_interval):
                with self.assertRaises(ValueError):
                    poller.poll_snapshots(
                        lambda _snapshot: None,
                        interval_seconds=invalid_interval,
                    )

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

    def test_poll_snapshots_continues_when_continue_on_error_is_enabled(self) -> None:
        expected_snapshot = SystemSnapshot(
            timestamp=2.0,
            cpu_percent=11.0,
            memory_percent=21.0,
        )
        collected_calls = 0
        observed: list[SystemSnapshot] = []
        observed_errors: list[str] = []
        sleep_calls: list[float] = []
        monotonic_values = iter([10.0, 10.1, 11.0, 11.2])

        def _monotonic() -> float:
            return next(monotonic_values)

        def _collect_snapshot() -> SystemSnapshot:
            nonlocal collected_calls
            collected_calls += 1
            if collected_calls == 1:
                raise RuntimeError("sensor glitch")
            return expected_snapshot

        def _should_stop() -> bool:
            return bool(observed) and bool(observed_errors)

        emitted = poller.poll_snapshots(
            observed.append,
            interval_seconds=1.0,
            should_stop=_should_stop,
            sleep=sleep_calls.append,
            monotonic=_monotonic,
            collect=_collect_snapshot,
            on_error=lambda exc: observed_errors.append(str(exc)),
            continue_on_error=True,
        )

        self.assertEqual(emitted, 1)
        self.assertEqual(observed, [expected_snapshot])
        self.assertEqual(observed_errors, ["sensor glitch"])
        self.assertEqual(collected_calls, 2)
        self.assertEqual(len(sleep_calls), 1)
        self.assertAlmostEqual(sleep_calls[0], 0.9)


if __name__ == "__main__":
    unittest.main()
