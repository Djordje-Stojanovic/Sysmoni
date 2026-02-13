# Aura Render Module

The render module is now native C++.

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
