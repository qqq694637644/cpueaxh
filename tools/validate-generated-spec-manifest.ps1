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
    foreach ($field in @('name', 'family', 'op', 'variant', 'flag_mask')) {
        if ($null -eq $spec.$field) {
            throw "Generated spec '$($spec.name)' is missing field '$field'."
        }
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
