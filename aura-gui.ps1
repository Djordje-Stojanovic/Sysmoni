param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$RemainingArgs
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildScript = Join-Path $repoRoot "installer\windows\build_shell_native.ps1"

$exeCandidates = @(
    (Join-Path $repoRoot "build\shell-native\dist\aura_shell.exe"),
    (Join-Path $repoRoot "build\shell-native\Release\aura_shell.exe"),
    (Join-Path $repoRoot "build\shell-native\aura_shell.exe")
)

$exePath = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $exePath) {
    Write-Host "[aura-gui] native GUI binary not found, building once..."
    & powershell -NoProfile -ExecutionPolicy Bypass -File $buildScript
    if ($LASTEXITCODE -ne 0) {
        throw "[aura-gui] build failed. If Qt6 is missing, run installer/windows/build_shell_native.ps1 with -Qt6Dir."
    }
    $exePath = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

if (-not $exePath) {
    throw "[aura-gui] build finished but aura_shell.exe was not found."
}

& $exePath @RemainingArgs
exit $LASTEXITCODE
