"""DVR downsampling and timeline query for 24h rewind support.

Uses LTTB (Largest Triangle Three Buckets) to reduce snapshot density
while preserving visual peaks and valleys for timeline rendering.
"""

from __future__ import annotations

from contracts.types import SystemSnapshot
from runtime.store import TelemetryStore


def downsample_lttb(
    snapshots: list[SystemSnapshot],
    target: int,
) -> list[SystemSnapshot]:
    """Downsample snapshots using the LTTB algorithm.

    Selects *target* points from *snapshots* that best preserve the visual
    shape of the cpu_percent signal over time.  Returns original objects
    (no copies).

    Raises ``ValueError`` when *target* < 2.
    """
    if isinstance(target, bool):
        raise ValueError("target must be an integer >= 2.")
    target = int(target)
    if target < 2:
        raise ValueError("target must be an integer >= 2.")

    n = len(snapshots)
    if n <= target:
        return list(snapshots)

    # Always keep first and last
    selected: list[SystemSnapshot] = [snapshots[0]]

    if target == 2:
        selected.append(snapshots[-1])
        return selected

    bucket_size = (n - 2) / (target - 2)

    prev_x = snapshots[0].timestamp
    prev_y = snapshots[0].cpu_percent

    for i in range(target - 2):
        # Current bucket boundaries
        bucket_start = int(1 + i * bucket_size)
        bucket_end = int(1 + (i + 1) * bucket_size)
        bucket_end = min(bucket_end, n - 1)

        # Next bucket boundaries (for average)
        next_start = int(1 + (i + 1) * bucket_size)
        next_end = int(1 + (i + 2) * bucket_size)
        next_end = min(next_end, n - 1)
        if i == target - 3:
            # Last bucket's "next" is just the final point
            next_end = n - 1
            next_start = n - 1

        # Average of next bucket
        avg_x = 0.0
        avg_y = 0.0
        next_count = next_end - next_start + 1
        for j in range(next_start, next_end + 1):
            avg_x += snapshots[j].timestamp
            avg_y += snapshots[j].cpu_percent
        avg_x /= next_count
        avg_y /= next_count

        # Find point in current bucket with largest triangle area
        best_area = -1.0
        best_idx = bucket_start
        for j in range(bucket_start, bucket_end):
            # Triangle area = 0.5 * |x1(y2-y3) + x2(y3-y1) + x3(y1-y2)|
            area = abs(
                prev_x * (snapshots[j].cpu_percent - avg_y)
                + snapshots[j].timestamp * (avg_y - prev_y)
                + avg_x * (prev_y - snapshots[j].cpu_percent)
            )
            if area > best_area:
                best_area = area
                best_idx = j

        selected.append(snapshots[best_idx])
        prev_x = snapshots[best_idx].timestamp
        prev_y = snapshots[best_idx].cpu_percent

    selected.append(snapshots[-1])
    return selected


def query_timeline(
    store: TelemetryStore,
    *,
    start: float | None = None,
    end: float | None = None,
    resolution: int = 500,
) -> list[SystemSnapshot]:
    """Query a time range from *store* and downsample for timeline display.

    Wraps ``store.between()`` + ``downsample_lttb()``.

    Raises ``ValueError`` when *resolution* < 2.
    """
    if isinstance(resolution, bool):
        raise ValueError("resolution must be an integer >= 2.")
    resolution = int(resolution)
    if resolution < 2:
        raise ValueError("resolution must be an integer >= 2.")

    snapshots = store.between(start_timestamp=start, end_timestamp=end)
    if not snapshots:
        return []
    return downsample_lttb(snapshots, resolution)
