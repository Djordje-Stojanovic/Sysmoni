from __future__ import annotations

import datetime as dt

from contracts.types import ProcessSample, SystemSnapshot

from ._metrics import sanitize_non_negative, sanitize_percent

DEFAULT_PROCESS_ROW_COUNT = 5
PROCESS_NAME_MAX_CHARS = 20
_PLACEHOLDER_TEXT = "collecting process data..."


def _truncate_process_name(name: str, *, max_chars: int = PROCESS_NAME_MAX_CHARS) -> str:
    if len(name) <= max_chars:
        return name
    if max_chars <= 3:
        return name[:max_chars]
    return f"{name[: max_chars - 3]}..."


def format_snapshot_lines(snapshot: SystemSnapshot) -> dict[str, str]:
    cpu_percent = sanitize_percent(snapshot.cpu_percent)
    memory_percent = sanitize_percent(snapshot.memory_percent)
    utc_timestamp = dt.datetime.fromtimestamp(
        snapshot.timestamp,
        tz=dt.timezone.utc,
    ).strftime("%H:%M:%S UTC")
    return {
        "cpu": f"CPU {cpu_percent:.1f}%",
        "memory": f"Memory {memory_percent:.1f}%",
        "timestamp": f"Updated {utc_timestamp}",
    }


def format_process_row(sample: ProcessSample, *, rank: int) -> str:
    memory_bytes = sanitize_non_negative(sample.memory_rss_bytes)
    memory_mb = memory_bytes / (1024.0 * 1024.0)
    cpu_percent = sanitize_percent(sample.cpu_percent)
    name = _truncate_process_name(sample.name)
    return (
        f"{rank:>2}. {name:<20}  CPU {cpu_percent:>5.1f}%  "
        f"RAM {memory_mb:>7.1f} MB"
    )


def format_process_rows(
    samples: list[ProcessSample],
    *,
    row_count: int = DEFAULT_PROCESS_ROW_COUNT,
) -> list[str]:
    if row_count <= 0:
        return []

    rows = [
        format_process_row(sample, rank=index + 1)
        for index, sample in enumerate(samples[:row_count])
    ]
    while len(rows) < row_count:
        rows.append(f"{len(rows) + 1:>2}. {_PLACEHOLDER_TEXT}")
    return rows


__all__ = [
    "DEFAULT_PROCESS_ROW_COUNT",
    "PROCESS_NAME_MAX_CHARS",
    "format_process_row",
    "format_process_rows",
    "format_snapshot_lines",
]
