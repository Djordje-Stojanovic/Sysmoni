"""Tests for src/runtime/dvr.py — LTTB downsampling and timeline queries."""

from __future__ import annotations

import pathlib
import sys
import time

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

import pytest  # noqa: E402

from contracts.types import SystemSnapshot  # noqa: E402
from runtime.dvr import downsample_lttb, query_timeline  # noqa: E402
from runtime.store import TelemetryStore  # noqa: E402


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_NOW = time.time()


def _make_snapshots(
    n: int,
    *,
    base_ts: float | None = None,
    cpu: float = 50.0,
    spikes: dict[int, float] | None = None,
) -> list[SystemSnapshot]:
    """Generate *n* snapshots at 1-second intervals with optional CPU spikes."""
    if base_ts is None:
        base_ts = _NOW - n  # recent enough to survive retention
    out: list[SystemSnapshot] = []
    for i in range(n):
        cpu_val = cpu if spikes is None or i not in spikes else spikes[i]
        out.append(SystemSnapshot(timestamp=base_ts + i, cpu_percent=cpu_val, memory_percent=40.0))
    return out


def _make_store(snapshots: list[SystemSnapshot] | None = None) -> TelemetryStore:
    """Create an in-memory store optionally pre-loaded with *snapshots*."""
    store = TelemetryStore(":memory:", now=lambda: _NOW)
    if snapshots:
        for snap in snapshots:
            store.append(snap)
    return store


# ===========================================================================
# downsample_lttb — validation
# ===========================================================================

class TestLttbValidation:
    def test_target_below_2_raises(self) -> None:
        with pytest.raises(ValueError, match="target must be"):
            downsample_lttb([], 1)

    def test_target_zero_raises(self) -> None:
        with pytest.raises(ValueError, match="target must be"):
            downsample_lttb([], 0)

    def test_target_negative_raises(self) -> None:
        with pytest.raises(ValueError, match="target must be"):
            downsample_lttb([], -5)

    def test_target_bool_raises(self) -> None:
        with pytest.raises(ValueError, match="target must be"):
            downsample_lttb([], True)

    def test_empty_input(self) -> None:
        assert downsample_lttb([], 2) == []

    def test_single_element(self) -> None:
        snaps = _make_snapshots(1)
        result = downsample_lttb(snaps, 2)
        assert result == snaps


# ===========================================================================
# downsample_lttb — passthrough
# ===========================================================================

class TestLttbPassthrough:
    def test_at_target_returns_copy(self) -> None:
        snaps = _make_snapshots(5)
        result = downsample_lttb(snaps, 5)
        assert result == snaps
        assert result is not snaps  # new list

    def test_below_target_returns_copy(self) -> None:
        snaps = _make_snapshots(3)
        result = downsample_lttb(snaps, 10)
        assert result == snaps
        assert result is not snaps


# ===========================================================================
# downsample_lttb — correctness
# ===========================================================================

class TestLttbCorrectness:
    def test_first_and_last_always_included(self) -> None:
        snaps = _make_snapshots(100)
        result = downsample_lttb(snaps, 10)
        assert result[0] is snaps[0]
        assert result[-1] is snaps[-1]

    def test_exact_target_count(self) -> None:
        snaps = _make_snapshots(200)
        result = downsample_lttb(snaps, 20)
        assert len(result) == 20

    def test_chronological_order_preserved(self) -> None:
        snaps = _make_snapshots(100)
        result = downsample_lttb(snaps, 15)
        timestamps = [s.timestamp for s in result]
        assert timestamps == sorted(timestamps)

    def test_cpu_spike_preserved(self) -> None:
        snaps = _make_snapshots(100, cpu=10.0, spikes={50: 99.0})
        result = downsample_lttb(snaps, 10)
        cpu_values = [s.cpu_percent for s in result]
        assert 99.0 in cpu_values

    def test_all_results_are_originals(self) -> None:
        snaps = _make_snapshots(50)
        result = downsample_lttb(snaps, 10)
        snap_ids = {id(s) for s in snaps}
        for s in result:
            assert id(s) in snap_ids


# ===========================================================================
# downsample_lttb — edge cases
# ===========================================================================

class TestLttbEdgeCases:
    def test_two_points_target_two(self) -> None:
        snaps = _make_snapshots(2)
        result = downsample_lttb(snaps, 2)
        assert result == snaps

    def test_three_points_target_two(self) -> None:
        snaps = _make_snapshots(3)
        result = downsample_lttb(snaps, 2)
        assert len(result) == 2
        assert result[0] is snaps[0]
        assert result[-1] is snaps[-1]

    def test_constant_data(self) -> None:
        """When all CPU values are equal, area is 0 for all; algorithm still picks one per bucket."""
        snaps = _make_snapshots(50, cpu=42.0)
        result = downsample_lttb(snaps, 10)
        assert len(result) == 10
        assert result[0] is snaps[0]
        assert result[-1] is snaps[-1]


# ===========================================================================
# query_timeline — validation
# ===========================================================================

class TestQueryTimelineValidation:
    def test_resolution_below_2_raises(self) -> None:
        store = _make_store()
        with pytest.raises(ValueError, match="resolution must be"):
            query_timeline(store, resolution=1)

    def test_resolution_bool_raises(self) -> None:
        store = _make_store()
        with pytest.raises(ValueError, match="resolution must be"):
            query_timeline(store, resolution=True)

    def test_empty_store_returns_empty(self) -> None:
        store = _make_store()
        assert query_timeline(store) == []


# ===========================================================================
# query_timeline — integration
# ===========================================================================

class TestQueryTimelineIntegration:
    def test_below_resolution_returns_all(self) -> None:
        snaps = _make_snapshots(10)
        store = _make_store(snaps)
        result = query_timeline(store, resolution=500)
        assert len(result) == 10

    def test_above_resolution_downsamples(self) -> None:
        snaps = _make_snapshots(200)
        store = _make_store(snaps)
        result = query_timeline(store, resolution=20)
        assert len(result) == 20

    def test_respects_time_window(self) -> None:
        base = _NOW - 100
        snaps = _make_snapshots(100, base_ts=base)
        store = _make_store(snaps)
        start = base + 20
        end = base + 50
        result = query_timeline(store, start=start, end=end, resolution=500)
        for s in result:
            assert start <= s.timestamp <= end

    def test_spike_preserved_through_roundtrip(self) -> None:
        snaps = _make_snapshots(200, cpu=10.0, spikes={100: 95.0})
        store = _make_store(snaps)
        result = query_timeline(store, resolution=20)
        cpu_values = [s.cpu_percent for s in result]
        assert 95.0 in cpu_values

    def test_open_ended_range(self) -> None:
        base = _NOW - 50
        snaps = _make_snapshots(50, base_ts=base)
        store = _make_store(snaps)
        mid = base + 25
        result = query_timeline(store, start=mid, resolution=500)
        assert all(s.timestamp >= mid for s in result)
        result2 = query_timeline(store, end=mid, resolution=500)
        assert all(s.timestamp <= mid for s in result2)
