# Aura RENDER Native Runtime

Windows-first native render module for Aura.

## Artifacts

- `aura_render_native.dll` - C ABI for render math, formatting, status, and widget model state.

## Build

```powershell
cmake -S src/render/native -B build/render-native -DCMAKE_BUILD_TYPE=Release
cmake --build build/render-native --config Release
```

## Test

```powershell
cmake -S tests/test_render -B build/render-native-tests -DCMAKE_BUILD_TYPE=Release
cmake --build build/render-native-tests --config Release
ctest --test-dir build/render-native-tests --output-on-failure -C Release
```

## ABI

Public C ABI header: `src/render/native/include/aura_render.h`.

## Notes

- Module is C++-only.
- Qt Widgets + RHI backend can be enabled with `-DRENDER_NATIVE_ENABLE_QT_WIDGETS=ON`.
- Root-level app wiring is requested in `groupchat.md` because render scope does not include root packaging files.
