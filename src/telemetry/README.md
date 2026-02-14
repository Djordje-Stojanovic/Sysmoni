# Aura Telemetry Module

Native SENSOR module for process/system telemetry collection.

## Paths

- Source: `src/telemetry/native/src`
- Headers: `src/telemetry/native/include`
- Tests: `tests/test_telemetry`

## Build (from repo root)

```powershell
cmake -S src/telemetry/native -B build/telemetry-native -G "Visual Studio 17 2022" -A x64
cmake --build build/telemetry-native --config Release
```

## Test (from repo root)

```powershell
cmake -S tests/test_telemetry -B build/telemetry-native-tests -G "Visual Studio 17 2022" -A x64
cmake --build build/telemetry-native-tests --config Release
ctest --test-dir build/telemetry-native-tests -C Release --output-on-failure
```

## Artifact

- `build/telemetry-native/Release/aura_telemetry_native.dll`
