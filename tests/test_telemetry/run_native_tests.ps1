param(
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $scriptRoot "build"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "cmake was not found. Install CMake to run native telemetry tests."
}

cmake -S $scriptRoot -B $buildDir -DCMAKE_BUILD_TYPE=$Configuration
cmake --build $buildDir --config $Configuration
ctest --test-dir $buildDir --output-on-failure -C $Configuration
