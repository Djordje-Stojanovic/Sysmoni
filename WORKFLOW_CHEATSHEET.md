# Aura Multi-Agent Workflow Cheat Sheet

## Your Old Workflow (what you're replacing)
```
4 terminals in VSCode → paste system prompts → agents commit → push to main
```

## Your New Workflow

### Option A: Manual Worktree Sessions (Start Here — Simpler)

Open 5 terminals in VSCode or Windows Terminal. Each one runs Claude Code in a different worktree.

```powershell
# Terminal 1 — FRONTEND agent
cd C:\AI\Sysmoni-frontend
claude
# Then tell it: "You are the frontend agent. Read CLAUDE.md and .claude/agents/frontend.md. 
#   Work on [task]. Create a feature branch, implement, test, commit, push, and open a PR."

# Terminal 2 — BACKEND agent  
cd C:\AI\Sysmoni-backend
claude
# Then tell it: "You are the backend agent. Read CLAUDE.md and .claude/agents/backend.md.
#   Work on [task]. Create a feature branch, implement, test, commit, push, and open a PR."

# Terminal 3 — REVIEWER agent
cd C:\AI\Sysmoni-reviewer
claude
# Then tell it: "You are the PR reviewer. Read CLAUDE.md and .claude/agents/pr-reviewer.md.
#   Review all open PRs. Approve or request changes. Merge approved PRs."

# Terminal 4 — ARCHITECT agent
cd C:\AI\Sysmoni-architect
claude
# Then tell it: "You are the architect. Read CLAUDE.md and .claude/agents/architect.md.
#   Review current architecture, clean up tech debt, update docs."

# Terminal 5 — MASTER (this is YOU or another Claude session)
cd C:\AI\TEST_GUI_Python
claude
# Then tell it: "You are the master orchestrator. Read CLAUDE.md and .claude/agents/master.md.
#   Create GitHub issues for the next features we need to build."
```

### Option B: Agent Teams (More Automated — Try After Option A Works)

Single terminal, Claude coordinates everything:
```powershell
cd C:\AI\TEST_GUI_Python
claude

# Then tell Claude:
> Create an agent team with 4 teammates:
> 1. Frontend agent: owns src/render/native/ and src/shell/native/
> 2. Backend agent: owns src/telemetry/native/ and src/runtime/native/  
> 3. Architect: owns docs, build scripts, infrastructure
> 4. Reviewer: reviews and merges PRs
>
> Task: [describe what you want built]
> Each agent creates a feature branch, implements, tests, and opens a PR.
> Reviewer reviews and merges approved PRs.
```

## Key Differences From Your Old Workflow

| Old Way | New Way |
|---------|---------|
| Agents push directly to `main` | Agents push to feature branches |
| No review before merge | Reviewer agent checks every PR |
| Agents coordinate via `groupchat.md` | Agents coordinate via Git branches + PRs |
| All agents in same folder | Each agent in its own worktree folder |
| Conflicts resolved manually | Conflicts prevented by module ownership |

## Keeping Worktrees in Sync

After the reviewer merges a PR into main, all worktrees need the latest main:

```powershell
# In each worktree, pull latest main and rebase your branch:
cd C:\AI\Sysmoni-frontend
git fetch origin
git rebase origin/main

cd C:\AI\Sysmoni-backend
git fetch origin
git rebase origin/main

# etc.
```

Or tell each agent at the start of a session:
"First, sync with main: git fetch origin && git rebase origin/main"

## Cleaning Up Worktrees (when done)

```powershell
# From your main repo:
cd C:\AI\TEST_GUI_Python

# List all worktrees
git worktree list

# Remove a worktree
git worktree remove C:\AI\Sysmoni-frontend
git worktree remove C:\AI\Sysmoni-backend
git worktree remove C:\AI\Sysmoni-reviewer
git worktree remove C:\AI\Sysmoni-architect

# Delete the branches too (after they're merged)
git branch -d agent/frontend
git branch -d agent/backend
git branch -d agent/reviewer
git branch -d agent/architect
```

## Quick Git Reference (for you)

```powershell
# See all branches
git branch -a

# See status of current branch
git status

# See what changed
git diff

# See open PRs on GitHub
gh pr list

# See a specific PR's diff
gh pr diff 1

# Merge a PR from command line
gh pr merge 1 --squash --delete-branch

# Check worktree status
git worktree list
```

## Emergency: "I Fucked Up Main"

If something bad gets merged to main:
```powershell
cd C:\AI\TEST_GUI_Python
git log --oneline -10          # see recent commits
git revert <commit-hash>       # create a new commit that undoes the bad one
git push origin main
```

Don't use `git reset --hard` unless you know what you're doing — it rewrites history.
