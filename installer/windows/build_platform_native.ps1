param(
    [string]$BuildDir = "build/platform-native",
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

cmake -S src/runtime/native -B $BuildDir -DCMAKE_BUILD_TYPE=$Config
cmake --build $BuildDir --config $Config

$outDir = Join-Path $BuildDir "dist"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Copy-Item -Force (Join-Path $BuildDir "$Config\aura.exe") $outDir -ErrorAction SilentlyContinue
Copy-Item -Force (Join-Path $BuildDir "$Config\aura_platform.dll") $outDir -ErrorAction SilentlyContinue
Copy-Item -Force (Join-Path $BuildDir "aura.exe") $outDir -ErrorAction SilentlyContinue
Copy-Item -Force (Join-Path $BuildDir "aura_platform.dll") $outDir -ErrorAction SilentlyContinue

Write-Host "Native artifacts staged in: $outDir"
