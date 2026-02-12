# Project Log

## 2026-02-12 23:01:19 +01:00 - Ship Readiness Snapshot (User Requested)

User request context:
- User asked to log what we are building, current progress, and how close we are to shipping.

What we are building:
- Aura/Sysmoni: a cinematic, local-only desktop system monitor (PySide6/QML/OpenGL target architecture).

What is implemented now:
- Stable telemetry CLI bootstrap with:
- Snapshot model invariants (`src/core/types.py`)
- Fixed-interval polling loop + concurrency-safe CPU priming (`src/core/poller.py`)
- CLI flow and controls (`src/main.py`) including `--json`, `--watch`, `--interval`, `--count`
- Regression tests covering current scope (`tests/test_main.py`, `tests/test_poller.py`, `tests/test_types.py`)

Current verified state:
- Test status: `python -m unittest discover -s tests -v` passes (28 tests).
- Git history emphasis: mostly reliability hardening in CLI/poller layer.

Readiness estimate:
- Full intended desktop product ship readiness: ~10-15%
- CLI telemetry alpha readiness: ~75-85%

Why full product is not near ship yet:
- Missing planned core modules: `src/core/pipeline.py`, `src/core/store.py`
- GUI stack not implemented: `src/gui/*` (window, panels, widgets, shaders)
- No Qt thread/signal integration layer yet
- No SQLite DVR persistence yet
- No CI/release scaffolding (e.g., `.github` workflows)
