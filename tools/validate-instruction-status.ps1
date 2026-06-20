param(
    [Parameter(Mandatory = $true)][string]$ManifestPath,
    [string]$StatusPath = 'docs/instruction-status.yml',
    [string]$RegressionDir = 'test/regression'
)

$ErrorActionPreference = 'Stop'

$AllowedStatuses = @(
    'todo',
    'implemented',
    'implemented_partial',
    'differential_tested',
    'manual_tested',
    'feature_gated',
    'unsafe_for_native',
    'blocked'
)

function Get-RequiredScalar {
    param(
        [Parameter(Mandatory = $true)][string]$Block,
        [Parameter(Mandatory = $true)][string]$Field,
        [Parameter(Mandatory = $true)][string]$FormName
    )
    $pattern = "(?m)^\s+${Field}:\s*(.+?)\s*$"
    $match = [regex]::Match($Block, $pattern)
    if (-not $match.Success) {
        throw "Instruction-status form '$FormName' missing required field '$Field'."
    }
    return $match.Groups[1].Value.Trim().Trim('"')
}

function Get-RequiredBoolean {
    param(
        [Parameter(Mandatory = $true)][string]$Block,
        [Parameter(Mandatory = $true)][string]$Field,
        [Parameter(Mandatory = $true)][string]$FormName
    )
    $pattern = "(?m)^\s+${Field}:\s*(true|false)\s*$"
    $match = [regex]::Match($Block, $pattern)
    if (-not $match.Success) {
        throw "Instruction-status form '$FormName' missing boolean coverage field '$Field'."
    }
    return $match.Groups[1].Value -eq 'true'
}

function Test-FeatureGateEnabled {
    param(
        [Parameter(Mandatory = $true)][string]$FeatureGate,
        [Parameter(Mandatory = $true)]$ManifestFeatures
    )
    if ($FeatureGate -eq 'base') {
        return $true
    }
    if ($FeatureGate -eq 'controlled_environment') {
        return $false
    }

    $property = $ManifestFeatures.PSObject.Properties[$FeatureGate]
    if ($null -eq $property) {
        throw "instruction-status.yml uses unknown feature_gate '$FeatureGate'."
    }
    return [bool]$property.Value
}

if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "Generated spec manifest is missing: $ManifestPath"
}
if (-not (Test-Path -LiteralPath $StatusPath -PathType Leaf)) {
    throw "Instruction status file is missing: $StatusPath"
}

$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
if ($manifest.schema -ne 'cpueaxh.generated-specs.v1') {
    throw "Unexpected generated spec manifest schema: $($manifest.schema)"
}
$specNames = @($manifest.specs | ForEach-Object { [string]$_.name })
$manifestInstructionForms = @($manifest.specs | ForEach-Object { [string]$_.instruction_form })

$regressionSelectors = @{}
if (Test-Path -LiteralPath $RegressionDir -PathType Container) {
    foreach ($file in Get-ChildItem -LiteralPath $RegressionDir -Filter '*.json' -File) {
        $json = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
        if (-not [string]::IsNullOrWhiteSpace([string]$json.case_selector)) {
            $regressionSelectors[[string]$json.case_selector] = $true
        }
    }
}

$status = Get-Content -LiteralPath $StatusPath -Raw
if ($status -notmatch '(?m)^schema:\s*cpueaxh\.instruction-status\.v1\s*$') {
    throw 'instruction-status.yml has an invalid or missing schema.'
}
foreach ($required in @('required_identity_fields:', 'status_values:', 'families:')) {
    if ($status -notmatch [regex]::Escape($required)) {
        throw "instruction-status.yml is missing section '$required'."
    }
}

$formMatches = [regex]::Matches(
    $status,
    '(?ms)^\s{10}-\s+name:\s*([^\r\n]+)\s*$.*?(?=^\s{10}-\s+name:\s*|^\s{6}-\s+mnemonic:\s*|^\s{2}[a-z0-9_]+:\s*$|\z)'
)
if ($formMatches.Count -eq 0) {
    throw 'instruction-status.yml has no form entries.'
}

$formNames = @{}
$generatedFormNames = @{}
foreach ($match in $formMatches) {
    $name = $match.Groups[1].Value.Trim().Trim('"')
    $block = $match.Value
    if ($formNames.ContainsKey($name)) {
        throw "instruction-status.yml has duplicate form name '$name'."
    }
    $formNames[$name] = $true

    foreach ($field in @('encoding', 'operands', 'operand_size', 'mode', 'feature_gate', 'status', 'unsafe_native_reason')) {
        [void](Get-RequiredScalar -Block $block -Field $field -FormName $name)
    }

    $featureGate = Get-RequiredScalar -Block $block -Field 'feature_gate' -FormName $name
    $formStatus = Get-RequiredScalar -Block $block -Field 'status' -FormName $name
    if ($formStatus -notin $AllowedStatuses) {
        throw "Instruction-status form '$name' uses unknown status '$formStatus'."
    }

    if ($block -notmatch '(?m)^\s+coverage:\s*$') {
        throw "Instruction-status form '$name' missing coverage block."
    }
    $generatedDifferential = Get-RequiredBoolean -Block $block -Field 'generated_differential' -FormName $name
    $regressionReplay = Get-RequiredBoolean -Block $block -Field 'regression_replay' -FormName $name
    $manualCoverage = Get-RequiredBoolean -Block $block -Field 'manual' -FormName $name

    if ($generatedDifferential -and $featureGate -eq 'controlled_environment') {
        throw "Instruction-status form '$name' cannot claim generated_differential under controlled_environment."
    }

    if ($generatedDifferential) {
        $generatedFormNames[$name] = $true
    }

    if ($generatedDifferential -and (Test-FeatureGateEnabled -FeatureGate $featureGate -ManifestFeatures $manifest.features)) {
        if ($name -notin $specNames) {
            throw "Instruction-status form '$name' claims generated_differential but is absent from generated-specs manifest."
        }
    }

    if ($regressionReplay -and $name -notin $regressionSelectors.Keys) {
        throw "Instruction-status form '$name' claims regression_replay but no test/regression JSON selects it."
    }

    if ($regressionReplay -and -not $generatedDifferential) {
        throw "Instruction-status form '$name' claims regression_replay without generated_differential."
    }

    if ($formStatus -eq 'unsafe_for_native') {
        if ($generatedDifferential) {
            throw "unsafe_for_native form '$name' must not claim generated_differential."
        }
        if (-not $manualCoverage) {
            throw "unsafe_for_native form '$name' must have manual/model coverage."
        }
        if ($block -match '(?m)^\s+unsafe_native_reason:\s*(null|""|\s*)$') {
            throw "unsafe_for_native form '$name' must explain unsafe_native_reason."
        }
    }

    if ($formStatus -in @('implemented', 'implemented_partial', 'differential_tested', 'manual_tested', 'feature_gated') -and
        -not ($generatedDifferential -or $regressionReplay -or $manualCoverage)) {
        throw "Instruction-status form '$name' has status '$formStatus' but no coverage is marked."
    }
}

foreach ($manifestForm in $manifestInstructionForms) {
    if ([string]::IsNullOrWhiteSpace($manifestForm)) {
        throw 'Generated spec manifest contains an empty instruction_form.'
    }
    if (-not $formNames.ContainsKey($manifestForm)) {
        throw "Generated spec manifest references unknown instruction_form '$manifestForm'."
    }
    if (-not $generatedFormNames.ContainsKey($manifestForm)) {
        throw "Generated spec manifest references instruction_form '$manifestForm' but instruction-status does not mark generated_differential=true."
    }
}

Write-Host "Instruction status validation passed. Forms: $($formMatches.Count)"
