from __future__ import annotations

import argparse
import json
import sys

from core.poller import collect_snapshot


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Aura telemetry bootstrap CLI.")
    parser.add_argument(
        "--json",
        action="store_true",
        help="Print a JSON snapshot instead of text output.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    try:
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

    if args.json:
        print(json.dumps(snapshot.to_dict(), sort_keys=True))
    else:
        print(
            f"cpu={snapshot.cpu_percent:.1f}% "
            f"mem={snapshot.memory_percent:.1f}% "
            f"ts={snapshot.timestamp:.3f}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
