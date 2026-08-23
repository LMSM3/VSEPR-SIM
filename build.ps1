#!/usr/bin/env pwsh
<#
.SYNOPSIS
    VSEPR-SIM top-level build wrapper (Ninja).
.DESCRIPTION
    Configures and builds using CMakePresets.json.  Ninja is the sole generator.
    The MSYS2 UCRT64 toolchain (C:/msys64/ucrt64/bin/) is used for all targets.

    Presets: release (default) | debug | test | vis | vview

.EXAMPLE
    .\build.ps1                        # configure + build release preset
    .\build.ps1 -Preset debug          # configure + build debug preset
    .\build.ps1 -Preset test           # test-only build
    .\build.ps1 -Clean                 # wipe build/ then rebuild release
    .\build.ps1 -ConfigureOnly         # cmake --preset <p>, no build step
    .\build.ps1 -Install               # build, then stage an install tree under dist/
    .\build.ps1 -Package               # build, install, and zip the staged bundle
#>

param(
    [string] $Preset        = "release",
    [switch] $Clean,
    [switch] $ConfigureOnly,
    [switch] $Install,
    [switch] $Package,
    [string] $InstallPrefix
)

$ErrorActionPreference = "Stop"
$Root  = $PSScriptRoot
$cmake = "C:\msys64\ucrt64\bin\cmake.exe"
if (-not (Test-Path $cmake)) { $cmake = "cmake" }  # fallback to PATH

# Resolve binary dir from preset name (mirrors CMakePresets.json)
$binaryDirs = @{
    "release" = "build"
    "debug"   = "build_debug"
    "test"    = "build_test_ninja"
    "vis"     = "build_vis"
    "vview"   = "build_vview"
}
$binSubdir = if ($binaryDirs.ContainsKey($Preset)) { $binaryDirs[$Preset] } else { "build" }
$binDir = Join-Path $Root $binSubdir

$cmakeListsPath = Join-Path $Root "CMakeLists.txt"
$versionMatch = Select-String -Path $cmakeListsPath -Pattern 'project\(vsepr-sim VERSION ([0-9.]+)' -AllMatches | Select-Object -First 1
$projectVersion = if ($versionMatch -and $versionMatch.Matches.Count -gt 0) {
    $versionMatch.Matches[0].Groups[1].Value
} else {
    "unknown"
}

if (($Install -or $Package) -and [string]::IsNullOrWhiteSpace($InstallPrefix)) {
    $InstallPrefix = Join-Path $Root ("dist\VSEPR-SIM-{0}-{1}" -f $projectVersion, $Preset)
}

if ($Clean -and (Test-Path $binDir)) {
    Write-Host "Cleaning $binDir ..." -ForegroundColor Yellow
    Remove-Item $binDir -Recurse -Force
}

Write-Host ""
Write-Host "VSEPR-SIM build  [preset: $Preset]" -ForegroundColor Cyan
Write-Host "  Source : $Root"
Write-Host "  Binary : $binDir"
Write-Host "  CMake  : $(&$cmake --version | Select-Object -First 1)"
Write-Host ""

Write-Host "Configuring..." -ForegroundColor Gray
& $cmake --preset $Preset -S $Root
if ($LASTEXITCODE -ne 0) { Write-Error "Configure failed"; exit $LASTEXITCODE }

if (-not $ConfigureOnly) {
    Write-Host "Building..." -ForegroundColor Gray
    & $cmake --build $binDir
    if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit $LASTEXITCODE }
    Write-Host ""
    Write-Host "Build complete." -ForegroundColor Green
}

if (($Install -or $Package) -and -not $ConfigureOnly) {
    if (Test-Path $InstallPrefix) {
        Write-Host "Cleaning install prefix $InstallPrefix ..." -ForegroundColor Yellow
        Remove-Item $InstallPrefix -Recurse -Force
    }

    Write-Host "Installing to $InstallPrefix ..." -ForegroundColor Gray
    & $cmake --install $binDir --prefix $InstallPrefix
    if ($LASTEXITCODE -ne 0) { Write-Error "Install failed"; exit $LASTEXITCODE }

    Write-Host "Install complete." -ForegroundColor Green

    if ($Package) {
        $zipPath = "$InstallPrefix.zip"
        if (Test-Path $zipPath) {
            Remove-Item $zipPath -Force
        }

        Write-Host "Creating package $zipPath ..." -ForegroundColor Gray
        Compress-Archive -Path $InstallPrefix -DestinationPath $zipPath -Force
        Write-Host "Package complete: $zipPath" -ForegroundColor Green
    }
}

