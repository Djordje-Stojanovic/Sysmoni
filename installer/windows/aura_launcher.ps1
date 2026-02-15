param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$RemainingArgs
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..")).Path
$defaultGenerator = "Visual Studio 17 2022"
$defaultArch = "x64"
$defaultConfig = "Release"
$debugLogPath = Join-Path $repoRoot "build\aura_launcher.debug.log"

function Write-Info {
    param([Parameter(Mandatory = $true)][string]$Message)
    Write-Host "[aura] $Message"
}

function Write-WarnInfo {
    param([Parameter(Mandatory = $true)][string]$Message)
    Write-Host "[aura] WARNING: $Message" -ForegroundColor Yellow
}

function Fail {
    param([Parameter(Mandatory = $true)][string]$Message)
    throw "[aura] $Message"
}

function Write-DebugTrace {
    param([Parameter(Mandatory = $true)][string]$Message)
    if ($env:AURA_LAUNCHER_DEBUG -ne "1") {
        return
    }

    $debugDir = Split-Path -Parent $debugLogPath
    if (-not (Test-Path $debugDir)) {
        New-Item -ItemType Directory -Force -Path $debugDir | Out-Null
    }
    Add-Content -Path $debugLogPath -Value ("{0} {1}" -f (Get-Date -Format o), $Message)
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

function Convert-ToArgArray {
    param(
        [AllowNull()]
        [object]$Value
    )

    if ($null -eq $Value) {
        return ,@()
    }

    return ,@($Value)
}

function Invoke-External {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [switch]$AllowFailure
    )

    & $Command @Arguments
    $exitCode = $LASTEXITCODE
    if (-not $AllowFailure -and $exitCode -ne 0) {
        Fail "$Command failed with exit code $exitCode."
    }
    return $exitCode
}

function Resolve-Qt6Dir {
    param([string]$Preferred = "")

    if ($Preferred) {
        if (-not (Test-Path $Preferred)) {
            Fail "Qt6Dir does not exist: $Preferred"
        }
        return (Resolve-Path $Preferred).Path
    }

    $candidates = New-Object System.Collections.Generic.List[string]

    if ($env:Qt6_DIR) {
        $candidates.Add($env:Qt6_DIR)
    }

    if ($env:CMAKE_PREFIX_PATH) {
        foreach ($entry in ($env:CMAKE_PREFIX_PATH -split ";")) {
            if ([string]::IsNullOrWhiteSpace($entry)) {
                continue
            }
            $prefix = $entry.Trim()
            if ($prefix -like "*Qt6") {
                $candidates.Add($prefix)
            }
            $candidates.Add((Join-Path $prefix "lib\cmake\Qt6"))
        }
    }

    $candidates.Add("C:\Qt\6.10.2\msvc2022_64\lib\cmake\Qt6")

    if (Test-Path "C:\Qt") {
        Get-ChildItem "C:\Qt" -Directory -ErrorAction SilentlyContinue |
            ForEach-Object {
                Get-ChildItem $_.FullName -Directory -ErrorAction SilentlyContinue |
                    Where-Object { $_.Name -like "msvc*" } |
                    ForEach-Object { $candidates.Add((Join-Path $_.FullName "lib\cmake\Qt6")) }
            }
    }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

function Set-QtRuntimeEnvironment {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Qt6Dir
    )

    $qtRoot = Resolve-Path (Join-Path $Qt6Dir "..\..\..") -ErrorAction SilentlyContinue
    if (-not $qtRoot) {
        return
    }

    $qtRootPath = $qtRoot.Path
    $qtBin = Join-Path $qtRootPath "bin"
    $qtPlugins = Join-Path $qtRootPath "plugins"
    $qtQml = Join-Path $qtRootPath "qml"

    if (Test-Path $qtBin) {
        $pathParts = $env:Path -split ";"
        if (-not ($pathParts -contains $qtBin)) {
            $env:Path = "$qtBin;$env:Path"
        }
    }

    if (Test-Path $qtPlugins) {
        $env:QT_PLUGIN_PATH = $qtPlugins
    }
    if (Test-Path $qtQml) {
        $env:QML2_IMPORT_PATH = $qtQml
    }
    $env:Qt6_DIR = $Qt6Dir
}

function Get-GuiExeCandidates {
    param([string]$Config = $defaultConfig)
    return @(
        (Join-Path $repoRoot "build\shell-native\dist\aura_shell.exe"),
        (Join-Path $repoRoot ("build\shell-native\{0}\aura_shell.exe" -f $Config)),
        (Join-Path $repoRoot "build\shell-native\aura_shell.exe")
    )
}

function Get-CliExeCandidates {
    param([string]$Config = $defaultConfig)
    return @(
        (Join-Path $repoRoot "build\platform-native\dist\aura.exe"),
        (Join-Path $repoRoot ("build\platform-native\{0}\aura.exe" -f $Config)),
        (Join-Path $repoRoot "build\platform-native\aura.exe")
    )
}

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

function Invoke-RootBuild {
    param([string]$Config = $defaultConfig)

    $buildDir = Join-Path $repoRoot "build"
    Invoke-External -Command "cmake" -Arguments @(
        "-S", $repoRoot,
        "-B", $buildDir,
        "-G", $defaultGenerator,
        "-A", $defaultArch
    ) | Out-Null

    Invoke-External -Command "cmake" -Arguments @(
        "--build", $buildDir,
        "--config", $Config
    ) | Out-Null
}

function Invoke-RootTests {
    param(
        [string]$Config = $defaultConfig,
        [string]$Regex = ""
    )

    $args = @("--test-dir", (Join-Path $repoRoot "build"), "-C", $Config, "--output-on-failure")
    if ($Regex) {
        $args += @("--tests-regex", $Regex)
    }
    Invoke-External -Command "ctest" -Arguments $args | Out-Null
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

function Confirm-Action {
    param(
        [Parameter(Mandatory = $true)][string]$Prompt,
        [switch]$Force
    )
    if ($Force) {
        return $true
    }
    $answer = Read-Host "$Prompt [y/N]"
    return $answer -match "^(y|yes)$"
}

function Copy-DirectoryContent {
    param(
        [Parameter(Mandatory = $true)][string]$SourceDir,
        [Parameter(Mandatory = $true)][string]$DestDir
    )

    if (-not (Test-Path $SourceDir)) {
        Fail "Source directory does not exist: $SourceDir"
    }
    New-Item -ItemType Directory -Force -Path $DestDir | Out-Null
    Copy-Item -Path (Join-Path $SourceDir "*") -Destination $DestDir -Recurse -Force
}

function Write-CmdLaunchers {
    param(
        [Parameter(Mandatory = $true)][string]$InstallRoot
    )

    $guiLauncher = @(
        "@echo off",
        "setlocal",
        "set ""AURA_HOME=%~dp0current""",
        "set ""AURA_EXE=%AURA_HOME%\gui\aura_shell.exe""",
        "if not exist ""%AURA_EXE%"" (",
        "  echo [aura] GUI executable not found: %AURA_EXE%",
        "  exit /b 1",
        ")",
        """%AURA_EXE%"" %*",
        "exit /b %errorlevel%"
    )
    $cliLauncher = @(
        "@echo off",
        "setlocal",
        "set ""AURA_HOME=%~dp0current""",
        "set ""AURA_EXE=%AURA_HOME%\cli\aura.exe""",
        "if not exist ""%AURA_EXE%"" (",
        "  echo [aura] CLI executable not found: %AURA_EXE%",
        "  exit /b 1",
        ")",
        """%AURA_EXE%"" %*",
        "exit /b %errorlevel%"
    )

    Set-Content -Path (Join-Path $InstallRoot "aura-gui.cmd") -Value $guiLauncher -Encoding Ascii
    Set-Content -Path (Join-Path $InstallRoot "aura-cli.cmd") -Value $cliLauncher -Encoding Ascii
}

function Try-CreateDesktopShortcut {
    param(
        [Parameter(Mandatory = $true)][string]$TargetPath
    )

    try {
        $desktopDir = [Environment]::GetFolderPath("Desktop")
        if (-not $desktopDir) {
            return
        }
        $shortcutPath = Join-Path $desktopDir "Aura.lnk"
        $shell = New-Object -ComObject WScript.Shell
        $shortcut = $shell.CreateShortcut($shortcutPath)
        $shortcut.TargetPath = $TargetPath
        $shortcut.WorkingDirectory = Split-Path -Parent $TargetPath
        $shortcut.WindowStyle = 1
        $shortcut.Description = "Aura System Monitor"
        $shortcut.Save()
    } catch {
        Write-WarnInfo "Could not create Desktop shortcut automatically."
    }
}

function Get-InstallRoot {
    $base = [Environment]::GetFolderPath("LocalApplicationData")
    if (-not $base) {
        Fail "Could not resolve LocalApplicationData."
    }
    return (Join-Path $base "Aura")
}

function Show-Usage {
    Write-Host @"
Aura user launcher

Usage:
  .\aura.cmd                      # launch GUI (default)
  .\aura.cmd gui [args...]        # launch GUI
  .\aura.cmd cli [args...]        # launch CLI
  .\aura.cmd launch [gui|cli]     # explicit launch mode

  .\aura.cmd build [all|gui|cli] [--debug|--release] [--qt6dir <path>] [--force]
  .\aura.cmd test [all|shell|render|telemetry|platform] [--debug|--release]
  .\aura.cmd clean [--force] [--dry-run]
  .\aura.cmd update [--dry-run]
  .\aura.cmd install [--debug|--release] [--qt6dir <path>]
  .\aura.cmd uninstall [--force] [--dry-run]
  .\aura.cmd doctor
  .\aura.cmd help

Compatibility:
  .\aura.cmd --gui ...
  .\aura.cmd --cli ...
  .\aura.cmd --json --no-persist   # legacy CLI flag forwarding

Friendly flag aliases:
  debug | release | force | dryrun
  config debug|release
  qt6dir <path> or qt6dir=<path>

Examples:
  .\aura.cmd build all debug
  .\aura.cmd test platform debug
  .\aura.cmd clean dryrun
  .\aura.cmd uninstall force
"@
}

function Parse-CommonFlags {
    param(
        [string[]]$InputArgs,
        [switch]$AllowQt6Dir,
        [switch]$AllowForce,
        [switch]$AllowDryRun
    )

    $config = $defaultConfig
    $qt6Dir = ""
    $force = $false
    $dryRun = $false
    $positional = New-Object System.Collections.Generic.List[string]

    $normalizedInput = Convert-ToArgArray -Value $InputArgs

    for ($i = 0; $i -lt $normalizedInput.Count; $i++) {
        $token = $normalizedInput[$i]
        $lower = $token.ToLowerInvariant()
        $handled = $false

        if ($AllowQt6Dir -and (
            $lower.StartsWith("--qt6dir=") -or
            $lower.StartsWith("-qt6dir=") -or
            $lower.StartsWith("/qt6dir=") -or
            $lower.StartsWith("qt6dir=")
        )) {
            $eqIndex = $token.IndexOf("=")
            if ($eqIndex -lt 0 -or $eqIndex -eq ($token.Length - 1)) {
                Fail "qt6dir requires a non-empty path."
            }
            $qt6Dir = $token.Substring($eqIndex + 1)
            $handled = $true
        }

        if ($handled) {
            continue
        }

        switch ($lower) {
            "--debug" {
                $config = "Debug"
                $handled = $true
                break
            }
            "-debug" {
                $config = "Debug"
                $handled = $true
                break
            }
            "/debug" {
                $config = "Debug"
                $handled = $true
                break
            }
            "debug" {
                $config = "Debug"
                $handled = $true
                break
            }
            "--release" {
                $config = "Release"
                $handled = $true
                break
            }
            "-release" {
                $config = "Release"
                $handled = $true
                break
            }
            "/release" {
                $config = "Release"
                $handled = $true
                break
            }
            "release" {
                $config = "Release"
                $handled = $true
                break
            }
            "--config" {
                if ($i + 1 -ge $normalizedInput.Count) {
                    Fail "--config requires debug or release."
                }
                $i += 1
                $next = $normalizedInput[$i].ToLowerInvariant()
                if ($next -eq "debug") {
                    $config = "Debug"
                } elseif ($next -eq "release") {
                    $config = "Release"
                } else {
                    Fail "--config accepts only debug or release."
                }
                $handled = $true
                break
            }
            "-config" {
                if ($i + 1 -ge $normalizedInput.Count) {
                    Fail "-config requires debug or release."
                }
                $i += 1
                $next = $normalizedInput[$i].ToLowerInvariant()
                if ($next -eq "debug") {
                    $config = "Debug"
                } elseif ($next -eq "release") {
                    $config = "Release"
                } else {
                    Fail "-config accepts only debug or release."
                }
                $handled = $true
                break
            }
            "/config" {
                if ($i + 1 -ge $normalizedInput.Count) {
                    Fail "/config requires debug or release."
                }
                $i += 1
                $next = $normalizedInput[$i].ToLowerInvariant()
                if ($next -eq "debug") {
                    $config = "Debug"
                } elseif ($next -eq "release") {
                    $config = "Release"
                } else {
                    Fail "/config accepts only debug or release."
                }
                $handled = $true
                break
            }
            "config" {
                if ($i + 1 -ge $normalizedInput.Count) {
                    Fail "config requires debug or release."
                }
                $i += 1
                $next = $normalizedInput[$i].ToLowerInvariant()
                if ($next -eq "debug") {
                    $config = "Debug"
                } elseif ($next -eq "release") {
                    $config = "Release"
                } else {
                    Fail "config accepts only debug or release."
                }
                $handled = $true
                break
            }
            "--force" {
                if ($AllowForce) {
                    $force = $true
                    $handled = $true
                }
                break
            }
            "-force" {
                if ($AllowForce) {
                    $force = $true
                    $handled = $true
                }
                break
            }
            "/force" {
                if ($AllowForce) {
                    $force = $true
                    $handled = $true
                }
                break
            }
            "force" {
                if ($AllowForce) {
                    $force = $true
                    $handled = $true
                }
                break
            }
            "--dry-run" {
                if ($AllowDryRun) {
                    $dryRun = $true
                    $handled = $true
                }
                break
            }
            "-dry-run" {
                if ($AllowDryRun) {
                    $dryRun = $true
                    $handled = $true
                }
                break
            }
            "/dry-run" {
                if ($AllowDryRun) {
                    $dryRun = $true
                    $handled = $true
                }
                break
            }
            "--dryrun" {
                if ($AllowDryRun) {
                    $dryRun = $true
                    $handled = $true
                }
                break
            }
            "-dryrun" {
                if ($AllowDryRun) {
                    $dryRun = $true
                    $handled = $true
                }
                break
            }
            "/dryrun" {
                if ($AllowDryRun) {
                    $dryRun = $true
                    $handled = $true
                }
                break
            }
            "dry-run" {
                if ($AllowDryRun) {
                    $dryRun = $true
                    $handled = $true
                }
                break
            }
            "dryrun" {
                if ($AllowDryRun) {
                    $dryRun = $true
                    $handled = $true
                }
                break
            }
            "--qt6dir" {
                if ($AllowQt6Dir) {
                    if ($i + 1 -ge $normalizedInput.Count) {
                        Fail "--qt6dir requires a path."
                    }
                    $i += 1
                    $qt6Dir = $normalizedInput[$i]
                    $handled = $true
                }
                break
            }
            "-qt6dir" {
                if ($AllowQt6Dir) {
                    if ($i + 1 -ge $normalizedInput.Count) {
                        Fail "-qt6dir requires a path."
                    }
                    $i += 1
                    $qt6Dir = $normalizedInput[$i]
                    $handled = $true
                }
                break
            }
            "/qt6dir" {
                if ($AllowQt6Dir) {
                    if ($i + 1 -ge $normalizedInput.Count) {
                        Fail "/qt6dir requires a path."
                    }
                    $i += 1
                    $qt6Dir = $normalizedInput[$i]
                    $handled = $true
                }
                break
            }
            "qt6dir" {
                if ($AllowQt6Dir) {
                    if ($i + 1 -ge $normalizedInput.Count) {
                        Fail "qt6dir requires a path."
                    }
                    $i += 1
                    $qt6Dir = $normalizedInput[$i]
                    $handled = $true
                }
                break
            }
        }
        if (-not $handled) {
            $positional.Add($token)
        }
    }

    return @{
        Config = $config
        Qt6Dir = $qt6Dir
        Force = $force
        DryRun = $dryRun
        Positional = $positional.ToArray()
    }
}

function Invoke-BuildCommand {
    param(
        [string]$Target = "all",
        [string]$Config = $defaultConfig,
        [string]$Qt6Dir = "",
        [switch]$Force
    )

    $targetLower = $Target.ToLowerInvariant()
    switch ($targetLower) {
        "all" {
            $null = Ensure-CliArtifacts -ForceBuild:$Force -Config $Config
            try {
                $null = Ensure-GuiArtifacts -ForceBuild:$Force -Config $Config -Qt6Dir $Qt6Dir
            } catch {
                Write-WarnInfo ("GUI build/deploy skipped for 'build all': {0}" -f $_.Exception.Message)
                Write-Info "CLI build succeeded. To enable GUI build, install Qt6 and run: .\aura.cmd build gui --qt6dir <path>"
            }
        }
        "gui" {
            $null = Ensure-GuiArtifacts -ForceBuild:$Force -Config $Config -Qt6Dir $Qt6Dir
        }
        "cli" {
            $null = Ensure-CliArtifacts -ForceBuild:$Force -Config $Config
        }
        default {
            Fail "Unknown build target '$Target'. Use all/gui/cli."
        }
    }

    Write-Info "Build completed: target=$targetLower config=$Config"
}

function Invoke-TestCommand {
    param(
        [string]$Target = "all",
        [string]$Config = $defaultConfig
    )

    $targetLower = $Target.ToLowerInvariant()
    switch ($targetLower) {
        "all" {
            Invoke-PlatformNativeTests -Config $Config
            Invoke-TelemetryNativeTests -Config $Config
            Invoke-CmakeTestSuite -SourceRelativePath "tests\test_render" -BuildRelativePath "build\render-native-tests" -Config $Config
            Invoke-CmakeTestSuite -SourceRelativePath "tests\test_shell" -BuildRelativePath "build\shell-native-tests" -Config $Config
        }
        "shell" {
            Invoke-CmakeTestSuite -SourceRelativePath "tests\test_shell" -BuildRelativePath "build\shell-native-tests" -Config $Config
        }
        "render" {
            Invoke-CmakeTestSuite -SourceRelativePath "tests\test_render" -BuildRelativePath "build\render-native-tests" -Config $Config
        }
        "telemetry" {
            Invoke-TelemetryNativeTests -Config $Config
        }
        "platform" {
            Invoke-PlatformNativeTests -Config $Config
        }
        default { Fail "Unknown test target '$Target'. Use all/shell/render/telemetry/platform." }
    }

    Write-Info "Tests passed: target=$targetLower config=$Config"
}

function Invoke-CleanCommand {
    param(
        [switch]$Force,
        [switch]$DryRun
    )

    $targets = @(
        (Join-Path $repoRoot "build"),
        (Join-Path $repoRoot "build-plan")
    ) | Where-Object { Test-Path $_ }

    if ($targets.Count -eq 0) {
        Write-Info "Nothing to clean."
        return
    }

    if ($DryRun) {
        Write-Info "Dry run: would remove"
        foreach ($path in $targets) {
            Write-Host "  $path"
        }
        return
    }

    if (-not (Confirm-Action -Prompt "Remove build artifacts under repo?" -Force:$Force)) {
        Write-Info "Clean cancelled."
        return
    }

    foreach ($path in $targets) {
        Remove-Item -Path $path -Recurse -Force
        Write-Info "Removed $path"
    }
}

function Invoke-UpdateCommand {
    param([switch]$DryRun)

    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        Fail "git is not installed or not on PATH."
    }

    if ($DryRun) {
        Write-Info "Dry run: git -C `"$repoRoot`" fetch --all --prune"
        Write-Info "Dry run: git -C `"$repoRoot`" pull --ff-only"
        return
    }

    Write-Info "Fetching latest refs..."
    Invoke-External -Command "git" -Arguments @("-C", $repoRoot, "fetch", "--all", "--prune") | Out-Null

    Write-Info "Pulling latest changes (fast-forward only)..."
    $pullCode = Invoke-External -Command "git" -Arguments @("-C", $repoRoot, "pull", "--ff-only") -AllowFailure
    if ($pullCode -ne 0) {
        Fail "Update failed. Resolve local git state (commit/stash/rebase), then retry."
    }

    Write-Info "Repository updated."
}

function Invoke-InstallCommand {
    param(
        [string]$Config = $defaultConfig,
        [string]$Qt6Dir = ""
    )

    $null = Ensure-CliArtifacts -Config $Config
    $null = Ensure-GuiArtifacts -Config $Config -Qt6Dir $Qt6Dir

    $installRoot = Get-InstallRoot
    $currentRoot = Join-Path $installRoot "current"
    $guiSource = Join-Path $repoRoot "build\shell-native\dist"
    $cliSource = Join-Path $repoRoot "build\platform-native\dist"
    $guiTarget = Join-Path $currentRoot "gui"
    $cliTarget = Join-Path $currentRoot "cli"

    New-Item -ItemType Directory -Force -Path $installRoot | Out-Null
    if (Test-Path $currentRoot) {
        Remove-Item -Path $currentRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $currentRoot | Out-Null

    Copy-DirectoryContent -SourceDir $guiSource -DestDir $guiTarget
    Copy-DirectoryContent -SourceDir $cliSource -DestDir $cliTarget
    Write-CmdLaunchers -InstallRoot $installRoot
    Try-CreateDesktopShortcut -TargetPath (Join-Path $installRoot "aura-gui.cmd")

    Write-Info "Installed Aura to: $installRoot"
    Write-Info "User launchers:"
    Write-Host "  $installRoot\aura-gui.cmd"
    Write-Host "  $installRoot\aura-cli.cmd"
}

function Invoke-UninstallCommand {
    param(
        [switch]$Force,
        [switch]$DryRun
    )

    $installRoot = Get-InstallRoot
    $desktopShortcut = Join-Path ([Environment]::GetFolderPath("Desktop")) "Aura.lnk"

    if ($DryRun) {
        Write-Info "Dry run: would remove"
        Write-Host "  $installRoot"
        Write-Host "  $desktopShortcut"
        return
    }

    if (-not (Confirm-Action -Prompt "Uninstall Aura from user profile?" -Force:$Force)) {
        Write-Info "Uninstall cancelled."
        return
    }

    if (Test-Path $installRoot) {
        Remove-Item -Path $installRoot -Recurse -Force
        Write-Info "Removed $installRoot"
    } else {
        Write-Info "Install root not found; nothing to remove."
    }

    if ($desktopShortcut -and (Test-Path $desktopShortcut)) {
        Remove-Item -Path $desktopShortcut -Force
        Write-Info "Removed $desktopShortcut"
    }
}

function Invoke-DoctorCommand {
    $checks = @()
    $checks += @{ Name = "git"; Present = ($null -ne (Get-Command git -ErrorAction SilentlyContinue)) }
    $checks += @{ Name = "cmake"; Present = ($null -ne (Get-Command cmake -ErrorAction SilentlyContinue)) }
    $checks += @{ Name = "ctest"; Present = ($null -ne (Get-Command ctest -ErrorAction SilentlyContinue)) }
    $checks += @{ Name = "powershell"; Present = ($null -ne (Get-Command powershell -ErrorAction SilentlyContinue)) }

    Write-Info "Repo: $repoRoot"
    foreach ($check in $checks) {
        $status = if ($check.Present) { "ok" } else { "missing" }
        Write-Host (" - {0}: {1}" -f $check.Name, $status)
    }

    $qt6 = Resolve-Qt6Dir
    if ($qt6) {
        Write-Host (" - Qt6_DIR: {0}" -f $qt6)
    } else {
        Write-Host " - Qt6_DIR: not found (GUI build will need Qt install/path)"
    }

    $guiPath = Resolve-FirstExistingPath -Candidates (Get-GuiExeCandidates)
    $cliPath = Resolve-FirstExistingPath -Candidates (Get-CliExeCandidates)
    Write-Host (" - GUI binary: {0}" -f ($(if ($guiPath) { $guiPath } else { "missing" })))
    Write-Host (" - CLI binary: {0}" -f ($(if ($cliPath) { $cliPath } else { "missing" })))
}

function Get-SubArgs {
    param([string[]]$InputArgs, [int]$StartAt)
    $normalizedArgs = Convert-ToArgArray -Value $InputArgs
    if ($normalizedArgs.Count -le $StartAt) {
        return @()
    }
    return $normalizedArgs[$StartAt..($normalizedArgs.Count - 1)]
}

function Dispatch-Command {
    param([string[]]$CommandArgs)

    $normalizedArgs = Convert-ToArgArray -Value $CommandArgs
    Write-DebugTrace ("Dispatch count={0} args={1}" -f $normalizedArgs.Count, ($normalizedArgs -join " | "))

    if ($normalizedArgs.Count -eq 0) {
        Write-DebugTrace "Route default_gui_no_args"
        return (Invoke-Gui -ForwardArgs @())
    }

    $first = $normalizedArgs[0]
    $firstLower = $first.ToLowerInvariant()
    Write-DebugTrace ("Route first={0}" -f $first)

    if ($first.StartsWith("-")) {
        switch ($firstLower) {
            "--help" { Show-Usage; return 0 }
            "-h" { Show-Usage; return 0 }
            "-?" { Show-Usage; return 0 }
            "--gui" {
                return (Invoke-Gui -ForwardArgs (Get-SubArgs -InputArgs $normalizedArgs -StartAt 1))
            }
            "--cli" {
                return (Invoke-Cli -ForwardArgs (Get-SubArgs -InputArgs $normalizedArgs -StartAt 1))
            }
            default {
                # Legacy behavior: raw flags map to CLI runtime.
                return (Invoke-Cli -ForwardArgs $normalizedArgs)
            }
        }
    }

    switch ($firstLower) {
        "help" { Show-Usage; return 0 }
        "gui" {
            return (Invoke-Gui -ForwardArgs (Get-SubArgs -InputArgs $normalizedArgs -StartAt 1))
        }
        "cli" {
            return (Invoke-Cli -ForwardArgs (Get-SubArgs -InputArgs $normalizedArgs -StartAt 1))
        }
        "launch" {
            $rest = Get-SubArgs -InputArgs $normalizedArgs -StartAt 1
            if ($rest.Count -gt 0 -and ($rest[0].ToLowerInvariant() -eq "cli")) {
                return (Invoke-Cli -ForwardArgs (Get-SubArgs -InputArgs $rest -StartAt 1))
            }
            if ($rest.Count -gt 0 -and ($rest[0].ToLowerInvariant() -eq "gui")) {
                return (Invoke-Gui -ForwardArgs (Get-SubArgs -InputArgs $rest -StartAt 1))
            }
            return (Invoke-Gui -ForwardArgs $rest)
        }
        "build" {
            $parsed = Parse-CommonFlags -InputArgs (Get-SubArgs -InputArgs $normalizedArgs -StartAt 1) -AllowQt6Dir -AllowForce
            $target = if ($parsed.Positional.Count -gt 0) { $parsed.Positional[0] } else { "all" }
            if ($parsed.Positional.Count -gt 1) {
                Fail "Too many positional arguments for build."
            }
            Invoke-BuildCommand -Target $target -Config $parsed.Config -Qt6Dir $parsed.Qt6Dir -Force:$parsed.Force
            return 0
        }
        "test" {
            $parsed = Parse-CommonFlags -InputArgs (Get-SubArgs -InputArgs $normalizedArgs -StartAt 1)
            $target = if ($parsed.Positional.Count -gt 0) { $parsed.Positional[0] } else { "all" }
            if ($parsed.Positional.Count -gt 1) {
                Fail "Too many positional arguments for test."
            }
            Invoke-TestCommand -Target $target -Config $parsed.Config
            return 0
        }
        "clean" {
            $parsed = Parse-CommonFlags -InputArgs (Get-SubArgs -InputArgs $normalizedArgs -StartAt 1) -AllowForce -AllowDryRun
            if ($parsed.Positional.Count -gt 0) {
                Fail "clean does not accept positional arguments."
            }
            Invoke-CleanCommand -Force:$parsed.Force -DryRun:$parsed.DryRun
            return 0
        }
        "update" {
            $parsed = Parse-CommonFlags -InputArgs (Get-SubArgs -InputArgs $normalizedArgs -StartAt 1) -AllowDryRun
            if ($parsed.Positional.Count -gt 0) {
                Fail "update does not accept positional arguments."
            }
            Invoke-UpdateCommand -DryRun:$parsed.DryRun
            return 0
        }
        "install" {
            $parsed = Parse-CommonFlags -InputArgs (Get-SubArgs -InputArgs $normalizedArgs -StartAt 1) -AllowQt6Dir
            if ($parsed.Positional.Count -gt 0) {
                Fail "install does not accept positional arguments."
            }
            Invoke-InstallCommand -Config $parsed.Config -Qt6Dir $parsed.Qt6Dir
            return 0
        }
        "uninstall" {
            $parsed = Parse-CommonFlags -InputArgs (Get-SubArgs -InputArgs $normalizedArgs -StartAt 1) -AllowForce -AllowDryRun
            if ($parsed.Positional.Count -gt 0) {
                Fail "uninstall does not accept positional arguments."
            }
            Invoke-UninstallCommand -Force:$parsed.Force -DryRun:$parsed.DryRun
            return 0
        }
        "deinstall" {
            $parsed = Parse-CommonFlags -InputArgs (Get-SubArgs -InputArgs $normalizedArgs -StartAt 1) -AllowForce -AllowDryRun
            if ($parsed.Positional.Count -gt 0) {
                Fail "deinstall does not accept positional arguments."
            }
            Invoke-UninstallCommand -Force:$parsed.Force -DryRun:$parsed.DryRun
            return 0
        }
        "doctor" {
            Invoke-DoctorCommand
            return 0
        }
        default {
            # Friendly fallback: unknown command is passed as CLI arguments.
            return (Invoke-Cli -ForwardArgs $normalizedArgs)
        }
    }
}

try {
    $effectiveArgs = Convert-ToArgArray -Value $RemainingArgs
    Write-DebugTrace ("Entry RemainingArgs={0}" -f (@($RemainingArgs) -join " | "))
    Write-DebugTrace ("Entry effectiveArgs={0}" -f (@($effectiveArgs) -join " | "))

    $code = Dispatch-Command -CommandArgs $effectiveArgs
    exit $code
} catch {
    Write-DebugTrace ("ERROR message={0}" -f $_.Exception.Message)
    if ($_.InvocationInfo -and $_.InvocationInfo.PositionMessage) {
        Write-DebugTrace ("ERROR position={0}" -f $_.InvocationInfo.PositionMessage.Replace([Environment]::NewLine, " "))
    }
    if ($_.ScriptStackTrace) {
        Write-DebugTrace ("ERROR stack={0}" -f $_.ScriptStackTrace.Replace([Environment]::NewLine, " || "))
    }
    Write-Error $_.Exception.Message
    exit 1
}
