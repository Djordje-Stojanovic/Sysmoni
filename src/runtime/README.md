# Aura Runtime Module

Native PLATFORM module for CLI, config, storage, and system service wiring.

## Paths

- Source: `src/runtime/native/src`
- Headers: `src/runtime/native/include`
- Tests: `tests/test_platform`

## Build (from repo root)

```powershell
cmake -S src/runtime/native -B build/platform-native -G "Visual Studio 17 2022" -A x64
cmake --build build/platform-native --config Release
```

## Run CLI (from repo root)

```powershell
.\build\platform-native\Release\aura.exe --json --no-persist
```

## Test (from repo root)

```powershell
cmake -S tests/test_platform -B build/platform-native-tests -G "Visual Studio 17 2022" -A x64
cmake --build build/platform-native-tests --config Release
ctest --test-dir build/platform-native-tests -C Release --output-on-failure
```
