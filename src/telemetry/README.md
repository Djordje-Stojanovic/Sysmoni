# Aura Telemetry (SENSOR) - Native Only

This module is C++-only.

- No Python runtime files are retained under `src/telemetry/**`.
- Core implementation lives in `src/telemetry/native/src/`.
- Public native interfaces live in `src/telemetry/native/include/`.

## Build (Windows, Visual Studio)

```powershell
cmake -S src/telemetry/native -B src/telemetry/native/build -G "Visual Studio 17 2022" -A x64
cmake --build src/telemetry/native/build --config Release
```

## Output

- `src/telemetry/native/build/Release/aura_telemetry_native.dll`
