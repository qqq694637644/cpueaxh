param(
    [string]$LogPath = 'build.log'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
    throw "Build log not found: $LogPath"
}

$warningPatterns = @(
    '(?i)\bwarning\s+(?:C\d{4}|LNK\d{4}|MSB\d{4}|D\d{4})\b',
    '(?i)\b[1-9][0-9]*\s+Warning\(s\)'
)

$warnings = @()
foreach ($pattern in $warningPatterns) {
    $warnings += @(Select-String -LiteralPath $LogPath -Pattern $pattern)
}
$warnings = @($warnings | Sort-Object Path,LineNumber,Line -Unique)

if ($warnings.Count -ne 0) {
    Write-Host "Build warnings detected in $LogPath. First 80 warning lines:"
    $warnings | Select-Object -First 80 | ForEach-Object {
        Write-Host ("{0}:{1}: {2}" -f $_.Path, $_.LineNumber, $_.Line.Trim())
    }
    throw "Build produced warnings: $($warnings.Count)."
}

Write-Host "Build log warning validation passed: $LogPath"
