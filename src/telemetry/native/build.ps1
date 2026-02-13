param(
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$includeDir = Join-Path $scriptRoot "include"
$sourceFile = Join-Path $scriptRoot "src\\aura_telemetry_native.cpp"
$outputDir = Join-Path $scriptRoot "bin"
$outputDll = Join-Path $outputDir "aura_telemetry_native.dll"

if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
    throw "cl.exe was not found. Install Visual Studio Build Tools and run from a Developer PowerShell."
}

New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

$optFlags = if ($Configuration -eq "Debug") { "/Od /Zi" } else { "/O2" }

& cl `
    /nologo `
    /std:c++20 `
    /EHsc `
    /LD `
    /MD `
    $optFlags `
    /I"$includeDir" `
    "$sourceFile" `
    /link `
    /OUT:"$outputDll" `
    iphlpapi.lib `
    psapi.lib

if ($LASTEXITCODE -ne 0) {
    throw "Native telemetry build failed with exit code $LASTEXITCODE."
}

Write-Host "Built native telemetry backend: $outputDll"
