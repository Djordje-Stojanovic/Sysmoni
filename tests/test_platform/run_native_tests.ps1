param(
    [string]$BuildDir = "build/platform-native-tests",
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

cmake -S tests/test_platform -B $BuildDir -DCMAKE_BUILD_TYPE=$Config
cmake --build $BuildDir --config $Config
ctest --test-dir $BuildDir --output-on-failure -C $Config
