param(
    [Parameter(Mandatory = $true)][string]$TestExe,
    [string]$WorkDir = 'strict-replay',
    [string]$LogPath = 'strict-replay.log'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $TestExe -PathType Leaf)) {
    throw "Test executable not found: $TestExe"
}

New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
if (Test-Path -LiteralPath $LogPath) {
    Remove-Item -LiteralPath $LogPath -Force
}

function Write-Fixture {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Json
    )
    Set-Content -LiteralPath (Join-Path $WorkDir $Name) -Value $Json -Encoding utf8
}

Write-Fixture 'valid-generated-image-hex.json' @'
{
  "schema": "cpueaxh.failure.v1",
  "case_selector": "add_rr_rax_rbx",
  "seed_index": 0,
  "image_hex": "00",
  "replay_hint": "test.exe --case add_rr_rax_rbx --seed-index 0"
}
'@

Write-Fixture 'generated-unknown-field.json' @'
{
  "schema": "cpueaxh.failure.v1",
  "case_selector": "add_rr_rax_rbx",
  "seed_index": 0,
  "unknown": true
}
'@

Write-Fixture 'generated-duplicate-case-selector.json' @'
{
  "schema": "cpueaxh.failure.v1",
  "case_selector": "add_rr_rax_rbx",
  "case_selector": "sub_rr_rax_rbx",
  "seed_index": 0
}
'@

Write-Fixture 'generated-missing-seed-index.json' @'
{
  "schema": "cpueaxh.failure.v1",
  "case_selector": "add_rr_rax_rbx"
}
'@

Write-Fixture 'generated-trailing-comma.json' @'
{
  "schema": "cpueaxh.failure.v1",
  "case_selector": "add_rr_rax_rbx",
  "seed_index": 0,
}
'@

Write-Fixture 'generated-duplicate-nested-field.json' @'
{
  "schema": "cpueaxh.failure.v1",
  "case_selector": "add_rr_rax_rbx",
  "seed_index": 0,
  "initial_state": {
    "schema": "cpueaxh.generated-initial-state.v1",
    "schema": "duplicate"
  }
}
'@

Write-Fixture 'generated-malformed-nested-object.json' @'
{
  "schema": "cpueaxh.failure.v1",
  "case_selector": "add_rr_rax_rbx",
  "seed_index": 0,
  "result_state": {
    "schema": "cpueaxh.generated-result-state.v1",
    "native_result": { "rip" "0x0" }
  }
}
'@

Write-Fixture 'manual-missing-coverage.json' @'
{
  "schema": "cpueaxh.manual-index.v1",
  "case_selector": "exception_priority",
  "category": "manual",
  "replay": "test.exe --manual-case exception_priority --record-bundle failure-bundle"
}
'@

Write-Fixture 'manual-wrong-coverage.json' @'
{
  "schema": "cpueaxh.manual-index.v1",
  "case_selector": "exception_priority",
  "category": "manual",
  "coverage": "wrong coverage",
  "replay": "test.exe --manual-case exception_priority --record-bundle failure-bundle"
}
'@

Write-Fixture 'manual-unknown-field.json' @'
{
  "schema": "cpueaxh.manual-index.v1",
  "case_selector": "exception_priority",
  "category": "manual",
  "coverage": "memory, stack, canonical-address, and UD exception ordering",
  "replay": "test.exe --manual-case exception_priority --record-bundle failure-bundle",
  "unknown": true
}
'@

function Invoke-ReplayExpect {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][int]$ExpectedExit
    )
    $path = Join-Path $WorkDir $Name
    "## $path expected=$ExpectedExit" | Tee-Object -FilePath $LogPath -Append
    & $TestExe --replay $path *> strict-replay-case.log
    $exitCode = $LASTEXITCODE
    Get-Content strict-replay-case.log | Tee-Object -FilePath $LogPath -Append
    if ($ExpectedExit -eq 0 -and $exitCode -ne 0) {
        throw "Expected replay success for $path, got $exitCode"
    }
    if ($ExpectedExit -ne 0 -and $exitCode -eq 0) {
        throw "Expected replay failure for $path, got success"
    }
}

Invoke-ReplayExpect 'valid-generated-image-hex.json' 0
Invoke-ReplayExpect 'generated-unknown-field.json' 2
Invoke-ReplayExpect 'generated-duplicate-case-selector.json' 2
Invoke-ReplayExpect 'generated-missing-seed-index.json' 2
Invoke-ReplayExpect 'generated-trailing-comma.json' 2
Invoke-ReplayExpect 'generated-duplicate-nested-field.json' 2
Invoke-ReplayExpect 'generated-malformed-nested-object.json' 2
Invoke-ReplayExpect 'manual-missing-coverage.json' 2
Invoke-ReplayExpect 'manual-wrong-coverage.json' 2
Invoke-ReplayExpect 'manual-unknown-field.json' 2

$global:LASTEXITCODE = 0
