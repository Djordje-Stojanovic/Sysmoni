# PARALLEL_OPS.md - Run 4 Codex Sessions Without Collision

## Problem

Old flow caused token burn:
- agents picked from same global priority pool
- file lock contention forced loops
- agents re-planned instead of shipping

## Fix

Directory ownership model:
- SENSOR -> `src/telemetry/**`
- RENDER -> `src/render/**`
- SHELL -> `src/shell/**`
- PLATFORM -> `src/runtime/**` (platform domain in this repo)

No overlap. No lock loops.

## C++ Rewrite Note

Aura is now native C++ direction.

- Use CMake + CTest as baseline verification for native modules.
- Prefer `Visual Studio 17 2022` generator (`-A x64`) on Windows.
- Use `.\aura.cmd --json --no-persist` and `.\aura.cmd --gui` for user-facing smoke checks.

## One-Time Migration Prompt

Use this exact launch prompt once when structure changes are needed:

```text
Restructure the codebase per ARCHITECTURE.md migration table. Move files, update all imports, verify tests pass. Commit and push. Do NOT start any feature work.
```

## Launch Steps

1. Open 4 Codex sessions.
2. Paste matching block from `DISPATCH.md` into each.
3. Let each engineer choose next highest-leverage task inside their own module.

## Cross-Module Coordination

Only through `groupchat.md`:

```text
MSG | <timestamp> | <agent> | to:user | type:request | locks:none | note:<contract or dependency request>
```

Then continue local placeholder work. Do not block.

## Why This Works

- Ownership is path-based, not opinion-based.
- Engineers do not compete for same files.
- Task autonomy is local to module and goal-driven.
- Coordination cost drops; shipping rate rises.

## Operating Cadence

1. Start of day:
   - scan `groupchat.md` requests
   - confirm four active codenames
2. During day:
   - each session ships independently
3. End of day:
   - verify each active session has commit + push + END entry

## Success Criteria

- 4 sessions run in parallel without collisions
- no stale-lock takeover flow
- most tokens convert into shipped code, not coordination overhead
