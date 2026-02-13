# Sysmoni

Sysmoni is a cinematic, local-only desktop system monitor for power users.
It is designed to feel like a cockpit: real-time telemetry, high-FPS rendering,
and a premium visual layer without cloud dependencies or telemetry upload.

## What It Will Be

- A single-window dashboard with dockable panels.
- Real-time CPU, memory, process, and thermal visualization.
- Smooth 60 FPS visuals powered by PySide6 + Qt Quick/QML + OpenGL shaders.
- A local telemetry DVR buffer (24h) backed by SQLite.
- Strictly local runtime: no cloud services and no external data collection.

## Core Constraints

- Python 3.12+ only.
- GUI thread remains render-focused; polling and storage stay off-thread.
- Keep idle overhead low so the monitor does not become system load.
- Keep architecture simple and remove unnecessary abstractions early.

## Current Status

Bootstrap phase:

- Runtime CLI entrypoint is in `src/runtime/main.py` (`src/main.py` remains a compatibility wrapper).
- Shared sample models are in `src/contracts/types.py`.
- Telemetry collection is in `src/telemetry/poller.py`.
- GUI shell is in `src/shell/window.py` (`src/gui/window.py` remains a compatibility wrapper).
- Dependency baseline exists in `pyproject.toml`.

No full cinematic panel stack is implemented yet.

## Tech Stack

- Runtime: Python 3.12+
- GUI: PySide6 / Qt Quick / QML
- Telemetry: psutil
- Storage: sqlite3 (stdlib)
- Dependency management: uv

## Local Setup

```bash
uv sync
uv run python src/main.py --json
```

If `psutil` is missing, install dependencies first with `uv sync`.

## Development Commands

```bash
uv run python src/main.py
uv run ruff check src/
uv run ruff format src/
uv run pyright src/
```

## Initial Build Plan

1. Stabilize data pipeline (poll -> snapshot -> storage).
2. Add threaded polling with Qt-safe signal handoff.
3. Build first panel with smooth live telemetry rendering.
4. Add DVR timeline playback over persisted snapshots.

## Repository Hygiene

- Keep changes small and shippable.
- Keep changes surgical and meaningful; every commit should move the project forward.
- Be realistic about scope and constraints; prefer practical progress over speculative rewrites.
- Use first-principles execution order: question requirement, delete, simplify, accelerate, automate.
- Prefer deleting complexity over adding abstraction.
- Validate behavior with lightweight local smoke checks before pushing.
