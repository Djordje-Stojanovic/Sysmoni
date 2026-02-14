param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$RemainingArgs
)

$ErrorActionPreference = "Stop"

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

function Resolve-Qt6Dir {
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

function Invoke-BuildScript {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScriptPath,
        [string]$Qt6Dir,
        [switch]$DeployOnly
    )

    $args = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $ScriptPath)
    if ($DeployOnly) {
        $args += "-DeployOnly"
    }
    if ($Qt6Dir) {
        $args += @("-Qt6Dir", $Qt6Dir)
    }
    & powershell @args
    if ($LASTEXITCODE -ne 0) {
        throw "[aura] build failed."
    }
}

function Ensure-GuiArtifacts {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string[]]$ExeCandidates
    )

    $buildScript = Join-Path $RepoRoot "installer\windows\build_shell_native.ps1"
    $distDir = Join-Path $RepoRoot "build\shell-native\dist"
    $distExe = Join-Path $distDir "aura_shell.exe"
    $bridgeDlls = @(
        "aura_telemetry_native.dll",
        "aura_render_native.dll",
        "aura_platform.dll"
    )
    $needsBuild = -not (Resolve-FirstExistingPath -Candidates $ExeCandidates)
    $missingBridgeDll = $false
    foreach ($dllName in $bridgeDlls) {
        if (-not (Test-Path (Join-Path $distDir $dllName))) {
            $missingBridgeDll = $true
            break
        }
    }
    $needsDeploy = (Test-Path $distExe) -and
                   ((-not (Test-Path (Join-Path $distDir "Qt6Core.dll"))) -or
                    (-not (Test-Path (Join-Path $distDir "platforms\qwindows.dll"))) -or
                    $missingBridgeDll)

    if ($needsBuild -or $needsDeploy) {
        $qt6Dir = Resolve-Qt6Dir
        if ($needsBuild) {
            Write-Host "[aura] GUI binary not found, building once..."
            Invoke-BuildScript -ScriptPath $buildScript -Qt6Dir $qt6Dir
        } else {
            Write-Host "[aura] GUI runtime files missing, repairing deployment..."
            Invoke-BuildScript -ScriptPath $buildScript -Qt6Dir $qt6Dir -DeployOnly
        }
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..")).Path

$mode = "gui"
$forwardArgs = @()
if ($RemainingArgs.Count -gt 0) {
    if ($RemainingArgs[0] -eq "--gui") {
        $mode = "gui"
        if ($RemainingArgs.Count -gt 1) {
            $forwardArgs = $RemainingArgs[1..($RemainingArgs.Count - 1)]
        }
    } elseif ($RemainingArgs[0] -eq "--cli") {
        $mode = "cli"
        if ($RemainingArgs.Count -gt 1) {
            $forwardArgs = $RemainingArgs[1..($RemainingArgs.Count - 1)]
        }
    } else {
        # Keep legacy behavior: explicit runtime flags continue to map to CLI.
        $mode = "cli"
        $forwardArgs = $RemainingArgs
    }
}

if ($mode -eq "gui") {
    $exeCandidates = @(
        (Join-Path $repoRoot "build\shell-native\dist\aura_shell.exe"),
        (Join-Path $repoRoot "build\shell-native\Release\aura_shell.exe"),
        (Join-Path $repoRoot "build\shell-native\aura_shell.exe")
    )

    Ensure-GuiArtifacts -RepoRoot $repoRoot -ExeCandidates $exeCandidates

    $exePath = Resolve-FirstExistingPath -Candidates $exeCandidates
    if (-not $exePath) {
        throw "[aura] GUI build finished but aura_shell.exe was not found."
    }

    $qt6Dir = Resolve-Qt6Dir
    if ($qt6Dir) {
        Set-QtRuntimeEnvironment -Qt6Dir $qt6Dir
    }

    & $exePath @forwardArgs
    $exitCode = $LASTEXITCODE
    if ($exitCode -eq -1073741515 -or $exitCode -eq 3221225781) {
        Write-Host "[aura] GUI failed to start because required runtime DLLs are missing."
        Write-Host "[aura] Fix command:"
        Write-Host "  powershell -ExecutionPolicy Bypass -File installer/windows/build_shell_native.ps1 -Qt6Dir `"C:\Qt\6.10.2\msvc2022_64\lib\cmake\Qt6`""
        exit 1
    }
    exit $exitCode
}

$cliBuildScript = Join-Path $repoRoot "installer\windows\build_platform_native.ps1"
$cliExeCandidates = @(
    (Join-Path $repoRoot "build\platform-native\dist\aura.exe"),
    (Join-Path $repoRoot "build\platform-native\Release\aura.exe"),
    (Join-Path $repoRoot "build\platform-native\aura.exe")
)

$cliExePath = Resolve-FirstExistingPath -Candidates $cliExeCandidates
if (-not $cliExePath) {
    Write-Host "[aura] native CLI binary not found, building once..."
    Invoke-BuildScript -ScriptPath $cliBuildScript
    $cliExePath = Resolve-FirstExistingPath -Candidates $cliExeCandidates
}

if (-not $cliExePath) {
    throw "[aura] build finished but aura.exe was not found."
}

& $cliExePath @forwardArgs
exit $LASTEXITCODE
