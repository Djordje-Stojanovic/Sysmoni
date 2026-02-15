# Aura (Sysmoni)

Windows-native, local-first system monitor built with C++20, CMake, and Qt6.

## User-First Entry Point

Use `aura.cmd` from repo root for normal usage:

```powershell
.\aura.cmd
```

That launches GUI by default.

## Quick Commands

```powershell
# Help
.\aura.cmd help

# Launch GUI / CLI
.\aura.cmd gui
.\aura.cmd cli --json --no-persist

# Build (all/gui/cli)
.\aura.cmd build all
.\aura.cmd build gui
.\aura.cmd build cli

# Tests (all/shell/render/telemetry/platform)
.\aura.cmd test all
.\aura.cmd test platform

# Maintenance
.\aura.cmd doctor
.\aura.cmd update dryrun
.\aura.cmd clean dryrun

# Install / uninstall user launchers
.\aura.cmd install
.\aura.cmd uninstall force
```

### Debug/Release Flags

`aura.cmd` accepts both long flags and user-friendly aliases:

- `--debug` or `debug`
- `--release` or `release`
- `--force` or `force`
- `--dry-run` or `dryrun`
- `--qt6dir <path>` or `qt6dir <path>`

Examples:

```powershell
.\aura.cmd build all debug
.\aura.cmd test telemetry debug
.\aura.cmd clean dryrun
```

## One-Time Setup (Windows)

- Visual Studio 2022 Build Tools (Desktop development with C++)
- CMake 3.23+

Optional (GUI only):

```powershell
uvx --from aqtinstall aqt.exe install-qt -O C:\Qt windows desktop 6.10.2 win64_msvc2022_64
powershell -ExecutionPolicy Bypass -File installer/windows/build_shell_native.ps1 -Qt6Dir "C:\Qt\6.10.2\msvc2022_64\lib\cmake\Qt6"
```

## AI/Automation Note

`aura.cmd` is the main human-facing interface.  
For AI/agent workflows, prefer module-specific build/test scripts and docs (`AGENTS.md`, `ai.md`, `coding_guidelines.md`) when you need lower-level control.
