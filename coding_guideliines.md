# coding_guideliines.md

Mandatory coordination rules for all Codex agents.
1. Read this file and `AGENTS.md` before starting any task.
2. Choose a random `codex_name` for this session.
3. `codex_name` length must be 6-30 characters.
4. Use only lowercase letters, numbers, and `_`.
5. Keep names unique in `codex_agents_logs.md`.
6. Before edits, add a `START` entry in `codex_agents_logs.md`.
7. START entry must include timestamp with timezone.
8. START entry must include a one-line task summary.
9. START entry must include working folder.
10. START entry must list exact files/folders you claim.
11. Do not edit files claimed by another active agent.
12. If scope changes, append a `SCOPE` entry first.
13. Keep claims minimal and release paths when done.
14. For every task, log a 3-line git summary.
15. `GIT-1`: commit type and concise intent.
16. `GIT-2`: files changed.
17. `GIT-3`: verification commands and result.
18. Every task must end with a git commit by that same agent.
19. Commit format: `<type>: <what and why>`.
20. Allowed types: `feat`, `fix`, `refactor`, `chore`, `docs`.
21. Keep one concern per commit.
22. Never amend or rewrite another agent's commit.
23. Never force push shared branches.
24. After commit, append an `END` entry.
25. END entry must list files/folders now free to work.
26. END entry should include commit id or commit message.
27. Record blockers, skipped checks, and risks in one short line.
28. If tooling is missing, log the exact missing command.
29. If no product code changed, commit docs/log updates anyway.
30. Do not delete prior log entries.
31. Keep log lines factual and short.
32. Example:
33. `START | 2026-02-12 21:44:42 +01:00 | codex_example | lock: AGENTS.md`
34. `GIT-1 | docs: add agent coordination rules`
35. `GIT-2 | files: AGENTS.md, coding_guideliines.md, codex_agents_logs.md`
36. `GIT-3 | verify: git status, manual review`
37. `groupchat.md` is the shared timestamped channel for direct agent-to-agent coordination.
38. Use `groupchat.md` for lock handoffs, clarifications, and release confirmations between parallel agents.
39. `coding_guideliines.md` and `AGENTS.md` are protected files; do not edit them unless the user explicitly requests it.
40. By default, agents may edit only product code plus `groupchat.md` and `codex_agents_logs.md`.
41. `END | free: AGENTS.md, coding_guideliines.md, codex_agents_logs.md`
42. Lock discipline is critical: re-check `codex_agents_logs.md` and `groupchat.md` before claiming locks, before each new file edit, before `git add`, and immediately before `git commit`.
43. Never take over another agent's claimed file/path without explicit user approval in the current session.
44. If a lock appears stale, send a `groupchat.md` handoff/request first, then wait for user approval before touching that path.
45. If conflicting lock claims appear, stop editing immediately, log the conflict in `groupchat.md`, and ask the user to resolve ownership.
46. All `groupchat.md` messages must follow the exact schema and start with `MSG |`; no free-form `NOTE` lines.
47. Allowed `groupchat.md` message types are `request`, `ack`, `info`, `handoff`, and `release`.
48. After each commit, release all claimed paths in both `codex_agents_logs.md` (`END`) and `groupchat.md` (`type:release`) in the same session.
49. If a lock protocol violation occurred, record it explicitly in `RISK` before `END`.
50. Make small, surgical changes by default; avoid broad refactors unless explicitly requested.
51. Each change must be meaningful and advance the project (bug fix, user value, reliability, or clarity).
52. Be realistic about constraints and scope; ship the simplest viable improvement first.
53. Apply Elon-first-principles order on execution decisions: question requirement, delete, simplify, accelerate, automate.
54. Do not add or expand CLI user-facing features (new flags/commands/modes) unless the user explicitly requested CLI work in the current task.
