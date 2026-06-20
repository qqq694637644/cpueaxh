param(
    [string[]]$HeaderGlobs = @('cpueaxh/cpu/*.hpp', 'cpueaxh/memory/*.hpp'),
    [string]$WorkDir = '.header-smoke'
)

$ErrorActionPreference = 'Stop'

function Resolve-VsDevCmd {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw "vswhere.exe not found: $vswhere"
    }
    $install = & $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -latest
    if ([string]::IsNullOrWhiteSpace($install)) {
        throw 'Visual Studio C++ toolchain not found.'
    }
    $vcvars = Join-Path $install 'VC/Auxiliary/Build/vcvars64.bat'
    if (-not (Test-Path -LiteralPath $vcvars -PathType Leaf)) {
        throw "vcvars64.bat not found: $vcvars"
    }
    return $vcvars
}

$headers = @()
foreach ($glob in $HeaderGlobs) {
    $root = Split-Path -Path $glob -Parent
    $leaf = Split-Path -Path $glob -Leaf
    if (-not (Test-Path -LiteralPath $root -PathType Container)) {
        throw "Header smoke root not found: $root"
    }
    $headers += @(Get-ChildItem -LiteralPath $root -Filter $leaf -File | Sort-Object FullName)
}
$headers = @($headers | Sort-Object FullName -Unique)
if ($headers.Count -eq 0) {
    throw 'No headers selected for smoke validation.'
}

New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
$vcvarsPath = Resolve-VsDevCmd
$repoRoot = (Get-Location).Path
$logPath = Join-Path $WorkDir 'cpueaxh-header-smoke.log'
if (Test-Path -LiteralPath $logPath) {
    Remove-Item -LiteralPath $logPath -Force
}

foreach ($header in $headers) {
    $relative = $header.FullName.Substring($repoRoot.Length + 1).Replace('\\', '/')
    $safe = $relative -replace '[^A-Za-z0-9_]', '_'
    $source = Join-Path $WorkDir "$safe.cpp"
    $object = Join-Path $WorkDir "$safe.obj"
    @"
#include "$relative"
int cpueaxh_individual_header_smoke_$safe() { return 0; }
"@ | Set-Content -LiteralPath $source -Encoding utf8

    "## $relative" | Tee-Object -FilePath $logPath -Append
    $cmd = '"' + $vcvarsPath + '" >nul && cl.exe /nologo /W3 /WX /std:c++20 /permissive- /EHsc /c "' + (Resolve-Path $source) + '" /Fo"' + (Join-Path $repoRoot $object) + '" /I "' + $repoRoot + '"'
    cmd.exe /d /s /c $cmd 2>&1 | Tee-Object -FilePath $logPath -Append
    if ($LASTEXITCODE -ne 0) {
        throw "Header smoke compile failed for $relative"
    }
}

Write-Host "cpueaxh header smoke validation passed. Headers: $($headers.Count)"
