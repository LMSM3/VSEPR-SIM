# VSEPR-Sim v2.3.1 - Windows Build & Package Script
# PowerShell version

$VERSION = "2.3.1"
$BUILD_DATE = Get-Date -Format "yyyyMMdd"
$PKG_NAME = "vsepr-sim-$VERSION"
$PKG_DIR = "dist\$PKG_NAME"

Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  VSEPR-Sim v2.3.1 - Windows Build & Package                  ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Check WSL
Write-Host "[STEP 1/8] Checking WSL environment..." -ForegroundColor Cyan
$wslCheck = wsl bash -c "echo OK"
if ($wslCheck -ne "OK") {
    Write-Host "[ERROR] WSL not available" -ForegroundColor Red
    exit 1
}
Write-Host "[SUCCESS] WSL available" -ForegroundColor Green
Write-Host ""

# Clean
Write-Host "[STEP 2/8] Cleaning build environment..." -ForegroundColor Cyan
if (Test-Path build) { Remove-Item -Recurse -Force build }
if (Test-Path dist) { Remove-Item -Recurse -Force dist }
if (Test-Path .venv) { Remove-Item -Recurse -Force .venv }
Write-Host "[SUCCESS] Clean complete" -ForegroundColor Green
Write-Host ""

# Build in WSL
Write-Host "[STEP 3/8] Building in WSL..." -ForegroundColor Cyan
wsl bash -c "cd /mnt/c/Users/Liam/Desktop/vsepr-sim && ./scripts/build_and_package.sh"

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║  ✅ Windows Build Complete!                                   ║" -ForegroundColor Green
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""
Write-Host "Package location: dist\" -ForegroundColor Cyan
Get-ChildItem -Path dist\*.tar.gz,dist\*.zip -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host "  $($_.Name) - $([math]::Round($_.Length/1MB, 2)) MB" -ForegroundColor White
}
Write-Host ""
