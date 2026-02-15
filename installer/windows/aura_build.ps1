function Invoke-ShellBuild {
    param(
        [string]$Config = $defaultConfig,
        [string]$Qt6Dir = "",
        [switch]$DeployOnly
    )

    $scriptPath = Join-Path $repoRoot "installer\windows\build_shell_native.ps1"
    if (-not (Test-Path $scriptPath)) {
        Fail "Missing script: $scriptPath"
    }

    $args = @(
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        $scriptPath,
        "-BuildDir",
        "build/shell-native",
        "-Config",
        $Config,
        "-Generator",
        $defaultGenerator,
        "-Arch",
        $defaultArch
    )
    if ($DeployOnly) {
        $args += "-DeployOnly"
    }
    if ($Qt6Dir) {
        $args += @("-Qt6Dir", $Qt6Dir)
    }

    & powershell @args
    if ($LASTEXITCODE -ne 0) {
        Fail "GUI build/deploy failed."
    }
}

function Invoke-PlatformBuild {
    param([string]$Config = $defaultConfig)

    $scriptPath = Join-Path $repoRoot "installer\windows\build_platform_native.ps1"
    if (-not (Test-Path $scriptPath)) {
        Fail "Missing script: $scriptPath"
    }

    $args = @(
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        $scriptPath,
        "-BuildDir",
        "build/platform-native",
        "-Config",
        $Config,
        "-Generator",
        $defaultGenerator,
        "-Arch",
        $defaultArch
    )

    & powershell @args
    if ($LASTEXITCODE -ne 0) {
        Fail "CLI build failed."
    }
}

function Ensure-GuiArtifacts {
    param(
        [switch]$ForceBuild,
        [string]$Config = $defaultConfig,
        [string]$Qt6Dir = ""
    )

    $candidates = Get-GuiExeCandidates -Config $Config
    $distDir = Join-Path $repoRoot "build\shell-native\dist"
    $distExe = Join-Path $distDir "aura_shell.exe"
    $bridgeDlls = @(
        "aura_telemetry_native.dll",
        "aura_render_native.dll",
        "aura_platform.dll"
    )

    $existing = Resolve-FirstExistingPath -Candidates $candidates
    $needsBuild = $ForceBuild -or (-not $existing)

    $missingBridge = $false
    foreach ($dll in $bridgeDlls) {
        if (-not (Test-Path (Join-Path $distDir $dll))) {
            $missingBridge = $true
            break
        }
    }

    $needsDeploy = (Test-Path $distExe) -and
                   ((-not (Test-Path (Join-Path $distDir "Qt6Core.dll"))) -or
                    (-not (Test-Path (Join-Path $distDir "platforms\qwindows.dll"))) -or
                    $missingBridge)

    if ($needsBuild -or $needsDeploy) {
        $resolvedQt6 = Resolve-Qt6Dir -Preferred $Qt6Dir
        $qt6Arg = ""
        if ($resolvedQt6) {
            $qt6Arg = $resolvedQt6
        }
        if ($needsBuild) {
            Write-Info "GUI binary not found (or force requested), building..."
            $null = Invoke-ShellBuild -Config $Config -Qt6Dir $qt6Arg
        } else {
            Write-Info "GUI runtime files missing, repairing deployment..."
            $null = Invoke-ShellBuild -Config $Config -Qt6Dir $qt6Arg -DeployOnly
        }
        $existing = Resolve-FirstExistingPath -Candidates $candidates
    }

    if (-not $existing) {
        Fail "GUI build finished but aura_shell.exe was not found."
    }

    return $existing
}

function Ensure-CliArtifacts {
    param(
        [switch]$ForceBuild,
        [string]$Config = $defaultConfig
    )

    $candidates = Get-CliExeCandidates -Config $Config
    $existing = Resolve-FirstExistingPath -Candidates $candidates

    if ($ForceBuild -or (-not $existing)) {
        Write-Info "CLI binary not found (or force requested), building..."
        $null = Invoke-PlatformBuild -Config $Config
        $existing = Resolve-FirstExistingPath -Candidates $candidates
    }

    if (-not $existing) {
        Fail "CLI build finished but aura.exe was not found."
    }

    return $existing
}

function Invoke-Gui {
    param(
        [string[]]$ForwardArgs,
        [switch]$ForceBuild,
        [string]$Config = $defaultConfig,
        [string]$Qt6Dir = ""
    )

    $exePath = Ensure-GuiArtifacts -ForceBuild:$ForceBuild -Config $Config -Qt6Dir $Qt6Dir
    Write-DebugTrace ("Invoke-Gui exe={0} args={1}" -f $exePath, (@($ForwardArgs) -join " | "))
    $resolvedQt6 = Resolve-Qt6Dir -Preferred $Qt6Dir
    if ($resolvedQt6) {
        Set-QtRuntimeEnvironment -Qt6Dir $resolvedQt6
    }

    & $exePath @ForwardArgs
    $exitCode = $LASTEXITCODE

    if ($exitCode -eq -1073741515 -or $exitCode -eq 3221225781) {
        Write-WarnInfo "GUI failed to start because runtime DLLs are missing."
        Write-Info "Fix command:"
        Write-Host "  powershell -ExecutionPolicy Bypass -File installer/windows/build_shell_native.ps1 -Qt6Dir `"C:\Qt\6.10.2\msvc2022_64\lib\cmake\Qt6`""
        return 1
    }

    return $exitCode
}

function Invoke-Cli {
    param(
        [string[]]$ForwardArgs,
        [switch]$ForceBuild,
        [string]$Config = $defaultConfig
    )

    $exePath = Ensure-CliArtifacts -ForceBuild:$ForceBuild -Config $Config
    Write-DebugTrace ("Invoke-Cli exe={0} args={1}" -f $exePath, (@($ForwardArgs) -join " | "))
    & $exePath @ForwardArgs
    return $LASTEXITCODE
}

function Invoke-CmakeTestSuite {
    param(
        [Parameter(Mandatory = $true)][string]$SourceRelativePath,
        [Parameter(Mandatory = $true)][string]$BuildRelativePath,
        [string]$Config = $defaultConfig
    )

    $sourceDir = Join-Path $repoRoot $SourceRelativePath
    $buildDir = Join-Path $repoRoot $BuildRelativePath

    Invoke-External -Command "cmake" -Arguments @(
        "-S", $sourceDir,
        "-B", $buildDir,
        "-G", $defaultGenerator,
        "-A", $defaultArch
    ) | Out-Null

    Invoke-External -Command "cmake" -Arguments @(
        "--build", $buildDir,
        "--config", $Config
    ) | Out-Null

    Invoke-External -Command "ctest" -Arguments @(
        "--test-dir", $buildDir,
        "-C", $Config,
        "--output-on-failure"
    ) | Out-Null
}

function Invoke-PlatformNativeTests {
    param([string]$Config = $defaultConfig)

    $scriptPath = Join-Path $repoRoot "tests\test_platform\run_native_tests.ps1"
    if (-not (Test-Path $scriptPath)) {
        Fail "Missing script: $scriptPath"
    }

    & powershell -NoProfile -ExecutionPolicy Bypass -File $scriptPath `
        -BuildDir "build/platform-native-tests" `
        -Config $Config `
        -Generator $defaultGenerator `
        -Arch $defaultArch
    if ($LASTEXITCODE -ne 0) {
        Fail "Platform native tests failed."
    }
}

function Invoke-TelemetryNativeTests {
    param([string]$Config = $defaultConfig)

    $scriptPath = Join-Path $repoRoot "tests\test_telemetry\run_native_tests.ps1"
    if (-not (Test-Path $scriptPath)) {
        Fail "Missing script: $scriptPath"
    }

    & powershell -NoProfile -ExecutionPolicy Bypass -File $scriptPath -Configuration $Config
    if ($LASTEXITCODE -ne 0) {
        Fail "Telemetry native tests failed."
    }
}
