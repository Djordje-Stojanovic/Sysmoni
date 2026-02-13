from __future__ import annotations

from typing import Protocol


class RecorderStatusProvider(Protocol):
    def get_status(self) -> tuple[str | None, int | None, str | None]:
        ...


def _normalized_sample_count(sample_count: int | None) -> int:
    return 0 if sample_count is None else sample_count


def format_initial_status(recorder: RecorderStatusProvider) -> str:
    db_path, sample_count, error = recorder.get_status()
    if db_path is None:
        return "Collecting telemetry..."
    if error is not None:
        return f"Collecting telemetry... | DVR unavailable: {error}"
    return (
        "Collecting telemetry... | "
        f"DVR samples: {_normalized_sample_count(sample_count)}"
    )


def format_stream_status(recorder: RecorderStatusProvider) -> str:
    db_path, sample_count, error = recorder.get_status()
    if db_path is None:
        return "Streaming telemetry"
    if error is not None:
        return f"Streaming telemetry | DVR unavailable: {error}"
    return f"Streaming telemetry | DVR samples: {_normalized_sample_count(sample_count)}"


__all__ = ["RecorderStatusProvider", "format_initial_status", "format_stream_status"]
