#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Demo version of random discovery (no binary required)
.DESCRIPTION
    Demonstrates the fuzzing system without requiring compiled binary
#>

param([int]$Iterations = 20)

$SessionName = "demo_discovery_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
$SessionDir = "outputs\sessions\$SessionName"

$Script:Total = 0
$Script:Success = 0
$Script:Rejected = 0

$Elements = @('H', 'C', 'N', 'O', 'F', 'P', 'S', 'Cl')

function New-RandomFormula {
    $pattern = Get-Random -Minimum 0 -Maximum 5
    
    switch ($pattern) {
        0 {
            # Simple: H2O, NH3
            $elem1 = $Elements | Get-Random
            $elem2 = $Elements | Get-Random
            $count1 = Get-Random -Minimum 1 -Maximum 5
            $count2 = Get-Random -Minimum 1 -Maximum 5
            "$elem1$count1$elem2$count2"
        }
        1 {
            # Complex: Ca(OH)2
            $elem1 = $Elements | Get-Random
            $elem2 = $Elements | Get-Random
            $elem3 = $Elements | Get-Random
            $count = Get-Random -Minimum 1 -Maximum 4
            "$elem1($elem2$elem3)$count"
        }
        2 {
            # Ionic: Na+, SO4 2-
            $elem = $Elements | Get-Random
            $count = Get-Random -Minimum 1 -Maximum 4
            $ion = @('+', '-', '2+', '2-') | Get-Random
            "$elem$count$ion"
        }
        3 {
            # Garbage
            $garbage = ""
            1..(Get-Random -Minimum 5 -Maximum 15) | ForEach-Object {
                $garbage += (@('(', ')', '+', '-', '1', '2', 'X', 'Y') | Get-Random)
            }
            $garbage
        }
        4 {
            # Edge cases
            @("", "123", "+++", "H1000") | Get-Random
        }
    }
}

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║          Random Molecule Discovery - DEMO MODE                ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""
Write-Host "Iterations: $Iterations (DEMO - simulated results)" -ForegroundColor Cyan
Write-Host "Session: $SessionName" -ForegroundColor Cyan
Write-Host ""

New-Item -ItemType Directory -Force -Path "$SessionDir\molecules" | Out-Null

for ($i = 1; $i -le $Iterations; $i++) {
    $Script:Total++
    $formula = New-RandomFormula
    
    # Simulate testing - simple validation
    $randomSuccess = (Get-Random -Minimum 0 -Maximum 100) -lt 40
    $hasContent = -not [string]::IsNullOrWhiteSpace($formula)
    
    if ($hasContent -and $randomSuccess) {
        $Script:Success++
        Write-Host "✓ [$i/$Iterations] SUCCESS: '$formula'" -ForegroundColor Green
        
        # Create dummy XYZ
        $xyzContent = "3`nDEMO molecule: $formula`nH    0.0    0.0    0.0`nH    1.0    0.0    0.0`nH    0.5    0.866  0.0"
        $xyzContent | Out-File "$SessionDir\molecules\${formula}_${i}.xyz"
    }
    else {
        $Script:Rejected++
        Write-Host "⊘ [$i/$Iterations] REJECTED: '$formula'" -ForegroundColor Cyan
    }
    
    Start-Sleep -Milliseconds 100
}

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║                    DEMO Results                               ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""
Write-Host "  Total Tested:     $Total" -ForegroundColor Cyan
Write-Host "  ✓ Success:        $Success" -ForegroundColor Green
Write-Host "  ⊘ Rejected:       $Rejected" -ForegroundColor Cyan
Write-Host ""
Write-Host "Session: $SessionDir" -ForegroundColor Cyan
Write-Host ""
Write-Host "This was a DEMO. Build the project and run:" -ForegroundColor Yellow
Write-Host "  .\scripts\random_discovery.ps1 -Iterations 100" -ForegroundColor White
Write-Host ""
