param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$RemainingArgs
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildScript = Join-Path $repoRoot "installer\windows\build_platform_native.ps1"
$guiLauncher = Join-Path $repoRoot "aura-gui.ps1"

if ($RemainingArgs.Count -gt 0 -and $RemainingArgs[0] -eq "--gui") {
    $forwardArgs = @()
    if ($RemainingArgs.Count -gt 1) {
        $forwardArgs = $RemainingArgs[1..($RemainingArgs.Count - 1)]
    }
    & $guiLauncher @forwardArgs
    exit $LASTEXITCODE
}

$exeCandidates = @(
    (Join-Path $repoRoot "build\platform-native\dist\aura.exe"),
    (Join-Path $repoRoot "build\platform-native\Release\aura.exe"),
    (Join-Path $repoRoot "build\platform-native\aura.exe")
)

$exePath = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $exePath) {
    Write-Host "[aura] native binary not found, building once..."
    & powershell -NoProfile -ExecutionPolicy Bypass -File $buildScript
    if ($LASTEXITCODE -ne 0) {
        throw "[aura] build failed."
    }
    $exePath = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

if (-not $exePath) {
    throw "[aura] build finished but aura.exe was not found."
}

& $exePath @RemainingArgs
exit $LASTEXITCODE
