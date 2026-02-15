# Aura

Windows-native system monitor. C++20, Qt6, real-time telemetry cockpit.

## Get Running in 3 Steps

**Prerequisites:** Visual Studio 2022 Build Tools (C++ workload), CMake 3.23+

```powershell
# 1. Install Qt6 (one-time, ~2 min)
uvx --from aqtinstall aqt.exe install-qt -O C:\Qt windows desktop 6.10.2 win64_msvc2022_64

# 2. Build
.\aura.cmd build gui

# 3. Run
.\aura.cmd gui
```

That's it. Qt6 is auto-discovered from `C:\Qt\` — no paths to remember.

## All Commands

```powershell
.\aura.cmd gui                      # launch GUI (auto-builds if needed)
.\aura.cmd cli --json --no-persist  # launch CLI

.\aura.cmd build gui                # build GUI (incremental)
.\aura.cmd build gui --force        # force full rebuild
.\aura.cmd build cli                # build CLI only
.\aura.cmd build all                # build everything

.\aura.cmd test all                 # run all test suites
.\aura.cmd test shell               # shell module tests only
.\aura.cmd test platform            # platform module tests only

.\aura.cmd doctor                   # check toolchain + Qt6 status
.\aura.cmd clean --force            # wipe build artifacts
.\aura.cmd install                  # install to user profile + desktop shortcut
.\aura.cmd help                     # full usage reference
```

### Flags

All build/test commands accept these anywhere:

| Flag | Short | Effect |
|------|-------|--------|
| `--debug` | `debug` | Debug build config |
| `--release` | `release` | Release build config (default) |
| `--force` | `force` | Force rebuild even if up-to-date |
| `--dry-run` | `dryrun` | Preview without executing |
| `--qt6dir <path>` | `qt6dir <path>` | Override Qt6 location |

```powershell
.\aura.cmd build all debug          # debug build
.\aura.cmd test shell debug         # test debug config
.\aura.cmd clean dryrun             # preview what would be deleted
```
