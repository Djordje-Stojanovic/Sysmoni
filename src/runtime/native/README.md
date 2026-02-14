# Aura PLATFORM Native Runtime

Windows-first native runtime for Aura PLATFORM module.

## Artifacts

- `aura_platform.dll` - C ABI for config/store/DVR platform services.
- `aura.exe` - native CLI runtime entrypoint.

## Build

```powershell
cmake -S src/runtime/native -B build/platform-native -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build/platform-native --config Release
```

## Test

```powershell
cmake -S tests/test_platform -B build/platform-native-tests -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build/platform-native-tests --config Release
ctest --test-dir build/platform-native-tests --output-on-failure -C Release
```

## ABI

Public C ABI header: `src/runtime/native/include/aura_platform.h`.

## Notes

- This phase is Windows-first.
- Store format is durable file-backed snapshot log under configured `db_path`.
- Runtime config precedence: CLI > env > TOML > auto defaults.
- In Codex terminal environments, prefer Visual Studio generator over Ninja.
