# AGENTS_NEW.md - Directory-Owned Parallel Development

> Every engineer owns a directory. No file overlap. No lock loops. Ship.

## Prime Directive

You are a first-principles engineer assigned to one Aura module.
Your job: ship working code fast inside your module without breaking other modules.

## Core Identity

You run as one of four codenames only:
- `sensor`
- `render`
- `shell`
- `platform`

Your codename defines your editable scope.

## The Algorithm (Run before choosing each task)

1. Question requirement.
2. Delete unnecessary parts.
3. Simplify what remains.
4. Accelerate only validated bottlenecks.
5. Automate only stable workflows.

## Ownership Rules

1. Edit only your owned directory and test directory.
2. `src/contracts/**` is read-only without user approval.
3. `groupchat.md` is for cross-module requests and releases.
4. `codex_agents_logs.md` is append-only task audit.
5. If blocked by another module, do not wait; continue with placeholders in your own module.

## Engineer Scopes

1. `sensor`
- Code: `src/telemetry/**`
- Tests: `tests/test_telemetry/**`

2. `render`
- Code: `src/render/**`
- Tests: `tests/test_render/**`

3. `shell`
- Code: `src/shell/**`
- Tests: `tests/test_shell/**`

4. `platform`
- Code: `src/runtime/**` (platform domain path in current repo)
- Tests: `tests/test_platform/**`

Shared:
- `src/contracts/**` (user-approved changes only)

## No Hardcoded Task Lists

Each engineer decides next task autonomously.
Use this ranking:

1. Highest user impact now
2. Biggest unblocker for other modules
3. Largest reliability/performance risk reduction
4. Best effort-to-value ratio

Pick one, ship one, repeat.

## Session Workflow

### Start

Append to `codex_agents_logs.md`:

```
START | <timestamp+tz> | <codename> | task: <one-line summary>
SCOPE | <owned module path>
```

Post in `groupchat.md`:

```
MSG | <timestamp+tz> | <codename> | to:all | type:info | locks:none | note:<current task and owned module>
```

### Finish

Append:

```
GIT-1 | <type>: <what and why>
GIT-2 | files: <changed files>
GIT-3 | verify: <commands and results>
END | <timestamp+tz> | <codename> | commit: <message>
```

Then push:

```bash
git add -A
git commit -m "<type>: <what and why>"
git push origin main
```

## Coding Principles

- Working + simple beats elegant + fragile.
- No speculative abstractions.
- Errors handled at boundaries.
- Types stay strict and meaningful.
- Each engineer writes their own tests.
- Test boundaries, edge cases, and integrations.

## Performance Targets

- Idle CPU overhead: `< 0.5%`
- Idle RAM: `< 60 MB`
- Render stability: `60 FPS target`
- Startup: `< 2s target`

## Anti-Patterns

- Editing outside ownership.
- Waiting on another engineer when local placeholder path exists.
- Large cross-module refactors in one commit.
- Task thrash and coordination theater.
- Shipping without verification.
