# launch_viewer.ps1
# =================
# Starts the supported fixed-timestep live viewer.

param(
    [string]$Artifact = "",
    [switch]$NoStdin
)

function Find-Exe([string]$name) {
    $candidates = @(
        (Join-Path $PSScriptRoot "..\build\$name.exe"),
        (Join-Path $PSScriptRoot "..\build_vis\$name.exe"),
        "$name.exe"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) { return (Resolve-Path $candidate).Path }
    }
    return $name
}

$viewer = Find-Exe "vsepr-view"
$arguments = @()
if ($Artifact) {
    $arguments += @("--artifact", $Artifact)
}
if ($NoStdin) {
    $arguments += "--no-stdin"
}

Write-Host "[launch_viewer] Starting $viewer"
& $viewer @arguments
exit $LASTEXITCODE
