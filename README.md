# Aura (Sysmoni)

Windows-native system monitor built with C++20, CMake, and Qt6 (GUI).

## Quick Start (Repo Root)

```powershell
.\aura.cmd
```

That is the default developer run path (GUI).  
For CLI:

```powershell
.\aura.cmd --cli --json --no-persist
```

## One-Time Setup (Windows)

- Visual Studio 2022 Build Tools (Desktop development with C++)
- CMake 3.23+

Optional (GUI only):

```powershell
uvx --from aqtinstall aqt.exe install-qt -O C:\Qt windows desktop 6.10.2 win64_msvc2022_64
powershell -ExecutionPolicy Bypass -File installer/windows/build_shell_native.ps1 -Qt6Dir "C:\Qt\6.10.2\msvc2022_64\lib\cmake\Qt6"
```

## Build Everything (Repo Root)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Run Tests (Repo Root)

```powershell
ctest --test-dir build -C Release --output-on-failure
```

## Common Commands

```powershell
# GUI
.\aura.cmd --gui

# CLI
.\aura.cmd --cli --json --no-persist

# Build platform CLI artifacts
powershell -ExecutionPolicy Bypass -File installer/windows/build_platform_native.ps1

# Build shell GUI artifacts (+ deploy Qt + bridge DLLs)
powershell -ExecutionPolicy Bypass -File installer/windows/build_shell_native.ps1 -Qt6Dir "C:\Qt\6.10.2\msvc2022_64\lib\cmake\Qt6"

# Platform-only test script
powershell -ExecutionPolicy Bypass -File tests/test_platform/run_native_tests.ps1
```

## Repo Layout

```text
src/
  telemetry/native/
  render/native/
  shell/native/
  runtime/native/
tests/
  test_telemetry/
  test_render/
  test_shell/
  test_platform/
```
