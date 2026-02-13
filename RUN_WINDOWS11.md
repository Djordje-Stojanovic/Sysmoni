# Windows 11 Run

```powershell
# one-time (if uv is missing)
powershell -ExecutionPolicy ByPass -c "irm https://astral.sh/uv/install.ps1 | iex"

# from repo root
uv run python src\gui\window.py
# or the canonical module path
uv run python src\shell\window.py
```
