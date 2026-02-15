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
