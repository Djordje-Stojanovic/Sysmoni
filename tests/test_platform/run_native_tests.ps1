param(
    [string]$BuildDir = "build/platform-native-tests",
    [string]$Config = "Release",
    [string]$Generator = "Visual Studio 17 2022",
    [string]$Arch = "x64"
)

$ErrorActionPreference = "Stop"

cmake -S tests/test_platform -B $BuildDir -G $Generator -A $Arch -DCMAKE_BUILD_TYPE=$Config
cmake --build $BuildDir --config $Config
ctest --test-dir $BuildDir --output-on-failure -C $Config
