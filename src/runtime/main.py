from __future__ import annotations

import argparse
import errno
import json
import math
import os
import sys
import threading

from contracts.types import SystemSnapshot
from runtime.config import resolve_runtime_config
from runtime.store import TelemetryStore
from telemetry.poller import collect_snapshot, run_polling_loop


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


def _positive_retention_seconds(value: str) -> float:
    try:
        retention_seconds = float(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(
            "retention must be a finite number greater than 0"
        ) from exc
    if not math.isfinite(retention_seconds) or retention_seconds <= 0:
        raise argparse.ArgumentTypeError(
            "retention must be a finite number greater than 0"
        )
    return retention_seconds


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
        "--retention-seconds",
        type=_positive_retention_seconds,
        default=None,
        help=(
            "Optional retention horizon in seconds for persisted telemetry. "
            "Defaults to AURA_RETENTION_SECONDS or store default."
        ),
    )
    parser.add_argument(
        "--no-persist",
        action="store_true",
        help="Disable persistence and run telemetry without opening a local store.",
    )
    parser.add_argument(
        "--latest",
        type=_positive_sample_count,
        default=None,
        help="Emit the latest N persisted snapshots, then exit.",
    )
    parser.add_argument(
        "--since",
        type=_finite_timestamp_seconds,
        default=None,
        help="Emit persisted snapshots with timestamp >= this value.",
    )
    parser.add_argument(
        "--until",
        type=_finite_timestamp_seconds,
        default=None,
        help="Emit persisted snapshots with timestamp <= this value.",
    )
    parser.add_argument(
        "--db-path",
        default=None,
        help=(
            "Optional SQLite DB path override. "
            "Defaults to AURA_DB_PATH or platform app-data location."
        ),
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


def _disable_store_after_write_failure(
    store: TelemetryStore | None,
    *,
    error: Exception,
    warning_emitted: bool,
) -> tuple[TelemetryStore | None, bool]:
    if store is None:
        return None, warning_emitted

    if not warning_emitted:
        print(f"DVR persistence disabled: {error}", file=sys.stderr)
        warning_emitted = True

    try:
        store.close()
    except Exception:
        pass

    return None, warning_emitted


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.count is not None and not args.watch:
        parser.error("--count requires --watch")
    if args.latest is not None and args.watch:
        parser.error("--latest cannot be used with --watch")
    has_range_read = args.since is not None or args.until is not None
    if has_range_read and args.watch:
        parser.error("--since/--until cannot be used with --watch")
    if has_range_read and args.latest is not None:
        parser.error("--since/--until cannot be used with --latest")
    if args.latest is not None and args.no_persist:
        parser.error("--latest cannot be used with --no-persist")
    if has_range_read and args.no_persist:
        parser.error("--since/--until cannot be used with --no-persist")
    if args.since is not None and args.until is not None and args.since > args.until:
        parser.error("--since must be less than or equal to --until")

    store: TelemetryStore | None = None
    try:
        config = resolve_runtime_config(args)

        if config.persistence_enabled:
            db_path = config.db_path
            if db_path is None:
                raise RuntimeError("Persistence enabled without a resolved db path.")
            try:
                store = TelemetryStore(
                    db_path,
                    retention_seconds=config.retention_seconds,
                )
            except Exception as exc:
                if config.db_source == "auto":
                    print(
                        f"DVR persistence disabled: {exc}",
                        file=sys.stderr,
                    )
                    store = None
                else:
                    raise

        if args.latest is not None:
            if store is None:
                raise RuntimeError("Unable to open telemetry store for --latest.")

            for snapshot in store.latest(limit=args.latest):
                _print_snapshot(snapshot, output_json=args.json)
            return 0

        if has_range_read:
            if store is None:
                raise RuntimeError("Unable to open telemetry store for --since/--until.")

            for snapshot in store.between(
                start_timestamp=args.since,
                end_timestamp=args.until,
            ):
                _print_snapshot(snapshot, output_json=args.json)
            return 0

        if args.watch:
            stop_event = threading.Event()
            remaining_samples = args.count
            store_warning_emitted = False

            def _on_snapshot(snapshot: SystemSnapshot) -> None:
                nonlocal remaining_samples, store_warning_emitted, store
                if store is not None:
                    try:
                        store.append(snapshot)
                    except Exception as exc:
                        store, store_warning_emitted = _disable_store_after_write_failure(
                            store,
                            error=exc,
                            warning_emitted=store_warning_emitted,
                        )
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
            try:
                store.append(snapshot)
            except Exception as exc:
                store, _ = _disable_store_after_write_failure(
                    store,
                    error=exc,
                    warning_emitted=False,
                )
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
