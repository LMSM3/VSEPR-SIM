# =============================================================================
# demo_viz_01_xyz_single.ps1
# Run from repo root:  powershell -ExecutionPolicy Bypass -File scripts\demo_viz_01_xyz_single.ps1
#
# What this does:
#   1. Emits a 64-atom Ar gas cloud (single XYZ frame)
#   2. Validates the .vsim script
#   3. Opens the XYZ file in the VSEPR-SIM terminal 3D viewer
# =============================================================================
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root   = Split-Path $PSScriptRoot -Parent
$Bin    = Join-Path $Root "installer\bin"
$Build  = Join-Path $Root "build"
$OutDir = Join-Path $Root "out\demo_viz_01"

$VseprSim = Join-Path $Bin "vsepr-sim.exe"
$Vsepr    = Join-Path $Build "vsepr.exe"

Write-Host ""
Write-Host "=====================================================" -ForegroundColor Cyan
Write-Host "  DEMO 1 - Single-frame XYZ: Ar gas cloud at 300 K"   -ForegroundColor Cyan
Write-Host "=====================================================" -ForegroundColor Cyan
Write-Host ""

# ── Step 1: Emit the gas cloud ────────────────────────────────────────────────
Write-Host "[1/3] Generating 64-atom Ar gas cloud (30 Ang cubic box, PBC on)..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OutXyz = Join-Path $OutDir "Ar_gas_300K.xyz"

& $Vsepr Ar@gas emit --cloud 64 --box 30,30,30 --pbc 2>&1 | Tee-Object -Variable emitOut
# vsepr writes to out.xyz in CWD; move it
if (Test-Path "out.xyz") {
	Move-Item "out.xyz" $OutXyz -Force
} elseif (-not (Test-Path $OutXyz)) {
	throw "emit did not produce out.xyz"
}
Write-Host "  -> $OutXyz" -ForegroundColor Green

# ── Step 2: Validate the .vsim script ────────────────────────────────────────
Write-Host ""
Write-Host "[2/3] Validating demo_viz_01_xyz_single.vsim..." -ForegroundColor Yellow
$VsimScript = Join-Path $Root "scripts\demo_viz_01_xyz_single.vsim"
& $VseprSim validate $VsimScript 2>&1

# ── Step 3: Open in 3D viewer ─────────────────────────────────────────────────
Write-Host ""
Write-Host "[3/3] Opening Ar_gas_300K.xyz in terminal 3D viewer..." -ForegroundColor Yellow
Write-Host "      Press [q] to quit the viewer." -ForegroundColor Gray
Write-Host ""
& $VseprSim open $OutXyz
