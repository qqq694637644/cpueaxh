param(
    [string]$Root = 'cpueaxh'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Root -PathType Container)) {
    throw "Source root not found: $Root"
}

$sourceFiles = @(Get-ChildItem -LiteralPath $Root -Recurse -File -Include '*.hpp','*.cpp' |
    Where-Object { $_.FullName -notmatch '\\(x64|Debug|Release)\\' } |
    Sort-Object FullName)

if ($sourceFiles.Count -eq 0) {
    throw "No source files found under $Root"
}

$forbiddenOperandReads = @()
foreach ($file in $sourceFiles) {
    $relative = $file.FullName.Substring((Get-Location).Path.Length + 1).Replace('\', '/')
    if ($relative -eq 'cpueaxh/cpu/memory.hpp') {
        continue
    }
    $matches = Select-String -LiteralPath $file.FullName -Pattern '\bread_memory_operand\s*\(' -AllMatches
    foreach ($match in $matches) {
        $forbiddenOperandReads += "${relative}:$($match.LineNumber): $($match.Line.Trim())"
    }
}

if ($forbiddenOperandReads.Count -ne 0) {
    throw "read_memory_operand() is only allowed inside cpueaxh/cpu/memory.hpp; use read_memory_operand_checked() at call sites that consume values. Offenders: $($forbiddenOperandReads -join '; ')"
}

$memory = Get-Content -LiteralPath 'cpueaxh/cpu/memory.hpp' -Raw
foreach ($required in @(
    'struct CpuReadResult8',
    'struct CpuReadResult16',
    'struct CpuReadResult32',
    'struct CpuReadResult64',
    'read_memory_byte_checked',
    'read_memory_word_checked',
    'read_memory_dword_checked',
    'read_memory_qword_checked',
    'read_memory_operand_checked'
)) {
    if ($memory -notmatch [regex]::Escape($required)) {
        throw "Missing checked memory read contract symbol: $required"
    }
}

if ($memory -notmatch 'const\s+CpuReadResult64\s+read_result\s*=\s*read_memory_operand_checked') {
    throw 'cpu_atomic_rmw_memory must use read_memory_operand_checked() before consuming an operand value.'
}

Write-Host 'Memory read contract validation passed.'
