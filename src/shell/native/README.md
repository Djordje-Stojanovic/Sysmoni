# Aura Shell Native (Windows)

Native SHELL module for the desktop UI (`aura_shell.exe`).

## Fast Run (from repo root)

```powershell
.\aura.cmd
```

```powershell
.\aura.cmd --gui
```

## Build Shell App Directly (from repo root)

```powershell
cmake -S src/shell/native -B build/shell-native -G "Visual Studio 17 2022" -A x64 -DQt6_DIR="C:\Qt\6.10.2\msvc2022_64\lib\cmake\Qt6"
cmake --build build/shell-native --config Release --target aura_shell
```

## Build + Deploy GUI Runtime (from repo root)

```powershell
powershell -ExecutionPolicy Bypass -File installer/windows/build_shell_native.ps1 -Qt6Dir "C:\Qt\6.10.2\msvc2022_64\lib\cmake\Qt6"
```

Artifacts are staged in `build/shell-native/dist`.

## Test (from repo root)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure --tests-regex "aura_shell_.*_tests"
```

## Notes

- Qt6 is required for `aura_shell.exe`.
- Shell core and tests still build without Qt6.
