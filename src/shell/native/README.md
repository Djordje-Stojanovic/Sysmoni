# Aura Native Shell (Windows, C++)

This directory contains the native `aura_shell.exe` scaffold for the shell
module. It is Windows-first and targets Qt6 with a hybrid Widgets + QML UI
composition.

## Build (Windows)

```powershell
cd src/shell/native
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target aura_shell
```

Expected binary:

`src/shell/native/build/Release/aura_shell.exe`

## Test

```powershell
cd src/shell/native
cmake --build build --config Release --target aura_shell_dock_model_tests
ctest --test-dir build -C Release --output-on-failure
```

## CLI Args

- `--interval <seconds>`
- `--db-path <path>`
- `--retention-seconds <seconds>`
- `--no-persist`

## Notes

- The current implementation is intentionally shell-owned groundwork:
  docking model + native window host + QML cockpit scene.
- Runtime launch cutover lives in the `platform` module and should call this
  binary via shell launch contracts.

