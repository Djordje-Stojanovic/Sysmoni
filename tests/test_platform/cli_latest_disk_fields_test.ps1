param(
    [Parameter(Mandatory = $true)]
    [string]$AuraExe
)

$ErrorActionPreference = "Stop"

function Fail {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message
    )
    Write-Error $Message
    exit 1
}

if (-not (Test-Path $AuraExe)) {
    Fail "Aura executable was not found: $AuraExe"
}

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) "aura_platform_cli_tests"
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

$dbPath = Join-Path $tempRoot ("latest_disk_fields_" + [Guid]::NewGuid().ToString("N") + ".db")
$baseTimestamp = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()

$firstLine = "{0},{1},{2},{3},{4}" -f $baseTimestamp, 10.0, 20.0, 12345.0, 67890.0
$secondLine = "{0},{1},{2},{3},{4}" -f ($baseTimestamp + 1), 11.0, 21.0, 22222.0, 33333.0

Set-Content -Path $dbPath -Value @($firstLine, $secondLine) -Encoding ascii

try {
    $output = & $AuraExe --json --latest 1 --db-path $dbPath 2>&1
    if ($LASTEXITCODE -ne 0) {
        Fail ("aura --latest failed with exit code " + $LASTEXITCODE + ": " + ($output | Out-String))
    }

    $text = ($output | Out-String).Trim()
    if ([string]::IsNullOrWhiteSpace($text)) {
        Fail "aura --latest returned no output."
    }

    if ($text -notmatch '"disk_read_bps"\s*:') {
        Fail ("Expected disk_read_bps key in output. Actual: " + $text)
    }
    if ($text -notmatch '"disk_write_bps"\s*:') {
        Fail ("Expected disk_write_bps key in output. Actual: " + $text)
    }
    if ($text -notmatch '"disk_read_bps"\s*:\s*22222(\.0+)?') {
        Fail ("Expected latest disk_read_bps value 22222. Actual: " + $text)
    }
    if ($text -notmatch '"disk_write_bps"\s*:\s*33333(\.0+)?') {
        Fail ("Expected latest disk_write_bps value 33333. Actual: " + $text)
    }
} finally {
    Remove-Item -Path $dbPath -ErrorAction SilentlyContinue
    Remove-Item -Path ($dbPath + ".tmp") -ErrorAction SilentlyContinue
    Remove-Item -Path ($dbPath + ".legacy.sqlite") -ErrorAction SilentlyContinue
}

Write-Host "platform_cli_latest_preserves_disk_fields: PASS"
