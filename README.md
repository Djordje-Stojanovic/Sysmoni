# Aura (Sysmoni)

Aura is a local-only desktop system monitor built around a cockpit-style GUI,
real-time telemetry, and a local DVR store. No cloud services are required.

## Current Product State

Aura is actively shippable as a Python package and runnable as both CLI and GUI.

Implemented now:
- Runtime CLI with live streaming, persistence controls, and range readback.
- GUI bootstrap (`--gui`) with deferred Qt imports for headless-safe testing.
- Frameless cockpit window with custom titlebar and draggable window controls.
- Docked multi-panel shell with tabbed left/center/right slots.
- Telemetry DVR backed by SQLite with retention pruning and schema migration.
- Timeline rewind path with LTTB downsampling and scrub-to-freeze/live-resume UX.
- Custom render widgets (radial gauge, sparkline, timeline, glass panel fallback).
- Installable package entrypoints (`aura`, `aura-gui`) and working wheel/sdist builds.

## Quick Start

```bash
uv sync
uv run aura --json
```

Run GUI:

```bash
uv run aura-gui
```

Compatibility wrapper still works:

```bash
uv run python src/main.py --gui
```

## CLI Usage

Primary command:

```bash
uv run aura [options]
```

Key options:
- `--json`: emit snapshots as JSON.
- `--watch`: stream continuously.
- `--interval <seconds>`: poll interval for watch mode (finite, > 0).
- `--count <n>`: stop watch mode after `n` snapshots.
- `--no-persist`: disable DVR persistence.
- `--db-path <path>`: explicit SQLite path override.
- `--retention-seconds <seconds>`: retention horizon override.
- `--latest <n>`: read latest persisted snapshots, then exit.
- `--since <timestamp>` / `--until <timestamp>`: read persisted range.
- `--gui`: launch GUI mode (incompatible with read/watch/json flags).

Behavioral guarantees:
- Closed output streams (for example broken pipe) exit cleanly.
- Persistence write failures degrade to live output with warnings.
- Keyboard interrupt returns exit code `130`.

## Configuration

Runtime config precedence:
1. CLI flags
2. Environment variables
3. Config file (`aura.toml`)
4. Platform defaults

Supported environment variables:
- `AURA_DB_PATH`
- `AURA_RETENTION_SECONDS`

Config file path:
- Windows: `%APPDATA%\Aura\aura.toml` (fallback `%LOCALAPPDATA%` then `~/AppData/Roaming`)
- macOS: `~/Library/Application Support/Aura/aura.toml`
- Linux: `$XDG_CONFIG_HOME/Aura/aura.toml` (fallback `~/.config/Aura/aura.toml`)

Config file example:

```toml
[persistence]
db_path = "C:/Users/you/AppData/Roaming/Aura/telemetry.sqlite"
retention_seconds = 86400
```

Default DB path:
- Windows: `%APPDATA%\Aura\telemetry.sqlite` (fallback `%LOCALAPPDATA%`)
- macOS: `~/Library/Application Support/Aura/telemetry.sqlite`
- Linux: `$XDG_DATA_HOME/Aura/telemetry.sqlite` (fallback `~/.local/share/Aura/telemetry.sqlite`)

## GUI and Rendering

GUI shell (`src/shell/window.py`) currently includes:
- Frameless window with custom `AuraTitleBar` (drag, minimize, maximize, close).
- Four panels:
  - Telemetry Overview (CPU/memory radial gauges + sparklines).
  - Top Processes (formatted process rows).
  - DVR Timeline (interactive scrubber, hover tooltip, time axis).
  - Render Surface (bridge status/hints and render-reactive CPU gauge).
- Docking state manager with move controls and per-slot tab activation.
- Background worker thread for snapshot/process polling.
- Frame-timed accent animation and dynamic stylesheet updates.

Render layer (`src/render/**`) provides:
- Metric/status/process formatting helpers.
- Theme system with accent-aware stylesheet generation.
- Frame discipline and compositor helpers.
- Custom widgets:
  - `RadialGauge`
  - `SparkLine`
  - `TimelineWidget`
  - `GlassPanel` (OpenGL shader path with QPainter fallback)

## Telemetry and Storage

Telemetry modules (`src/telemetry/**`):
- `poller.py`: system snapshots, top-process collection, fixed-interval polling.
- `network.py`: total and per-second network bytes/packets.
- `disk.py`: total and per-second disk bytes/ops.
- `thermal.py`: psutil temperatures with WMI fallback on Windows.

Storage and DVR (`src/runtime/**`):
- `store.py`: SQLite store with:
  - retention pruning on startup/write/read paths
  - legacy schema migration (`timestamp` PK -> `id` PK)
  - thread-safe append/read/count operations
- `dvr.py`:
  - LTTB downsampling (`downsample_lttb`)
  - timeline query helper (`query_timeline`)

## Packaging

`pyproject.toml` defines installable entrypoints:
- Console script: `aura = runtime.main:main`
- GUI script: `aura-gui = runtime.app:launch_gui`

Build artifacts:

```bash
uv build
```

This produces:
- `dist/aura-<version>.tar.gz`
- `dist/aura-<version>-py3-none-any.whl`

## Testing and Quality

Project checks:

```bash
uv run pytest tests/ -x
uv run ruff check src/ tests/
uv run pyright src/
```

Module test suites:
- `tests/test_platform/**`
- `tests/test_render/**`
- `tests/test_shell/**`
- `tests/test_telemetry/**`

## Repository Layout

```text
src/
  contracts/
  runtime/
  telemetry/
  render/
  shell/
  gui/        (compatibility wrapper)
  main.py     (compatibility wrapper)
tests/
  test_platform/
  test_render/
  test_shell/
  test_telemetry/
```

## Recent Highlights (Last 50 Commits)

Major shipped additions reflected in current code:
- Shell DVR timeline panel with freeze-on-scrub and live resume.
- Packaging entrypoints and packaging verification tests.
- GlassPanel OpenGL widget with painter fallback.
- TOML config support with platform-aware config path resolution.
- Thermal telemetry collector with psutil/WMI fallback.
- Frameless titlebar with drag/min/max/close controls.
- GUI lifecycle bootstrap and `--gui` dispatch path.
- RadialGauge/SparkLine widgets integrated into cockpit panels.
- Timeline widget with scrubber, hover tooltip, and time-axis labels.
- Disk and network telemetry collectors with rate computation.
- LTTB downsampling + runtime timeline query helper.
- Render frame-discipline compositor/theme primitives.
- Runtime resilience improvements for persistence failures and stream errors.
- Top-process collector performance/reliability hardening (top-k + PID reuse safety).
