#!/usr/bin/env pwsh
<#
.SYNOPSIS
    VSEPR-Sim Build Script (wrapper)
.DESCRIPTION
    Thin wrapper that calls the canonical build script.
    All build logic is in scripts/build/build.ps1
#>

param(
    [Parameter(ValueFromRemainingArguments=$true)]
    $PassThruArgs
)

$canonicalScript = Join-Path $PSScriptRoot "scripts" "build" "build.ps1"

if (-not (Test-Path $canonicalScript)) {
    Write-Error "Canonical build script not found: $canonicalScript"
    exit 1
}

& $canonicalScript @PassThruArgs
exit $LASTEXITCODE
