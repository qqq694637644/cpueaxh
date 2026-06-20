param(
    [string]$InstructionDir = 'cpueaxh/instructions'
)

$ErrorActionPreference = 'Stop'

$umbrellaNames = @(
    'integer_instructions.hpp',
    'control_flow_instructions.hpp',
    'stack_instructions.hpp',
    'string_instructions.hpp',
    'simd_sse_instructions.hpp',
    'simd_avx_instructions.hpp',
    'crypto_instructions.hpp',
    'system_escape_instructions.hpp',
    'x87_instructions.hpp'
)
$excluded = @(
    'instruction_common.hpp',
    'all_instructions.hpp'
) + $umbrellaNames

if (-not (Test-Path -LiteralPath $InstructionDir -PathType Container)) {
    throw "Instruction directory missing: $InstructionDir"
}

$headers = @(Get-ChildItem -LiteralPath $InstructionDir -Filter '*.hpp' -File | Where-Object { $_.Name -notin $excluded } | Sort-Object Name)
if ($headers.Count -eq 0) {
    throw "No instruction implementation headers found under $InstructionDir"
}

$membership = @{}
foreach ($header in $headers) {
    $membership[$header.Name] = New-Object System.Collections.Generic.List[string]
}

foreach ($umbrellaName in $umbrellaNames) {
    $path = Join-Path $InstructionDir $umbrellaName
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing instruction-family umbrella header: $path"
    }
    $content = Get-Content -LiteralPath $path -Raw
    foreach ($header in $headers) {
        $pattern = '#include\s+"' + [regex]::Escape($header.Name) + '"'
        if ($content -match $pattern) {
            $membership[$header.Name].Add($umbrellaName)
        }
    }
}

$missing = @()
$duplicates = @()
foreach ($header in $headers) {
    $owners = @($membership[$header.Name])
    if ($owners.Count -eq 0) {
        $missing += $header.Name
    }
    elseif ($owners.Count -gt 1) {
        $duplicates += ("$($header.Name): $($owners -join ', ')")
    }
}

if ($missing.Count -ne 0) {
    throw "Instruction headers missing from every family umbrella: $($missing -join ', ')"
}
if ($duplicates.Count -ne 0) {
    throw "Instruction headers assigned to multiple family umbrellas: $($duplicates -join '; ')"
}

Write-Host "Instruction module coverage validation passed. Headers: $($headers.Count)"
