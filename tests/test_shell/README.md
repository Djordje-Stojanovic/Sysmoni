# Shell Test Suite

Shell is native-only C++.

- Source: `src/shell/native/**`
- Native tests: `tests/test_shell/native/**`
- Runner: root CMake + CTest

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure --tests-regex "aura_shell_.*_tests"
```

If Qt6 is unavailable, CMake still builds `aura_shell_core` and CTest binaries.
Install Qt6 and set `Qt6_DIR`/`CMAKE_PREFIX_PATH` to enable `aura_shell.exe`.
