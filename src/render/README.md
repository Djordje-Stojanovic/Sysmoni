# Aura Render Module

Native RENDER module for theme, formatting, status, and widget-model primitives.

## Paths

- Source: `src/render/native/src`
- Headers: `src/render/native/include`
- Tests: `tests/test_render`
- C ABI: `src/render/native/include/aura_render.h`

## Build (from repo root)

```powershell
cmake -S src/render/native -B build/render-native -G "Visual Studio 17 2022" -A x64
cmake --build build/render-native --config Release
```

## Test (from repo root)

```powershell
cmake -S tests/test_render -B build/render-native-tests -G "Visual Studio 17 2022" -A x64
cmake --build build/render-native-tests --config Release
ctest --test-dir build/render-native-tests -C Release --output-on-failure
```

## Artifact

- `build/render-native/Release/aura_render_native.dll`
