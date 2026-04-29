#!/usr/bin/env pwsh
# =========================================================================
# install_z88dk.ps1 - Install z88dk toolchain for TI-83+/84+ development
# =========================================================================
#
# Downloads and installs z88dk (the Z80 C cross-compiler).
# Sets environment variables needed by zcc.
#
# Run once:  .\install_z88dk.ps1
# Then:      .\tibuild.ps1 src\hello.c HELLO
#
# This script:
#   1. Downloads z88dk nightly (Windows) from GitHub
#   2. Extracts to C:\z88dk
#   3. Sets ZCCCFG and adds bin to PATH (user scope)
#   4. Verifies zcc runs
# =========================================================================

$ErrorActionPreference = "Stop"

$Z88DK_DIR   = "C:\z88dk"
$Z88DK_URL   = "https://github.com/z88dk/z88dk/releases/download/v2.4/z88dk-win32-2.4.zip"
$Z88DK_ZIP   = "$env:TEMP\z88dk-win32-2.4.zip"

Write-Host "=== z88dk Installer for TI-84+ ===" -ForegroundColor Cyan
Write-Host ""

# --- Check if already installed ---
if (Get-Command zcc -ErrorAction SilentlyContinue) {
    Write-Host "zcc already on PATH:" -ForegroundColor Green
    zcc --version 2>&1 | Select-Object -First 1
    Write-Host ""
    Write-Host "To reinstall, remove $Z88DK_DIR and clear PATH entry."
    exit 0
}

# --- Download ---
if (Test-Path $Z88DK_ZIP) {
    Write-Host "Using cached download: $Z88DK_ZIP"
} else {
    Write-Host "Downloading z88dk from GitHub..."
    Write-Host "  URL: $Z88DK_URL"
    Invoke-WebRequest -Uri $Z88DK_URL -OutFile $Z88DK_ZIP -UseBasicParsing
    Write-Host "  Downloaded: $((Get-Item $Z88DK_ZIP).Length / 1MB) MB"
}

# --- Extract ---
if (Test-Path $Z88DK_DIR) {
    Write-Host "Removing old installation at $Z88DK_DIR..."
    Remove-Item -Recurse -Force $Z88DK_DIR
}

Write-Host "Extracting to $Z88DK_DIR..."
Expand-Archive -Path $Z88DK_ZIP -DestinationPath "C:\" -Force

# The archive may extract to C:\z88dk or C:\z88dk-win32-latest\z88dk
# Normalize to C:\z88dk
if (-not (Test-Path "$Z88DK_DIR\bin\zcc.exe")) {
    $inner = Get-ChildItem "C:\" -Directory | Where-Object {
        $_.Name -like "z88dk*" -and (Test-Path "$($_.FullName)\bin\zcc.exe")
    } | Select-Object -First 1
    if ($inner) {
        Move-Item $inner.FullName $Z88DK_DIR -Force
        Write-Host "  Normalized to $Z88DK_DIR"
    }
}

# Check for nested structure (some releases have z88dk/z88dk/)
if (-not (Test-Path "$Z88DK_DIR\bin\zcc.exe") -and (Test-Path "$Z88DK_DIR\z88dk\bin\zcc.exe")) {
    $nested = "$Z88DK_DIR\z88dk"
    $tempDir = "C:\z88dk_tmp"
    Move-Item $nested $tempDir -Force
    Remove-Item $Z88DK_DIR -Recurse -Force
    Move-Item $tempDir $Z88DK_DIR -Force
    Write-Host "  Unwrapped nested directory"
}

if (-not (Test-Path "$Z88DK_DIR\bin\zcc.exe")) {
    Write-Host "ERROR: zcc.exe not found after extraction." -ForegroundColor Red
    Write-Host "Directory contents:"
    Get-ChildItem $Z88DK_DIR -Recurse -Depth 2 | Select-Object FullName
    exit 1
}

# --- Environment Variables ---
Write-Host ""
Write-Host "Setting environment variables..."

# ZCCCFG points to the lib/config directory
$ZCCCFG = "$Z88DK_DIR\lib\config"
if (-not (Test-Path $ZCCCFG)) {
    # Some builds use different structure
    $ZCCCFG = (Get-ChildItem "$Z88DK_DIR" -Recurse -Directory -Filter "config" |
               Where-Object { Test-Path "$($_.FullName)\*.cfg" } |
               Select-Object -First 1).FullName
}

[System.Environment]::SetEnvironmentVariable("ZCCCFG", $ZCCCFG, "User")
$env:ZCCCFG = $ZCCCFG
Write-Host "  ZCCCFG = $ZCCCFG"

# Add bin to PATH
$binDir = "$Z88DK_DIR\bin"
$userPath = [System.Environment]::GetEnvironmentVariable("PATH", "User")
if ($userPath -notlike "*$binDir*") {
    [System.Environment]::SetEnvironmentVariable("PATH", "$binDir;$userPath", "User")
    Write-Host "  Added $binDir to user PATH"
}
$env:PATH = "$binDir;$env:PATH"

# --- Verify ---
Write-Host ""
Write-Host "Verifying installation..." -ForegroundColor Cyan
try {
    $ver = & "$binDir\zcc.exe" +ti83plus 2>&1 | Select-Object -First 3
    Write-Host "  zcc responds:" -ForegroundColor Green
    $ver | ForEach-Object { Write-Host "    $_" }
} catch {
    Write-Host "  zcc test:" -ForegroundColor Yellow
    & "$binDir\zcc.exe" --version 2>&1 | Select-Object -First 1
}

Write-Host ""
Write-Host "=== Installation complete ===" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:"
Write-Host "  cd ti84\native"
Write-Host "  ..\..\..\tibuild.ps1 src\hello.c HELLO"
Write-Host ""
Write-Host "Or restart your terminal to pick up PATH changes."
