# Shell Test Suite

Shell is native-only C++.

- Source: `src/shell/native/**`
- Native tests: `src/shell/native/tests/**`
- Runner: CMake + CTest

```powershell
cmake -S src/shell/native -B src/shell/native/build -G "Visual Studio 17 2022" -A x64
cmake --build src/shell/native/build --config Release
ctest --test-dir src/shell/native/build -C Release --output-on-failure
```

If Qt6 is unavailable, CMake still builds `aura_shell_core` and CTest binaries.
Install Qt6 and set `Qt6_DIR`/`CMAKE_PREFIX_PATH` to enable `aura_shell.exe`.
