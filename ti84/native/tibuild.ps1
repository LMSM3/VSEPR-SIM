#!/usr/bin/env pwsh
# =========================================================================
# tibuild.ps1 - One-command C-to-.8xp pipeline for TI-83+/84+
# =========================================================================
#
# Usage:
#   tibuild.ps1 <source.c> <PROGNAME> [options]
#
# Examples:
#   tibuild.ps1 src\hello.c HELLO
#   tibuild.ps1 src\fatigue.c FATIGUE -shell ion
#   tibuild.ps1 src\hello.c HELLO -target 8xp
#   tibuild.ps1 src\hello.c HELLO -target 8xk
#
# Pipeline (Commandment 12 — three layers):
#   Layer 1: source.c                    (your code)
#   Layer 2: linked Z80 machine code     (zcc cross-compile)
#   Layer 3: TI container file           (.8xp or .8xk)
#
# The 18 Commandments are enforced:
#   - #1:  .8xp vs .8xk chosen explicitly (-target)
#   - #2:  Execution model chosen explicitly (-shell)
#   - #3:  Z80 only (TI-84+ SE, not CE/eZ80)
#   - #7:  Name uppercased, max 8 chars, alphanumeric only
#   - #11: Correct origin via z88dk target config
#   - #13: -create-app only used for .8xk Flash apps
#   - #17: Start with hello.c (this script makes that easy)
# =========================================================================

param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$Source,

    [Parameter(Mandatory=$true, Position=1)]
    [string]$Name,

    [ValidateSet("8xp", "8xk")]
    [string]$Target = "8xp",

    [ValidateSet("none", "ion", "mirageos", "dcs")]
    [string]$Shell = "ion",

    [switch]$Clean,
    [switch]$ShowCmd
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir  = Join-Path $ScriptDir "build"
$OutDir    = Join-Path $ScriptDir "out"

# =========================================================================
#  Validation (Commandments 3, 7, 10, 14)
# =========================================================================

# Commandment 7: naming constraints
$Name = $Name.ToUpper()
if ($Name.Length -gt 8) {
    Write-Host "ERR: Program name '$Name' exceeds 8 chars (Commandment 7)" -ForegroundColor Red
    exit 1
}
if ($Name -notmatch '^[A-Z][A-Z0-9]*$') {
    Write-Host "ERR: Program name '$Name' must be uppercase alphanumeric, start with letter (Commandment 7)" -ForegroundColor Red
    exit 1
}

# Source file exists
$SourcePath = Resolve-Path $Source -ErrorAction SilentlyContinue
if (-not $SourcePath) {
    Write-Host "ERR: Source file not found: $Source" -ForegroundColor Red
    exit 1
}
$SourcePath = $SourcePath.Path

# zcc must be available
if (-not (Get-Command zcc -ErrorAction SilentlyContinue)) {
    # Try loading from known install
    if (Test-Path "C:\z88dk\bin\zcc.exe") {
        $env:PATH = "C:\z88dk\bin;$env:PATH"
        $env:ZCCCFG = "C:\z88dk\lib\config"
    } else {
        Write-Host "ERR: zcc not found. Run install_z88dk.ps1 first." -ForegroundColor Red
        exit 1
    }
}

# Ensure build/out dirs exist
New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

# =========================================================================
#  Clean
# =========================================================================

if ($Clean) {
    Write-Host "Cleaning build artifacts..."
    Remove-Item "$BuildDir\*" -Force -ErrorAction SilentlyContinue
    Remove-Item "$OutDir\*" -Force -ErrorAction SilentlyContinue
}

# =========================================================================
#  Build (Commandments 2, 4, 5, 6, 11, 13, 14)
# =========================================================================

Write-Host ""
Write-Host "=== tibuild ===" -ForegroundColor Cyan
Write-Host "  Source:  $SourcePath"
Write-Host "  Name:    $Name"
Write-Host "  Target:  .$Target"
Write-Host "  Shell:   $Shell"
Write-Host ""

$binFile  = Join-Path $BuildDir "$Name.bin"
$outFile  = Join-Path $OutDir "$Name.$Target"

# --- Build command assembly ---
# Commandment 3: +ti83p covers TI-83+, TI-84+, TI-84+ SE (all Z80)
$zccArgs = @(
    "+ti83p"                # Z80 target (Commandment 3)
    "-vn"                   # No verbose unless requested
    "-O2"                   # Optimize
    "--math32"              # 32-bit IEEE float (Z80 needs explicit math lib)
    "-lm"                   # Link math library (log10, pow, sqrt)
    "-o", $binFile          # Output binary
)

# Commandment 2: execution model determines compiler flags
switch ($Target) {
    "8xp" {
        # Commandment 13: do NOT use -create-app for .8xp
        # We compile to binary, then wrap separately
        switch ($Shell) {
            "ion"      {
                $zccArgs += "-startup=4"     # Ion shell
                $zccArgs += "-pragma-define:CRT_ENABLE_RESTART=1"
            }
            "mirageos" {
                $zccArgs += "-startup=5"     # MirageOS
                $zccArgs += "-pragma-define:CRT_ENABLE_RESTART=1"
            }
            "dcs"      {
                $zccArgs += "-startup=6"     # Doors CS
                $zccArgs += "-pragma-define:CRT_ENABLE_RESTART=1"
            }
            "none"     {
                $zccArgs += "-startup=1"     # No shell (raw ASM program)
            }
        }
    }
    "8xk" {
        # Commandment 13: -create-app IS correct for Flash apps
        $zccArgs += "-create-app"
        $zccArgs += "-startup=0"
    }
}

# Include path for our headers
$includeDir = Join-Path $ScriptDir "include"
if (Test-Path $includeDir) {
    $zccArgs += "-I$includeDir"
}

# Source file last
$zccArgs += $SourcePath

# --- Compile ---
Write-Host "Step 1: Compile (C -> Z80 binary)" -ForegroundColor Yellow
if ($ShowCmd) {
    Write-Host "  zcc $($zccArgs -join ' ')"
}

$compileOutput = & zcc @zccArgs 2>&1
$compileExit = $LASTEXITCODE

if ($compileExit -ne 0) {
    Write-Host "COMPILE FAILED (exit $compileExit):" -ForegroundColor Red
    $compileOutput | ForEach-Object { Write-Host "  $_" }
    exit 2
}

# z88dk may produce .bin directly or with different extension
$actualBin = $binFile
if (-not (Test-Path $actualBin)) {
    # Try without extension, or with _code suffix
    $candidates = @(
        "$BuildDir\${Name}_code.bin",
        "$BuildDir\$Name",
        "$BuildDir\${Name}.bin"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { $actualBin = $c; break }
    }
}

if ($Target -eq "8xk") {
    # For Flash apps, -create-app produces the .8xk directly
    $appFile = Get-ChildItem $BuildDir -Filter "*.8xk" | Select-Object -First 1
    if ($appFile) {
        Copy-Item $appFile.FullName $outFile -Force
        Write-Host ""
        Write-Host "=== BUILD SUCCESS ===" -ForegroundColor Green
        Write-Host "  Output: $outFile"
        Write-Host "  Size:   $((Get-Item $outFile).Length) bytes"
        exit 0
    } else {
        Write-Host "ERR: -create-app did not produce .8xk" -ForegroundColor Red
        exit 3
    }
}

# --- Wrap as .8xp (Commandments 5, 6, 14, 15, 16) ---
Write-Host "Step 2: Wrap (binary -> .8xp)" -ForegroundColor Yellow

if (-not (Test-Path $actualBin)) {
    Write-Host "ERR: Binary not found at $actualBin" -ForegroundColor Red
    Write-Host "  Build dir contents:"
    Get-ChildItem $BuildDir | ForEach-Object { Write-Host "    $($_.Name) ($($_.Length) bytes)" }
    exit 3
}

# Use z88dk's appmake if available, otherwise use our bin2var
$appmake = Get-Command "z88dk-appmake" -ErrorAction SilentlyContinue
if (-not $appmake) {
    $appmake = Get-Command "appmake" -ErrorAction SilentlyContinue
}

if ($appmake) {
    # z88dk appmake can produce .8xp
    $appArgs = @(
        "+ti8x"
        "-b", $actualBin
        "-o", $outFile
        "--noloader"        # We handle shell format in compile step
    )
    if ($ShowCmd) { Write-Host "  appmake $($appArgs -join ' ')" }
    & $appmake.Source @appArgs 2>&1 | ForEach-Object { if ($ShowCmd) { Write-Host "  $_" } }
}

if (-not (Test-Path $outFile)) {
    # Fallback: use our PowerShell bin2var (Commandments 5, 6, 15)
    Write-Host "  Using built-in bin2var wrapper..."
    & (Join-Path $ScriptDir "bin2var.ps1") -BinFile $actualBin -OutFile $outFile -Name $Name
}

# --- Verify (Commandment 16) ---
if (-not (Test-Path $outFile)) {
    Write-Host "ERR: Output file not created" -ForegroundColor Red
    exit 4
}

$fileBytes = [System.IO.File]::ReadAllBytes($outFile)
$header = [System.Text.Encoding]::ASCII.GetString($fileBytes[0..([Math]::Min(10, $fileBytes.Length-1))])

Write-Host ""
Write-Host "Step 3: Verify (Commandment 16)" -ForegroundColor Yellow
Write-Host "  File:    $outFile"
Write-Host "  Size:    $($fileBytes.Length) bytes"
Write-Host "  Header:  $($header.Substring(0, [Math]::Min(8, $header.Length)))"

if ($header.StartsWith("**TI83F*") -or $header.StartsWith("**TI84F*")) {
    Write-Host "  Status:  VALID TI header" -ForegroundColor Green
} else {
    Write-Host "  Status:  WARNING - header doesn't match expected TI format" -ForegroundColor Yellow
    Write-Host "           File may not transfer correctly."
}

Write-Host ""
Write-Host "=== BUILD SUCCESS ===" -ForegroundColor Green
Write-Host "  Output: $outFile"
Write-Host "  Size:   $($fileBytes.Length) bytes"
Write-Host "  Send:   TI Connect CE -> Send to Device"
Write-Host ""
Write-Host "  Commandment 18 reminder:" -ForegroundColor Yellow
Write-Host "  'sendable' != 'runnable' - test on calc!" -ForegroundColor Yellow
