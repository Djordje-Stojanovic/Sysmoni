# PARALLEL_OPS.md

## Goal

Run 5 Codex sessions in parallel without token-wasting lock loops.

## Why Loops Happened

- Agents were selecting from one shared "most important task" pool.
- File-level locking forced repeated re-planning.
- Agents blocked on each other for small cross-part dependencies.

## New System

1. Fixed module ownership (directory-level).
2. Fixed codename per module.
3. Fixed priority queue per module.
4. Cross-module dependencies requested through `groupchat.md`, not by editing foreign files.
5. All sessions commit+push independently.

## Launch Sequence

1. Start one migration/stability session when structure changes are needed.
2. Launch 5 parallel sessions using prompts from `DISPATCH.md`.
3. Keep each session in its own module forever.

## Runtime Rules

1. No agent may choose work from another module backlog.
2. No agent may lock whole repo after startup.
3. If a task needs contract change:
   - Post request in `groupchat.md`.
   - Continue with local placeholder/stub.
4. Re-check `codex_agents_logs.md` + `groupchat.md` before commit.

## Anti-Collision Protocol

- Ownership key is path, not task description.
- Priority tie-breaker is local queue order, never "global importance".
- If two agents ask for the same shared contract change, merge request at user level once.

## Daily Operating Cadence (Low Overhead)

1. Start-of-day:
   - Quick scan of `groupchat.md` requests.
   - Confirm module assignments.
2. During day:
   - Agents run independently in their lanes.
3. End-of-day:
   - Verify each active agent produced commit+push+END lines.

## Success Criteria

- Parallel sessions do not touch same files.
- Zero stale-lock takeovers.
- Token usage goes into shipped code, not lock negotiation.
