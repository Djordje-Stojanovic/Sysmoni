# Aura / Sysmoni — Native Windows System Monitor

## What This Is
A native C++ system monitor for Windows with a Qt6 cockpit UI. Collects CPU, memory, disk, and network telemetry. Displays real-time and DVR (recorded timeline) data through a frame-disciplined rendering pipeline.

## Tech Stack
- **Language**: C++20
- **Compiler**: Visual Studio 2022 Build Tools (MSVC)
- **Build**: CMake with "Visual Studio 17 2022" generator, -A x64
- **UI**: Qt6 (6.10.2 msvc2022_64) — Widgets + QML
- **Tests**: CTest for native test suites
- **Platform**: Windows-first (GetSystemTimes, GlobalMemoryStatusEx, MoveFileEx)
- **VCS**: Git + GitHub, branch-based workflow with PR reviews

## Architecture — 4 Native Modules

Each module has a **C ABI boundary** (opaque handles, no exceptions across boundary, thread-local error reporting).

| Module | Directory | Responsibility |
|--------|-----------|----------------|
| **SENSOR** | `src/telemetry/native/` | CPU, memory, disk, network telemetry collection. Top-k process sampling. |
| **RENDER** | `src/render/native/` | Frame compositor, style tokens, math primitives, widget models (RadialGauge, SparkLine, Timeline, GlassPanel). Qt renderer hooks. |
| **SHELL** | `src/shell/native/` | Qt6 frameless window, docking system (L/C/R), 4 panels, QML cockpit scene, dynamic DLL bridge loading. |
| **PLATFORM** | `src/runtime/native/` | Runtime config (CLI>env>TOML>auto), atomic file store with crash recovery, DVR with LTTB downsampling, system snapshot collection. |

## Agent Ownership

| Agent | Modules | Directories |
|-------|---------|-------------|
| **Frontend** | RENDER + SHELL | `src/render/native/`, `src/shell/native/`, `qml/`, `tests/test_render/`, `tests/test_shell/` |
| **Backend** | SENSOR + PLATFORM | `src/telemetry/native/`, `src/runtime/native/`, `tests/test_telemetry/`, `tests/test_platform/` |
| **Architect** | Infra + Docs | `CLAUDE.md`, `ARCHITECTURE.md`, `AGENTS.md`, `installer/`, launchers, CMake root, `.github/` |
| **Master** | Coordination | GitHub issues, roadmap, feature planning |
| **Reviewer** | Quality gate | PR reviews, approvals, merges |

### Critical Rule: NO CROSS-MODULE FILE EDITS
An agent may ONLY modify files in its owned directories. If an agent needs a change in another module, it creates a GitHub issue or communicates through a PR comment. Violation of this rule causes merge conflicts and breaks parallel work.

## Git Workflow

### Branch Naming
```
feature/<module>-<description>   # new functionality
fix/<module>-<description>       # bug fixes
refactor/<module>-<description>  # code improvements
chore/<description>              # build, docs, infrastructure
docs/<description>               # documentation only
```

### Commit Messages (Conventional Commits)
```
feat(sensor): add GPU temperature collector
fix(render): clamp delta to prevent frame skip
refactor(platform): simplify config resolution
chore: update CMake minimum version
docs: update architecture diagram
```

### Workflow for Implementation Agents (frontend, backend)
1. `git checkout -b feature/<module>-<description>`
2. Implement changes (only in your owned files)
3. Build and run tests (must pass)
4. `git add . && git commit -m "feat(<module>): description"`
5. `git push origin feature/<module>-<description>`
6. `gh pr create --title "feat(<module>): description" --body "what and why"`
7. Wait for reviewer approval before starting next task

### Workflow for Reviewer
1. `gh pr list` to see open PRs
2. `gh pr diff <number>` to review changes
3. Check: module boundary respected? Tests pass? C ABI safe?
4. `gh pr review <number> --approve` or `--request-changes --body "reason"`
5. `gh pr merge <number> --squash --delete-branch`

### NEVER push directly to main. ALL changes go through PRs.

## Build & Test Commands

```powershell
# PLATFORM
cmake -S src/runtime/native -B build/platform-native -G "Visual Studio 17 2022" -A x64
cmake --build build/platform-native --config Release
ctest --test-dir build/platform-native-tests -C Release --output-on-failure

# RENDER
cmake -S tests/test_render -B build/render-native-tests -G "Visual Studio 17 2022" -A x64
cmake --build build/render-native-tests --config Release
ctest --test-dir build/render-native-tests -C Release --output-on-failure

# SHELL (Qt6 required)
cmake -S src/shell/native -B src/shell/native/build -G "Visual Studio 17 2022" -A x64
cmake --build src/shell/native/build --config Release
ctest --test-dir src/shell/native/build -C Release --output-on-failure

# TELEMETRY
cmake -S tests/test_telemetry -B tests/test_telemetry/build -G "Visual Studio 17 2022" -A x64
cmake --build tests/test_telemetry/build --config Release
ctest --test-dir tests/test_telemetry/build -C Release --output-on-failure
```

## Qt6 Setup (one-time)
```powershell
uvx --from aqtinstall aqt.exe install-qt -O C:\Qt windows desktop 6.10.2 win64_msvc2022_64
```

## GUI Build
```powershell
powershell -ExecutionPolicy Bypass -File installer/windows/build_shell_native.ps1 -Qt6Dir "C:\Qt\6.10.2\msvc2022_64\lib\cmake\Qt6"
```

## One-Command Run
```powershell
.\aura.cmd --json --no-persist   # CLI mode
.\aura.cmd --gui                  # GUI mode
```

## Technical Standards
- **C ABI**: Opaque handles, no exceptions across boundary, fallback return values, thread-local error reporting
- **Atomic persistence**: Temp file write → MoveFileEx REPLACE_EXISTING + WRITE_THROUGH
- **Crash recovery**: Startup stale-temp cleanup, corrupt line tolerance
- **Frame discipline**: clamp_delta_seconds, max_catchup_frames, next_frame_delay
- **Graceful degradation**: Telemetry unavailable → zeroed snapshots; Render unavailable → fallback formatting
