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
