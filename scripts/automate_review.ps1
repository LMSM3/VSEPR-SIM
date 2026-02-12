################################################################################
# VSEPR-Sim Automated Review (PowerShell)
# Reviews each discovered molecule with detailed output and visualization
################################################################################

param(
    [string]$SessionDir = ""
)

$ErrorActionPreference = "Continue"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir

$VseprBin = Join-Path $ProjectRoot "build\bin\vsepr.exe"
if (-not (Test-Path $VseprBin)) {
    $VseprBin = Join-Path $ProjectRoot "build\bin\vsepr"
}

$ReviewOutput = Join-Path $ProjectRoot "outputs\reviews"

# ============================================================================
# Functions
# ============================================================================

function Write-Header {
    Write-Host ""
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
    Write-Host "║                                                                ║" -ForegroundColor Magenta
    Write-Host "║             VSEPR-Sim Automated Review                         ║" -ForegroundColor Magenta
    Write-Host "║          Detailed Analysis of Discovered Molecules             ║" -ForegroundColor Magenta
    Write-Host "║                                                                ║" -ForegroundColor Magenta
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
    Write-Host ""
}

function Find-LatestSession {
    $sessions = Get-ChildItem -Path (Join-Path $ProjectRoot "outputs\sessions") -Directory -Filter "random_discovery_*" -ErrorAction SilentlyContinue | 
        Sort-Object LastWriteTime -Descending | 
        Select-Object -First 1
    
    if ($sessions) {
        return $sessions.FullName
    }
    return $null
}

function Review-Molecule {
    param(
        [string]$Formula,
        [int]$Index,
        [int]$Total,
        [string]$ReviewDir
    )
    
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "Molecule [$Index/$Total]: $Formula" -ForegroundColor Yellow
    Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host ""
    
    # Try to open Wikipedia page for common chemicals
    $wikiScript = Join-Path $ProjectRoot "scripts\wiki.ps1"
    & $wikiScript auto $Formula 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "📖 Opened Wikipedia page" -ForegroundColor Green
    }
    Write-Host ""
    
    # Create review directory
    $safeFormula = $Formula -replace '[^a-zA-Z0-9()+\-]', '' | Select-Object -First 50
    if ([string]::IsNullOrWhiteSpace($safeFormula)) {
        $safeFormula = "mol_$Index"
    }
    
    $molReviewDir = Join-Path $ReviewDir "${safeFormula}_${Index}"
    New-Item -ItemType Directory -Path $molReviewDir -Force | Out-Null
    
    $xyzFile = Join-Path $molReviewDir "geometry.xyz"
    $htmlFile = Join-Path $molReviewDir "viewer.html"
    $logFile = Join-Path $molReviewDir "build.log"
    
    Write-Host "▶ Building molecule with full details..." -ForegroundColor Cyan
    Write-Host ""
    
    # Build molecule
    $output = & $VseprBin build $Formula --optimize --output $xyzFile --viz $htmlFile 2>&1 | Tee-Object -Variable buildOutput
    $output | Out-File -FilePath $logFile -Encoding utf8
    $exitCode = $LASTEXITCODE
    
    Write-Host ""
    
    if ($exitCode -eq 0) {
        Write-Host "✓ Build successful" -ForegroundColor Green
        
        # Display XYZ contents
        if (Test-Path $xyzFile) {
            Write-Host ""
            Write-Host "▶ Geometry (XYZ format):" -ForegroundColor Cyan
            Write-Host "─────────────────────────────────────────────────────────────" -ForegroundColor Yellow
            Get-Content $xyzFile | Write-Host
            Write-Host "─────────────────────────────────────────────────────────────" -ForegroundColor Yellow
            
            # Copy to clipboard
            try {
                Get-Content $xyzFile | Set-Clipboard
                Write-Host "✓ Geometry copied to clipboard" -ForegroundColor Green
            } catch {
                Write-Host "⚠ Could not copy to clipboard" -ForegroundColor Yellow
            }
            
            # File statistics
            Write-Host ""
            Write-Host "▶ File Information:" -ForegroundColor Cyan
            $atomCount = (Get-Content $xyzFile -First 1).Trim()
            $fileSize = "{0:N2} KB" -f ((Get-Item $xyzFile).Length / 1KB)
            Write-Host "  • Atoms: $atomCount" -ForegroundColor Gray
            Write-Host "  • File size: $fileSize" -ForegroundColor Gray
            Write-Host "  • XYZ file: $xyzFile" -ForegroundColor Gray
            
            if (Test-Path $htmlFile) {
                $htmlSize = "{0:N2} KB" -f ((Get-Item $htmlFile).Length / 1KB)
                Write-Host "  • HTML viewer: $htmlFile ($htmlSize)" -ForegroundColor Gray
                
                # Auto-open HTML visualization
                Start-Process $htmlFile
                Write-Host "  • Auto-opened HTML viewer" -ForegroundColor Green
            }
        }
        
        # Extract key information
        Write-Host ""
        Write-Host "▶ Analysis Summary:" -ForegroundColor Cyan
        
        $logContent = Get-Content $logFile -Raw
        
        # Geometry type
        if ($logContent -match "Geometry\s+(\w+)") {
            Write-Host "  • Geometry: $($matches[1])" -ForegroundColor Gray
        }
        
        # Energy
        if ($logContent -match "Final energy\s+([\d.]+)\s+kcal/mol") {
            Write-Host "  • Final energy: $($matches[1]) kcal/mol" -ForegroundColor Gray
        }
        
        # Convergence
        if ($logContent -match "converged in (\d+) iterations") {
            Write-Host "  • Optimization converged in $($matches[1]) iterations" -ForegroundColor Gray
        }
        
        # Lone pairs
        if ($logContent -match "Lone pairs\s+(\d+)") {
            Write-Host "  • Lone pairs: $($matches[1])" -ForegroundColor Gray
        }
        
        # Bonds
        if ($logContent -match "Bonds\s+(\d+)") {
            Write-Host "  • Bonds: $($matches[1])" -ForegroundColor Gray
        }
        
    } else {
        Write-Host "✗ Build failed" -ForegroundColor Red
    }
    
    Write-Host ""
    Write-Host "▶ Review files saved to: $molReviewDir" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "⏱  Waiting 10 seconds before next molecule..." -ForegroundColor Yellow
    Start-Sleep -Seconds 10
}

# ============================================================================
# Main
# ============================================================================

Write-Header

# Check VSEPR binary
if (-not (Test-Path $VseprBin)) {
    Write-Host "Error: VSEPR binary not found at: $VseprBin" -ForegroundColor Red
    Write-Host "Please build the project first: .\build.ps1"
    exit 1
}

# Determine session directory
if ([string]::IsNullOrWhiteSpace($SessionDir)) {
    $SessionDir = Find-LatestSession
    if (-not $SessionDir) {
        Write-Host "Error: No random discovery sessions found" -ForegroundColor Red
        Write-Host "Run: .\scripts\random_discovery.ps1 <iterations>"
        exit 1
    }
    Write-Host "Using latest session: $(Split-Path -Leaf $SessionDir)" -ForegroundColor Cyan
} else {
    if (-not (Test-Path $SessionDir)) {
        Write-Host "Error: Session directory not found: $SessionDir" -ForegroundColor Red
        exit 1
    }
}

# Check success log
$successLog = Join-Path $SessionDir "success.log"
if (-not (Test-Path $successLog)) {
    Write-Host "Error: No success log found at: $successLog" -ForegroundColor Red
    exit 1
}

# Count molecules
$molecules = Get-Content $successLog
$total = $molecules.Count
Write-Host "Found: $total successful molecules to review" -ForegroundColor Cyan
Write-Host ""

# Create review directory
$ReviewOutput = Join-Path $ReviewOutput (Split-Path -Leaf $SessionDir)
New-Item -ItemType Directory -Path $ReviewOutput -Force | Out-Null

Write-Host "Review output directory: $ReviewOutput" -ForegroundColor Cyan
Write-Host ""

# Confirm
$response = Read-Host "Continue with automated review? [y/N]"
if ($response -notmatch '^[Yy]$') {
    Write-Host "Review cancelled."
    exit 0
}

# Process each molecule
$index = 1
foreach ($line in $molecules) {
    $parts = $line -split '\|'
    $formula = $parts[0]
    
    Review-Molecule -Formula $formula -Index $index -Total $total -ReviewDir $ReviewOutput
    $index++
}

# Final summary
Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║                     Review Complete                            ║" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""
Write-Host "Reviewed: $total molecules" -ForegroundColor Cyan
Write-Host "Results: $ReviewOutput" -ForegroundColor Cyan
Write-Host ""
Write-Host "You can now:" -ForegroundColor Yellow
Write-Host "  • Open HTML viewers in your browser" -ForegroundColor Gray
Write-Host "  • Load XYZ files in visualization software" -ForegroundColor Gray
Write-Host "  • Review build logs for detailed information" -ForegroundColor Gray
Write-Host ""
