# AGENTS.md - First-Principles Parallel Delivery

> Keep what works. Delete what does not move the product.

## PRIME DIRECTIVE

Ship working code fast without breaking existing behavior.
Every decision follows this order: question requirement -> delete -> simplify -> accelerate -> automate.

## MANDATORY FILES

- `ai.md` (core system prompt and engineering ethos)
- `coding_guideliines.md` (execution rules/checklist)
- `ARCHITECTURE.md` (module ownership and split)
- `DISPATCH.md` (launch prompts by team size)
- `PARALLEL_OPS.md` (parallel run protocol)
- `codex_agents_logs.md` (START/SCOPE/GIT-1/GIT-2/GIT-3/END audit)
- `groupchat.md` (cross-agent request/release channel)

## MODULE OWNERSHIP (DEFAULT)

- `Overhead`: all `*.md` docs/process policy files
- `Contracts`: `src/contracts/**`
- `Telemetry`: `src/telemetry/**`
- `Runtime Platform`: `src/runtime/**`
- `Shell GUI`: `src/shell/**`
- `Compatibility`: `src/main.py`, `src/core/**`, `src/gui/**`
- Tests mirror module ownership under `tests/`

No agent edits another module path unless the user explicitly approves.

## SHARED-CHANGE RULE

`src/contracts/**` is shared API surface.
Treat it as protected: request change in `groupchat.md`, wait for user approval, then apply.

## THE ALGORITHM

1. **Question requirement**: confirm user value; remove assumptions.
2. **Delete**: remove unnecessary parts first.
3. **Simplify**: fewer files, fewer branches, fewer abstractions.
4. **Accelerate**: optimize only after design is minimal.
5. **Automate**: automate only stable manual flow.

## EXECUTION WORKFLOW

### Start

1. Read mandatory files.
2. Pick one owned task.
3. Append `START` + `LOCKS` in `codex_agents_logs.md`.
4. Post one `type:info` line in `groupchat.md`.

### Build

- Make small surgical changes.
- Keep one concern per commit.
- Re-check `codex_agents_logs.md` and `groupchat.md` before touching any new path.

### Finish

1. Run relevant verification commands.
2. Commit and push.
3. Append `GIT-1`, `GIT-2`, `GIT-3`, `END` in `codex_agents_logs.md`.
4. Post `type:release` in `groupchat.md` with freed paths.

## LOG SCHEMA

`codex_agents_logs.md`:
- `START | <timestamp+tz> | <codename> | task: <summary>`
- `LOCKS | folder: <path> | files: <paths>`
- optional `SCOPE | ...`
- `GIT-1 | <type>: <what and why>`
- `GIT-2 | files: <changed files>`
- `GIT-3 | verify: <commands + result>`
- optional `RISK | <short risk/blocker>`
- `END | <timestamp+tz> | <codename> | free to work: <paths> | commit: <message>`

`groupchat.md`:
- `MSG | <timestamp> | <agent> | to:<agent|all|user> | type:<request|ack|info|handoff|release> | locks:<paths|none> | note:<one line>`

## STABILITY (NON-NEGOTIABLE)

```bash
git add -A && git commit -m "<type>: <what and why>" && git push origin main
```

- No force push.
- No history rewrites on shared branch.
- Do not leave session without commit + push unless blocked by user decision.

## CODING PRINCIPLES

- Working + simple beats clever + fragile.
- Avoid future-proof abstractions until repeated need exists.
- Handle errors at boundaries.
- Keep files focused; split if responsibility becomes mixed.
- Search before building new functionality.
- Each engineer owns their own tests (no separate QA lane).

## PROJECT CONTEXT

Aura is a local-only, premium desktop system monitor.
Primary goals:
- smooth real-time telemetry UX
- low idle overhead
- shippable reliability on Windows

Current structure:

```
src/
|-- contracts/
|-- telemetry/
|-- runtime/
|-- shell/
|-- core/   (compatibility shims)
|-- gui/    (compatibility shims)
`-- main.py (compatibility entrypoint)
```

## COMMANDS

```bash
dev:       uv run python src/main.py
test:      uv run pytest tests/ -x
lint:      uv run ruff check src/ tests/
format:    uv run ruff format src/ tests/
typecheck: uv run pyright src/
deps:      uv sync
profile:   uv run python -m cProfile -o aura.prof src/main.py
```

## ANTI-PATTERNS

- Global "pick next important task" without module boundary
- Cross-module edits without approval
- Waiting idle on another agent when placeholder work is possible
- Large speculative rewrites
- Extra process that does not increase shipped value
