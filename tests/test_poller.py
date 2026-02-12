from __future__ import annotations

import pathlib
import sys
import unittest


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from core import poller  # noqa: E402


class _PsutilStub:
    def __init__(self, cpu_percent: float = 12.5, memory_percent: float = 41.0) -> None:
        self._cpu_percent = cpu_percent
        self._memory_percent = memory_percent
        self.cpu_intervals: list[float | None] = []

    def cpu_percent(self, interval: float | None = None) -> float:
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


if __name__ == "__main__":
    unittest.main()
