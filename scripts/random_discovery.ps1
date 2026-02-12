#!/usr/bin/env pwsh
<#
.SYNOPSIS
    VSEPR-Sim Random Molecule Discovery - Fuzzing Test
.DESCRIPTION
    Automated molecule generation and visualization with random formulas
    Tests formula parser robustness with 10,000 iterations
.PARAMETER Iterations
    Number of random formulas to test (default: 10000)
.PARAMETER Quick
    Run quick test with 100 iterations
#>

param(
    [int]$Iterations = 10000,
    [switch]$Quick
)

if ($Quick) {
    $Iterations = 100
}

$ErrorActionPreference = "Continue"
$ProjectRoot = $PSScriptRoot | Split-Path
$SessionName = "random_discovery_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
$SessionDir = Join-Path $ProjectRoot "outputs\sessions\$SessionName"
$VseprBin = Join-Path $ProjectRoot "build\bin\vsepr.exe"

# Statistics
$Script:Total = 0
$Script:Success = 0
$Script:Failed = 0
$Script:Rejected = 0
$Script:Timeout = 0

# ============================================================================
# Random Generation Functions
# ============================================================================

$Elements = @('H', 'C', 'N', 'O', 'F', 'P', 'S', 'Cl', 'Br', 'I', 'Si', 'B', 'Al', 'Fe', 'Cu', 'Zn', 'Ag', 'Au', 'Li', 'Na', 'K', 'Ca', 'Mg')
$Ions = @('+', '-', '2+', '2-', '3+', '3-')

function Get-RandomElement {
    $Elements | Get-Random
}

function Get-RandomCount {
    Get-Random -Minimum 1 -Maximum 9
}

function Get-RandomIon {
    if ((Get-Random -Minimum 0 -Maximum 3) -eq 0) {
        $Ions | Get-Random
    } else {
        ""
    }
}

function New-SimpleFormula {
    $elem1 = Get-RandomElement
    $count1 = Get-RandomCount
    $elem2 = Get-RandomElement
    $count2 = Get-RandomCount
    
    "$elem1$count1$elem2$count2"
}

function New-ComplexFormula {
    $elem1 = Get-RandomElement
    $count1 = Get-RandomCount
    $elem2 = Get-RandomElement
    $elem3 = Get-RandomElement
    $count2 = Get-RandomCount
    
    "$elem1$count1($elem2$elem3)$count2"
}

function New-IonicFormula {
    $elem = Get-RandomElement
    $count = Get-RandomCount
    $ion = Get-RandomIon
    
    "$elem$count$ion"
}

function New-GarbageFormula {
    $len = Get-Random -Minimum 5 -Maximum 25
    $result = ""
    
    for ($i = 0; $i -lt $len; $i++) {
        $choice = Get-Random -Minimum 0 -Maximum 6
        switch ($choice) {
            0 { $result += Get-RandomElement }
            1 { $result += (Get-Random -Minimum 0 -Maximum 10) }
            2 { $result += "(" }
            3 { $result += ")" }
            4 { $result += "+" }
            5 { $result += "-" }
        }
    }
    
    $result
}

function New-EdgeCase {
    $cases = @(
        "",
        "X",
        "123",
        "H1000000",
        "((((H))))",
        "H2O3N4C5S6",
        "+++---",
        "H2O H2O",
        "水",
        "H2O`nNH3"
    )
    
    $cases | Get-Random
}

function New-RandomFormula {
    $pattern = Get-Random -Minimum 0 -Maximum 10
    
    switch ($pattern) {
        {$_ -in 0,1,2} { New-SimpleFormula }
        {$_ -in 3,4} { New-ComplexFormula }
        {$_ -in 5,6} { New-IonicFormula }
        {$_ -in 7,8} { New-GarbageFormula }
        9 { New-EdgeCase }
    }
}

# ============================================================================
# Testing Functions
# ============================================================================

function Test-Formula {
    param(
        [string]$Formula,
        [int]$Index
    )
    
    # Safe filename
    $safeFormula = $Formula -replace '[^\w\+\-\(\)]', '' 
    if ([string]::IsNullOrWhiteSpace($safeFormula)) {
        $safeFormula = "empty_$Index"
    }
    $safeFormula = $safeFormula.Substring(0, [Math]::Min(50, $safeFormula.Length))
    
    $outputBase = Join-Path $SessionDir "molecules\${safeFormula}_${Index}"
    $xyzFile = "$outputBase.xyz"
    $htmlFile = "$outputBase.html"
    $logFile = "$outputBase.log"
    
    # Try to build
    $startTime = Get-Date
    
    try {
        $process = Start-Process -FilePath $VseprBin `
            -ArgumentList "build", "`"$Formula`"", "--optimize", "--output", "`"$xyzFile`"", "--viz", "`"$htmlFile`"" `
            -RedirectStandardOutput $logFile `
            -RedirectStandardError $logFile `
            -NoNewWindow -PassThru -Wait `
            -TimeoutDuration 5000
        
        $endTime = Get-Date
        $duration = ($endTime - $startTime).TotalSeconds
        
        if ($process.ExitCode -eq 0) {
            $Script:Success++
            Write-Host "✓ [$Index/$Iterations] SUCCESS: '$Formula' ($($duration.ToString('F2'))s)" -ForegroundColor Green
            
            Add-Content -Path (Join-Path $SessionDir "success.log") -Value "$Formula|SUCCESS|$duration|$(Get-Date -Format o)"
            
            # Add to cache
            if (Test-Path $xyzFile) {
                bash "$ProjectRoot/scripts/cache.sh" add $xyzFile $safeFormula 2>$null
            }
            
            return $true
        }
    } catch {
        # Timeout or error
        $endTime = Get-Date
        $duration = ($endTime - $startTime).TotalSeconds
    }
    
    # Check log for rejection
    if (Test-Path $logFile) {
        $logContent = Get-Content $logFile -Raw
        
        if ($logContent -match "parse|invalid|unknown") {
            $Script:Rejected++
            Write-Host "⊘ [$Index/$Iterations] REJECTED: '$Formula'" -ForegroundColor Cyan
            Add-Content -Path (Join-Path $SessionDir "rejected.log") -Value "$Formula|REJECTED|$duration|$(Get-Date -Format o)"
        } elseif ($duration -ge 5) {
            $Script:Timeout++
            Write-Host "⏱ [$Index/$Iterations] TIMEOUT: '$Formula'" -ForegroundColor Yellow
            Add-Content -Path (Join-Path $SessionDir "timeout.log") -Value "$Formula|TIMEOUT|$duration|$(Get-Date -Format o)"
        } else {
            $Script:Failed++
            Write-Host "✗ [$Index/$Iterations] FAILED: '$Formula'" -ForegroundColor Red
            Add-Content -Path (Join-Path $SessionDir "failed.log") -Value "$Formula|FAILED|$duration|$(Get-Date -Format o)"
        }
    }
    
    return $false
}

# ============================================================================
# Main Execution
# ============================================================================

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║                                                               ║" -ForegroundColor Magenta
Write-Host "║          VSEPR-Sim Random Molecule Discovery                  ║" -ForegroundColor Magenta
Write-Host "║           Fuzzing Test - Formula Parser                      ║" -ForegroundColor Magenta
Write-Host "║                                                               ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""
Write-Host "Iterations: $Iterations" -ForegroundColor Cyan
Write-Host "Session: $SessionName" -ForegroundColor Cyan
Write-Host ""

# Check binary
if (-not (Test-Path $VseprBin)) {
    Write-Host "Error: VSEPR binary not found at $VseprBin" -ForegroundColor Red
    Write-Host "Build the project first: .\build.bat"
    exit 1
}

# Create session
New-Item -ItemType Directory -Force -Path "$SessionDir\molecules" | Out-Null
New-Item -ItemType Directory -Force -Path "$SessionDir\visualizations" | Out-Null
New-Item -ItemType Directory -Force -Path "$SessionDir\analysis" | Out-Null

# Session metadata
@{
    session_id = $SessionName
    name = "Random Discovery Fuzzing Test"
    created = Get-Date -Format o
    iterations = $Iterations
    status = "running"
} | ConvertTo-Json | Out-File "$SessionDir\session.json"

# Initialize logs
New-Item -ItemType File -Force -Path "$SessionDir\success.log" | Out-Null
New-Item -ItemType File -Force -Path "$SessionDir\rejected.log" | Out-Null
New-Item -ItemType File -Force -Path "$SessionDir\failed.log" | Out-Null
New-Item -ItemType File -Force -Path "$SessionDir\timeout.log" | Out-Null

Write-Host "Starting random molecule generation..." -ForegroundColor Cyan
Write-Host ""

$startTotal = Get-Date

# Main loop
for ($i = 1; $i -le $Iterations; $i++) {
    $Script:Total++
    
    $formula = New-RandomFormula
    Test-Formula -Formula $formula -Index $i
    
    # Progress
    if ($i % 100 -eq 0) {
        $progress = [int]($i * 100 / $Iterations)
        Write-Host ""
        Write-Host "═══ Progress: $i/$Iterations ($progress%) ═══" -ForegroundColor Magenta
        Write-Host "  Success: $Success | Rejected: $Rejected | Failed: $Failed | Timeout: $Timeout" -ForegroundColor White
        Write-Host ""
    }
}

$endTotal = Get-Date
$durationTotal = ($endTotal - $startTotal).TotalSeconds

# Generate report
$reportFile = Join-Path $SessionDir "analysis\report.md"
$successRate = if ($Total -gt 0) { [math]::Round($Success * 100 / $Total, 2) } else { 0 }
$robustness = if ($Total -gt 0) { [math]::Round(($Success + $Rejected) * 100 / $Total, 2) } else { 0 }

@"
# Random Molecule Discovery - Fuzzing Test Report

**Session**: $SessionName  
**Date**: $(Get-Date)  
**Duration**: $($durationTotal.ToString('F0'))s ($($($durationTotal / 60).ToString('F2')) minutes)

---

## Summary Statistics

| Metric | Count | Percentage |
|--------|-------|------------|
| **Total Iterations** | $Total | 100% |
| **Successful** | $Success | $([math]::Round($Success * 100 / $Total, 2))% |
| **Parser Rejected** | $Rejected | $([math]::Round($Rejected * 100 / $Total, 2))% |
| **Failed** | $Failed | $([math]::Round($Failed * 100 / $Total, 2))% |
| **Timeout** | $Timeout | $([math]::Round($Timeout * 100 / $Total, 2))% |

**Success Rate**: ${successRate}%  
**Parser Robustness**: ${robustness}% (handled gracefully)

---

## Performance

- **Average time per molecule**: $([math]::Round($durationTotal / $Total, 3))s
- **Successful molecules per second**: $([math]::Round($Success / $durationTotal, 2))
- **Total molecules tested**: $Total

---

## Files Generated

- **XYZ geometries**: $Success files in ``molecules/``
- **HTML visualizations**: $Success files in ``molecules/``
- **Session**: ``$SessionDir``

---

**Generated by**: VSEPR-Sim Random Discovery Fuzzer (PowerShell)  
**Report Date**: $(Get-Date -Format o)
"@ | Out-File $reportFile

# Update session metadata
@{
    session_id = $SessionName
    name = "Random Discovery Fuzzing Test"
    created = Get-Date -Format o
    completed = Get-Date -Format o
    iterations = $Iterations
    duration_seconds = $durationTotal
    results = @{
        total = $Total
        success = $Success
        rejected = $Rejected
        failed = $Failed
        timeout = $Timeout
    }
    status = "completed"
} | ConvertTo-Json | Out-File "$SessionDir\session.json"

# Final summary
Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║                    Final Statistics                           ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""
Write-Host "  Total Tested:     $Total" -ForegroundColor Cyan
Write-Host "  ✓ Success:        $Success ($successRate%)" -ForegroundColor Green
Write-Host "  ⊘ Rejected:       $Rejected ($([math]::Round($Rejected * 100 / $Total, 1))%)" -ForegroundColor Cyan
Write-Host "  ✗ Failed:         $Failed ($([math]::Round($Failed * 100 / $Total, 1))%)" -ForegroundColor Red
Write-Host "  ⏱ Timeout:        $Timeout ($([math]::Round($Timeout * 100 / $Total, 1))%)" -ForegroundColor Yellow
Write-Host ""
Write-Host "Session: $SessionDir" -ForegroundColor Cyan
Write-Host "Report:  $reportFile" -ForegroundColor Cyan
Write-Host ""
Write-Host "Fuzzing test completed!" -ForegroundColor Green
Write-Host ""
