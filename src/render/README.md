# Aura Render Module

The render module is now native C++.

Status:
- No Python runtime sources in `src/render/**`.
- No Python tests in `tests/test_render/**`.

## Source

- Core: `src/render/native/src/*.cpp`
- Headers: `src/render/native/include/**/*.h|hpp`

## Public Interface

- C ABI exposed via `src/render/native/include/aura_render.h`

## Build

```powershell
cmake -S src/render/native -B build/render-native -DCMAKE_BUILD_TYPE=Release
cmake --build build/render-native --config Release
```

```powershell
cmake -S tests/test_render -B build/render-native-tests -G "Visual Studio 17 2022" -A x64
cmake --build build/render-native-tests --config Release
ctest --test-dir build/render-native-tests -C Release --output-on-failure
```
