# coding_guideliines.md

Mandatory execution checklist for every Codex session.

## Read Order

1. `ai.md`
2. `AGENTS.MD`
3. `ARCHITECTURE.md`
4. `DISPATCH.md`
5. `PARALLEL_OPS.md`

## Session Rules

1. Pick a unique `codex_name` (`[a-z0-9_]`, 6-30 chars).
2. Append `START` in `codex_agents_logs.md` with timestamp and module scope.
3. Post one `type:info` line in `groupchat.md`.
4. Edit only your owned module path.
5. Do not use lock-claim process for unrelated paths. Directory ownership is the conflict prevention model.
6. For `src/contracts/**` changes: post `type:request` to `to:user` in `groupchat.md` and wait approval.
7. If blocked by another module, continue with placeholder in your module, do not idle.

## Commit Rules

1. One concern per commit.
2. Commit message format: `<type>: <what and why>`.
3. Allowed types: `feat`, `fix`, `refactor`, `chore`, `docs`.
4. After commit append:
   - `GIT-1`
   - `GIT-2`
   - `GIT-3`
   - optional `RISK`
   - `END`
5. End every session with `git push origin main`.

## Verification Rules

Run relevant checks for changed scope.

For native C++ modules (preferred baseline):

```powershell
cmake -S <module_source> -B <build_dir> -G "Visual Studio 17 2022" -A x64
cmake --build <build_dir> --config Release
ctest --test-dir <build_dir> -C Release --output-on-failure
```

For transitional Python modules:

```bash
uv run pytest tests/ -x
uv run ruff check src/ tests/
uv run pyright src/
```

If a command cannot run, record exactly why in `RISK`.

## Message Format

`groupchat.md` lines must use:

`MSG | <timestamp> | <agent> | to:<agent|all|user> | type:<request|ack|info|handoff|release> | locks:<paths|none> | note:<one-line actionable message>`
