# DISPATCH.md

## Master Launch Line

Use this at the top of every agent prompt:

`Read ai.md, AGENTS.md, coding_guideliines.md, ARCHITECTURE.md first. You own only your module path. Execute, test, commit, push.`

## Team Size Modes

### 1 Engineer Mode (Founder Sprint)

Prompt name: `SOLO_OPERATOR`

Scope:
- `src/**`
- `tests/**`
- Overhead docs only when needed

Rule:
- Work in this order: telemetry -> runtime store -> shell -> render scaffolding -> platform shipping.

### 2 Engineer Mode (Lean Parallel)

1. `CORE_OPERATOR`
Scope: `src/contracts/**`, `src/telemetry/**`, `src/runtime/**`, `tests/test_contracts/**`, `tests/test_telemetry/**`, `tests/test_platform/**`

2. `SHELL_OPERATOR`
Scope: `src/shell/**`, future `src/render/**`, `tests/test_shell/**`, future `tests/test_render/**`

### 5 Engineer Mode (Recommended)

1. `ATLAS_SENSOR`
Scope: `src/telemetry/**`, `tests/test_telemetry/**`
Priority: sensors, process telemetry, sampling performance.

2. `NOVA_REPLAY`
Scope: `src/runtime/store.py`, future `src/runtime/dvr*.py`, `tests/test_platform/test_store.py`
Priority: DVR correctness, retention, replay/query stability.

3. `ORION_SHELL`
Scope: `src/shell/**`, `tests/test_shell/**`
Priority: window shell, panel framework, interaction flow.

4. `VEGA_RENDER`
Scope: future `src/render/**`, future `tests/test_render/**`
Priority: custom widgets, shader pipeline, frame budget.

5. `FORGE_PLATFORM`
Scope: `src/runtime/main.py`, future installer/release files, `tests/test_platform/test_main.py`
Priority: runtime UX, packaging, startup/update/licensing path.

### 10 Engineer Mode (Scale-Out)

Split each module into two non-overlapping lanes:

1. `sensor_probe` -> low-level probes (`src/telemetry/probes/**`)
2. `sensor_pipeline` -> smoothing/buffers (`src/telemetry/pipeline/**`)
3. `replay_store` -> SQLite schema/query (`src/runtime/store.py`, `src/runtime/db/**`)
4. `replay_api` -> replay/export surfaces (`src/runtime/dvr*.py`)
5. `shell_window` -> window/docking (`src/shell/window.py`, `src/shell/dock/**`)
6. `shell_panels` -> panel implementations (`src/shell/panels/**`)
7. `render_widgets` -> graph/orb/gauge (`src/render/widgets/**`)
8. `render_effects` -> shaders/compositor (`src/render/shaders/**`, `src/render/compositor.py`)
9. `platform_ship` -> installer/update (`installer/**`, release scripts)
10. `platform_runtime` -> CLI/config/tray/licensing (`src/runtime/main.py`, `src/runtime/config*.py`)

For 20+ agents:
- Clone only inside existing module lanes.
- Never create cross-lane ownership.
- Keep one owner per concrete file path.

## Fixed Prompt Template (Per Engineer)

Copy and fill:

```
You are <CODENAME>.
Read ai.md + AGENTS.md + coding_guideliines.md + ARCHITECTURE.md.

You may edit only:
- <owned paths>
- codex_agents_logs.md (append-only)
- groupchat.md (append-only)

You may not edit:
- Other module paths
- src/contracts/** without user approval

Execution loop:
1) Pick highest-priority unfinished task in your module.
2) Ship a small change.
3) Run relevant tests.
4) Commit and push.
5) Log START/GIT-1/GIT-2/GIT-3/END.

If blocked by another module:
- Post `MSG | ... | type:request` in groupchat.md
- Continue with placeholder work in your own module.
```
