# coding_guideliines.md

Execution checklist for every Codex session.

1. Read `ai.md` and `AGENTS.md` first.
2. Use a unique `codex_name` (`[a-z0-9_]`, 6-30 chars).
3. Before edits, append `START` and `LOCKS` in `codex_agents_logs.md`.
4. Announce claim in `groupchat.md` with one `MSG | ... | type:info` line.
5. Edit only your owned module paths unless user approved cross-module scope.
6. Re-check `codex_agents_logs.md` + `groupchat.md` before:
   - claiming new paths
   - editing a new file
   - `git add`
   - `git commit`
7. If shared contract work is needed (`src/contracts/**`), post `type:request` and wait for approval.
8. Keep changes small and measurable (bug fix, capability, reliability, clarity).
9. One concern per commit, commit format: `<type>: <what and why>`.
10. Allowed commit types: `feat`, `fix`, `refactor`, `chore`, `docs`.
11. Each engineer writes their own tests for their module (no separate QA lane).
12. Run relevant verification commands; log results.
13. After commit, append:
    - `GIT-1`
    - `GIT-2`
    - `GIT-3`
    - optional `RISK`
    - `END`
14. Post `type:release` in `groupchat.md` with freed paths.
15. End session with `git push origin main`.
16. Never rewrite or force-push shared history.
17. Keep first-principles execution order: question requirement -> delete -> simplify -> accelerate -> automate.
