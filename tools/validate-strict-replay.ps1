param(
    [Parameter(Mandatory = $true)][string]$TestExe,
    [string]$FixtureRoot = 'test/replay-fixtures',
    [string]$LogPath = 'strict-replay.log'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $TestExe -PathType Leaf)) {
    throw "Test executable not found: $TestExe"
}
if (-not (Test-Path -LiteralPath $FixtureRoot -PathType Container)) {
    throw "Replay fixture root not found: $FixtureRoot"
}

if (Test-Path -LiteralPath $LogPath) {
    Remove-Item -LiteralPath $LogPath -Force
}

function Invoke-ReplayExpect {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][int]$ExpectedExit
    )
    "## $Path expected=$ExpectedExit" | Tee-Object -FilePath $LogPath -Append
    & $TestExe --replay $Path *> strict-replay-case.log
    $exitCode = $LASTEXITCODE
    Get-Content strict-replay-case.log | Tee-Object -FilePath $LogPath -Append
    if ($ExpectedExit -eq 0 -and $exitCode -ne 0) {
        throw "Expected replay success for $Path, got $exitCode"
    }
    if ($ExpectedExit -ne 0 -and $exitCode -eq 0) {
        throw "Expected replay failure for $Path, got success"
    }
}

$validRoot = Join-Path $FixtureRoot 'valid'
$invalidRoot = Join-Path $FixtureRoot 'invalid'
$validFixtures = @(Get-ChildItem -LiteralPath $validRoot -Filter '*.json' -File | Sort-Object Name)
$invalidFixtures = @(Get-ChildItem -LiteralPath $invalidRoot -Filter '*.json' -File | Sort-Object Name)
if ($validFixtures.Count -eq 0) {
    throw "No valid replay fixtures found in $validRoot"
}
if ($invalidFixtures.Count -eq 0) {
    throw "No invalid replay fixtures found in $invalidRoot"
}

foreach ($fixture in $validFixtures) {
    Invoke-ReplayExpect $fixture.FullName 0
}
foreach ($fixture in $invalidFixtures) {
    Invoke-ReplayExpect $fixture.FullName 2
}

$global:LASTEXITCODE = 0
