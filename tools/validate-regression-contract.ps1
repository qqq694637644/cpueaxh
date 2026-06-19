$ErrorActionPreference = 'Stop'

$RequiredStage3Gates = @(
    'public_helper_full_regression',
    'generated_long_fuzz',
    'undefined_flags',
    'exception_priority',
    'memory_access_order',
    'simd_state_boundary',
    'hardware_feature_matrix'
)

$RequiredGeneratorFamilies = @(
    'integer_alu',
    'memory_rmw',
    'stack',
    'control_flow',
    'string_ops',
    'vector_simd',
    'x87',
    'unsafe_native'
)

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

function Assert-SetEquals {
    param(
        [Parameter(Mandatory = $true)][string[]]$Actual,
        [Parameter(Mandatory = $true)][string[]]$Expected,
        [Parameter(Mandatory = $true)][string]$Label
    )
    $actualSorted = @($Actual | Sort-Object -Unique)
    $expectedSorted = @($Expected | Sort-Object -Unique)
    $missing = @($expectedSorted | Where-Object { $_ -notin $actualSorted })
    $extra = @($actualSorted | Where-Object { $_ -notin $expectedSorted })
    if ($missing.Count -ne 0 -or $extra.Count -ne 0) {
        throw "$Label mismatch. Missing: $($missing -join ', ') Extra: $($extra -join ', ')"
    }
}

function Get-YamlNamedBlocks {
    param([Parameter(Mandatory = $true)][string]$Content)
    $blocks = @{}
    $matches = [regex]::Matches($Content, '(?ms)^\s+-\s+name:\s*([^\r\n]+)\s*$.*?(?=^\s+-\s+name:\s*|\z)')
    foreach ($match in $matches) {
        $name = $match.Groups[1].Value.Trim()
        $blocks[$name] = $match.Value
    }
    return $blocks
}

function Assert-Stage3GateManifest {
    $path = 'docs/stage3-regression-gates.yml'
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing required file: $path"
    }
    $content = Get-Content -LiteralPath $path -Raw
    if ($content -notmatch '(?m)^schema:\s*cpueaxh\.stage3-regression-gates\.v1\s*$') {
        throw 'stage3-regression-gates.yml has an invalid or missing schema.'
    }
    foreach ($field in @('policy:', 'required_gates:', 'high_risk_files:')) {
        if ($content -notmatch [regex]::Escape($field)) {
            throw "stage3-regression-gates.yml is missing required section: $field"
        }
    }

    $blocks = Get-YamlNamedBlocks -Content $content
    Assert-SetEquals -Actual ([string[]]$blocks.Keys) -Expected $RequiredStage3Gates -Label 'Stage3 gate name set'

    foreach ($gate in $RequiredStage3Gates) {
        $block = $blocks[$gate]
        foreach ($field in @('category', 'purpose', 'command')) {
            if ($block -notmatch "(?m)^\s+${field}:\s*\S+") {
                throw "Stage3 gate '$gate' missing required field '$field'."
            }
        }
    }
}

function Assert-GeneratorTemplates {
    $path = 'docs/generator-templates.yml'
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing required file: $path"
    }
    $content = Get-Content -LiteralPath $path -Raw
    if ($content -notmatch '(?m)^schema:\s*cpueaxh\.generator-templates\.v1\s*$') {
        throw 'generator-templates.yml has an invalid or missing schema.'
    }
    foreach ($section in @('naming:', 'shared_requirements:', 'families:')) {
        if ($content -notmatch [regex]::Escape($section)) {
            throw "generator-templates.yml is missing required section: $section"
        }
    }
    foreach ($requirement in @('exact_selector_required: true', 'stable_names: true', 'deterministic_seed: true', 'safe_return_path: true', 'flag_mask_required: true')) {
        if ($content -notmatch [regex]::Escape($requirement)) {
            throw "generator-templates.yml missing shared requirement: $requirement"
        }
    }

    $familiesSectionMatch = [regex]::Match($content, '(?ms)^families:\s*$.*\z')
    if (-not $familiesSectionMatch.Success) {
        throw 'generator-templates.yml is missing a parseable families section.'
    }
    $familiesSection = $familiesSectionMatch.Value
    $familyMatches = [regex]::Matches($familiesSection, '(?m)^\s{2}([a-z0-9_]+):\s*$')
    $families = @($familyMatches | ForEach-Object { $_.Groups[1].Value })
    Assert-SetEquals -Actual $families -Expected $RequiredGeneratorFamilies -Label 'Generator template family set'

    foreach ($family in $RequiredGeneratorFamilies) {
        $escapedFamily = [regex]::Escape($family)
        $blockMatch = [regex]::Match($content, "(?ms)^\s{2}${escapedFamily}:\s*$.*?(?=^\s{2}[a-z0-9_]+:\s*$|\z)")
        if (-not $blockMatch.Success) {
            throw "Generator family '$family' block not found."
        }
        $block = $blockMatch.Value
        foreach ($field in @('examples:', 'required_fields:', 'special_rules:')) {
            if ($block -notmatch [regex]::Escape($field)) {
                throw "Generator family '$family' missing required section '$field'."
            }
        }
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

function Assert-ManualIndexRecords {
    $dir = 'test/manual'
    if (-not (Test-Path -LiteralPath $dir -PathType Container)) {
        throw "Manual replay directory is missing: $dir"
    }
    $files = Get-ChildItem -LiteralPath $dir -Filter '*.json' -File
    if (-not $files -or $files.Count -eq 0) {
        throw "Manual replay directory has no JSON records: $dir"
    }
    foreach ($file in $files) {
        $json = Get-Content -LiteralPath $file.FullName -Raw
        if ($json -notmatch '"schema"\s*:\s*"cpueaxh\.manual-index\.v1"') {
            throw "Manual replay record missing required schema: $($file.FullName)"
        }
        if ($json -notmatch '"case_selector"\s*:\s*"[^"]+"') {
            throw "Manual replay record missing non-empty case_selector: $($file.FullName)"
        }
        if ($json -notmatch '"category"\s*:\s*"(manual|unsafe-native)"') {
            throw "Manual replay record category must be manual or unsafe-native: $($file.FullName)"
        }
        if ($json -notmatch '"replay"\s*:\s*"test\.exe --manual-case [^"]+"') {
            throw "Manual replay record must include a --manual-case replay command: $($file.FullName)"
        }
    }
}

Assert-FileContains -Path 'docs/instruction-status.yml' -Pattern 'required_identity_fields' -Message 'instruction-status.yml must define form-level identity fields.'
Assert-FileContains -Path 'docs/instruction-status.yml' -Pattern 'unsafe_for_native' -Message 'instruction-status.yml must keep unsafe_for_native status available.'
Assert-FileContains -Path 'docs/instruction-status.yml' -Pattern 'stage3_contracts' -Message 'instruction-status.yml must link stage3 contracts.'
Assert-FileContains -Path 'docs/replay-schema.md' -Pattern 'cpueaxh\.host-features\.v1' -Message 'replay schema must document host feature records.'
Assert-FileContains -Path 'TEST_FRAMEWORK_PLAN_CN.md' -Pattern '第三阶段' -Message 'Chinese plan must preserve stage 3 section.'
Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern '--list-gates' -Message 'required CI must log stage3 regression gates.'
Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern 'test\\manual\\exception_priority\.json' -Message 'required CI must replay a manual-index sample.'
Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern 'validate-regression-contract\.ps1' -Message 'required CI must run regression contract validation.'
Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern 'validate-stage3-gate-output\.ps1' -Message 'required CI must validate stage3 gate output consistency.'
Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern '--dump-specs generated-specs\.json' -Message 'required CI must dump the generated spec manifest.'
Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern 'validate-generated-spec-manifest\.ps1' -Message 'required CI must validate the generated spec manifest.'
Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern 'validate-instruction-status\.ps1' -Message 'required CI must validate instruction-status coverage.'
Assert-FileContains -Path 'tools/validate-generated-spec-manifest.ps1' -Pattern 'cpueaxh\.generated-specs\.v1' -Message 'generated spec manifest validator must check schema.'
Assert-FileContains -Path 'tools/validate-instruction-status.ps1' -Pattern 'instruction-status\.yml has an invalid or missing schema' -Message 'instruction status validator must check schema.'
Assert-FileContains -Path 'docs/hardware-runner-matrix.md' -Pattern 'GitHub hosted runner feature matrix' -Message 'runner matrix doc must describe hosted-runner validation.'
Assert-FileContains -Path 'docs/hardware-runner-matrix.md' -Pattern 'hosted-runner-only' -Message 'runner matrix doc must avoid self-hosted hardware requirements.'

Assert-Stage3GateManifest
Assert-GeneratorTemplates
Assert-NonEmptyJsonCorpus
Assert-ManualIndexRecords

Write-Host 'Regression contract validation passed.'
