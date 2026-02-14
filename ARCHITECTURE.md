# ARCHITECTURE.md - Module Ownership and Parallel Development

> 4 engineers. Zero file overlap. Zero lock loops. Each owns a directory.

## Part 1 - Current Product Modules

| Module | Purpose | Owned Paths |
|---|---|---|
| Overhead | Process, prompts, shared docs | All root `*.md` files |
| Telemetry | Data collection and sensor pipeline | `src/telemetry/**` |
| Render | Visual primitives and effects | `src/render/**` |
| Shell | Window, panels, interaction | `src/shell/**` |
| Platform | Runtime, store, packaging surface | `src/runtime/**` |

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
- Code: `src/runtime/**`
- Tests: `tests/test_platform/**`

## C++ Rewrite Baseline

- Product direction is now native C++.
- Platform runtime has been cut over to native C++ (`src/runtime/native/**`).
- Platform test suite is native C++ (`tests/test_platform/native/**`).
- Build standard: CMake + Visual Studio 2022 generator on Windows (`-A x64`).

## Target Directory Layout

```text
src/
|-- telemetry/
|   `-- native/
|-- render/
|   `-- native/
|-- shell/
|   `-- native/
`-- runtime/
    `-- native/
        |-- include/
        `-- src/

tests/
|-- test_telemetry/
|-- test_render/
|-- test_shell/
`-- test_platform/
    `-- native/
```

## Interface Contracts

Cross-module type contracts are defined as C++ headers within each module's `native/include/` directory. Coordination happens through `groupchat.md`.

- SENSOR -> SHELL: telemetry snapshots via C ABI (`telemetry_engine.h`)
- RENDER -> SHELL: visual primitives via C ABI (`aura_render.h`)
- SENSOR -> PLATFORM: persistent telemetry stream to DVR/store
- SHELL -> PLATFORM: runtime config, window state, platform calls

## Coordination Rules (No Lock Theater)

1. Each engineer edits only their directory.
2. Cross-module type changes are coordinated via `groupchat.md` requests.
3. `groupchat.md` is only for cross-module requests/replies/releases.
4. `codex_agents_logs.md` is append-only audit.
5. No global task queue. Each engineer chooses highest-leverage next task in their module.
