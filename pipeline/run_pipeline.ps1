#!/usr/bin/env pwsh
<#
.SYNOPSIS
    VSEPR-SIM Web Pipeline Launcher (Windows / PowerShell)

.DESCRIPTION
    Runs the full test + post-process + host pipeline on Windows without WSL.
    Mirrors pipeline/run_pipeline.sh but uses PowerShell + native tools.

    Stages:
      1  build        cmake configure + build
      2  ctest        run CTest suites, write results JSON
      3  pytest       run pykernel tests, write results JSON
      4  figures      regenerate PNG figures
      5  postprocess  merge results → HTML dashboard
      6  host         start FastAPI host at http://localhost:8765

.EXAMPLE
    .\pipeline\run_pipeline.ps1
    .\pipeline\run_pipeline.ps1 -Stages 2,3,5,6
    .\pipeline\run_pipeline.ps1 -Stages 6         # host only
    .\pipeline\run_pipeline.ps1 -Port 9000
    .\pipeline\run_pipeline.ps1 -NoBuild -Watch

.PARAMETER Stages
    Comma-separated stage numbers to execute (default: 1,2,3,4,5,6)

.PARAMETER Port
    Port for the web host (default: 8765)

.PARAMETER Jobs
    Parallel cmake jobs (default: $env:NUMBER_OF_PROCESSORS)s

.PARAMETER NoBuild
    Skip stage 1 (cmake build)

.PARAMETER Watch
    Start uvicorn in --reload mode (live reload on file changes)

.PARAMETER BuildType
    CMake build type (default: Release)
#>

param(
    [string]   $Stages    = "1,2,3,4,5,7,6",
    [int]      $Port      = 8765,
    [int]      $Jobs      = [int]$env:NUMBER_OF_PROCESSORS,
    [switch]   $NoBuild,
    [switch]   $Watch,
    [string]   $BuildType = "Release"
)

$ErrorActionPreference = "Continue"

# ── Paths ─────────────────────────────────────────────────────────────────────
$Root       = (Resolve-Path "$PSScriptRoot\..").Path
$BuildDir   = "$Root\build"
$OutDir     = "$Root\out\pipeline"
$ResultsDir = "$OutDir\results"
$ReportsDir = "$OutDir\reports"
$FigSrc     = "$Root\docs\figures"
$FigDst     = "$ReportsDir\figures"
$LogFile    = "$OutDir\pipeline.log"

$PythonExe  = "$Root\.venv\Scripts\python.exe"
if (-not (Test-Path $PythonExe)) { $PythonExe = "python" }

# ── Active stages set ─────────────────────────────────────────────────────────
$ActiveStages = @{}
if ($NoBuild) { $Stages = ($Stages -split ',' | Where-Object { $_ -ne '1' }) -join ',' }
foreach ($s in ($Stages -split ',')) { $ActiveStages[$s.Trim()] = $true }

function Is-Active([string]$s) { return $ActiveStages.ContainsKey($s) }

# ── Helpers ───────────────────────────────────────────────────────────────────
function Write-Section([int]$n, [string]$title) {
    Write-Host ""
    Write-Host "══════════════════════════════════════════" -ForegroundColor Blue
    Write-Host "  Stage ${n}: ${title}" -ForegroundColor Blue
    Write-Host "══════════════════════════════════════════" -ForegroundColor Blue
}
function Write-Ok([string]$msg)   { Write-Host "  ✔  $msg" -ForegroundColor Green }
function Write-Fail([string]$msg) { Write-Host "  ✘  $msg" -ForegroundColor Red }
function Write-Info([string]$msg) { Write-Host "  →  $msg" -ForegroundColor Cyan }

# Init output dirs
New-Item -ItemType Directory -Force $ResultsDir, $ReportsDir, $FigDst | Out-Null
"" | Set-Content $LogFile

$StageResults = [System.Collections.Generic.List[string]]::new()
$PipelineStart = [DateTimeOffset]::UtcNow

function Record-Stage([string]$id,[string]$name,[string]$status,[string]$elapsed) {
    $StageResults.Add("${id}:${name}:${status}:${elapsed}")
}

Write-Host "VSEPR-SIM Pipeline  stages=$Stages  port=$Port" -ForegroundColor White
Write-Host (Get-Date -Format "yyyy-MM-dd HH:mm:ss")

# =============================================================================
# STAGE 1 — CMake Build
# =============================================================================
if (Is-Active "1") {
    Write-Section 1 "CMake Build"
    $T0 = [DateTimeOffset]::UtcNow
    Write-Info "Root: $Root"
    Write-Info "Build: $BuildDir  Config: $BuildType  Jobs: $Jobs"

    cmake -S "$Root" -B "$BuildDir" `
        -DCMAKE_BUILD_TYPE="$BuildType" `
        -DBUILD_TESTS=ON -DBUILD_APPS=ON -DBUILD_VIS=OFF 2>&1 |
        Tee-Object -FilePath $LogFile -Append | Out-Null

    cmake --build "$BuildDir" --config "$BuildType" -j $Jobs 2>&1 |
        Tee-Object -FilePath $LogFile -Append | Select-Object -Last 5 |
        ForEach-Object { Write-Host "    $_" }

    $Elapsed = [int]([DateTimeOffset]::UtcNow - $T0).TotalSeconds
    Write-Ok "Build completed in ${Elapsed}s"
    Record-Stage 1 "cmake_build" "pass" "${Elapsed}s"
}

# =============================================================================
# STAGE 2 — CTest
# =============================================================================
if (Is-Active "2") {
    Write-Section 2 "CTest Suites"
    $T0 = [DateTimeOffset]::UtcNow

    $CtestOut = ctest --test-dir "$BuildDir" `
        --build-config "$BuildType" `
        --output-on-failure `
        -T Test 2>&1

    $CtestOut | Tee-Object -FilePath $LogFile -Append | Select-Object -Last 20 |
        ForEach-Object { Write-Host "    $_" }

    $CtestExit = $LASTEXITCODE
    $CtestStatus = if ($CtestExit -eq 0) { "pass" } else { "fail" }

    # Parse counts from output
    $SummaryLine = $CtestOut | Select-String "\d+ tests? passed" | Select-Object -Last 1
    $CtestPass = if ($SummaryLine) { [regex]::Match($SummaryLine, "(\d+) tests? passed").Groups[1].Value } else { "0" }
    $CtestFail = if ($SummaryLine) { [regex]::Match($SummaryLine, "(\d+) tests? failed").Groups[1].Value } else { "0" }
    $CtestTotal = [int]$CtestPass + [int]$CtestFail

    @{
        suite    = "ctest"
        timestamp = (Get-Date -Format "o")
        total    = $CtestTotal
        passed   = [int]$CtestPass
        failed   = [int]$CtestFail
        exit_code = $CtestExit
        status   = $CtestStatus
        build_dir = $BuildDir
    } | ConvertTo-Json | Set-Content "$ResultsDir\ctest_results.json"

    $Elapsed = [int]([DateTimeOffset]::UtcNow - $T0).TotalSeconds
    if ($CtestExit -eq 0) {
        Write-Ok "CTest: ${CtestPass}/${CtestTotal} passed in ${Elapsed}s"
        Record-Stage 2 "ctest" "pass" "${Elapsed}s"
    } else {
        Write-Fail "CTest: ${CtestFail} failed (${CtestPass}/${CtestTotal}) in ${Elapsed}s"
        Record-Stage 2 "ctest" "fail" "${Elapsed}s"
    }
}

# =============================================================================
# STAGE 3 — pytest
# =============================================================================
if (Is-Active "3") {
    Write-Section 3 "pytest (pykernel)"
    $T0 = [DateTimeOffset]::UtcNow
    $PytestJson = "$ResultsDir\pytest_results.json"

    $PytestOut = & $PythonExe -m pytest pykernel/tests/ --tb=short -q 2>&1
    $PytestExit = $LASTEXITCODE
    $PytestOut | Tee-Object -FilePath $LogFile -Append | Select-Object -Last 10 |
        ForEach-Object { Write-Host "    $_" }

    $PyPass  = if ($PytestOut -match "(\d+) passed") { [int]$Matches[1] } else { 0 }
    $PyFail  = if ($PytestOut -match "(\d+) failed") { [int]$Matches[1] } else { 0 }
    $PyTotal = $PyPass + $PyFail
    $PyStatus = if ($PytestExit -eq 0) { "pass" } else { "fail" }

    @{
        suite     = "pytest"
        timestamp = (Get-Date -Format "o")
        total     = $PyTotal
        passed    = $PyPass
        failed    = $PyFail
        exit_code = $PytestExit
        status    = $PyStatus
    } | ConvertTo-Json | Set-Content $PytestJson

    $Elapsed = [int]([DateTimeOffset]::UtcNow - $T0).TotalSeconds
    if ($PytestExit -eq 0) {
        Write-Ok "pytest: ${PyPass}/${PyTotal} passed in ${Elapsed}s"
        Record-Stage 3 "pytest" "pass" "${Elapsed}s"
    } else {
        Write-Fail "pytest: ${PyFail} failed (${PyPass}/${PyTotal}) in ${Elapsed}s"
        Record-Stage 3 "pytest" "fail" "${Elapsed}s"
    }
}

# =============================================================================
# STAGE 4 — Figures
# =============================================================================
if (Is-Active "4") {
    Write-Section 4 "Figure Generation"
    $T0 = [DateTimeOffset]::UtcNow
    Write-Info "Overview figures..."
    & $PythonExe scripts/render_overview_figures.py 2>&1 |
        Tee-Object -FilePath $LogFile -Append |
        ForEach-Object { Write-Host "    $_" }

    # Copy figures
    if (Test-Path $FigSrc) {
        Copy-Item "$FigSrc\*.png" $FigDst -Force -ErrorAction SilentlyContinue
        $FigCount = (Get-ChildItem "$FigDst\*.png" -ErrorAction SilentlyContinue).Count
        $Elapsed = [int]([DateTimeOffset]::UtcNow - $T0).TotalSeconds
        Write-Ok "${FigCount} figures ready in ${Elapsed}s"
        Record-Stage 4 "figures" "pass" "${Elapsed}s"
    } else {
        Write-Info "No docs/figures directory found — skipping copy"
        Record-Stage 4 "figures" "pass" "0s"
    }
}

# =============================================================================
# STAGE 5 — Post-process
# =============================================================================
if (Is-Active "5") {
    Write-Section 5 "Post-processing"
    $T0 = [DateTimeOffset]::UtcNow

    & $PythonExe pipeline/postprocess.py `
        --results-dir "$ResultsDir" `
        --reports-dir "$ReportsDir" `
        --fig-dir     "$FigDst" `
        --root        "$Root" 2>&1 |
        Tee-Object -FilePath $LogFile -Append |
        ForEach-Object { Write-Host "    $_" }

    $Elapsed = [int]([DateTimeOffset]::UtcNow - $T0).TotalSeconds
    Write-Ok "Post-process complete in ${Elapsed}s"
    Record-Stage 5 "postprocess" "pass" "${Elapsed}s"
}

# =============================================================================
# Pipeline summary JSON
# =============================================================================
$TotalElapsed = [int]([DateTimeOffset]::UtcNow - $PipelineStart).TotalSeconds
$StageJsonArr = $StageResults | ForEach-Object {
    $parts = $_ -split ':'
    [ordered]@{ id=$parts[0]; name=$parts[1]; status=$parts[2]; elapsed=$parts[3] }
}

@{
    pipeline       = "vsepr-sim"
    version        = "3.0.0"
    timestamp      = (Get-Date -Format "o")
    total_elapsed_s = $TotalElapsed
    stages         = @($StageJsonArr)
} | ConvertTo-Json -Depth 5 | Set-Content "$ResultsDir\pipeline_summary.json"

Write-Host ""
Write-Host "Pipeline summary (${TotalElapsed}s):" -ForegroundColor White
foreach ($entry in $StageResults) {
    $parts = $entry -split ':'
    if ($parts[2] -eq "pass") {
        Write-Ok "Stage $($parts[0]) $($parts[1])  ($($parts[3]))"
    } else {
        Write-Fail "Stage $($parts[0]) $($parts[1])  ($($parts[3]))"
    }
}

# =============================================================================
# STAGE 7 — Chemistry Subsystem Validation
# =============================================================================
if (Is-Active "7") {
    Write-Section 7 "Chemistry Subsystem"
    $T0 = [DateTimeOffset]::UtcNow
    Write-Info "Validating chem_shell controller..."

    $ChemCheck = & $PythonExe -c @"
import sys; sys.path.insert(0, '$($Root -replace '\\','/')'); from chem.chem_shell.chem_bridge import validate; r = validate(); print('OK' if r['ok'] else 'FAIL: ' + r.get('error','')); sys.exit(0 if r['ok'] else 1)
"@ 2>&1

    $ChemExit = $LASTEXITCODE
    $Elapsed = [int]([DateTimeOffset]::UtcNow - $T0).TotalSeconds

    if ($ChemExit -eq 0) {
        Write-Ok "chem_shell validated in ${Elapsed}s"
        Record-Stage 7 "chem_shell" "pass" "${Elapsed}s"
    } else {
        Write-Fail "chem_shell validation failed: $ChemCheck"
        Record-Stage 7 "chem_shell" "fail" "${Elapsed}s"
    }
}

# =============================================================================
# STAGE 6 — Host
# =============================================================================
if (Is-Active "6") {
    Write-Section 6 "Web Host"
    Write-Info "Starting server at http://localhost:${Port}"
    Write-Info "Press Ctrl+C to stop"
    Write-Host ""

    if ($Watch) {
        & $PythonExe -m uvicorn pipeline.host:app `
            --host 0.0.0.0 --port $Port `
            --reload --reload-dir "$OutDir" `
            --reload-dir "$Root\docs\figures"
    } else {
        & $PythonExe -m uvicorn pipeline.host:app `
            --host 0.0.0.0 --port $Port
    }
}
