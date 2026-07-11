# =============================================================================
# demo_viz_02_xyzf_anim.ps1
# Run from repo root:  powershell -ExecutionPolicy Bypass -File scripts\demo_viz_02_xyzf_anim.ps1
#
# What this does:
#   1. Generates OsO4 and CH3HgI multi-frame xyzf trajectories (gen_xyz_demo)
#   2. Validates the .vsim script
#   3. Opens the OsO4 xyzf in the terminal trajectory viewer (animated)
# =============================================================================
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root       = Split-Path $PSScriptRoot -Parent
$Bin        = Join-Path $Root "installer\bin"
$Build      = Join-Path $Root "build"
$OutDir     = Join-Path $Root "out\demo_viz_02"

$VseprSim   = Join-Path $Bin   "vsepr-sim.exe"
$GenXyz     = Join-Path $Build "gen_xyz_demo.exe"

Write-Host ""
Write-Host "==================================================================" -ForegroundColor Cyan
Write-Host "  DEMO 2 - Multi-frame XYZF animation: OsO4 NVT trajectory"        -ForegroundColor Cyan
Write-Host "==================================================================" -ForegroundColor Cyan
Write-Host ""

# ── Step 1: Generate xyzf trajectory files ────────────────────────────────────
Write-Host "[1/3] Generating OsO4 multi-frame trajectory via gen_xyz_demo..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# gen_xyz_demo writes to a directory argument
& $GenXyz $OutDir 2>&1

$XyzfPath = Join-Path $OutDir "osmium_tetroxide.xyzf"
if (-not (Test-Path $XyzfPath)) {
	throw "gen_xyz_demo did not produce osmium_tetroxide.xyzf in $OutDir"
}

# Report what was written
Write-Host ""
Write-Host "  Output files:" -ForegroundColor Green
Get-ChildItem $OutDir | ForEach-Object {
	Write-Host ("    {0,-40} {1,8:N0} bytes" -f $_.Name, $_.Length)
}
Write-Host ""
Write-Host "  -> Trajectory: $XyzfPath" -ForegroundColor Green

# ── Step 2: Validate the .vsim script ────────────────────────────────────────
Write-Host ""
Write-Host "[2/3] Validating demo_viz_02_xyzf_anim.vsim..." -ForegroundColor Yellow
$VsimScript = Join-Path $Root "scripts\demo_viz_02_xyzf_anim.vsim"
& $VseprSim validate $VsimScript 2>&1

# ── Step 3: Open in trajectory viewer ────────────────────────────────────────
Write-Host ""
Write-Host "[3/3] Opening osmium_tetroxide.xyzf in trajectory viewer (5 frames)..." -ForegroundColor Yellow
Write-Host "      Controls: [q]=quit  [p]=pause/resume  frames auto-advance" -ForegroundColor Gray
Write-Host ""
& $VseprSim open $XyzfPath
