# PLATFORM Native Tests

This directory now contains native C++ tests for PLATFORM.

## Run

```powershell
cmake -S tests/test_platform -B build/platform-native-tests -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build/platform-native-tests --config Release
ctest --test-dir build/platform-native-tests --output-on-failure -C Release
```

Python platform tests were intentionally removed as part of the 100% native cutover.
