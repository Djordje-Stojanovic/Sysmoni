from __future__ import annotations

import pathlib
import sys
import unittest

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from render.status import format_initial_status, format_stream_status  # noqa: E402


class _RecorderStub:
    def __init__(
        self,
        *,
        db_path: str | None,
        sample_count: int | None,
        error: str | None,
    ) -> None:
        self._status = (db_path, sample_count, error)

    def get_status(self) -> tuple[str | None, int | None, str | None]:
        return self._status


class RenderStatusTests(unittest.TestCase):
    def test_format_initial_status_without_dvr_path(self) -> None:
        recorder = _RecorderStub(db_path=None, sample_count=None, error=None)

        self.assertEqual(format_initial_status(recorder), "Collecting telemetry...")

    def test_format_stream_status_without_dvr_path(self) -> None:
        recorder = _RecorderStub(db_path=None, sample_count=None, error=None)

        self.assertEqual(format_stream_status(recorder), "Streaming telemetry")

    def test_status_with_configured_dvr_and_no_error(self) -> None:
        recorder = _RecorderStub(db_path="telemetry.sqlite", sample_count=7, error=None)

        self.assertEqual(
            format_initial_status(recorder),
            "Collecting telemetry... | DVR samples: 7",
        )
        self.assertEqual(
            format_stream_status(recorder),
            "Streaming telemetry | DVR samples: 7",
        )

    def test_status_with_configured_dvr_and_none_sample_count(self) -> None:
        recorder = _RecorderStub(db_path="telemetry.sqlite", sample_count=None, error=None)

        self.assertEqual(
            format_initial_status(recorder),
            "Collecting telemetry... | DVR samples: 0",
        )
        self.assertEqual(
            format_stream_status(recorder),
            "Streaming telemetry | DVR samples: 0",
        )

    def test_status_with_configured_dvr_and_error(self) -> None:
        recorder = _RecorderStub(
            db_path="telemetry.sqlite",
            sample_count=4,
            error="disk full",
        )

        self.assertEqual(
            format_initial_status(recorder),
            "Collecting telemetry... | DVR unavailable: disk full",
        )
        self.assertEqual(
            format_stream_status(recorder),
            "Streaming telemetry | DVR unavailable: disk full",
        )


if __name__ == "__main__":
    unittest.main()
