# Windows 11 Run (C++ Native)

From repo root:

```powershell
# fastest GUI run: one command, no args
.\aura.cmd

# explicit GUI mode
.\aura.cmd --gui

# CLI mode
.\aura.cmd --cli --json --no-persist

# Verify toolchain in this shell
cmake --version
cl

# one-time Qt6 install for GUI
uvx --from aqtinstall aqt.exe install-qt -O C:\Qt windows desktop 6.10.2 win64_msvc2022_64
powershell -ExecutionPolicy Bypass -File installer/windows/build_shell_native.ps1 -Qt6Dir "C:\Qt\6.10.2\msvc2022_64\lib\cmake\Qt6"

# Build native platform runtime
cmake -S src/runtime/native -B src/build/platform-native -G "Visual Studio 17 2022" -A x64
cmake --build src/build/platform-native --config Release

# Run CLI
.\src\build\platform-native\Release\aura.exe --json

# Run platform native tests
cmake -S tests/test_platform -B src/build/platform-native-tests -G "Visual Studio 17 2022" -A x64
cmake --build src/build/platform-native-tests --config Release
ctest --test-dir src/build/platform-native-tests -C Release --output-on-failure
```

Shortcuts:

```powershell
powershell -ExecutionPolicy Bypass -File tests/test_platform/run_native_tests.ps1
powershell -ExecutionPolicy Bypass -File installer/windows/build_platform_native.ps1
powershell -ExecutionPolicy Bypass -File installer/windows/build_shell_native.ps1
```
