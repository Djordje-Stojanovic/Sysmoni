# Windows 11 Run (C++ Native)

From repo root:

```powershell
# fastest: auto-build + run
.\aura.cmd --json --no-persist

# fastest GUI run
.\aura-gui.cmd

# Verify toolchain in this shell
cmake --version
cl

# Build native platform runtime
cmake -S src/runtime/native -B src/build/platform-native -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build src/build/platform-native --config Release

# Run CLI
.\src\build\platform-native\Release\aura.exe --json

# Run platform native tests
cmake -S tests/test_platform -B src/build/platform-native-tests -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build src/build/platform-native-tests --config Release
ctest --test-dir src/build/platform-native-tests -C Release --output-on-failure
```

Shortcuts:

```powershell
powershell -ExecutionPolicy Bypass -File tests/test_platform/run_native_tests.ps1
powershell -ExecutionPolicy Bypass -File installer/windows/build_platform_native.ps1
powershell -ExecutionPolicy Bypass -File installer/windows/build_shell_native.ps1
```
