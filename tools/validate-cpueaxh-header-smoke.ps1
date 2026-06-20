param(
    [string[]]$CoreHeaderGlobs = @('cpueaxh/cpu/*.hpp', 'cpueaxh/memory/*.hpp'),
    [string]$InstructionDir = 'cpueaxh/instructions',
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

function Add-SmokeCase {
    param(
        [Parameter(Mandatory = $true)][System.Collections.Generic.List[object]]$Cases,
        [Parameter(Mandatory = $true)]$Header,
        [string[]]$PreambleIncludes = @()
    )
    $Cases.Add([pscustomobject]@{
        Header = $Header
        PreambleIncludes = $PreambleIncludes
    })
}

$coreHeaders = @()
foreach ($glob in $CoreHeaderGlobs) {
    $root = Split-Path -Path $glob -Parent
    $leaf = Split-Path -Path $glob -Leaf
    if (-not (Test-Path -LiteralPath $root -PathType Container)) {
        throw "Header smoke root not found: $root"
    }
    $coreHeaders += @(Get-ChildItem -LiteralPath $root -Filter $leaf -File | Sort-Object FullName)
}
$coreHeaders = @($coreHeaders | Sort-Object FullName -Unique)

if (-not (Test-Path -LiteralPath $InstructionDir -PathType Container)) {
    throw "Instruction smoke root not found: $InstructionDir"
}
$instructionUmbrellas = @(Get-ChildItem -LiteralPath $InstructionDir -Filter '*_instructions.hpp' -File | ForEach-Object { $_.Name })
$instructionExcluded = @('instruction_common.hpp', 'all_instructions.hpp') + $instructionUmbrellas
$instructionHeaders = @(Get-ChildItem -LiteralPath $InstructionDir -Filter '*.hpp' -File |
    Where-Object { $_.Name -notin $instructionExcluded } |
    Sort-Object FullName)

$smokeCases = New-Object 'System.Collections.Generic.List[object]'
foreach ($header in $coreHeaders) {
    Add-SmokeCase -Cases $smokeCases -Header $header
}
foreach ($header in $instructionHeaders) {
    Add-SmokeCase -Cases $smokeCases -Header $header -PreambleIncludes @('cpueaxh/instructions/instruction_common.hpp')
}

if ($smokeCases.Count -eq 0) {
    throw 'No headers selected for smoke validation.'
}

New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
$vcvarsPath = Resolve-VsDevCmd
$repoRoot = (Get-Location).Path
$logPath = Join-Path $WorkDir 'cpueaxh-header-smoke.log'
if (Test-Path -LiteralPath $logPath) {
    Remove-Item -LiteralPath $logPath -Force
}

foreach ($case in $smokeCases) {
    $header = $case.Header
    $relative = $header.FullName.Substring($repoRoot.Length + 1).Replace('\\', '/')
    $safe = $relative -replace '[^A-Za-z0-9_]', '_'
    $source = Join-Path $WorkDir "$safe.cpp"
    $object = Join-Path $WorkDir "$safe.obj"
    $sourceLines = New-Object 'System.Collections.Generic.List[string]'
    foreach ($include in $case.PreambleIncludes) {
        $sourceLines.Add('#include "' + $include + '"')
    }
    $sourceLines.Add('#include "' + $relative + '"')
    $sourceLines.Add('int cpueaxh_individual_header_smoke_' + $safe + '() { return 0; }')
    $sourceLines | Set-Content -LiteralPath $source -Encoding utf8

    "## $relative" | Tee-Object -FilePath $logPath -Append
    $cmd = '"' + $vcvarsPath + '" >nul && cl.exe /nologo /W3 /WX /std:c++20 /permissive- /EHsc /c "' + (Resolve-Path $source) + '" /Fo"' + (Join-Path $repoRoot $object) + '" /I "' + $repoRoot + '"'
    cmd.exe /d /s /c $cmd 2>&1 | Tee-Object -FilePath $logPath -Append
    if ($LASTEXITCODE -ne 0) {
        throw "Header smoke compile failed for $relative"
    }
}

Write-Host "cpueaxh header smoke validation passed. Core headers: $($coreHeaders.Count). Instruction headers: $($instructionHeaders.Count)."
