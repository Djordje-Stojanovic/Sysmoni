param(
    [string]$BuildDir = "build/shell-native",
    [string]$Config = "Release",
    [string]$Generator = "Visual Studio 17 2022",
    [string]$Arch = "x64",
    [string]$Qt6Dir = "",
    [switch]$DeployOnly
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

function Resolve-FirstExistingPath {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }
    return $null
}

function Build-NativeTarget {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceDir,
        [Parameter(Mandatory = $true)]
        [string]$BuildDir,
        [Parameter(Mandatory = $true)]
        [string]$Target,
        [Parameter(Mandatory = $true)]
        [string]$Generator,
        [Parameter(Mandatory = $true)]
        [string]$Arch,
        [Parameter(Mandatory = $true)]
        [string]$Config
    )

    $configureArgs = @(
        "-S", $SourceDir,
        "-B", $BuildDir,
        "-G", $Generator
    )

    if ($Generator -like "Visual Studio*") {
        $configureArgs += @("-A", $Arch)
    }

    if ($Generator -notlike "Visual Studio*" -and $Generator -notlike "Xcode*") {
        $configureArgs += "-DCMAKE_BUILD_TYPE=$Config"
    }

    Invoke-External -Command "cmake" -Arguments $configureArgs
    Invoke-External -Command "cmake" -Arguments @("--build", $BuildDir, "--config", $Config, "--target", $Target)
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

function Resolve-QtRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Qt6CmakeDir
    )

    $root = Resolve-Path (Join-Path $Qt6CmakeDir "..\..\..") -ErrorAction SilentlyContinue
    if ($root) {
        return $root.Path
    }
    return $null
}

function Ensure-BridgeDependencies {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string]$OutputDir,
        [Parameter(Mandatory = $true)]
        [string]$Generator,
        [Parameter(Mandatory = $true)]
        [string]$Arch,
        [Parameter(Mandatory = $true)]
        [string]$Config
    )

    $telemetryName = "aura_telemetry_native.dll"
    $renderName = "aura_render_native.dll"
    $platformName = "aura_platform.dll"

    $telemetryCandidates = @(
        (Join-Path $RepoRoot "build\telemetry-native\$Config\$telemetryName"),
        (Join-Path $RepoRoot "src\telemetry\native\build\$Config\$telemetryName"),
        (Join-Path $RepoRoot "tests\test_telemetry\build\$Config\$telemetryName"),
        (Join-Path $RepoRoot "tests\test_telemetry\build\telemetry_native\$Config\$telemetryName")
    )
    $telemetryDll = Resolve-FirstExistingPath -Candidates $telemetryCandidates
    if (-not $telemetryDll) {
        Write-Host "Building telemetry bridge dependency..."
        Build-NativeTarget `
            -SourceDir (Join-Path $RepoRoot "src\telemetry\native") `
            -BuildDir (Join-Path $RepoRoot "build\telemetry-native") `
            -Target "aura_telemetry_native" `
            -Generator $Generator `
            -Arch $Arch `
            -Config $Config
        $telemetryDll = Resolve-FirstExistingPath -Candidates $telemetryCandidates
    }
    if (-not $telemetryDll) {
        throw "Failed to resolve $telemetryName after build."
    }
    Copy-Item -Force $telemetryDll (Join-Path $OutputDir $telemetryName)

    $renderCandidates = @(
        (Join-Path $RepoRoot "build\render-native\$Config\$renderName"),
        (Join-Path $RepoRoot "build\render-native-tests\$Config\$renderName"),
        (Join-Path $RepoRoot "build\render-native-tests\render-native\$Config\$renderName"),
        (Join-Path $RepoRoot "src\render\native\build\$Config\$renderName")
    )
    $renderDll = Resolve-FirstExistingPath -Candidates $renderCandidates
    if (-not $renderDll) {
        Write-Host "Building render bridge dependency..."
        Build-NativeTarget `
            -SourceDir (Join-Path $RepoRoot "src\render\native") `
            -BuildDir (Join-Path $RepoRoot "build\render-native") `
            -Target "aura_render_native" `
            -Generator $Generator `
            -Arch $Arch `
            -Config $Config
        $renderDll = Resolve-FirstExistingPath -Candidates $renderCandidates
    }
    if (-not $renderDll) {
        throw "Failed to resolve $renderName after build."
    }
    Copy-Item -Force $renderDll (Join-Path $OutputDir $renderName)

    $platformCandidates = @(
        (Join-Path $RepoRoot "build\platform-native\dist\$platformName"),
        (Join-Path $RepoRoot "build\platform-native\$Config\$platformName"),
        (Join-Path $RepoRoot "build\platform-native-vs\dist\$platformName"),
        (Join-Path $RepoRoot "build\platform-native-vs\$Config\$platformName")
    )
    $platformDll = Resolve-FirstExistingPath -Candidates $platformCandidates
    if (-not $platformDll) {
        Write-Host "Building runtime bridge dependency..."
        $platformBuildScript = Join-Path $RepoRoot "installer\windows\build_platform_native.ps1"
        & powershell -NoProfile -ExecutionPolicy Bypass -File $platformBuildScript `
            -BuildDir "build/platform-native" `
            -Config $Config `
            -Generator $Generator `
            -Arch $Arch
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to build runtime bridge dependency."
        }
        $platformDll = Resolve-FirstExistingPath -Candidates $platformCandidates
    }
    if (-not $platformDll) {
        throw "Failed to resolve $platformName after build."
    }
    Copy-Item -Force $platformDll (Join-Path $OutputDir $platformName)
}

function Deploy-QtRuntime {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Qt6CmakeDir,
        [Parameter(Mandatory = $true)]
        [string]$OutputDir,
        [Parameter(Mandatory = $true)]
        [string]$ShellExePath,
        [Parameter(Mandatory = $true)]
        [string]$QmlDir
    )

    $qtRoot = Resolve-QtRoot -Qt6CmakeDir $Qt6CmakeDir
    if (-not $qtRoot) {
        throw "Failed to resolve Qt root from Qt6_DIR: $Qt6CmakeDir"
    }

    $qtBin = Join-Path $qtRoot "bin"
    $windeployqt = Join-Path $qtBin "windeployqt.exe"
    if (-not (Test-Path $windeployqt)) {
        throw "windeployqt.exe was not found under: $qtBin"
    }

    Write-Host "Deploying Qt runtime with windeployqt..."
    Invoke-External -Command $windeployqt -Arguments @(
        "--release",
        "--no-compiler-runtime",
        "--no-translations",
        "--qmldir",
        $QmlDir,
        $ShellExePath
    )
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..")
$sourceDir = Join-Path $repoRoot "src\shell\native"
$qmlDir = Join-Path $sourceDir "qml"
$resolvedBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $repoRoot $BuildDir }

$resolvedQt6 = Resolve-Qt6Dir
$outDir = Join-Path $resolvedBuildDir "dist"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

if (-not $DeployOnly) {
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

    if ($resolvedQt6) {
        $configureArgs += "-DQt6_DIR=$resolvedQt6"
    }

    Invoke-External -Command "cmake" -Arguments $configureArgs

    $vcxproj = Join-Path $resolvedBuildDir "aura_shell.vcxproj"
    if (-not (Test-Path $vcxproj)) {
        throw "Qt6 was not found. Install Qt6 (msvc2022_64) or pass -Qt6Dir <path-to-Qt6/lib/cmake/Qt6>."
    }

    Invoke-External -Command "cmake" -Arguments @("--build", $resolvedBuildDir, "--config", $Config, "--target", "aura_shell")

    Copy-Item -Force (Join-Path $resolvedBuildDir "$Config\aura_shell.exe") $outDir -ErrorAction SilentlyContinue
    Copy-Item -Force (Join-Path $resolvedBuildDir "aura_shell.exe") $outDir -ErrorAction SilentlyContinue
} else {
    Write-Host "Deploy-only mode: skipping configure/build and reusing existing aura_shell.exe."
}

$distExe = Join-Path $outDir "aura_shell.exe"
if (-not (Test-Path $distExe)) {
    Copy-Item -Force (Join-Path $resolvedBuildDir "$Config\aura_shell.exe") $outDir -ErrorAction SilentlyContinue
    Copy-Item -Force (Join-Path $resolvedBuildDir "aura_shell.exe") $outDir -ErrorAction SilentlyContinue
}

if (-not (Test-Path $distExe)) {
    throw "aura_shell.exe was not found in: $outDir"
}

Ensure-BridgeDependencies `
    -RepoRoot $repoRoot `
    -OutputDir $outDir `
    -Generator $Generator `
    -Arch $Arch `
    -Config $Config

if (-not $resolvedQt6) {
    $cacheFile = Join-Path $resolvedBuildDir "CMakeCache.txt"
    if (Test-Path $cacheFile) {
        $qtCacheLine = Select-String -Path $cacheFile -Pattern "^Qt6_DIR:PATH=" | Select-Object -First 1
        if ($qtCacheLine) {
            $resolvedQt6 = $qtCacheLine.Line.Substring("Qt6_DIR:PATH=".Length)
        }
    }
}

if (-not $resolvedQt6 -or -not (Test-Path $resolvedQt6)) {
    throw "Qt6_DIR could not be resolved for runtime deployment."
}

Deploy-QtRuntime -Qt6CmakeDir $resolvedQt6 -OutputDir $outDir -ShellExePath $distExe -QmlDir $qmlDir

Write-Host "Native shell artifacts staged in: $outDir"
