# run_cli_tests.ps1
# VSEPR-Sim CLI smoke/regression automation (PowerShell)
# Runs representative user-path commands, validates exit codes and artifacts
#
# Usage:
#   .\scripts\run_cli_tests.ps1
#   $env:VSEPR_BIN=".\build\bin\vsepr.exe"; .\scripts\run_cli_tests.ps1
#
# Exit codes:
#   0 = all tests passed
#   1 = at least one test failed

param(
    [switch]$SkipViz,
    [switch]$Verbose
)

$ErrorActionPreference = "Continue"

# ============================================================================
# Configuration
# ============================================================================

$ProjectRoot = Split-Path -Parent $PSScriptRoot

# Respect user override; otherwise auto-detect
$VseprBin = $env:VSEPR_BIN
if (-not $VseprBin) {
    if (Test-Path "$ProjectRoot\build\bin\vsepr.exe") {
        $VseprBin = "$ProjectRoot\build\bin\vsepr.exe"
    } elseif (Test-Path "$ProjectRoot\build\bin\molecule_builder.exe") {
        $VseprBin = "$ProjectRoot\build\bin\molecule_builder.exe"
    } else {
        Write-Host "✗ Could not find VSEPR binary" -ForegroundColor Red
        Write-Host "  Tried:" -ForegroundColor Yellow
        Write-Host "    $ProjectRoot\build\bin\vsepr.exe" -ForegroundColor Yellow
        Write-Host "    $ProjectRoot\build\bin\molecule_builder.exe" -ForegroundColor Yellow
        Write-Host "  Set `$env:VSEPR_BIN or run 'cmake --build build' first" -ForegroundColor Yellow
        exit 1
    }
}

# Test output directories
$LogDir = "$ProjectRoot\logs\test_runs"
$Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$RunDir = "$LogDir\$Timestamp"
$OutDir = "$RunDir\artifacts"
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

$StdoutLog = "$RunDir\stdout.log"
$StderrLog = "$RunDir\stderr.log"
$SummaryLog = "$RunDir\summary.log"

Set-Content -Path $StdoutLog -Value ""
Set-Content -Path $StderrLog -Value ""
Set-Content -Path $SummaryLog -Value ""

# ============================================================================
# Helper Functions
# ============================================================================

$script:PassCount = 0
$script:FailCount = 0

function Say {
    param([string]$Message)
    $Message | Tee-Object -FilePath $SummaryLog -Append | Write-Host
}

function Run-Cmd {
    param(
        [string]$Name,
        [int]$ExpectedRc,
        [string[]]$Args
    )
    
    $SafeName = $Name -replace '[/ ]', '__'
    $OutFile = "$RunDir\$SafeName.out"
    $ErrFile = "$RunDir\$SafeName.err"
    
    Say "• $Name : $VseprBin $($Args -join ' ')"
    
    $output = & $VseprBin @Args 2>&1
    $rc = $LASTEXITCODE
    
    $output | Out-File -FilePath $OutFile -Encoding UTF8
    $output | Add-Content -Path $StdoutLog
    
    if ($rc -ne $ExpectedRc) {
        Say "  ✗ FAIL: $Name (rc=$rc, expected=$ExpectedRc)"
        Say "    stdout: $OutFile"
        $script:FailCount++
        return $false
    }
    
    Say "  ✓ OK (rc=$rc)"
    $script:PassCount++
    return $true
}

function Assert-FileExistsNonempty {
    param([string]$Path)
    
    if (-not (Test-Path $Path) -or (Get-Item $Path).Length -eq 0) {
        Say "  ✗ FAIL: expected non-empty file: $Path"
        $script:FailCount++
        return $false
    }
    
    Say "  ✓ file exists: $Path"
    $script:PassCount++
    return $true
}

function Assert-XyzHeaderSane {
    param([string]$Path)
    
    if (-not (Test-Path $Path)) {
        Say "  ✗ FAIL: missing xyz: $Path"
        $script:FailCount++
        return $false
    }
    
    $lines = Get-Content $Path -TotalCount 2
    $line1 = $lines[0].Trim()
    $line2 = $lines[1].Trim()
    
    if ($line1 -notmatch '^\d+$') {
        Say "  ✗ FAIL: xyz line1 not an integer atom count: '$line1' ($Path)"
        $script:FailCount++
        return $false
    }
    
    if ([string]::IsNullOrWhiteSpace($line2)) {
        Say "  ✗ FAIL: xyz line2 comment missing/empty ($Path)"
        $script:FailCount++
        return $false
    }
    
    Say "  ✓ xyz header sane: $Path (atoms=$line1)"
    $script:PassCount++
    return $true
}

function Assert-Contains {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Label
    )
    
    $content = Get-Content $Path -Raw -ErrorAction SilentlyContinue
    if (-not $content -or $content -notmatch $Pattern) {
        Say "  ✗ FAIL: missing '$Label' (/$Pattern/) in $Path"
        $script:FailCount++
        return $false
    }
    
    Say "  ✓ contains: $Label"
    $script:PassCount++
    return $true
}

# ============================================================================
# Preflight
# ============================================================================

Say "=== VSEPR-Sim CLI Test Automation ==="
Say "Binary:    $VseprBin"
Say "Run dir:   $RunDir"
Say "Artifacts: $OutDir"
Say "Date:      $(Get-Date)"
Say ""

# ============================================================================
# Test Suite: Help & Version
# ============================================================================

Say "=== Test Suite: Help & Version ==="

Run-Cmd "help --help" 0 @("--help") | Out-Null
Run-Cmd "help -h" 0 @("-h") | Out-Null

# Version paths
$verOk = $false
if (Run-Cmd "version --version" 0 @("--version")) { $verOk = $true }
if (Run-Cmd "version -v" 0 @("-v")) { $verOk = $true }
if (Run-Cmd "version subcmd" 0 @("version")) { $verOk = $true }

if (-not $verOk) {
    Say "  ⚠ WARNING: No version command worked"
    Say "  This is acceptable if not yet implemented"
} else {
    Say "  ✓ At least one version command works"
    $script:PassCount++
}

Say ""

# ============================================================================
# Test Suite: Basic Build
# ============================================================================

Say "=== Test Suite: Basic Build ==="

Run-Cmd "build H2O" 0 @("H2O") | Out-Null
Run-Cmd "build CH4" 0 @("CH4") | Out-Null
Run-Cmd "build NH3" 0 @("NH3") | Out-Null
Run-Cmd "build CO2" 0 @("CO2") | Out-Null

Say ""

# ============================================================================
# Test Suite: Optimization
# ============================================================================

Say "=== Test Suite: Optimization ==="

Run-Cmd "H2O --optimize" 0 @("H2O", "--optimize") | Out-Null
Run-Cmd "CH4 --no-opt" 0 @("CH4", "--no-opt") | Out-Null
Run-Cmd "NH3 --opt" 0 @("NH3", "--opt") | Out-Null

Say ""

# ============================================================================
# Test Suite: Output Files
# ============================================================================

Say "=== Test Suite: Output Files (XYZ) ==="

$H2O_XYZ = "$OutDir\water_opt.xyz"
$CH4_XYZ = "$OutDir\methane_opt.xyz"
$NH3_XYZ = "$OutDir\ammonia_opt.xyz"

Run-Cmd "H2O --xyz output" 0 @("H2O", "--xyz", $H2O_XYZ) | Out-Null
Assert-FileExistsNonempty $H2O_XYZ | Out-Null
Assert-XyzHeaderSane $H2O_XYZ | Out-Null

Run-Cmd "CH4 --xyz output" 0 @("CH4", "--xyz", $CH4_XYZ) | Out-Null
Assert-FileExistsNonempty $CH4_XYZ | Out-Null
Assert-XyzHeaderSane $CH4_XYZ | Out-Null

Run-Cmd "NH3 --xyz output" 0 @("NH3", "--xyz", $NH3_XYZ, "--no-opt") | Out-Null
Assert-FileExistsNonempty $NH3_XYZ | Out-Null
Assert-XyzHeaderSane $NH3_XYZ | Out-Null

Say ""

# ============================================================================
# Test Suite: Visualization
# ============================================================================

Say "=== Test Suite: Visualization ==="

if ($SkipViz) {
    Say "  • Viz tests skipped (-SkipViz)"
} else {
    $vizOk = $false
    
    Push-Location $OutDir
    
    $output = & $VseprBin "H2O" "--xyz" "H2O_test.xyz" 2>&1
    $rc = $LASTEXITCODE
    
    if ($rc -eq 0) {
        $vfile = Get-ChildItem -Filter "*_viewer.html" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($vfile) {
            Say "  ✓ Viz artifact: $OutDir\$($vfile.Name)"
            $script:PassCount++
            $vizOk = $true
        } else {
            Say "  ⚠ XYZ created but no *_viewer.html found"
        }
    }
    
    Pop-Location
    
    if (-not $vizOk) {
        Say "  • No validated HTML output (may require explicit --viz flag)"
    }
}

Say ""

# ============================================================================
# Test Suite: Complex Molecules
# ============================================================================

Say "=== Test Suite: Complex Molecules ==="

Run-Cmd "C6H12O6 (glucose)" 0 @("C6H12O6", "--xyz", "$OutDir\glucose.xyz") | Out-Null
Run-Cmd "C10H22 (decane)" 0 @("C10H22", "--xyz", "$OutDir\decane.xyz") | Out-Null
Run-Cmd "H2SO4 (sulfuric)" 0 @("H2SO4", "--xyz", "$OutDir\h2so4.xyz") | Out-Null
Run-Cmd "CaCO3 (calcium)" 0 @("CaCO3", "--xyz", "$OutDir\caco3.xyz") | Out-Null

Say ""

# ============================================================================
# Test Suite: Error Handling
# ============================================================================

Say "=== Test Suite: Error Handling ==="

Run-Cmd "invalid formula" 1 @("XYZZY") | Out-Null
Run-Cmd "unknown element" 1 @("Zz99") | Out-Null

Say ""

# ============================================================================
# Test Suite: Formula Parser
# ============================================================================

Say "=== Test Suite: Formula Parser ==="

if (Test-Path "$ProjectRoot\build\bin\test_formula_parser.exe") {
    Say "• Running formula parser tests..."
    
    $output = & "$ProjectRoot\build\bin\test_formula_parser.exe" 2>&1
    $rc = $LASTEXITCODE
    
    $output | Out-File "$RunDir\formula_parser.out"
    
    if ($rc -eq 0) {
        Say "  ✓ Formula parser tests PASSED"
        $script:PassCount++
    } else {
        Say "  ✗ Formula parser tests FAILED (rc=$rc)"
        Say "    Output: $RunDir\formula_parser.out"
        $script:FailCount++
    }
} else {
    Say "  • Formula parser tests not built (skipping)"
}

Say ""

# ============================================================================
# Test Suite: Batch Processing
# ============================================================================

Say "=== Test Suite: Batch Processing ==="

if (Test-Path "$ProjectRoot\build\bin\vsepr_batch.exe") {
    $BatchOut = "$OutDir\batch_test"
    New-Item -ItemType Directory -Path $BatchOut -Force | Out-Null
    
    Run-Cmd "vsepr_batch simple" 0 @("H2O, CO2 -per{50,50}", "--out", $BatchOut, "--dry-run") | Out-Null
    
    Say "  • Batch runner available"
} else {
    Say "  • Batch runner not built (skipping)"
}

Say ""

# ============================================================================
# Validation: Output Content
# ============================================================================

Say "=== Content Validation ==="

if (Test-Path $H2O_XYZ) {
    Assert-Contains $H2O_XYZ "^3" "atom count = 3" | Out-Null
    Assert-Contains $H2O_XYZ "[OH]" "contains O or H atoms" | Out-Null
}

if (Test-Path $StdoutLog) {
    Assert-Contains $StdoutLog "H2O|molecule|build" "mentions molecules" | Out-Null
}

Say ""

# ============================================================================
# Summary
# ============================================================================

Say "=== Test Summary ==="
Say "Passed: $script:PassCount"
Say "Failed: $script:FailCount"
Say "Total:  $($script:PassCount + $script:FailCount)"
Say ""
Say "Logs:"
Say "  stdout:  $StdoutLog"
Say "  stderr:  $StderrLog"
Say "  summary: $SummaryLog"
Say ""

if ($script:FailCount -ne 0) {
    Say "✗ Some tests failed"
    Say "  Review logs in $RunDir"
    exit 1
}

Say "✓ All tests passed"
Say "  Artifacts saved to $OutDir"
exit 0
