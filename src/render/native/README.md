# Aura Render Native Runtime

Native render library used by shell UI and tests.

## Artifact

- `build/render-native/Release/aura_render_native.dll`

## ABI

- `src/render/native/include/aura_render.h`

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

## Optional Flag

Enable Qt widget hooks during configure:

```powershell
cmake -S src/render/native -B build/render-native -G "Visual Studio 17 2022" -A x64 -DRENDER_NATIVE_ENABLE_QT_WIDGETS=ON
```
