param(
    [Parameter(Mandatory = $true)][string]$ManifestPath,
    [string]$StatusPath = 'docs/instruction-status.json',
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
        throw "instruction-status.json uses unknown feature_gate '$FeatureGate'."
    }
    return [bool]$property.Value
}

function Require-StringField {
    param(
        [Parameter(Mandatory = $true)]$Object,
        [Parameter(Mandatory = $true)][string]$Field,
        [Parameter(Mandatory = $true)][string]$FormName
    )
    if ($null -eq $Object.PSObject.Properties[$Field] -or [string]::IsNullOrWhiteSpace([string]$Object.$Field)) {
        throw "Instruction-status form '$FormName' missing required field '$Field'."
    }
    return [string]$Object.$Field
}

function Require-BoolField {
    param(
        [Parameter(Mandatory = $true)]$Object,
        [Parameter(Mandatory = $true)][string]$Field,
        [Parameter(Mandatory = $true)][string]$FormName
    )
    if ($null -eq $Object.PSObject.Properties[$Field]) {
        throw "Instruction-status form '$FormName' missing boolean coverage field '$Field'."
    }
    if ($Object.$Field -isnot [bool]) {
        throw "Instruction-status form '$FormName' coverage field '$Field' must be boolean."
    }
    return [bool]$Object.$Field
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

$status = Get-Content -LiteralPath $StatusPath -Raw | ConvertFrom-Json
if ($status.schema -ne 'cpueaxh.instruction-status.v1') {
    throw "instruction-status.json has an invalid or missing schema: $($status.schema)"
}
foreach ($section in @('required_identity_fields', 'status_values', 'stage3_contracts', 'forms')) {
    if ($null -eq $status.PSObject.Properties[$section]) {
        throw "instruction-status.json is missing section '$section'."
    }
}
if (-not $status.forms -or $status.forms.Count -eq 0) {
    throw 'instruction-status.json has no form entries.'
}

$formNames = @{}
$generatedFormNames = @{}
foreach ($form in $status.forms) {
    $name = Require-StringField -Object $form -Field 'name' -FormName '<unknown>'
    if ($formNames.ContainsKey($name)) {
        throw "instruction-status.json has duplicate form name '$name'."
    }
    $formNames[$name] = $true

    foreach ($field in @('encoding', 'operands_raw', 'operand_size', 'mode', 'feature_gate', 'status', 'unsafe_native_reason')) {
        [void](Require-StringField -Object $form -Field $field -FormName $name)
    }
    if ($null -eq $form.coverage) {
        throw "Instruction-status form '$name' missing coverage block."
    }

    $featureGate = [string]$form.feature_gate
    $formStatus = [string]$form.status
    if ($formStatus -notin $AllowedStatuses) {
        throw "Instruction-status form '$name' uses unknown status '$formStatus'."
    }

    $generatedDifferential = Require-BoolField -Object $form.coverage -Field 'generated_differential' -FormName $name
    $regressionReplay = Require-BoolField -Object $form.coverage -Field 'regression_replay' -FormName $name
    $manualCoverage = Require-BoolField -Object $form.coverage -Field 'manual' -FormName $name

    if ($generatedDifferential -and $featureGate -eq 'controlled_environment') {
        throw "Instruction-status form '$name' cannot claim generated_differential under controlled_environment."
    }

    if ($generatedDifferential) {
        $generatedFormNames[$name] = $true
    }

    if ($generatedDifferential -and (Test-FeatureGateEnabled -FeatureGate $featureGate -ManifestFeatures $manifest.features)) {
        if ($name -notin $manifestInstructionForms) {
            throw "Instruction-status form '$name' claims generated_differential but is absent from generated manifest instruction_form values."
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
        if ([string]::IsNullOrWhiteSpace([string]$form.unsafe_native_reason) -or [string]$form.unsafe_native_reason -eq 'null') {
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

Write-Host "Instruction status validation passed. Forms: $($status.forms.Count)"
