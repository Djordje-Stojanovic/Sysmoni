param(
    [string]$BuildDir = "build/platform-native",
    [string]$Config = "Release",
    [string]$Generator = "Visual Studio 17 2022",
    [string]$Arch = "x64"
)

$ErrorActionPreference = "Stop"

function Invoke-External {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Command failed with exit code $LASTEXITCODE"
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\\..")
$sourceDir = Join-Path $repoRoot "src\\runtime\\native"
$resolvedBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $repoRoot $BuildDir }

$configureArgs = @(
    "-S", $sourceDir,
    "-B", $resolvedBuildDir,
    "-G", $Generator
)

if ($Generator -like "Visual Studio*") {
    $configureArgs += @("-A", $Arch)
}

if ($Generator -notlike "Visual Studio*" -and $Generator -notlike "Xcode*") {
    $configureArgs += "-DCMAKE_BUILD_TYPE=$Config"
}

Invoke-External -Command "cmake" -Arguments $configureArgs
Invoke-External -Command "cmake" -Arguments @("--build", $resolvedBuildDir, "--config", $Config)

$outDir = Join-Path $resolvedBuildDir "dist"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Copy-Item -Force (Join-Path $resolvedBuildDir "$Config\aura.exe") $outDir -ErrorAction SilentlyContinue
Copy-Item -Force (Join-Path $resolvedBuildDir "$Config\aura_platform.dll") $outDir -ErrorAction SilentlyContinue
Copy-Item -Force (Join-Path $resolvedBuildDir "aura.exe") $outDir -ErrorAction SilentlyContinue
Copy-Item -Force (Join-Path $resolvedBuildDir "aura_platform.dll") $outDir -ErrorAction SilentlyContinue

Write-Host "Native artifacts staged in: $outDir"
