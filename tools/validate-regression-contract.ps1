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

function Assert-TestFrameworkContains {
    param(
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Message
    )
    $paths = @('test/demo/framework.hpp')
    if (Test-Path -LiteralPath 'test/framework' -PathType Container) {
        $paths += @(Get-ChildItem -LiteralPath 'test/framework' -Filter '*.hpp' -File | ForEach-Object { $_.FullName })
    }
    $content = ($paths | ForEach-Object { Get-Content -LiteralPath $_ -Raw }) -join "`n"
    if ($content -notmatch $Pattern) {
        throw $Message
    }
}

function Assert-CoreHeadersHavePragmaOnce {
    $headerRoots = @('cpueaxh/cpu', 'cpueaxh/memory', 'cpueaxh/instructions')
    foreach ($root in $headerRoots) {
        if (-not (Test-Path -LiteralPath $root -PathType Container)) {
            throw "Missing core header root: $root"
        }
        Get-ChildItem -LiteralPath $root -Recurse -File -Filter '*.hpp' | ForEach-Object {
            $firstNonBlank = Get-Content -LiteralPath $_.FullName | Where-Object { $_.Trim().Length -ne 0 } | Select-Object -First 1
            if ($firstNonBlank -notmatch '^\s*#pragma\s+once\b') {
                throw "Core header lacks #pragma once: $($_.FullName)"
            }
        }
    }
}

function Assert-CoreHeaderSmokeTranslationUnits {
    $required = @(
        'cpueaxh/header_smoke/header_smoke_core.cpp',
        'cpueaxh/header_smoke/header_smoke_integer.cpp',
        'cpueaxh/header_smoke/header_smoke_control_flow.cpp',
        'cpueaxh/header_smoke/header_smoke_stack.cpp',
        'cpueaxh/header_smoke/header_smoke_string.cpp',
        'cpueaxh/header_smoke/header_smoke_simd_sse.cpp',
        'cpueaxh/header_smoke/header_smoke_simd_avx.cpp',
        'cpueaxh/header_smoke/header_smoke_crypto.cpp',
        'cpueaxh/header_smoke/header_smoke_system.cpp',
        'cpueaxh/header_smoke/header_smoke_x87.cpp',
        'cpueaxh/header_smoke/header_smoke_all_instructions.cpp'
    )
    foreach ($path in $required) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Missing core header smoke translation unit: $path"
        }
        $projectPath = $path.Replace('cpueaxh/', '').Replace('/', '\')
        Assert-FileContains -Path 'cpueaxh/cpueaxh.vcxproj' -Pattern ([regex]::Escape($projectPath)) -Message "cpueaxh.vcxproj must compile $projectPath"
    }
}

function Assert-CpueaxhInternalUsesInstructionModules {
    Assert-FileContains -Path 'cpueaxh/cpueaxh_internal.hpp' -Pattern 'cpu/core\.hpp' -Message 'cpueaxh_internal.hpp must include cpu/core.hpp.'
    Assert-FileContains -Path 'cpueaxh/cpueaxh_internal.hpp' -Pattern 'instructions/all_instructions\.hpp' -Message 'cpueaxh_internal.hpp must include instruction family umbrella.'
    $internal = Get-Content -LiteralPath 'cpueaxh/cpueaxh_internal.hpp' -Raw
    if ($internal -match 'instructions/(add|mov|avx_vex|sse2_|x87_)') {
        throw 'cpueaxh_internal.hpp must not return to direct per-instruction includes.'
    }
}

function Assert-StrictReplayFixtures {
    foreach ($path in @('test/replay-fixtures/valid', 'test/replay-fixtures/invalid')) {
        if (-not (Test-Path -LiteralPath $path -PathType Container)) {
            throw "Missing strict replay fixture directory: $path"
        }
        $fixtures = @(Get-ChildItem -LiteralPath $path -Filter '*.json' -File)
        if ($fixtures.Count -eq 0) {
            throw "Strict replay fixture directory has no JSON fixtures: $path"
        }
    }
    Assert-FileContains -Path 'tools/validate-strict-replay.ps1' -Pattern 'test/replay-fixtures' -Message 'strict replay validator must consume checked-in fixtures.'
    Assert-FileContains -Path 'tools/validate-strict-replay.ps1' -Pattern 'Get-ChildItem' -Message 'strict replay validator must enumerate checked-in fixtures.'
}

function Assert-GeneratedManifestPolicyFields {
    Assert-FileContains -Path 'test/framework/types.hpp' -Pattern 'instruction_form' -Message 'generated manifest must emit instruction_form.'
    Assert-FileContains -Path 'test/framework/types.hpp' -Pattern 'native_safety' -Message 'generated manifest must emit native_safety.'
    Assert-FileContains -Path 'test/framework/types.hpp' -Pattern 'write_compare_policy' -Message 'generated manifest must emit explicit compare policy.'
    Assert-FileContains -Path 'tools/validate-generated-spec-manifest.ps1' -Pattern 'reason_if_disabled' -Message 'manifest validator must require disabled-compare reasons.'
    Assert-FileContains -Path 'tools/validate-generated-spec-manifest.ps1' -Pattern 'reason_if_not_compared' -Message 'manifest validator must require flag non-compare reasons.'
}

function Assert-RequiredCoverageGates {
    Assert-TestFrameworkContains -Pattern '--require-feature' -Message 'CLI must support --require-feature.'
    Assert-TestFrameworkContains -Pattern '--require-spec' -Message 'CLI must support --require-spec.'
    Assert-TestFrameworkContains -Pattern '--require-family' -Message 'CLI must support --require-family.'
    Assert-TestFrameworkContains -Pattern 'validate_required_generated_coverage' -Message 'runner must enforce required generated coverage gates.'
    Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern 'Validate required coverage gates' -Message 'CI must validate required coverage gates.'
    Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern 'require-coverage\.log' -Message 'CI must preserve required coverage gate logs.'
    Assert-FileContains -Path '.github/workflows/extended-regression.yml' -Pattern 'Validate required coverage gates' -Message 'extended regression must validate required coverage gates.'
}

function Assert-NoLegacyFrameworkJsonExtractors {
    $content = Get-ChildItem -LiteralPath 'test/framework' -Filter '*.hpp' -File |
        ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }
    $joined = $content -join "`n"
    if ($joined -match 'json_extract_string|json_extract_u64') {
        throw 'framework strict replay must not expose legacy json_extract_* helpers.'
    }
}

function Assert-FrameworkHeadersDoNotRequireUmbrellaOrder {
    $bad = @(Get-ChildItem -LiteralPath 'test/framework' -Filter '*.hpp' -File |
        Where-Object { (Get-Content -LiteralPath $_.FullName -Raw) -match 'keep include order there' })
    if ($bad.Count -ne 0) {
        throw "Framework headers still claim umbrella include-order dependence: $($bad.Name -join ', ')"
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
        if ($json -notmatch '"case_selector"\s*:\s*"([^"]+)"') {
            throw "Manual replay record missing non-empty case_selector: $($file.FullName)"
        }
        $caseSelector = $Matches[1]
        if ($json -notmatch '"category"\s*:\s*"(manual|unsafe-native)"') {
            throw "Manual replay record category must be manual or unsafe-native: $($file.FullName)"
        }
        if ($json -notmatch '"replay"\s*:\s*"([^"]+)"') {
            throw "Manual replay record must include a replay command: $($file.FullName)"
        }
        $replayCommand = $Matches[1]
        if ($replayCommand -notmatch "(^| )--manual-case\s+$([regex]::Escape($caseSelector))(\s|$)") {
            throw "Manual replay record must include a --manual-case replay command: $($file.FullName)"
        }
    }
}

Assert-FileContains -Path 'docs/instruction-status.yml' -Pattern 'required_identity_fields' -Message 'instruction-status.yml must define form-level identity fields.'
Assert-FileContains -Path 'docs/instruction-status.yml' -Pattern 'unsafe_for_native' -Message 'instruction-status.yml must keep unsafe_for_native status available.'
Assert-FileContains -Path 'docs/instruction-status.yml' -Pattern 'stage3_contracts' -Message 'instruction-status.yml must link stage3 contracts.'
Assert-FileContains -Path 'docs/replay-schema.md' -Pattern 'cpueaxh\.host-features\.v1' -Message 'replay schema must document host feature records.'
Assert-FileContains -Path 'docs/replay-schema.md' -Pattern 'cpueaxh\.generated-initial-state\.v1' -Message 'replay schema must document generated initial-state records.'
Assert-FileContains -Path 'docs/replay-schema.md' -Pattern 'cpueaxh\.generated-result-state\.v1' -Message 'replay schema must document generated result-state records.'
Assert-FileContains -Path 'docs/replay-schema.md' -Pattern 'host_features' -Message 'replay schema must document host feature snapshots in failure records.'
Assert-FileContains -Path 'docs/replay-schema.md' -Pattern 'family/model/stepping' -Message 'replay schema must document CPU family/model/stepping fields.'
Assert-TestFrameworkContains -Pattern 'cpueaxh\.generated-initial-state\.v1' -Message 'failure recorder must emit generated initial-state records.'
Assert-TestFrameworkContains -Pattern 'cpueaxh\.generated-result-state\.v1' -Message 'failure recorder must emit generated result-state records.'
Assert-TestFrameworkContains -Pattern 'attach_host_features_to_failure' -Message 'failure recorder must attach host feature snapshots.'
Assert-TestFrameworkContains -Pattern 'host_brand' -Message 'failure recorder must include host CPU brand strings.'
Assert-TestFrameworkContains -Pattern 'manual_case_has_exact_runner' -Message 'manual replay must require exact registered runners.'
Assert-TestFrameworkContains -Pattern 'manual replay mode: exact registered runner' -Message 'manual replay must not fall back to the full manual suite.'
Assert-TestFrameworkContains -Pattern 'has no exact runner' -Message 'missing manual exact runners must fail explicitly.'
if ((Get-ChildItem -LiteralPath 'test/framework' -Filter '*.hpp' -File | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join "`n" -match 'manual replay mode: full manual special suite') {
    throw 'manual replay must not run the full manual suite as a fallback.'
}
Assert-FileContains -Path 'TEST_FRAMEWORK_PLAN_CN.md' -Pattern '第三阶段' -Message 'Chinese plan must preserve stage 3 section.'
Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern '--list-gates' -Message 'required CI must log stage3 regression gates.'
Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern 'test\\manual\\exception_priority\.json' -Message 'required CI must replay a manual-index sample.'
Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern 'validate-regression-contract\.ps1' -Message 'required CI must run regression contract validation.'
Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern 'validate-stage3-gate-output\.ps1' -Message 'required CI must validate stage3 gate output consistency.'
Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern '--dump-specs generated-specs\.json' -Message 'required CI must dump the generated spec manifest.'
Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern 'validate-generated-spec-manifest\.ps1' -Message 'required CI must validate the generated spec manifest.'
Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern 'validate-instruction-status\.ps1' -Message 'required CI must validate instruction-status coverage.'
Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern 'validate-strict-replay\.ps1' -Message 'required CI must validate strict replay schema rejection.'
Assert-FileContains -Path '.github/workflows/extended-regression.yml' -Pattern 'if-no-files-found:\s*error' -Message 'extended regression diagnostics must fail when expected evidence is missing.'
Assert-FileContains -Path '.github/workflows/extended-regression.yml' -Pattern 'build\.log' -Message 'extended regression must capture build.log.'
Assert-FileContains -Path '.github/workflows/extended-regression.yml' -Pattern 'validate-strict-replay\.ps1' -Message 'extended regression must validate strict replay schema rejection.'
Assert-FileContains -Path 'tools/validate-generated-spec-manifest.ps1' -Pattern 'cpueaxh\.generated-specs\.v1' -Message 'generated spec manifest validator must check schema.'
Assert-FileContains -Path 'tools/validate-instruction-status.ps1' -Pattern 'instruction-status\.yml has an invalid or missing schema' -Message 'instruction status validator must check schema.'
Assert-FileContains -Path 'cpueaxh/cpu/executor.hpp' -Pattern 'CPUEAXH_STRICT_INTERNAL' -Message 'executor must keep strict internal decode checks enabled.'
Assert-FileContains -Path 'docs/hardware-runner-matrix.md' -Pattern 'GitHub hosted runner feature matrix' -Message 'runner matrix doc must describe hosted-runner validation.'
Assert-FileContains -Path 'docs/hardware-runner-matrix.md' -Pattern 'hosted-runner-only' -Message 'runner matrix doc must avoid self-hosted hardware requirements.'

Assert-Stage3GateManifest
Assert-GeneratorTemplates
Assert-NonEmptyJsonCorpus
Assert-ManualIndexRecords
Assert-CoreHeadersHavePragmaOnce
Assert-CoreHeaderSmokeTranslationUnits
Assert-CpueaxhInternalUsesInstructionModules
Assert-StrictReplayFixtures
Assert-GeneratedManifestPolicyFields
Assert-RequiredCoverageGates
Assert-NoLegacyFrameworkJsonExtractors
Assert-FrameworkHeadersDoNotRequireUmbrellaOrder

Write-Host 'Regression contract validation passed.'
