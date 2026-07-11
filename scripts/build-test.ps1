# build-test.ps1
# Build and run a single C++ test target from a VSEPR-SIM repo.
#
# Usage:
#   .\scripts\build-test.ps1 [-Cmd run] [-Target test_wind_tui]
#   .\scripts\build-test.ps1 rebuild test_flower_health
#   .\scripts\build-test.ps1 build test_wind_tui
#
# VSEPR-SIM 3.0.1

param(
    [ValidateSet("build","run","clean","rebuild","help")]
    [string]$Cmd = "run",
    [string]$Target = "test_wind_tui",
    [string]$BuildDir = "build",
    [string]$Config = "Release"
)

$ErrorActionPreference = "Continue"
$ProjectRoot = (Resolve-Path "$PSScriptRoot\..").Path

# -- helpers ----------------------------------------------------------

function Find-Exe {
    $candidates = @(
        "$ProjectRoot\$BuildDir\tests\$Target.exe",
        "$ProjectRoot\$BuildDir\bin\$Target.exe"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { return $c }
    }
    return $null
}

function Get-Jobs {
    $env:NUMBER_OF_PROCESSORS
    if (-not $env:NUMBER_OF_PROCESSORS) { return 4 }
}

# -- actions ----------------------------------------------------------

function Do-Build {
    $src = "$ProjectRoot\tests\$Target.cpp"
    if (-not (Test-Path $src)) {
        Write-Host "Error: source not found: $src" -ForegroundColor Red
        exit 1
    }

    Write-Host "=== CMake Build: $Target ===" -ForegroundColor Cyan
    Write-Host "  . Project  : $ProjectRoot"
    Write-Host "  . Config   : $Config"
    Write-Host "  . Target   : $Target"

    cmake -S "$ProjectRoot" -B "$ProjectRoot\$BuildDir" `
        -DCMAKE_BUILD_TYPE="$Config" `
        -DBUILD_TESTS=ON `
        -DBUILD_APPS=OFF `
        -DBUILD_VIS=OFF `
        2>&1 | Out-Null

    $buildOutput = cmake --build "$ProjectRoot\$BuildDir" --target "$Target" -j (Get-Jobs) 2>&1
    $buildOutput | ForEach-Object { Write-Host $_ }

    if ($LASTEXITCODE -ne 0) {
        Write-Host "  x Build failed" -ForegroundColor Red
        exit 1
    }
    Write-Host "  + Build complete" -ForegroundColor Green
}

function Do-Run {
    $exe = Find-Exe
    if (-not $exe) {
        Write-Host "  . Executable missing, building first..." -ForegroundColor Yellow
        Do-Build
        $exe = Find-Exe
        if (-not $exe) {
            Write-Host "Error: build succeeded but executable not found." -ForegroundColor Red
            exit 1
        }
    }

    Write-Host ""
    Write-Host "=== Running: $Target ===" -ForegroundColor Cyan
    & $exe
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  x Test failed (exit $LASTEXITCODE)" -ForegroundColor Red
        exit $LASTEXITCODE
    }
}

function Do-Clean {
    $exe = Find-Exe
    if ($exe) {
        Remove-Item -Force $exe
        Write-Host "  + Removed $exe" -ForegroundColor Green
    } else {
        Write-Host "  . Nothing to clean"
    }
}

# -- main -------------------------------------------------------------

switch ($Cmd) {
    "build"   { Do-Build }
    "run"     { Do-Run }
    "clean"   { Do-Clean }
    "rebuild" {
        Do-Clean
        Do-Build
        Do-Run
    }
    "help" {
        Write-Host "=== VSEPR-SIM Single-Target Test Builder ===" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "Usage:"
        Write-Host "  .\scripts\build-test.ps1 [build|run|clean|rebuild] [TARGET]"
        Write-Host ""
        Write-Host "Arguments:"
        Write-Host "  build     Compile the target"
        Write-Host "  run       Compile (if needed) then execute"
        Write-Host "  clean     Remove the target binary"
        Write-Host "  rebuild   clean + build + run"
        Write-Host "  TARGET    CMake test target name (default: test_wind_tui)"
        Write-Host ""
        Write-Host "Examples:"
        Write-Host "  .\scripts\build-test.ps1 run test_wind_tui"
        Write-Host "  .\scripts\build-test.ps1 rebuild test_flower_health"
    }
}
