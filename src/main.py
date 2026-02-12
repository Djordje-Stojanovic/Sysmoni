from __future__ import annotations

import argparse
import errno
import json
import math
import os
import sys
import threading

from core.poller import collect_snapshot, run_polling_loop
from core.store import TelemetryStore
from core.types import SystemSnapshot


_CLOSED_STREAM_ERRNOS = {errno.EPIPE, errno.EINVAL}


def _positive_interval_seconds(value: str) -> float:
    interval = float(value)
    if not math.isfinite(interval) or interval <= 0:
        raise argparse.ArgumentTypeError(
            "interval must be a finite number greater than 0"
        )
    return interval


def _positive_sample_count(value: str) -> int:
    try:
        sample_count = int(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(
            "count must be an integer greater than 0"
        ) from exc

    if sample_count <= 0:
        raise argparse.ArgumentTypeError("count must be an integer greater than 0")

    return sample_count


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
    parser.add_argument(
        "--count",
        type=_positive_sample_count,
        default=None,
        help="Number of snapshots to emit before exiting in --watch mode.",
    )
    parser.add_argument(
        "--db-path",
        default=None,
        help="Optional SQLite DB path for persisting emitted snapshots.",
    )
    return parser


def _print_snapshot(
    snapshot: SystemSnapshot,
    *,
    output_json: bool,
    flush: bool = False,
) -> None:
    if output_json:
        print(json.dumps(snapshot.to_dict(), sort_keys=True), flush=flush)
        return

    print(
        f"cpu={snapshot.cpu_percent:.1f}% "
        f"mem={snapshot.memory_percent:.1f}% "
        f"ts={snapshot.timestamp:.3f}",
        flush=flush,
    )


def _redirect_stdout_to_devnull() -> None:
    """Avoid interpreter shutdown flush errors after a closed-pipe write failure."""
    try:
        devnull_fd = os.open(os.devnull, os.O_WRONLY)
    except OSError:
        return

    try:
        os.dup2(devnull_fd, sys.stdout.fileno())
    except Exception:
        pass
    finally:
        os.close(devnull_fd)


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.count is not None and not args.watch:
        parser.error("--count requires --watch")

    store: TelemetryStore | None = None
    try:
        if args.db_path:
            store = TelemetryStore(args.db_path)

        if args.watch:
            stop_event = threading.Event()
            remaining_samples = args.count

            def _on_snapshot(snapshot: SystemSnapshot) -> None:
                nonlocal remaining_samples
                if store is not None:
                    store.append(snapshot)
                _print_snapshot(
                    snapshot,
                    output_json=args.json,
                    flush=True,
                )
                if remaining_samples is None:
                    return

                remaining_samples -= 1
                if remaining_samples <= 0:
                    stop_event.set()

            run_polling_loop(
                args.interval,
                _on_snapshot,
                stop_event=stop_event,
            )
            return 0

        snapshot = collect_snapshot()
        if store is not None:
            store.append(snapshot)
        _print_snapshot(snapshot, output_json=args.json)
        return 0
    except OSError as exc:
        if isinstance(exc, BrokenPipeError) or exc.errno in _CLOSED_STREAM_ERRNOS:
            _redirect_stdout_to_devnull()
            return 0
        print(f"Failed to collect telemetry snapshot: {exc}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        print("Interrupted by user.", file=sys.stderr)
        return 130
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 2
    except Exception as exc:
        print(f"Failed to collect telemetry snapshot: {exc}", file=sys.stderr)
        return 2
    finally:
        if store is not None:
            store.close()


if __name__ == "__main__":
    raise SystemExit(main())
