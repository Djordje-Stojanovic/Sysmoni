param(
    [string]$BuildDir = "build/shell-native",
    [string]$Config = "Release",
    [string]$Generator = "Visual Studio 17 2022",
    [string]$Arch = "x64",
    [string]$Qt6Dir = ""
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

function Resolve-Qt6Dir {
    if ($Qt6Dir) {
        if (-not (Test-Path $Qt6Dir)) {
            throw "Qt6Dir does not exist: $Qt6Dir"
        }
        return (Resolve-Path $Qt6Dir).Path
    }

    $candidates = Get-ChildItem "C:\Qt" -Directory -ErrorAction SilentlyContinue |
        ForEach-Object {
            Get-ChildItem $_.FullName -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -like "msvc*" } |
                ForEach-Object { Join-Path $_.FullName "lib\cmake\Qt6" }
        }

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..")
$sourceDir = Join-Path $repoRoot "src\shell\native"
$resolvedBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $repoRoot $BuildDir }

$configureArgs = @(
    "-S", $sourceDir,
    "-B", $resolvedBuildDir,
    "-G", $Generator,
    "-DAURA_SHELL_BUILD_APP=ON"
)

if ($Generator -like "Visual Studio*") {
    $configureArgs += @("-A", $Arch)
}

if ($Generator -notlike "Visual Studio*" -and $Generator -notlike "Xcode*") {
    $configureArgs += "-DCMAKE_BUILD_TYPE=$Config"
}

$resolvedQt6 = Resolve-Qt6Dir
if ($resolvedQt6) {
    $configureArgs += "-DQt6_DIR=$resolvedQt6"
}

Invoke-External -Command "cmake" -Arguments $configureArgs

$vcxproj = Join-Path $resolvedBuildDir "aura_shell.vcxproj"
if (-not (Test-Path $vcxproj)) {
    throw "Qt6 was not found. Install Qt6 (msvc2022_64) or pass -Qt6Dir <path-to-Qt6/lib/cmake/Qt6>."
}

Invoke-External -Command "cmake" -Arguments @("--build", $resolvedBuildDir, "--config", $Config, "--target", "aura_shell")

$outDir = Join-Path $resolvedBuildDir "dist"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Copy-Item -Force (Join-Path $resolvedBuildDir "$Config\aura_shell.exe") $outDir -ErrorAction SilentlyContinue
Copy-Item -Force (Join-Path $resolvedBuildDir "aura_shell.exe") $outDir -ErrorAction SilentlyContinue

Write-Host "Native shell artifacts staged in: $outDir"
