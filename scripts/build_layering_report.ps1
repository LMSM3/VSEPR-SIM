<#
.SYNOPSIS
    VSEPR-SIM Automatic Layering Report Builder (MiKTeX)

.DESCRIPTION
    Detects or installs MiKTeX, generates layering data from the source tree
    via Python, and compiles the LaTeX report to PDF.

    Pipeline:
      1. Detect MiKTeX (pdflatex on PATH or standard install locations)
      2. If missing, download and install MiKTeX (portable or user-level)
      3. Run generate_layering_report.py to produce layering_data.tex
      4. Compile layering_report.tex -> layering_report.pdf (two passes)
      5. Clean auxiliary files

    Anti-black-box: every step prints its action and outcome.
    Deterministic: same source tree -> identical PDF.

.PARAMETER SkipInstall
    If set, skip the MiKTeX auto-install step and fail if pdflatex is not found.

.PARAMETER OutputDir
    Directory for the final PDF. Defaults to outputs/reports.

.EXAMPLE
    .\scripts\build_layering_report.ps1
    .\scripts\build_layering_report.ps1 -SkipInstall
    .\scripts\build_layering_report.ps1 -OutputDir "C:\Reports"

.NOTES
    Reference: .github/copilot-instructions.md  sections 2, 5, 9
#>

[CmdletBinding()]
param(
    [switch]$SkipInstall,
    [string]$OutputDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ============================================================================
# Configuration
# ============================================================================

$ROOT = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
# Handle case where script is run from repo root
if (-not (Test-Path (Join-Path $ROOT "CMakeLists.txt"))) {
    $ROOT = Split-Path -Parent $PSScriptRoot
}
if (-not (Test-Path (Join-Path $ROOT "CMakeLists.txt"))) {
    $ROOT = $PSScriptRoot | Split-Path -Parent
}
if (-not (Test-Path (Join-Path $ROOT "CMakeLists.txt"))) {
    $ROOT = (Get-Location).Path
}

$REPORTING_DIR = Join-Path $ROOT "reporting"
$TEX_FILE      = Join-Path $REPORTING_DIR "layering_report.tex"
$DATA_SCRIPT   = Join-Path $REPORTING_DIR "generate_layering_report.py"

if (-not $OutputDir) {
    $OutputDir = Join-Path $ROOT "outputs" "reports"
}

$MIKTEX_PORTABLE_DIR = Join-Path $ROOT ".miktex"
$MIKTEX_INSTALLER_URL = "https://miktex.org/download/ctan/systems/win32/miktex/setup/windows-x64/basic-miktex-24.1-x64.exe"

# ============================================================================
# Helpers
# ============================================================================

function Write-Banner {
    Write-Host ""
    Write-Host "================================================================" -ForegroundColor Cyan
    Write-Host "  VSEPR-SIM Automatic Layering Report Builder (MiKTeX)"        -ForegroundColor Cyan
    Write-Host "================================================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  Root:       $ROOT"
    Write-Host "  Reporting:  $REPORTING_DIR"
    Write-Host "  Output:     $OutputDir"
    Write-Host ""
}

function Write-Step([string]$msg) {
    Write-Host "[*] $msg" -ForegroundColor Yellow
}

function Write-Ok([string]$msg) {
    Write-Host "[OK] $msg" -ForegroundColor Green
}

function Write-Err([string]$msg) {
    Write-Host "[ERROR] $msg" -ForegroundColor Red
}

function Write-Warn([string]$msg) {
    Write-Host "[WARN] $msg" -ForegroundColor DarkYellow
}

# ============================================================================
# Step 1: Detect MiKTeX / pdflatex
# ============================================================================

function Find-PdfLatex {
    # Check PATH
    $cmd = Get-Command "pdflatex" -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    # Check standard MiKTeX install locations
    $candidates = @(
        "$env:LOCALAPPDATA\Programs\MiKTeX\miktex\bin\x64\pdflatex.exe",
        "C:\Program Files\MiKTeX\miktex\bin\x64\pdflatex.exe",
        "$env:LOCALAPPDATA\MiKTeX\miktex\bin\x64\pdflatex.exe",
        (Join-Path $MIKTEX_PORTABLE_DIR "texmfs\install\miktex\bin\x64\pdflatex.exe")
    )

    foreach ($path in $candidates) {
        if (Test-Path $path) {
            return $path
        }
    }

    return $null
}

# ============================================================================
# Step 2: Install MiKTeX if needed
# ============================================================================

function Install-MiKTeX {
    Write-Step "MiKTeX not found. Attempting automatic installation..."

    if ($SkipInstall) {
        Write-Err "pdflatex not found and -SkipInstall was specified."
        Write-Host ""
        Write-Host "  Install MiKTeX manually from: https://miktex.org/download" -ForegroundColor White
        Write-Host "  Or run this script without -SkipInstall for auto-install." -ForegroundColor White
        exit 1
    }

    # Check for winget first (preferred)
    $winget = Get-Command "winget" -ErrorAction SilentlyContinue
    if ($winget) {
        Write-Step "Installing MiKTeX via winget..."
        try {
            & winget install MiKTeX.MiKTeX --accept-package-agreements --accept-source-agreements 2>&1 | Out-Null
            # Refresh PATH
            $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" +
                        [System.Environment]::GetEnvironmentVariable("Path", "User")

            $pdflatex = Find-PdfLatex
            if ($pdflatex) {
                Write-Ok "MiKTeX installed via winget: $pdflatex"
                return $pdflatex
            }
        } catch {
            Write-Warn "winget install failed: $_"
        }
    }

    # Fallback: direct download
    $installerPath = Join-Path $env:TEMP "miktex-setup.exe"

    Write-Step "Downloading MiKTeX installer..."
    Write-Host "  URL: $MIKTEX_INSTALLER_URL"

    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -Uri $MIKTEX_INSTALLER_URL -OutFile $installerPath -UseBasicParsing
        Write-Ok "Downloaded: $installerPath"
    } catch {
        Write-Err "Failed to download MiKTeX: $_"
        Write-Host ""
        Write-Host "  Install MiKTeX manually from: https://miktex.org/download" -ForegroundColor White
        exit 1
    }

    Write-Step "Running MiKTeX installer (user-level, auto-install packages)..."
    Write-Host "  This may take several minutes."

    try {
        $proc = Start-Process -FilePath $installerPath `
            -ArgumentList "--unattended", "--user-install", "--auto-install=yes" `
            -Wait -PassThru -NoNewWindow
        if ($proc.ExitCode -ne 0) {
            Write-Warn "Installer exited with code $($proc.ExitCode)"
        }
    } catch {
        Write-Err "Installer failed: $_"
        Write-Host "  Install MiKTeX manually from: https://miktex.org/download" -ForegroundColor White
        exit 1
    }

    # Refresh PATH
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" +
                [System.Environment]::GetEnvironmentVariable("Path", "User")

    $pdflatex = Find-PdfLatex
    if ($pdflatex) {
        Write-Ok "MiKTeX installed: $pdflatex"
        return $pdflatex
    }

    Write-Err "MiKTeX installed but pdflatex not found on PATH."
    Write-Host "  Restart your terminal and re-run this script." -ForegroundColor White
    exit 1
}

# ============================================================================
# Step 3: Generate layering data
# ============================================================================

function Invoke-DataGeneration {
    Write-Step "Generating layering data from source tree..."

    $python = Get-Command "python" -ErrorAction SilentlyContinue
    if (-not $python) {
        $python = Get-Command "python3" -ErrorAction SilentlyContinue
    }
    if (-not $python) {
        Write-Err "Python not found. Install Python 3 and ensure it is on PATH."
        exit 1
    }

    $pythonExe = $python.Source
    Write-Host "  Python: $pythonExe"
    Write-Host "  Script: $DATA_SCRIPT"

    & $pythonExe $DATA_SCRIPT --root $ROOT
    if ($LASTEXITCODE -ne 0) {
        Write-Err "Data generation failed (exit code $LASTEXITCODE)."
        exit 1
    }

    $dataFile = Join-Path $REPORTING_DIR "layering_data.tex"
    if (-not (Test-Path $dataFile)) {
        Write-Err "Expected data file not found: $dataFile"
        exit 1
    }

    Write-Ok "Data file: $dataFile"
}

# ============================================================================
# Step 4: Compile LaTeX
# ============================================================================

function Invoke-LaTeXCompilation([string]$pdflatex) {
    Write-Step "Compiling LaTeX report..."

    if (-not (Test-Path $TEX_FILE)) {
        Write-Err "LaTeX source not found: $TEX_FILE"
        exit 1
    }

    # MiKTeX auto-install missing packages on first run
    $env:MIKTEX_ENABLEINSTALLER = "t"

    Push-Location $REPORTING_DIR
    try {
        # Pass 1 — generate AUX, TOC
        Write-Host "  Pass 1/2..."
        & $pdflatex -interaction=nonstopmode -halt-on-error "layering_report.tex" 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Warn "Pass 1 had warnings (continuing to pass 2)."
        }

        # Pass 2 — resolve references and TOC
        Write-Host "  Pass 2/2..."
        & $pdflatex -interaction=nonstopmode -halt-on-error "layering_report.tex" 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Warn "Pass 2 had warnings. Check layering_report.log for details."
        }

        $pdfPath = Join-Path $REPORTING_DIR "layering_report.pdf"
        if (-not (Test-Path $pdfPath)) {
            Write-Err "PDF not generated. Check $REPORTING_DIR\layering_report.log"
            Get-Content (Join-Path $REPORTING_DIR "layering_report.log") -Tail 30
            exit 1
        }

        Write-Ok "PDF compiled: $pdfPath"
        return $pdfPath

    } finally {
        Pop-Location
    }
}

# ============================================================================
# Step 5: Deploy and clean
# ============================================================================

function Invoke-DeployAndClean([string]$pdfPath) {
    # Copy to output directory
    if (-not (Test-Path $OutputDir)) {
        New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
    }

    $destPdf = Join-Path $OutputDir "layering_report.pdf"
    Copy-Item $pdfPath $destPdf -Force
    Write-Ok "Deployed: $destPdf"

    # Clean auxiliary files
    Write-Step "Cleaning auxiliary files..."
    $auxExtensions = @(".aux", ".log", ".toc", ".out", ".fls", ".fdb_latexmk", ".synctex.gz")
    foreach ($ext in $auxExtensions) {
        $auxFile = Join-Path $REPORTING_DIR ("layering_report" + $ext)
        if (Test-Path $auxFile) {
            Remove-Item $auxFile -Force
        }
    }
    Write-Ok "Auxiliary files cleaned."
}

# ============================================================================
# Main
# ============================================================================

Write-Banner

# Validate project root
if (-not (Test-Path (Join-Path $ROOT "CMakeLists.txt"))) {
    Write-Err "CMakeLists.txt not found in $ROOT. Not a VSEPR-SIM project root."
    exit 1
}

# Step 1: Find pdflatex
Write-Step "Detecting MiKTeX / pdflatex..."
$pdflatex = Find-PdfLatex

if ($pdflatex) {
    Write-Ok "Found: $pdflatex"
    # Show version
    $ver = & $pdflatex --version 2>&1 | Select-Object -First 1
    Write-Host "  Version: $ver"
} else {
    # Step 2: Install
    $pdflatex = Install-MiKTeX
}

Write-Host ""

# Step 3: Generate data
Invoke-DataGeneration
Write-Host ""

# Step 4: Compile
$pdfPath = Invoke-LaTeXCompilation $pdflatex
Write-Host ""

# Step 5: Deploy and clean
Invoke-DeployAndClean $pdfPath

# Summary
Write-Host ""
Write-Host "================================================================" -ForegroundColor Green
Write-Host "  Layering Report Complete" -ForegroundColor Green
Write-Host "================================================================" -ForegroundColor Green
Write-Host ""
Write-Host "  PDF:  $(Join-Path $OutputDir 'layering_report.pdf')"
Write-Host "  Data: $(Join-Path $REPORTING_DIR 'layering_data.tex')"
Write-Host ""
Write-Host "  To regenerate:" -ForegroundColor Gray
Write-Host "    .\scripts\build_layering_report.ps1" -ForegroundColor Gray
Write-Host ""
