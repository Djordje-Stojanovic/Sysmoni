# Aura Platform Native Runtime

Native runtime used by the Aura CLI.

## Artifacts

- `build/platform-native/Release/aura.exe`
- `build/platform-native/Release/aura_platform.dll`

## ABI

- `src/runtime/native/include/aura_platform.h`

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
