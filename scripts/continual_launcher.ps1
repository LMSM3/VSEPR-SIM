<#
.SYNOPSIS
    Continual Formation Engine -- Launcher & Monitor

.DESCRIPTION
    Starts the continual orchestrator as a persistent background process.
    Designed to run before leaving for school/work.  Monitors health,
    logs everything, and can be stopped gracefully via:
        touch continual_results/STOP       (Unix)
        New-Item continual_results/STOP    (PowerShell)

    Smart resource management:
    - Monitors CPU/memory/disk
    - Throttles if system is under pressure
    - Checkpoints every cycle (safe to kill)
    - Resume with: .\continual_launcher.ps1 -Resume

.PARAMETER Hours
    How many hours to run (default: 24)

.PARAMETER BatchSize
    Formulas per cycle (default: 30)

.PARAMETER Seeds
    Seeds per formula for ensemble (default: 3)

.PARAMETER OutputDir
    Results directory (default: continual_results)

.PARAMETER Resume
    Resume from existing checkpoint

.PARAMETER ChartOnly
    Generate charts from existing data without running

.PARAMETER Background
    Run orchestrator as a background job

.EXAMPLE
    # Start 8-hour session before leaving for class
    .\continual_launcher.ps1 -Hours 8

    # Resume where you left off
    .\continual_launcher.ps1 -Resume

    # Just regenerate charts
    .\continual_launcher.ps1 -ChartOnly

    # Full 24h overnight run with larger batches
    .\continual_launcher.ps1 -Hours 24 -BatchSize 50 -Seeds 5 -Background
#>

# Tip: for a single-call start with live status, dot-source cfe_load.ps1 instead:
#   . .\scripts\cfe_load.ps1
#   Start-CFE -Hours 24

[CmdletBinding()]
param(
    [double]$Hours = 24,
    [int]$BatchSize = 30,
    [int]$Seeds = 3,
    [string]$OutputDir = "continual_results",
    [switch]$Resume,
    [switch]$ChartOnly,
    [switch]$Background
)

$ErrorActionPreference = "Continue"
$ProjectRoot = $PSScriptRoot | Split-Path
Set-Location $ProjectRoot

# ============================================================================
# Configuration
# ============================================================================

$Orchestrator = Join-Path $ProjectRoot "scripts\continual_orchestrator.py"
$RunnerExe = Join-Path $ProjectRoot "build\continual_runner.exe"
$FullOutputDir = Join-Path $ProjectRoot $OutputDir
$LogFile = Join-Path $FullOutputDir "launcher.log"
$PidFile = Join-Path $FullOutputDir "orchestrator.pid"

# ============================================================================
# Utility functions
# ============================================================================

function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    $ts = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $entry = "[$ts] [$Level] $Message"
    Write-Host $entry
    if (Test-Path (Split-Path $LogFile)) {
        Add-Content -Path $LogFile -Value $entry
    }
}

function Get-SystemHealth {
    $cpu = (Get-CimInstance Win32_Processor | Measure-Object -Property LoadPercentage -Average).Average
    $mem = Get-CimInstance Win32_OperatingSystem
    $memUsed = [math]::Round(($mem.TotalVisibleMemorySize - $mem.FreePhysicalMemory) / 1MB, 1)
    $memTotal = [math]::Round($mem.TotalVisibleMemorySize / 1MB, 1)
    $memPct = [math]::Round(($memUsed / $memTotal) * 100, 0)

    $disk = Get-PSDrive C
    $diskFreeGB = [math]::Round($disk.Free / 1GB, 1)

    return @{
        CPU = $cpu
        MemUsedGB = $memUsed
        MemTotalGB = $memTotal
        MemPercent = $memPct
        DiskFreeGB = $diskFreeGB
    }
}

function Test-RunnerBuilt {
    if (Test-Path $RunnerExe) {
        Write-Log "Runner found: $RunnerExe"
        return $true
    }
    Write-Log "Runner not built: $RunnerExe" "WARN"
    Write-Log "Will use fallback pipeline (atomistic-sim)" "WARN"
    return $false
}

function Show-Banner {
    $health = Get-SystemHealth
    Write-Host ""
    Write-Host "  +=======================================================+" -ForegroundColor Cyan
    Write-Host "  |  Continual Formation Engine -- Launcher                |" -ForegroundColor Cyan
    Write-Host "  +=======================================================+" -ForegroundColor Cyan
    Write-Host "  |  Duration:   $Hours hours" -ForegroundColor White
    Write-Host "  |  Batch:      $BatchSize formulas/cycle" -ForegroundColor White
    Write-Host "  |  Seeds:      $Seeds per formula" -ForegroundColor White
    Write-Host "  |  Output:     $FullOutputDir" -ForegroundColor White
    Write-Host "  |  CPU:        $($health.CPU)%" -ForegroundColor White
    Write-Host "  |  Memory:     $($health.MemUsedGB)/$($health.MemTotalGB) GB ($($health.MemPercent)%)" -ForegroundColor White
    Write-Host "  |  Disk free:  $($health.DiskFreeGB) GB" -ForegroundColor White
    Write-Host "  +=======================================================+" -ForegroundColor Cyan
    Write-Host "  |  To stop:  New-Item $OutputDir/STOP                   |" -ForegroundColor Yellow
    Write-Host "  +=======================================================+" -ForegroundColor Cyan
    Write-Host ""
}

function Start-Orchestrator {
    param([bool]$IsResume)

    # Build args
    $pyArgs = @(
        $Orchestrator,
        "--out", $FullOutputDir,
        "--hours", $Hours,
        "--batch-size", $BatchSize,
        "--seeds", $Seeds
    )

    if (Test-Path $RunnerExe) {
        $pyArgs += @("--runner", $RunnerExe)
    }

    if ($IsResume) {
        $pyArgs += "--resume"
    }

    Write-Log "Starting orchestrator: python $($pyArgs -join ' ')"

    if ($Background) {
        Write-Log "Running as background job"
        $job = Start-Job -ScriptBlock {
            param($py, $args_list)
            & python @args_list 2>&1
        } -ArgumentList "python", $pyArgs

        Set-Content -Path $PidFile -Value $job.Id
        Write-Log "Background job started: ID=$($job.Id)"
        Write-Host ""
        Write-Host "  Background job started (ID: $($job.Id))" -ForegroundColor Green
        Write-Host "  Check status:  Get-Job $($job.Id) | Receive-Job" -ForegroundColor Gray
        Write-Host "  Stop:          New-Item $OutputDir/STOP" -ForegroundColor Gray
        Write-Host "  Or:            Stop-Job $($job.Id)" -ForegroundColor Gray
        Write-Host ""
    } else {
        # Foreground -- with health monitoring
        Write-Log "Running in foreground (Ctrl+C to stop)"

        $process = Start-Process python -ArgumentList $pyArgs `
            -NoNewWindow -PassThru -RedirectStandardOutput (Join-Path $FullOutputDir "stdout.log") `
            -RedirectStandardError (Join-Path $FullOutputDir "stderr.log")

        Set-Content -Path $PidFile -Value $process.Id
        Write-Log "Process started: PID=$($process.Id)"

        # Monitor loop
        $monitorInterval = 30  # seconds between info refreshes
        $startTime = Get-Date
        $InfoScript = Join-Path $ProjectRoot "scripts\cfe_info.py"

        while (-not $process.HasExited) {
            Start-Sleep -Seconds $monitorInterval

            $health = Get-SystemHealth

            # Throttle check
            if ($health.MemPercent -gt 90) {
                Write-Log "Memory pressure: $($health.MemPercent)% -- consider stopping" "WARN"
            }
            if ($health.DiskFreeGB -lt 1.0) {
                Write-Log "Disk space critical: $($health.DiskFreeGB) GB -- stopping" "ERROR"
                New-Item -Path (Join-Path $FullOutputDir "STOP") -ItemType File -Force | Out-Null
            }

            # Live info panel via cfe_info.py
            if (Test-Path $InfoScript) {
                Write-Host ""
                python $InfoScript $FullOutputDir 2>$null
                Write-Host ("  System   CPU {0,3}%   Mem {1,3}%   Disk free {2} GB" -f
                            $health.CPU, $health.MemPercent, $health.DiskFreeGB) -ForegroundColor DarkGray
            } else {
                # Fallback: plain progress line if cfe_info.py is missing
                $elapsed   = (Get-Date) - $startTime
                $ledger    = Join-Path $FullOutputDir "ledger.csv"
                $lineCount = 0
                if (Test-Path $ledger) {
                    $lineCount = (Get-Content $ledger | Measure-Object -Line).Lines - 1
                    if ($lineCount -lt 0) { $lineCount = 0 }
                }
                $rate = if ($elapsed.TotalHours -gt 0) {
                    [math]::Round($lineCount / $elapsed.TotalHours, 0)
                } else { 0 }
                Write-Host ("  [{0}] Formations: {1}  Rate: {2}/hr  CPU: {3}%  Mem: {4}%" -f
                            $elapsed.ToString('hh\:mm\:ss'), $lineCount,
                            $rate, $health.CPU, $health.MemPercent)
            }
        }

        Write-Host ""
        Write-Log "Orchestrator exited with code: $($process.ExitCode)"

        # Final charts
        Write-Log "Generating final charts..."
        & python $Orchestrator --chart --out $FullOutputDir
    }
}

# ============================================================================
# Main
# ============================================================================

# Create output directory
New-Item -Path $FullOutputDir -ItemType Directory -Force | Out-Null

if ($ChartOnly) {
    Write-Log "Chart-only mode"
    & python $Orchestrator --chart --out $FullOutputDir
    exit 0
}

# Pre-flight checks
Write-Log "Pre-flight checks..."

if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    Write-Log "Python not found in PATH" "ERROR"
    exit 1
}

# Check matplotlib
$matplotCheck = python -c "import matplotlib; print('ok')" 2>&1
if ($matplotCheck -ne "ok") {
    Write-Log "Installing matplotlib + numpy..."
    pip install matplotlib numpy --quiet
}

Test-RunnerBuilt | Out-Null

Show-Banner

# Check for existing session
$checkpoint = Join-Path $FullOutputDir "checkpoint.json"
if ($Resume -and (Test-Path $checkpoint)) {
    $state = Get-Content $checkpoint | ConvertFrom-Json
    Write-Log "Resuming session $($state.session_id) from cycle $($state.cycle)"
    Write-Host "  Resuming: cycle $($state.cycle), $($state.total_completed) completions" -ForegroundColor Green
    Start-Orchestrator -IsResume $true
} else {
    Write-Log "Starting new session ($Hours hours, $BatchSize batch, $Seeds seeds)"
    Start-Orchestrator -IsResume $false
}

# Summary
Write-Host ""
Write-Host "  === SESSION ENDED ===" -ForegroundColor Cyan
$ledger = Join-Path $FullOutputDir "ledger.csv"
if (Test-Path $ledger) {
    $lines = (Get-Content $ledger | Measure-Object -Line).Lines - 1
    Write-Host "  Total formations: $lines" -ForegroundColor White
    Write-Host "  Ledger:  $ledger" -ForegroundColor Gray
    Write-Host "  Charts:  $(Join-Path $FullOutputDir 'charts')" -ForegroundColor Gray
}
Write-Host "  To resume: .\continual_launcher.ps1 -Resume" -ForegroundColor Yellow
Write-Host ""
