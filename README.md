# Aura (Sysmoni)

Aura is now a native C++ desktop system monitor rewrite.

This repository has moved from Python-first prototyping to C++-first implementation for runtime, packaging, and performance-critical paths.

## Stack Status

- Core direction: native C++ (Windows-first in current phase)
- Platform module (`src/runtime/**`): fully native C++
- Platform tests (`tests/test_platform/**`): native C++ (CTest)
- Legacy Python docs/entrypoints for platform were removed

## Toolchain (Best Practice)

Use Visual Studio Build Tools + CMake. In this repo, prefer the Visual Studio generator in Codex/CI shells.

Required on Windows:
- Visual Studio 2022 Build Tools with C++ workload
- CMake

PowerShell profile now auto-loads VS dev environment via `Use-VsCpp`.

Quick verification:

```powershell
cmake --version
cl
ninja --version
```

## Build (Platform Native)

From repo root:

```powershell
cmake -S src/runtime/native -B src/build/platform-native -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build src/build/platform-native --config Release
```

Run:

```powershell
.\src\build\platform-native\Release\aura.exe --json
```

## Test (Platform Native)

```powershell
cmake -S tests/test_platform -B src/build/platform-native-tests -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build src/build/platform-native-tests --config Release
ctest --test-dir src/build/platform-native-tests -C Release --output-on-failure
```

Or use script:

```powershell
powershell -ExecutionPolicy Bypass -File tests/test_platform/run_native_tests.ps1
```

## Installer Staging (Platform Native)

```powershell
powershell -ExecutionPolicy Bypass -File installer/windows/build_platform_native.ps1
```

## Repository Layout

```text
src/
  contracts/
  telemetry/
  render/
  shell/
  runtime/
    native/
tests/
  test_platform/
    native/
```

## Notes

- In this environment, Ninja may hang under some shell wrappers; Visual Studio generator is the default best-practice path.
- Cross-module rewrites (telemetry/render/shell) continue in their owned directories.
