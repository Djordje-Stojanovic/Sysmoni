# ARCHITECTURE.md -- Module Ownership and Parallel Development

> 4 engineers. Zero file overlap. Zero lock loops. Each owns a directory.

## Part 1 -- Current Product Modules

| Module | Purpose | Owned Paths | Status |
|---|---|---|---|
| Overhead | Process, prompts, shared docs | All root `*.md` files | Active |
| Telemetry (SENSOR) | Data collection and sensor pipeline | `src/telemetry/**` | C ABI DLL implemented |
| Render (RENDER) | Visual primitives and effects | `src/render/**` | C ABI DLL implemented |
| Shell (SHELL) | Window, panels, interaction | `src/shell/**` | Static lib + Qt6 app implemented |
| Platform (PLATFORM) | Runtime, store, packaging surface | `src/runtime/**` | C ABI DLL + CLI implemented |

## Part 2 -- 2-3 Year Work Balance

| Engineer | Module | 2-3 Year Work Focus | Workload |
|---|---|---|---|
| SENSOR | Telemetry | Sensors, process telemetry, smoothing, native hot paths | ~25% |
| RENDER | Render | Shader pipeline, widgets, compositor, 60 FPS discipline | ~25% |
| SHELL | Shell | Frameless shell, panel system, docking, UX interactions | ~25% |
| PLATFORM | Platform | DVR/store/runtime config, installer/update/licensing/tray | ~25% |

Overhead remains shared and lean.

## Part 3 -- Final Ownership Split (Strict)

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

## Build System -- Root Superbuild

A root `CMakeLists.txt` provides a single-command build for all modules:

```bash
# Configure (Qt6 optional -- omit Qt6_DIR to skip shell app)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
      -DQt6_DIR="C:/Qt/6.10.2/msvc2022_64/lib/cmake/Qt6"

# Build everything
cmake --build build --config Release

# Run all tests
ctest --test-dir build -C Release --output-on-failure
```

### Superbuild structure

The root CMakeLists.txt adds each native module as a subdirectory:

```
CMakeLists.txt (root superbuild)
  |-- src/runtime/native/    -> aura_platform (DLL) + aura (CLI)
  |-- src/telemetry/native/  -> aura_telemetry_native (DLL)
  |-- src/render/native/     -> aura_render_native (DLL)
  |-- src/shell/native/      -> aura_shell_core (static) + aura_shell (Qt6 app)
  |-- tests/                 -> test executables linked against the above
```

All build artifacts land in `build/bin/` so DLLs are co-located with executables
and tests can find them without manual copy steps.

### Qt6 handling

Qt6 is optional. When not found:
- SHELL app target (`aura_shell`) is not built; core library and tests still build.
- RENDER Qt widget hooks are disabled; pure-math and C ABI layer still builds.
- PLATFORM and SENSOR have no Qt dependency and always build.

### Individual module builds

Each module retains its own standalone CMakeLists.txt for isolated development.
See `CLAUDE.md` for per-module build commands.

## Target Directory Layout

```text
src/
|-- telemetry/
|   `-- native/
|       |-- include/         # C ABI header: aura_telemetry.h
|       `-- src/             # telemetry_engine.cpp, aura_telemetry_native.cpp
|-- render/
|   `-- native/
|       |-- include/         # C ABI header: aura_render.h
|       `-- src/             # c_api.cpp, math.cpp, theme.cpp, widgets.cpp, ...
|-- shell/
|   `-- native/
|       |-- include/         # bridge headers
|       |-- src/             # main.cpp, cockpit_controller.cpp, dock_model.cpp, ...
|       |-- qml/             # CockpitScene.qml
`-- runtime/
    `-- native/
        |-- include/         # C ABI header: aura_platform.h
        `-- src/             # c_api.cpp, config_win.cpp, store_sqlite.cpp, ...

tests/
|-- test_telemetry/native/   # test_telemetry_native.cpp
|-- test_render/             # render_native_tests.cpp
|-- test_shell/native/       # shell native bridge/controller tests
`-- test_platform/native/    # test_platform_native.cpp
```

## Cross-Module Dependencies

```
SHELL --> SENSOR   (reads telemetry snapshots via C ABI)
SHELL --> RENDER   (uses visual primitives via C ABI)
SHELL --> PLATFORM (reads config, calls runtime APIs)

SENSOR --> (system APIs only, no module deps)
RENDER --> (standalone, optional Qt6 for widget hooks)
PLATFORM --> (standalone, Windows APIs + SQLite)
```

The SHELL module includes headers from all three other modules via relative
paths in its `target_include_directories`. No circular dependencies exist.

## Interface Contracts

Cross-module type contracts are defined as C headers within each module's
`native/include/` directory. All boundaries use the C ABI:

- **SENSOR C ABI** (`aura_telemetry.h`): Opaque engine handle, snapshot structs,
  top-k process sampling. Consumers: SHELL (telemetry_bridge.cpp).
- **RENDER C ABI** (`aura_render.h`): Style tokens, math primitives, widget
  models (RadialGauge, SparkLine, Timeline, GlassPanel). Consumers: SHELL
  (render_bridge.cpp).
- **PLATFORM C ABI** (`aura_platform.h`): Runtime config resolution
  (CLI>env>TOML>auto), atomic file store with crash recovery, DVR with LTTB
  downsampling. Consumers: SHELL (timeline_bridge.cpp), CLI (runtime_cli.cpp).

### C ABI rules

- Opaque handles (pointers to forward-declared structs)
- No C++ exceptions across boundary; thread-local error reporting
- Fallback return values on error
- `extern "C"` linkage on all exported symbols

## Coordination Rules (No Lock Theater)

1. Each engineer edits only their directory.
2. Cross-module type changes are coordinated via `groupchat.md` requests.
3. `groupchat.md` is only for cross-module requests/replies/releases.
4. `codex_agents_logs.md` is append-only audit.
5. No global task queue. Each engineer chooses highest-leverage next task in their module.
