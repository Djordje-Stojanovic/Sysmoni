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

        # Strip prefix: --foo, -foo, /foo, foo -> foo
        $key = $lower -replace '^(--|[-/])', ''

        # Handle qt6dir=<path> inline
        if ($AllowQt6Dir -and $key.StartsWith("qt6dir=")) {
            $eqIndex = $token.IndexOf("=")
            if ($eqIndex -lt 0 -or $eqIndex -eq ($token.Length - 1)) {
                Fail "qt6dir requires a non-empty path."
            }
            $qt6Dir = $token.Substring($eqIndex + 1)
            continue
        }

        $handled = $false
        switch ($key) {
            "debug"   { $config = "Debug";   $handled = $true; break }
            "release" { $config = "Release"; $handled = $true; break }
            "config" {
                if ($i + 1 -ge $normalizedInput.Count) {
                    Fail "config requires debug or release."
                }
                $i += 1
                $next = $normalizedInput[$i].ToLowerInvariant()
                if ($next -eq "debug") { $config = "Debug" }
                elseif ($next -eq "release") { $config = "Release" }
                else { Fail "config accepts only debug or release." }
                $handled = $true; break
            }
            "force" {
                if ($AllowForce) { $force = $true; $handled = $true }
                break
            }
            { $_ -in "dry-run", "dryrun" } {
                if ($AllowDryRun) { $dryRun = $true; $handled = $true }
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
