$ErrorActionPreference = 'Stop'

function Assert-FileContains {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Message
    )
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing required file: $Path"
    }
    $content = Get-Content -LiteralPath $Path -Raw
    if ($content -notmatch $Pattern) {
        throw $Message
    }
}

function Assert-NonEmptyJsonCorpus {
    $dir = 'test/regression'
    if (-not (Test-Path -LiteralPath $dir -PathType Container)) {
        throw "Regression corpus directory is missing: $dir"
    }
    $files = Get-ChildItem -LiteralPath $dir -Filter '*.json' -File
    if (-not $files -or $files.Count -eq 0) {
        throw "Regression corpus has no replay JSON files: $dir"
    }
    foreach ($file in $files) {
        $json = Get-Content -LiteralPath $file.FullName -Raw
        if ($json -notmatch '"schema"\s*:\s*"cpueaxh\.failure\.v1"') {
            throw "Replay record missing required schema: $($file.FullName)"
        }
        if ($json -notmatch '"case_selector"\s*:\s*"[^"]+"') {
            throw "Replay record missing non-empty case_selector: $($file.FullName)"
        }
        if ($json -notmatch '"seed_index"\s*:\s*[0-9]+(\s*[,}])') {
            throw "Replay record seed_index must be an unquoted non-negative integer: $($file.FullName)"
        }
    }
}

Assert-FileContains -Path 'docs/instruction-status.yml' -Pattern 'required_identity_fields' -Message 'instruction-status.yml must define form-level identity fields.'
Assert-FileContains -Path 'docs/instruction-status.yml' -Pattern 'unsafe_for_native' -Message 'instruction-status.yml must keep unsafe_for_native status available.'
Assert-FileContains -Path 'docs/stage3-regression-gates.yml' -Pattern 'public_helper_full_regression' -Message 'stage3 gates must include public helper full regression gate.'
Assert-FileContains -Path 'docs/stage3-regression-gates.yml' -Pattern 'undefined_flags' -Message 'stage3 gates must include undefined flags gate.'
Assert-FileContains -Path 'docs/stage3-regression-gates.yml' -Pattern 'memory_access_order' -Message 'stage3 gates must include memory access order gate.'
Assert-FileContains -Path 'docs/generator-templates.yml' -Pattern 'integer_alu' -Message 'generator templates must include integer_alu family.'
Assert-FileContains -Path 'docs/generator-templates.yml' -Pattern 'unsafe_native' -Message 'generator templates must include unsafe_native policy.'
Assert-FileContains -Path 'docs/replay-schema.md' -Pattern 'cpueaxh\.host-features\.v1' -Message 'replay schema must document host feature records.'
Assert-FileContains -Path 'TEST_FRAMEWORK_PLAN_CN.md' -Pattern '第三阶段' -Message 'Chinese plan must preserve stage 3 section.'
Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern '--list-gates' -Message 'required CI must log stage3 regression gates.'
Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern 'validate-regression-contract\.ps1' -Message 'required CI must run regression contract validation.'

Assert-NonEmptyJsonCorpus

Write-Host 'Regression contract validation passed.'
