from __future__ import annotations

import pathlib
import sys
import unittest

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from contracts.types import ProcessSample, SystemSnapshot  # noqa: E402
from render.formatting import (  # noqa: E402
    format_process_row,
    format_process_rows,
    format_snapshot_lines,
)


class RenderFormattingTests(unittest.TestCase):
    def test_format_snapshot_lines_uses_expected_live_strings(self) -> None:
        snapshot = SystemSnapshot(timestamp=0.0, cpu_percent=12.34, memory_percent=56.78)

        lines = format_snapshot_lines(snapshot)

        self.assertEqual(lines["cpu"], "CPU 12.3%")
        self.assertEqual(lines["memory"], "Memory 56.8%")
        self.assertEqual(lines["timestamp"], "Updated 00:00:00 UTC")

    def test_format_process_row_formats_rank_cpu_and_memory(self) -> None:
        row = format_process_row(
            ProcessSample(
                pid=101,
                name="worker",
                cpu_percent=4.5,
                memory_rss_bytes=2 * 1024 * 1024,
            ),
            rank=2,
        )

        self.assertEqual(
            row,
            " 2. worker                CPU   4.5%  RAM     2.0 MB",
        )

    def test_format_process_rows_truncates_and_pads_placeholders(self) -> None:
        rows = format_process_rows(
            [
                ProcessSample(
                    pid=101,
                    name="very_long_process_name_for_truncation",
                    cpu_percent=12.3,
                    memory_rss_bytes=3 * 1024 * 1024,
                ),
                ProcessSample(
                    pid=102,
                    name="worker",
                    cpu_percent=4.5,
                    memory_rss_bytes=2 * 1024 * 1024,
                ),
            ],
            row_count=3,
        )

        self.assertEqual(len(rows), 3)
        self.assertIn(" 1. very_long_process...", rows[0])
        self.assertIn("CPU  12.3%", rows[0])
        self.assertIn("RAM     3.0 MB", rows[0])
        self.assertIn(" 2. worker", rows[1])
        self.assertEqual(rows[2], " 3. collecting process data...")

    def test_format_process_rows_returns_empty_for_non_positive_row_count(self) -> None:
        sample = ProcessSample(
            pid=101,
            name="worker",
            cpu_percent=4.5,
            memory_rss_bytes=2 * 1024 * 1024,
        )
        self.assertEqual(format_process_rows([sample], row_count=0), [])
        self.assertEqual(format_process_rows([sample], row_count=-3), [])


if __name__ == "__main__":
    unittest.main()
