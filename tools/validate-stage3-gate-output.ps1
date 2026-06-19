param(
    [string]$ManifestPath = 'docs/stage3-regression-gates.yml',
    [Parameter(Mandatory = $true)][string]$OutputPath
)

$ErrorActionPreference = 'Stop'

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

function Get-ManifestGates {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing stage3 gate manifest: $Path"
    }
    $content = Get-Content -LiteralPath $Path -Raw
    if ($content -notmatch '(?m)^schema:\s*cpueaxh\.stage3-regression-gates\.v1\s*$') {
        throw "Invalid stage3 gate manifest schema: $Path"
    }

    $entries = @{}
    $matches = [regex]::Matches($content, '(?ms)^\s+-\s+name:\s*([^\r\n]+)\s*$.*?(?=^\s+-\s+name:\s*|\z)')
    foreach ($match in $matches) {
        $name = $match.Groups[1].Value.Trim()
        $block = $match.Value
        if ($block -notmatch '(?m)^\s+category:\s*([^\r\n]+)\s*$') {
            throw "Gate '$name' missing category in manifest."
        }
        $category = $Matches[1].Trim()
        if ($block -notmatch '(?m)^\s+command:\s*([^\r\n]+)\s*$') {
            throw "Gate '$name' missing command in manifest."
        }
        $command = $Matches[1].Trim()
        $entries[$name] = [pscustomobject]@{
            Name = $name
            Category = $category
            Command = $command
        }
    }
    if ($entries.Count -eq 0) {
        throw "No stage3 gates found in manifest: $Path"
    }
    return $entries
}

function Get-LoggedGates {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing stage3 gate output: $Path"
    }

    $entries = @{}
    foreach ($line in Get-Content -LiteralPath $Path) {
        if ($line -match '^([a-z0-9_]+)\s+\[([^\]]+)\].*\|\s*(.+)\s*$') {
            $name = $Matches[1].Trim()
            $entries[$name] = [pscustomobject]@{
                Name = $name
                Category = $Matches[2].Trim()
                Command = $Matches[3].Trim()
            }
        }
    }
    if ($entries.Count -eq 0) {
        throw "No stage3 gates found in output: $Path"
    }
    return $entries
}

$manifestGates = Get-ManifestGates -Path $ManifestPath
$loggedGates = Get-LoggedGates -Path $OutputPath

Assert-SetEquals -Actual ([string[]]$loggedGates.Keys) -Expected ([string[]]$manifestGates.Keys) -Label 'Stage3 gate output names'

foreach ($name in $manifestGates.Keys) {
    $expected = $manifestGates[$name]
    $actual = $loggedGates[$name]
    if ($actual.Category -ne $expected.Category) {
        throw "Stage3 gate '$name' category mismatch. Expected '$($expected.Category)' got '$($actual.Category)'."
    }
    if ($actual.Command -ne $expected.Command) {
        throw "Stage3 gate '$name' command mismatch. Expected '$($expected.Command)' got '$($actual.Command)'."
    }
}

Write-Host "Stage3 gate output validation passed."
