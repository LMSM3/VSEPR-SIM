#!/usr/bin/env pwsh
<#
.SYNOPSIS
    VSEPR-Sim Build Script (wrapper)
.DESCRIPTION
    Thin wrapper — forwards all arguments to scripts/build/build.ps1.
    No param block so $args captures named flags verbatim (e.g. -Clean, -WSL).
.EXAMPLE
    .\build.ps1
    .\build.ps1 -Clean
    .\build.ps1 -WSL
    .\build.ps1 -WSL -Vis
    .\build.ps1 -BuildType Debug
    .\build.ps1 -WSL -Clean -Vis
#>

$canonicalScript = Join-Path $PSScriptRoot 'scripts' 'build' 'build.ps1'

if (-not (Test-Path $canonicalScript)) {
    Write-Error "Canonical build script not found: $canonicalScript"
    exit 1
}

& $canonicalScript @args
exit $LASTEXITCODE
