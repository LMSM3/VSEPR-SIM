# installer/qt_deploy.ps1
# ============================================================================
# Runs windeployqt6 to collect Qt runtime DLLs needed by vsepr-desktop.exe
# and vsepr-launcher.exe, then compiles the Inno Setup installer.
#
# Prerequisites:
#   1. cmake --preset release   (build\vsepr-desktop.exe must exist)
#   2. windeployqt6.exe         (on PATH or in Qt bin directory)
#   3. Inno Setup 6             (iscc.exe; https://jrsoftware.org/isinfo.php)
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File installer\qt_deploy.ps1
#   powershell -ExecutionPolicy Bypass -File installer\qt_deploy.ps1 -InnoSetupPath "C:\Program Files (x86)\Inno Setup 6\iscc.exe"
#
# What it does:
#   1. Locates windeployqt6.exe
#   2. Runs it against vsepr-desktop.exe and vsepr-launcher.exe
#      -> output: installer\qt_runtime\  (platform DLLs, plugins, etc.)
#   3. Enables the Qt runtime [Files] line in setup.iss (removes leading ';')
#   4. Locates iscc.exe and compiles the installer
#   5. Reports the output .exe path and size
#
# ============================================================================

param(
	[string]$BuildDir      = "build",
	[string]$QtRuntimeDir  = "installer\qt_runtime",
	[string]$InnoSetupPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Repo = Split-Path -Parent $PSCommandPath

Push-Location $Repo

Write-Host ""
Write-Host "=== VSEPR-SIM Qt Deploy + Installer Build ==="
Write-Host "Repo: $Repo"
Write-Host ""

# ---------------------------------------------------------------------------
# 1. Locate windeployqt6
# ---------------------------------------------------------------------------
$WinDeploy = ""
$wdq_candidates = @(
	"C:\msys64\ucrt64\bin\windeployqt6.exe",
	"C:\Qt\6\msvc2019_64\bin\windeployqt6.exe",
	"C:\Qt\6\mingw_64\bin\windeployqt6.exe"
)
foreach ($c in $wdq_candidates) {
	if ($c -and (Test-Path $c)) { $WinDeploy = $c; break }
}
if (-not $WinDeploy) {
	$found = Get-Command windeployqt6.exe -ErrorAction SilentlyContinue
	if ($found) { $WinDeploy = $found.Source }
}
if (-not $WinDeploy) {
	Write-Error "windeployqt6.exe not found. Add your Qt6 bin directory to PATH."
}
Write-Host "windeployqt6: $WinDeploy"

# ---------------------------------------------------------------------------
# 2. Collect Qt runtime DLLs
# ---------------------------------------------------------------------------
$OutDir = Join-Path $Repo $QtRuntimeDir
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$Exes = @(
	(Join-Path $Repo "$BuildDir\vsepr-desktop.exe"),
	(Join-Path $Repo "$BuildDir\vsepr-launcher.exe")
)
$existing = $Exes | Where-Object { Test-Path $_ }
if ($existing.Count -eq 0) {
	Write-Error "Neither vsepr-desktop.exe nor vsepr-launcher.exe found in $BuildDir. Run cmake --preset release first."
}

Write-Host "Collecting Qt runtime -> $QtRuntimeDir ..."
foreach ($exe in $existing) {
	Write-Host "  Processing: $(Split-Path $exe -Leaf)"
	& $WinDeploy --dir $OutDir --no-translations --no-system-d3d-compiler $exe
	if ($LASTEXITCODE -ne 0) { Write-Error "windeployqt6 failed for $exe" }
}
$fileCount = (Get-ChildItem $OutDir -Recurse -File).Count
Write-Host "Qt runtime collected: $fileCount files"

# ---------------------------------------------------------------------------
# 3. Enable Qt runtime line in setup.iss
# ---------------------------------------------------------------------------
$IssPath = Join-Path $Repo "installer\setup.iss"
$iss = [System.IO.File]::ReadAllText($IssPath, [System.Text.Encoding]::UTF8)
$before = ';Source: "installer\qt_runtime\*"; DestDir: "{app}\bin"; Flags: ignoreversion recursesubdirs createallsubdirs'
$after  =  'Source: "installer\qt_runtime\*"; DestDir: "{app}\bin"; Flags: ignoreversion recursesubdirs createallsubdirs'
if ($iss.Contains($before)) {
	$iss = $iss.Replace($before, $after)
	[System.IO.File]::WriteAllText($IssPath, $iss, [System.Text.Encoding]::UTF8)
	Write-Host "setup.iss: Qt runtime [Files] line enabled."
} else {
	Write-Host "setup.iss: Qt runtime line already enabled or not found -- verify manually."
}

# ---------------------------------------------------------------------------
# 4. Locate iscc.exe
# ---------------------------------------------------------------------------
if (-not $InnoSetupPath) {
	$iscc_candidates = @(
		"C:\Program Files (x86)\Inno Setup 6\iscc.exe",
		"C:\Program Files\Inno Setup 6\iscc.exe"
	)
	foreach ($c in $iscc_candidates) {
		if (Test-Path $c) { $InnoSetupPath = $c; break }
	}
	if (-not $InnoSetupPath) {
		$found2 = Get-Command iscc.exe -ErrorAction SilentlyContinue
		if ($found2) { $InnoSetupPath = $found2.Source }
	}
}
if (-not $InnoSetupPath) {
	Write-Host ""
	Write-Warning "iscc.exe not found. Qt DLLs are staged but installer was not compiled."
	Write-Host "Install Inno Setup 6: https://jrsoftware.org/isinfo.php"
	Write-Host "Then run:  iscc installer\setup.iss"
	Pop-Location
	exit 0
}
Write-Host "iscc: $InnoSetupPath"

# ---------------------------------------------------------------------------
# 5. Compile installer
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "Compiling installer ..."
New-Item -ItemType Directory -Force -Path (Join-Path $Repo "installer\output") | Out-Null
& $InnoSetupPath (Join-Path $Repo "installer\setup.iss")
if ($LASTEXITCODE -ne 0) { Write-Error "iscc compilation failed" }

$output = Get-ChildItem (Join-Path $Repo "installer\output") -Filter "*.exe" |
		  Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($output) {
	$sizeMB = [math]::Round($output.Length / 1MB, 1)
	Write-Host ""
	Write-Host "=== Installer ready ==="
	Write-Host "  Path: $($output.FullName)"
	Write-Host "  Size: $sizeMB MB"
} else {
	Write-Warning "Installer .exe not found in installer\output\ -- check iscc output above."
}

Pop-Location
