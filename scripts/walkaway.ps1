<#
.SYNOPSIS
    Walk-away improvement automation — start and leave.

.DESCRIPTION
    Launches the three-phase walk-away improvement system:
      Phase A: Continuous polynomial fits (11-15 layer) + eigen counters
      Phase B: Python library (already packaged via pyproject.toml)
      Phase C: Closed slow-loop (flagging, simulation, reporting)

    Run this script, walk away, come back to accumulated data.

.EXAMPLE
    .\scripts\walkaway.ps1
    .\scripts\walkaway.ps1 -PhaseA -Specs "NaCl@crystal","Fe@crystal"
    .\scripts\walkaway.ps1 -PhaseC -Iterations 10
    .\scripts\walkaway.ps1 -All
#>

param(
    [switch]$PhaseA,
    [switch]$PhaseC,
    [switch]$All,
    [switch]$ScanOnly,
    [string[]]$Specs = @("H2O", "NaCl@crystal", "Fe@crystal", "C6H6"),
    [int]$Runs = 0,
    [int]$Iterations = 0,
    [double]$Delay = 5.0,
    [string]$OutputDir = "out/walkaway",
    [string]$ImprovementDir = "out/improvement"
)

$ErrorActionPreference = "Stop"
$ROOT = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not (Test-Path "$ROOT/pykernel/__init__.py")) {
    $ROOT = Split-Path -Parent $PSScriptRoot
    if (-not (Test-Path "$ROOT/pykernel/__init__.py")) {
        $ROOT = $PSScriptRoot | Split-Path -Parent
    }
}

Write-Host "=" * 60 -ForegroundColor Cyan
Write-Host "VSEPR-SIM Walk-Away Improvement System" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Cyan
Write-Host "  Project root: $ROOT"
Write-Host "  Timestamp:    $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Host ""

# Ensure Python is available
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    $python = Get-Command python3 -ErrorAction SilentlyContinue
}
if (-not $python) {
    Write-Host "ERROR: Python not found in PATH" -ForegroundColor Red
    exit 1
}
Write-Host "  Python: $($python.Source)"

# Check numpy
$numpyCheck = & $python.Source -c "import numpy; print(numpy.__version__)" 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "  Installing numpy..." -ForegroundColor Yellow
    & $python.Source -m pip install numpy --quiet
}
else {
    Write-Host "  numpy: $numpyCheck"
}

# Install pykernel in development mode if not already
Push-Location $ROOT
$pykernelCheck = & $python.Source -c "import pykernel; print(pykernel.__version__)" 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "  Installing pykernel in development mode..." -ForegroundColor Yellow
    & $python.Source -m pip install -e . --quiet
}
else {
    Write-Host "  pykernel: $pykernelCheck"
}
Pop-Location

Write-Host ""

# Determine what to run
if (-not $PhaseA -and -not $PhaseC -and -not $All -and -not $ScanOnly) {
    $All = $true
}

if ($ScanOnly) {
    Write-Host "[Scan] Scanning flagged files..." -ForegroundColor Green
    & $python.Source -m pykernel scan
    exit 0
}

if ($PhaseA -or $All) {
    Write-Host "[Phase A] Continuous Runner — poly fits + eigen counters" -ForegroundColor Green
    Write-Host "  Specs: $($Specs -join ', ')"
    Write-Host "  Runs: $(if ($Runs -eq 0) { 'infinite' } else { $Runs })"
    Write-Host "  Output: $OutputDir"
    Write-Host ""

    $specArgs = @()
    foreach ($s in $Specs) {
        $specArgs += "--spec"
        $specArgs += $s
    }

    if ($All) {
        # Run Phase A in background, Phase C in foreground
        $phaseAJob = Start-Job -ScriptBlock {
            param($py, $root, $specArgs, $runs, $delay, $outDir)
            Set-Location $root
            $allArgs = @("-m", "pykernel", "run") + $specArgs + @("--runs", $runs, "--delay", $delay, "--output", $outDir)
            & $py @allArgs
        } -ArgumentList $python.Source, $ROOT, $specArgs, $Runs, $Delay, $OutputDir

        Write-Host "  Phase A started as background job: $($phaseAJob.Id)" -ForegroundColor Yellow
    }
    else {
        # Run Phase A in foreground
        Push-Location $ROOT
        $allArgs = @("-m", "pykernel", "run") + $specArgs + @("--runs", "$Runs", "--delay", "$Delay", "--output", $OutputDir)
        & $python.Source @allArgs
        Pop-Location
        exit 0
    }
}

if ($PhaseC -or $All) {
    Write-Host ""
    Write-Host "[Phase C] Improvement Loop — closed slow loop" -ForegroundColor Green
    Write-Host "  Iterations: $(if ($Iterations -eq 0) { 'infinite' } else { $Iterations })"
    Write-Host "  Output: $ImprovementDir"
    Write-Host ""

    Push-Location $ROOT
    & $python.Source -m pykernel improve --iterations $Iterations --delay $Delay --output $ImprovementDir
    Pop-Location
}

# If both phases were running, clean up Phase A
if ($All -and $phaseAJob) {
    Write-Host ""
    Write-Host "[Cleanup] Stopping Phase A background job..." -ForegroundColor Yellow
    Stop-Job $phaseAJob -ErrorAction SilentlyContinue
    Receive-Job $phaseAJob -ErrorAction SilentlyContinue
    Remove-Job $phaseAJob -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "=" * 60 -ForegroundColor Cyan
Write-Host "Walk-away session complete." -ForegroundColor Cyan
Write-Host "  Phase A output: $OutputDir"
Write-Host "  Phase C output: $ImprovementDir"
Write-Host "=" * 60 -ForegroundColor Cyan
