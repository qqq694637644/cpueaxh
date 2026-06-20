param(
    [string]$ManifestPath = 'docs/stage3-regression-gates.json',
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
    $manifest = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    if ($manifest.schema -ne 'cpueaxh.stage3-regression-gates.v1') {
        throw "Invalid stage3 gate manifest schema: $Path"
    }
    if (-not $manifest.required_gates -or $manifest.required_gates.Count -eq 0) {
        throw "No stage3 gates found in manifest: $Path"
    }

    $entries = @{}
    foreach ($gate in $manifest.required_gates) {
        foreach ($field in @('name', 'category', 'command')) {
            if ($null -eq $gate.PSObject.Properties[$field] -or [string]::IsNullOrWhiteSpace([string]$gate.$field)) {
                throw "Stage3 gate manifest entry is missing field '$field'."
            }
        }
        $name = [string]$gate.name
        if ($entries.ContainsKey($name)) {
            throw "Duplicate stage3 gate manifest entry: $name"
        }
        $entries[$name] = [pscustomobject]@{
            Name = $name
            Category = [string]$gate.category
            Command = [string]$gate.command
        }
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
