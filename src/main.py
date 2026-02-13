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


class _ClosedOutputStreamError(RuntimeError):
    """Raised when stdout closes while emitting telemetry output."""


def _is_closed_stream_error(exc: OSError) -> bool:
    return isinstance(exc, BrokenPipeError) or exc.errno in _CLOSED_STREAM_ERRNOS


def _positive_interval_seconds(value: str) -> float:
    try:
        interval = float(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(
            "interval must be a finite number greater than 0"
        ) from exc
    if not math.isfinite(interval) or interval <= 0:
        raise argparse.ArgumentTypeError(
            "interval must be a finite number greater than 0"
        )
    return interval


def _finite_timestamp_seconds(value: str) -> float:
    try:
        timestamp = float(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("timestamp must be a finite number") from exc
    if not math.isfinite(timestamp):
        raise argparse.ArgumentTypeError("timestamp must be a finite number")
    return timestamp


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
        "--latest",
        type=_positive_sample_count,
        default=None,
        help="Emit the latest N persisted snapshots from --db-path, then exit.",
    )
    parser.add_argument(
        "--since",
        type=_finite_timestamp_seconds,
        default=None,
        help="Emit persisted snapshots with timestamp >= this value; requires --db-path.",
    )
    parser.add_argument(
        "--until",
        type=_finite_timestamp_seconds,
        default=None,
        help="Emit persisted snapshots with timestamp <= this value; requires --db-path.",
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
    line = (
        json.dumps(snapshot.to_dict(), sort_keys=True)
        if output_json
        else (
            f"cpu={snapshot.cpu_percent:.1f}% "
            f"mem={snapshot.memory_percent:.1f}% "
            f"ts={snapshot.timestamp:.3f}"
        )
    )
    try:
        print(line, flush=flush)
    except OSError as exc:
        if _is_closed_stream_error(exc):
            raise _ClosedOutputStreamError from exc
        raise


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
    if args.latest is not None and args.watch:
        parser.error("--latest cannot be used with --watch")
    if args.latest is not None and not args.db_path:
        parser.error("--latest requires --db-path")
    has_range_read = args.since is not None or args.until is not None
    if has_range_read and args.watch:
        parser.error("--since/--until cannot be used with --watch")
    if has_range_read and args.latest is not None:
        parser.error("--since/--until cannot be used with --latest")
    if has_range_read and not args.db_path:
        parser.error("--since/--until require --db-path")
    if args.since is not None and args.until is not None and args.since > args.until:
        parser.error("--since must be less than or equal to --until")

    store: TelemetryStore | None = None
    try:
        if args.db_path:
            store = TelemetryStore(args.db_path)

        if args.latest is not None:
            if store is None:
                parser.error("--latest requires --db-path")

            for snapshot in store.latest(limit=args.latest):
                _print_snapshot(snapshot, output_json=args.json)
            return 0

        if has_range_read:
            if store is None:
                parser.error("--since/--until require --db-path")

            for snapshot in store.between(
                start_timestamp=args.since,
                end_timestamp=args.until,
            ):
                _print_snapshot(snapshot, output_json=args.json)
            return 0

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
    except _ClosedOutputStreamError:
        _redirect_stdout_to_devnull()
        return 0
    except OSError as exc:
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
