param(
    [Parameter(Mandatory = $true)][string]$ManifestPath,
    [string]$RegressionDir = 'test/regression'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "Generated spec manifest is missing: $ManifestPath"
}

$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
if ($manifest.schema -ne 'cpueaxh.generated-specs.v1') {
    throw "Unexpected generated spec manifest schema: $($manifest.schema)"
}
if (-not $manifest.specs -or $manifest.specs.Count -eq 0) {
    throw 'Generated spec manifest has no specs.'
}
if ([int]$manifest.spec_count -ne $manifest.specs.Count) {
    throw "Generated spec count mismatch. Header=$($manifest.spec_count) actual=$($manifest.specs.Count)"
}

$names = @($manifest.specs | ForEach-Object { [string]$_.name })
$emptyNames = @($names | Where-Object { [string]::IsNullOrWhiteSpace($_) })
if ($emptyNames.Count -ne 0) {
    throw 'Generated spec manifest contains empty spec names.'
}
$duplicateNames = @($names | Group-Object | Where-Object { $_.Count -gt 1 } | ForEach-Object { $_.Name })
if ($duplicateNames.Count -ne 0) {
    throw "Generated spec manifest contains duplicate names: $($duplicateNames -join ', ')"
}

foreach ($spec in $manifest.specs) {
    foreach ($field in @('name', 'instruction_form', 'family', 'operation', 'encoding', 'operand_shape', 'feature_gate', 'native_safety', 'op', 'variant', 'flag_mask', 'compare')) {
        if ($null -eq $spec.$field) {
            throw "Generated spec '$($spec.name)' is missing field '$field'."
        }
    }
    if ([string]::IsNullOrWhiteSpace([string]$spec.instruction_form)) {
        throw "Generated spec '$($spec.name)' has an empty instruction_form."
    }
    if ([string]::IsNullOrWhiteSpace([string]$spec.operation)) {
        throw "Generated spec '$($spec.name)' has an empty operation."
    }
    foreach ($compareField in @('gprs', 'rip', 'rsp_normalized', 'mxcsr', 'xmm', 'ymm', 'x87', 'memory_regions', 'flags')) {
        if ($null -eq $spec.compare.$compareField) {
            throw "Generated spec '$($spec.name)' compare policy is missing '$compareField'."
        }
    }
    foreach ($disabledState in @('rip', 'mxcsr', 'xmm', 'ymm', 'x87')) {
        if ($false -eq [bool]$spec.compare.$disabledState.enabled -and [string]::IsNullOrWhiteSpace([string]$spec.compare.$disabledState.reason_if_disabled)) {
            throw "Generated spec '$($spec.name)' compare policy disables '$disabledState' without a reason."
        }
    }
    foreach ($flagField in @('defined', 'defined_output_mask', 'must_preserve_mask', 'compare_all_defined', 'reason_if_not_compared')) {
        if ($null -eq $spec.compare.flags.$flagField) {
            throw "Generated spec '$($spec.name)' flag compare policy is missing '$flagField'."
        }
    }
    if ([uint64]$spec.compare.flags.defined_output_mask -eq 0 -and [string]::IsNullOrWhiteSpace([string]$spec.compare.flags.reason_if_not_compared)) {
        throw "Generated spec '$($spec.name)' does not compare flags and has no reason_if_not_compared."
    }
}

foreach ($required in @('add_rr_rax_rbx', 'mov_mem_roundtrip', 'add_mem')) {
    if ($required -notin $names) {
        throw "Generated spec manifest is missing required baseline spec: $required"
    }
}

if (-not (Test-Path -LiteralPath $RegressionDir -PathType Container)) {
    throw "Regression corpus directory is missing: $RegressionDir"
}
foreach ($file in Get-ChildItem -LiteralPath $RegressionDir -Filter '*.json' -File) {
    $json = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($json.schema -ne 'cpueaxh.failure.v1') {
        throw "Regression replay record has unsupported schema: $($file.FullName)"
    }
    if ([string]::IsNullOrWhiteSpace([string]$json.case_selector)) {
        throw "Regression replay record has no case_selector: $($file.FullName)"
    }
    if ($json.case_selector -notin $names) {
        throw "Regression replay case_selector '$($json.case_selector)' is missing from generated spec manifest. File: $($file.FullName)"
    }
}

Write-Host "Generated spec manifest validation passed."
