$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptDir = $PSScriptRoot
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
