# ARCHITECTURE.md

## Pass 1 - Current-State Modules (What Exists Today)

| Module | Purpose | Files |
|---|---|---|
| Overhead | Agent/process/docs/governance | `AGENTS.md`, `ai.md`, `coding_guideliines.md`, `groupchat.md`, `codex_agents_logs.md`, `README.md`, `RUN_WINDOWS11.md`, other `*.md` |
| Contracts | Shared telemetry data types | `src/contracts/types.py`, `src/contracts/__init__.py` |
| Telemetry | Live system data collection | `src/telemetry/poller.py`, `src/telemetry/__init__.py` |
| Runtime Platform | Persistence + CLI runtime services | `src/runtime/main.py`, `src/runtime/store.py`, `src/runtime/__init__.py` |
| Shell GUI | Live desktop window/app shell | `src/shell/window.py`, `src/shell/__init__.py` |
| Compatibility | Legacy import shims (temporary) | `src/main.py`, `src/core/*.py`, `src/gui/*.py` |
| Tests | Module-split test ownership | `tests/test_contracts/*`, `tests/test_telemetry/*`, `tests/test_platform/*`, `tests/test_shell/*` |

## Pass 2 - 2-3 Year Balanced Product Modules

| Module | 2-3 Year Ownership Scope | Approx Workload |
|---|---|---|
| Telemetry Engine | CPU/GPU/disk/network/thermal/battery/process probes, smoothing, native hot paths | 20% |
| DVR + Data Runtime | SQLite DVR, retention/query engine, replay API, export, reliability | 20% |
| Render Engine | Custom drawing primitives, shaders, animation/interpolation, frame budget pipeline | 20% |
| Shell + Panels | Frameless window, panel system, docking/interaction, command UX | 20% |
| Platform + Distribution | Packaging, install/update, licensing, startup/tray, crash handling | 20% |

Overhead is a coordination layer, not a full-time product module.

## Pass 3 - Final Owner Split for 5 Parallel Codex Sessions

This split is designed to run 5 sessions in parallel with near-zero overlap.

1. `ATLAS_SENSOR` (Telemetry Owner)
Files: `src/telemetry/**`, `tests/test_telemetry/**`

2. `NOVA_REPLAY` (DVR/Data Runtime Owner)
Files: `src/runtime/store.py`, future `src/runtime/dvr*.py`, `tests/test_platform/test_store.py`

3. `ORION_SHELL` (Window + Panels Owner)
Files: `src/shell/**`, `tests/test_shell/**`

4. `VEGA_RENDER` (Render/Shader Owner)
Files: future `src/render/**`, `tests/test_render/**`

5. `FORGE_PLATFORM` (CLI/Install/Ship Owner)
Files: `src/runtime/main.py`, future `installer/**`, release scripts, `tests/test_platform/test_main.py`

Shared and user-approved only:
- `src/contracts/**`
- Overhead docs (`*.md`) when policy/process changes are requested

## Migration Map (Completed)

| Old Path | New Path |
|---|---|
| `src/core/types.py` | `src/contracts/types.py` |
| `src/core/poller.py` | `src/telemetry/poller.py` |
| `src/core/store.py` | `src/runtime/store.py` |
| `src/main.py` | `src/runtime/main.py` (+ thin compatibility wrapper kept at `src/main.py`) |
| `src/gui/window.py` | `src/shell/window.py` (+ thin compatibility wrapper kept at `src/gui/window.py`) |
| `tests/test_types.py` | `tests/test_contracts/test_types.py` |
| `tests/test_poller.py` | `tests/test_telemetry/test_poller.py` |
| `tests/test_store.py` | `tests/test_platform/test_store.py` |
| `tests/test_main.py` | `tests/test_platform/test_main.py` |
| `tests/test_gui_window.py` | `tests/test_shell/test_window.py` |

Note:
- Platform code is in `src/runtime/` (not `src/platform/`) to avoid collisions with Python's stdlib `platform` module.
