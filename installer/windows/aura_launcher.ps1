param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$RemainingArgs
)

. (Join-Path $PSScriptRoot "aura_common.ps1")
. (Join-Path $PSScriptRoot "aura_qt.ps1")
. (Join-Path $PSScriptRoot "aura_argparse.ps1")
. (Join-Path $PSScriptRoot "aura_build.ps1")
. (Join-Path $PSScriptRoot "aura_install.ps1")

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
