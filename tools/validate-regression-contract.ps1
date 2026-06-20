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
    $project = Get-Content -LiteralPath 'cpueaxh/cpueaxh.vcxproj' -Raw
    $filters = ''
    if (Test-Path -LiteralPath 'cpueaxh/cpueaxh.vcxproj.filters' -PathType Leaf) {
        $filters = Get-Content -LiteralPath 'cpueaxh/cpueaxh.vcxproj.filters' -Raw
    }
    foreach ($path in $required) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Missing core header smoke translation unit: $path"
        }
        $projectPath = $path.Replace('cpueaxh/', '').Replace('/', '\')
        if ($project -match [regex]::Escape($projectPath)) {
            throw "cpueaxh.vcxproj must not compile header smoke translation units into the normal library: $projectPath"
        }
        if ($filters -match [regex]::Escape($projectPath)) {
            throw "cpueaxh.vcxproj.filters must not list header smoke translation units as normal project sources: $projectPath"
        }
    }
}

function Assert-IndividualHeaderSmokeScript {
    Assert-FileContains -Path 'tools/validate-cpueaxh-header-smoke.ps1' -Pattern 'cpueaxh/cpu/\*\.hpp' -Message 'cpueaxh header smoke script must cover cpu headers.'
    Assert-FileContains -Path 'tools/validate-cpueaxh-header-smoke.ps1' -Pattern 'cpueaxh/memory/\*\.hpp' -Message 'cpueaxh header smoke script must cover memory headers.'
    Assert-FileContains -Path 'tools/validate-cpueaxh-header-smoke.ps1' -Pattern 'cpueaxh/instructions' -Message 'cpueaxh header smoke script must cover instruction implementation headers.'
    Assert-FileContains -Path 'tools/validate-cpueaxh-header-smoke.ps1' -Pattern 'instruction_common\.hpp' -Message 'instruction header smoke must include the instruction prerequisite header.'
    Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern 'validate-cpueaxh-header-smoke\.ps1' -Message 'required CI must compile core header smoke.'
    Assert-FileContains -Path '.github/workflows/extended-regression.yml' -Pattern 'validate-cpueaxh-header-smoke\.ps1' -Message 'extended CI must compile core header smoke.'
    Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern 'cpueaxh-header-smoke\.log' -Message 'required CI must preserve header smoke logs.'
}

function Assert-BuildLogWarningGate {
    Assert-FileContains -Path 'tools/validate-build-log-zero-warnings.ps1' -Pattern 'Build produced warnings' -Message 'build-log validator must fail on warnings.'
    Assert-FileContains -Path 'tools/validate-build-log-zero-warnings.ps1' -Pattern 'Warning\\\(s\\\)' -Message 'build-log validator must reject nonzero warning summaries.'
    Assert-FileContains -Path '.github/workflows/msvc-test.yml' -Pattern 'validate-build-log-zero-warnings\.ps1 -LogPath build\.log' -Message 'required CI must fail when build.log contains warnings.'
    Assert-FileContains -Path '.github/workflows/extended-regression.yml' -Pattern 'validate-build-log-zero-warnings\.ps1 -LogPath build\.log' -Message 'extended CI must fail when build.log contains warnings.'
}

function Assert-CpueaxhInternalUsesInstructionModules {
    Assert-FileContains -Path 'cpueaxh/cpueaxh_internal.hpp' -Pattern 'cpu/core\.hpp' -Message 'cpueaxh_internal.hpp must include cpu/core.hpp.'
    Assert-FileContains -Path 'cpueaxh/cpueaxh_internal.hpp' -Pattern 'instructions/all_instructions\.hpp' -Message 'cpueaxh_internal.hpp must include instruction family umbrella.'
    $internal = Get-Content -LiteralPath 'cpueaxh/cpueaxh_internal.hpp' -Raw
    if ($internal -match 'instructions/(add|mov|avx_vex|sse2_|x87_)') {
        throw 'cpueaxh_internal.hpp must not return to direct per-instruction includes.'
    }
    if ($internal -match '#include\s+"[^"]*\\') {
        throw 'cpueaxh_internal.hpp include paths must use forward slashes.'
    }
}

function Assert-InstructionModuleCoverage {
    Assert-FileContains -Path 'tools/validate-instruction-module-coverage.ps1' -Pattern 'Instruction module coverage validation passed' -Message 'instruction module coverage validator must exist.'
    & ./tools/validate-instruction-module-coverage.ps1
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
    Assert-FileContains -Path 'test/replay-fixtures/invalid/generated-unknown-nested-field.json' -Pattern '"unexpected"' -Message 'strict replay fixtures must reject unknown nested diagnostic fields.'
    Assert-FileContains -Path 'test/replay-fixtures/valid/generated-host-features.json' -Pattern 'cpueaxh\.host-features\.v1' -Message 'strict replay fixtures must accept well-formed nested host feature diagnostics.'
    Assert-TestFrameworkContains -Pattern 'json_validate_generated_replay_diagnostics' -Message 'replay parser must validate generated diagnostic fields.'
    Assert-TestFrameworkContains -Pattern 'json_validate_generated_initial_state' -Message 'replay parser must validate nested initial_state diagnostics.'
    Assert-TestFrameworkContains -Pattern 'json_validate_generated_result_state' -Message 'replay parser must validate nested result_state diagnostics.'
    Assert-TestFrameworkContains -Pattern 'json_validate_host_features_value' -Message 'replay parser must validate nested host_features diagnostics.'
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

function Assert-AllFailuresCollection {
    Assert-TestFrameworkContains -Pattern '--fail-fast' -Message 'CLI must expose explicit --fail-fast.'
    Assert-TestFrameworkContains -Pattern '--record-failures' -Message 'CLI must expose --record-failures.'
    Assert-TestFrameworkContains -Pattern 'write_failures_record' -Message 'framework must write all-failures records.'
    Assert-TestFrameworkContains -Pattern 'cpueaxh\.failures\.v1' -Message 'all-failures record must have a schema.'
    Assert-TestFrameworkContains -Pattern 'FAIL collected failures' -Message 'runner must report collected failures instead of first-failure only.'
    Assert-TestFrameworkContains -Pattern 'failures\.json' -Message 'record bundles must include failures.json.'
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

function Get-CppFunctionBody {
    param(
        [string]$Path,
        [string]$FunctionName
    )

    $content = Get-Content -LiteralPath $Path -Raw
    $nameIndex = $content.IndexOf($FunctionName)
    if ($nameIndex -lt 0) {
        throw "Missing expected function $FunctionName in $Path"
    }

    $braceIndex = $content.IndexOf('{', $nameIndex)
    if ($braceIndex -lt 0) {
        throw "Missing function body for $FunctionName in $Path"
    }

    $depth = 0
    for ($index = $braceIndex; $index -lt $content.Length; $index++) {
        $ch = $content[$index]
        if ($ch -eq '{') {
            $depth++
        }
        elseif ($ch -eq '}') {
            $depth--
            if ($depth -eq 0) {
                return $content.Substring($braceIndex + 1, $index - $braceIndex - 1)
            }
        }
    }

    throw "Unterminated function body for $FunctionName in $Path"
}

function Assert-FunctionDefaultContainsUnreachable {
    param(
        [string]$Path,
        [string]$FunctionName,
        [string]$Message
    )

    $body = Get-CppFunctionBody -Path $Path -FunctionName $FunctionName
    $defaultIndex = $body.IndexOf('default:')
    if ($defaultIndex -lt 0) {
        throw "$Message Missing default label in $($FunctionName): $Path"
    }

    $defaultBlock = $body.Substring($defaultIndex)
    if ($defaultBlock -notmatch '(?s)default:\s*CPUEAXH_UNREACHABLE\(\)\s*;') {
        throw "$Message Default must call CPUEAXH_UNREACHABLE() in $($FunctionName): $Path"
    }
}

function Assert-RoundSwitchesUseUnreachableDefault {
    $checks = @(
        @{ Path = 'cpueaxh/instructions/roundsd.hpp'; FunctionName = 'roundsd_host_rounding_mode' },
        @{ Path = 'cpueaxh/instructions/roundss.hpp'; FunctionName = 'roundss_host_rounding_mode' },
        @{ Path = 'cpueaxh/instructions/avx_vex_ops.hpp'; FunctionName = 'avx_host_rounding_mode' }
    )

    foreach ($check in $checks) {
        $body = Get-CppFunctionBody -Path $check.Path -FunctionName $check.FunctionName
        $defaultIndex = $body.IndexOf('default:')
        if ($defaultIndex -lt 0) {
            throw "Round mode switch must keep an explicit unreachable default: $($check.Path)"
        }

        $defaultBlock = $body.Substring($defaultIndex)
        if ($defaultBlock -match '(?s)default:\s*return\b') {
            throw "Round mode switch still uses a silent default fallback: $($check.Path)"
        }
        if ($defaultBlock -notmatch '(?s)default:\s*CPUEAXH_UNREACHABLE\(\)\s*;') {
            throw "Round mode switch must mark masked default as unreachable: $($check.Path)"
        }
    }
}

function Assert-MaskedSemanticSwitchesUseUnreachableDefault {
    $checks = @(
        @{ Path = 'cpueaxh/instructions/sse_convert.hpp'; FunctionName = 'sse_convert_round_float_to_integer' },
        @{ Path = 'cpueaxh/instructions/sse2_convert.hpp'; FunctionName = 'sse2_convert_round_fp_to_integer' },
        @{ Path = 'cpueaxh/instructions/sse_cmp.hpp'; FunctionName = 'evaluate_sse_cmp_predicate' },
        @{ Path = 'cpueaxh/instructions/sse2_cmp_pd.hpp'; FunctionName = 'evaluate_sse2_cmp_pd_predicate' },
        @{ Path = 'cpueaxh/instructions/jcc.hpp'; FunctionName = 'eval_condition' },
        @{ Path = 'cpueaxh/cpu/executor.hpp'; FunctionName = 'cpu_executor_eval_jcc_condition' }
    )

    foreach ($check in $checks) {
        Assert-FunctionDefaultContainsUnreachable -Path $check.Path -FunctionName $check.FunctionName -Message 'Masked semantic switch must expose unexpected default paths.'
    }

    Assert-FileContains -Path 'cpueaxh/cpu/executor.hpp' -Pattern 'case 0xF:\s*return !zf && \(sf == of\);' -Message 'Jcc executor helper must handle cond 0xF explicitly instead of folding it into default.'
}

function Assert-CheckedMemoryReadHelpers {
    Assert-FileContains -Path 'tools/validate-memory-read-contract.ps1' -Pattern 'read_memory_operand\(\) is only allowed inside cpueaxh/cpu/memory\.hpp' -Message 'memory read contract validator must forbid raw shared operand reads outside memory.hpp.'
    Assert-FileContains -Path 'cpueaxh/cpu/memory.hpp' -Pattern 'struct CpuReadResult8' -Message 'memory helpers must expose checked byte reads.'
    Assert-FileContains -Path 'cpueaxh/cpu/memory.hpp' -Pattern 'read_memory_byte_checked' -Message 'memory helpers must expose read_memory_byte_checked.'
    Assert-FileContains -Path 'cpueaxh/cpu/memory.hpp' -Pattern 'read_memory_word_checked' -Message 'memory helpers must expose read_memory_word_checked.'
    Assert-FileContains -Path 'cpueaxh/cpu/memory.hpp' -Pattern 'read_memory_dword_checked' -Message 'memory helpers must expose read_memory_dword_checked.'
    Assert-FileContains -Path 'cpueaxh/cpu/memory.hpp' -Pattern 'read_memory_qword_checked' -Message 'memory helpers must expose read_memory_qword_checked.'
    Assert-FileContains -Path 'cpueaxh/cpu/memory.hpp' -Pattern 'read_memory_operand_checked' -Message 'shared memory operand readers must have a checked variant.'
    Assert-FileContains -Path 'cpueaxh/cpu/memory.hpp' -Pattern 'CpuReadResult64 read_result' -Message 'atomic RMW helpers must use checked memory reads before consuming values.'
    & ./tools/validate-memory-read-contract.ps1
}

function Assert-AvxVexModuleSplit {
    $required = @(
        'avx_vex_common.hpp',
        'avx_vex_decode.hpp',
        'avx_vex_ops.hpp',
        'avx_vex_execute.hpp',
        'avx_vex.hpp'
    )
    foreach ($name in $required) {
        $path = Join-Path 'cpueaxh/instructions' $name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Missing AVX/VEX module header: $path"
        }
        $firstNonBlank = Get-Content -LiteralPath $path | Where-Object { $_.Trim().Length -ne 0 } | Select-Object -First 1
        if ($firstNonBlank -notmatch '^\s*#pragma\s+once\b') {
            throw "AVX/VEX module header lacks #pragma once: $path"
        }
        Assert-FileContains -Path 'cpueaxh/instructions/simd_avx_instructions.hpp' -Pattern ([regex]::Escape($name)) -Message "simd AVX umbrella must include $name."
    }

    $umbrellaLines = @(Get-Content -LiteralPath 'cpueaxh/instructions/avx_vex.hpp')
    if ($umbrellaLines.Count -gt 40) {
        throw 'avx_vex.hpp must remain a small umbrella; put implementation into avx_vex_* modules.'
    }
    Assert-FileContains -Path 'cpueaxh/instructions/avx_vex.hpp' -Pattern 'avx_vex_execute\.hpp' -Message 'avx_vex.hpp must include the execution module.'
    Assert-FileContains -Path 'cpueaxh/instructions/avx_vex_common.hpp' -Pattern 'struct AVXRegister256' -Message 'AVX/VEX common module must own shared register types.'
    Assert-FileContains -Path 'cpueaxh/instructions/avx_vex_decode.hpp' -Pattern 'decode_avx_vex_modrm' -Message 'AVX/VEX decode module must own decode helpers.'
    Assert-FileContains -Path 'cpueaxh/instructions/avx_vex_ops.hpp' -Pattern 'apply_avx_round_ps_intrinsic' -Message 'AVX/VEX ops module must own operation helpers.'
    Assert-FileContains -Path 'cpueaxh/instructions/avx_vex_execute.hpp' -Pattern 'execute_avx_vex' -Message 'AVX/VEX execute module must own dispatch.'
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
    $path = 'docs/stage3-regression-gates.json'
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing required file: $path"
    }
    $manifest = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    if ($manifest.schema -ne 'cpueaxh.stage3-regression-gates.v1') {
        throw 'stage3-regression-gates.json has an invalid or missing schema.'
    }
    foreach ($section in @('policy', 'required_gates')) {
        if ($null -eq $manifest.PSObject.Properties[$section]) {
            throw "stage3-regression-gates.json is missing required section: $section"
        }
    }
    if ($null -eq $manifest.policy.high_risk_files -or $manifest.policy.high_risk_files.Count -eq 0) {
        throw 'stage3-regression-gates.json must list high_risk_files.'
    }

    $gates = @($manifest.required_gates | ForEach-Object { [string]$_.name })
    Assert-SetEquals -Actual $gates -Expected $RequiredStage3Gates -Label 'Stage3 gate name set'

    foreach ($gate in $manifest.required_gates) {
        foreach ($field in @('category', 'purpose', 'command')) {
            if ($null -eq $gate.PSObject.Properties[$field] -or [string]::IsNullOrWhiteSpace([string]$gate.$field)) {
                throw "Stage3 gate '$($gate.name)' missing required field '$field'."
            }
        }
    }
}

function Assert-GeneratorTemplates {
    $path = 'docs/generator-templates.json'
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing required file: $path"
    }
    $manifest = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    if ($manifest.schema -ne 'cpueaxh.generator-templates.v1') {
        throw 'generator-templates.json has an invalid or missing schema.'
    }
    foreach ($section in @('naming', 'shared_requirements', 'families')) {
        if ($null -eq $manifest.PSObject.Properties[$section]) {
            throw "generator-templates.json is missing required section: $section"
        }
    }
    foreach ($requirement in @('exact_selector_required', 'stable_names')) {
        if (-not [bool]$manifest.naming.$requirement) {
            throw "generator-templates.json missing naming requirement: $requirement"
        }
    }
    foreach ($requirement in @('deterministic_seed', 'safe_return_path', 'flag_mask_required')) {
        if (-not [bool]$manifest.shared_requirements.$requirement) {
            throw "generator-templates.json missing shared requirement: $requirement"
        }
    }
    $families = @($manifest.families.PSObject.Properties | ForEach-Object { $_.Name })
    Assert-SetEquals -Actual $families -Expected $RequiredGeneratorFamilies -Label 'Generator template family set'

    foreach ($family in $RequiredGeneratorFamilies) {
        $entry = $manifest.families.$family
        if ($null -eq $entry.examples -or $entry.examples.Count -eq 0) {
            throw "generator family '$family' must list examples."
        }
        if ($null -eq $entry.required_fields -or $entry.required_fields.Count -eq 0) {
            throw "generator family '$family' must list required_fields."
        }
        if ($null -eq $entry.special_rules -or $entry.special_rules.Count -eq 0) {
            throw "generator family '$family' must list special_rules."
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

Assert-FileContains -Path 'docs/instruction-status.json' -Pattern 'required_identity_fields' -Message 'instruction-status.json must define form-level identity fields.'
Assert-FileContains -Path 'docs/instruction-status.json' -Pattern 'unsafe_for_native' -Message 'instruction-status.json must keep unsafe_for_native status available.'
Assert-FileContains -Path 'docs/instruction-status.json' -Pattern 'stage3_contracts' -Message 'instruction-status.json must link stage3 contracts.'
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
Assert-FileContains -Path '.github/workflows/extended-regression.yml' -Pattern 'strict-replay\.log' -Message 'extended regression must preserve strict replay logs.'
Assert-FileContains -Path '.github/workflows/extended-regression.yml' -Pattern 'manual-replay\.log' -Message 'extended regression must preserve manual replay logs.'
Assert-FileContains -Path '.github/workflows/extended-regression.yml' -Pattern 'manual-index\.log' -Message 'extended regression must preserve manual index logs.'
Assert-FileContains -Path '.github/workflows/extended-regression.yml' -Pattern 'stage3-gates\.log' -Message 'extended regression must preserve stage3 gate logs.'
Assert-FileContains -Path '.github/workflows/extended-regression.yml' -Pattern 'cpu-features\.json' -Message 'extended regression must preserve CPU feature evidence.'
Assert-FileContains -Path '.github/workflows/extended-regression.yml' -Pattern 'generated-specs\.json' -Message 'extended regression must preserve generated spec evidence.'
Assert-FileContains -Path '.github/workflows/extended-regression.yml' -Pattern 'extended-test-run\.log' -Message 'extended regression must preserve long regression logs.'
Assert-FileContains -Path 'tools/validate-generated-spec-manifest.ps1' -Pattern 'cpueaxh\.generated-specs\.v1' -Message 'generated spec manifest validator must check schema.'
Assert-FileContains -Path 'tools/validate-instruction-status.ps1' -Pattern 'instruction-status\.json has an invalid or missing schema' -Message 'instruction status validator must check JSON schema.'
Assert-FileContains -Path 'cpueaxh/cpu/executor.hpp' -Pattern 'CPUEAXH_STRICT_INTERNAL' -Message 'executor must keep strict internal decode checks enabled.'
Assert-FileContains -Path 'docs/hardware-runner-matrix.md' -Pattern 'GitHub hosted runner feature matrix' -Message 'runner matrix doc must describe hosted-runner validation.'
Assert-FileContains -Path 'docs/hardware-runner-matrix.md' -Pattern 'hosted-runner-only' -Message 'runner matrix doc must avoid self-hosted hardware requirements.'

Assert-Stage3GateManifest
Assert-GeneratorTemplates
Assert-NonEmptyJsonCorpus
Assert-ManualIndexRecords
Assert-CoreHeadersHavePragmaOnce
Assert-CoreHeaderSmokeTranslationUnits
Assert-IndividualHeaderSmokeScript
Assert-BuildLogWarningGate
Assert-CpueaxhInternalUsesInstructionModules
Assert-InstructionModuleCoverage
Assert-StrictReplayFixtures
Assert-GeneratedManifestPolicyFields
Assert-RequiredCoverageGates
Assert-AllFailuresCollection
Assert-NoLegacyFrameworkJsonExtractors
Assert-FrameworkHeadersDoNotRequireUmbrellaOrder
Assert-RoundSwitchesUseUnreachableDefault
Assert-MaskedSemanticSwitchesUseUnreachableDefault
Assert-CheckedMemoryReadHelpers
Assert-AvxVexModuleSplit

Write-Host 'Regression contract validation passed.'
