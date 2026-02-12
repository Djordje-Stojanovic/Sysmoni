from __future__ import annotations

import argparse
import json
import sys
import threading

from core.poller import collect_snapshot, run_polling_loop
from core.types import SystemSnapshot


def _positive_interval_seconds(value: str) -> float:
    interval = float(value)
    if interval <= 0:
        raise argparse.ArgumentTypeError("interval must be greater than 0")
    return interval


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Aura telemetry bootstrap CLI.")
    parser.add_argument(
        "--json",
        action="store_true",
        help="Print a JSON snapshot instead of text output.",
    )
    parser.add_argument(
        "--watch",
        action="store_true",
        help="Stream snapshots continuously until interrupted.",
    )
    parser.add_argument(
        "--interval",
        type=_positive_interval_seconds,
        default=1.0,
        help="Polling interval in seconds for --watch mode (default: 1.0).",
    )
    return parser


def _print_snapshot(snapshot: SystemSnapshot, *, output_json: bool) -> None:
    if output_json:
        print(json.dumps(snapshot.to_dict(), sort_keys=True))
        return

    print(
        f"cpu={snapshot.cpu_percent:.1f}% "
        f"mem={snapshot.memory_percent:.1f}% "
        f"ts={snapshot.timestamp:.3f}"
    )


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    try:
        if args.watch:
            run_polling_loop(
                args.interval,
                lambda snapshot: _print_snapshot(snapshot, output_json=args.json),
                stop_event=threading.Event(),
            )
            return 0

        snapshot = collect_snapshot()
    except KeyboardInterrupt:
        print("Interrupted by user.", file=sys.stderr)
        return 130
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 2
    except Exception as exc:
        print(f"Failed to collect telemetry snapshot: {exc}", file=sys.stderr)
        return 2

    _print_snapshot(snapshot, output_json=args.json)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
