# Telemetry Native Tests

Telemetry tests are native C++ tests only.

## Run with CMake/CTest (Windows)

```powershell
cmake -S tests/test_telemetry -B tests/test_telemetry/build -G "Visual Studio 17 2022" -A x64
cmake --build tests/test_telemetry/build --config Release
ctest --test-dir tests/test_telemetry/build -C Release --output-on-failure
```

## Convenience Script

```powershell
.\tests\test_telemetry\run_native_tests.ps1
```
