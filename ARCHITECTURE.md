# ARCHITECTURE.md - Module Ownership and Parallel Development

> 4 engineers. Zero file overlap. Zero lock loops. Each owns a directory.

## Part 1 - Current Product Modules

| Module | Purpose | Owned Paths |
|---|---|---|
| Overhead | Process, prompts, shared docs | All root `*.md` files |
| Contracts | Shared dataclasses and interfaces | `src/contracts/**` |
| Telemetry | Data collection and pipeline | `src/telemetry/**` |
| Render | Visual primitives and effects | `src/render/**` |
| Shell | Window, panels, interaction | `src/shell/**` |
| Platform | Runtime, store, shipping surface | `src/runtime/**` |

## Part 2 - 2-3 Year Work Balance

| Engineer | Module | 2-3 Year Work Focus | Workload |
|---|---|---|---|
| SENSOR | Telemetry | Sensors, process telemetry, smoothing, native hot paths | ~25% |
| RENDER | Render | Shader pipeline, widgets, compositor, 60 FPS discipline | ~25% |
| SHELL | Shell | Frameless shell, panel system, docking, UX interactions | ~25% |
| PLATFORM | Platform | DVR/store/runtime config, installer/update/licensing/tray | ~25% |

Overhead remains shared and lean.

## Part 3 - Final Ownership Split (Strict)

1. **SENSOR**
- Code: `src/telemetry/**`
- Tests: `tests/test_telemetry/**`

2. **RENDER**
- Code: `src/render/**`
- Tests: `tests/test_render/**`

3. **SHELL**
- Code: `src/shell/**`
- Tests: `tests/test_shell/**`

4. **PLATFORM**
- Code: `src/runtime/**` (platform domain path in this repo)
- Tests: `tests/test_platform/**`

Shared protected surface:
- `src/contracts/**` (user approval required for changes)

## Target Directory Layout

```text
src/
|-- contracts/
|   |-- types.py
|   |-- signals.py
|   `-- constants.py
|-- telemetry/
|   |-- poller.py
|   |-- gpu.py
|   |-- disk.py
|   |-- network.py
|   |-- thermal.py
|   |-- battery.py
|   |-- processes.py
|   |-- pipeline.py
|   |-- wmi.py
|   `-- native/
|-- render/
|   |-- engine.py
|   |-- theme.py
|   |-- animation.py
|   |-- compositor.py
|   |-- shaders/
|   `-- widgets/
|-- shell/
|   |-- app.py
|   |-- window.py
|   |-- titlebar.py
|   |-- dock.py
|   |-- shortcuts.py
|   `-- panels/
`-- platform/   (logical module; implemented as src/runtime/ in this repo)
    |-- main.py
    |-- store.py
    |-- dvr.py
    |-- config.py
    |-- tray.py
    |-- autostart.py
    |-- updater.py
    |-- licensing.py
    |-- crash.py
    `-- installer/
```

## Migration Table

| Current File | Module Owner | New Location |
|---|---|---|
| `src/contracts/types.py` | Contracts | unchanged |
| `src/telemetry/poller.py` | SENSOR | unchanged |
| `src/shell/window.py` | SHELL | unchanged |
| `src/runtime/store.py` | PLATFORM | unchanged (platform domain path) |
| `src/runtime/main.py` | PLATFORM | unchanged (platform domain path) |
| `tests/test_telemetry/test_poller.py` | SENSOR | unchanged |
| `tests/test_telemetry/test_types.py` | SENSOR | unchanged |
| `tests/test_shell/test_window.py` | SHELL | unchanged |
| `tests/test_platform/test_store.py` | PLATFORM | unchanged |
| `tests/test_platform/test_main.py` | PLATFORM | unchanged |

## Interface Contracts

- SENSOR -> SHELL: sensor snapshots and process samples from `src/contracts/types.py`
- RENDER -> SHELL: widgets/theme/compositor interfaces
- SENSOR -> PLATFORM: persistent telemetry stream to DVR/store
- SHELL -> PLATFORM: config, window state, tray/platform calls

## Coordination Rules (No Lock Theater)

1. Each engineer edits only their directory.
2. `src/contracts/**` is read-only until user-approved request.
3. `groupchat.md` is only for cross-module requests/replies/releases.
4. `codex_agents_logs.md` is append-only audit.
5. No global task queue. Each engineer chooses highest-leverage next task in their module.
