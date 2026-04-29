<#
.SYNOPSIS
    CFE-Load -- Continual Formation Engine: single-call loader

.DESCRIPTION
    One function: Start-CFE.
    Boots the continual engine, shows a spinner while it initialises,
    wires cfe_info.py into the live monitor loop so you get a live
    status line while the engine runs in the foreground.

    Drop this file anywhere and dot-source it:
        . .\scripts\cfe_load.ps1

    Then just call:
        Start-CFE              # 24h default run
        Start-CFE -Hours 8     # before leaving for class
        Start-CFE -Resume      # pick up from checkpoint
        Start-CFE -Info        # status only, no run

.PARAMETER Hours
    Run duration in hours (default 24).

.PARAMETER OutputDir
    Results directory (default: continual_results).

.PARAMETER BatchSize
    Formulas per cycle (default: 30).

.PARAMETER Seeds
    Seeds per formula (default: 3).

.PARAMETER Resume
    Resume from existing checkpoint.

.PARAMETER Info
    Show status info only -- does not start a run.

.PARAMETER Watch
    After starting the engine in the background, open a watch loop
    that refreshes the cfe_info panel every few seconds.

.EXAMPLE
    . .\scripts\cfe_load.ps1
    Start-CFE -Hours 8

.EXAMPLE
    . .\scripts\cfe_load.ps1
    Start-CFE -Resume -Watch

.EXAMPLE
    . .\scripts\cfe_load.ps1
    Start-CFE -Info
#>

# -- Resolve project root from this script's location ------------------------
$_CFE_Root      = Split-Path $PSScriptRoot
$_CFE_Launcher  = Join-Path $_CFE_Root "scripts\continual_launcher.ps1"
$_CFE_Orch      = Join-Path $_CFE_Root "scripts\continual_orchestrator.py"
$_CFE_Info      = Join-Path $_CFE_Root "scripts\cfe_info.py"
$_CFE_Runner    = Join-Path $_CFE_Root "build\continual_runner.exe"


# -- Colour helpers (safe on older PS) ----------------------------------------
function _CFE_Write {
    param([string]$Text, [ConsoleColor]$Color = [ConsoleColor]::White)
    $prev = $Host.UI.RawUI.ForegroundColor
    $Host.UI.RawUI.ForegroundColor = $Color
    Write-Host $Text
    $Host.UI.RawUI.ForegroundColor = $prev
}

function _CFE_WriteInline {
    param([string]$Text, [ConsoleColor]$Color = [ConsoleColor]::White)
    $prev = $Host.UI.RawUI.ForegroundColor
    $Host.UI.RawUI.ForegroundColor = $Color
    Write-Host $Text -NoNewline
    $Host.UI.RawUI.ForegroundColor = $prev
}


# -- Spinner -------------------------------------------------------------------
function Invoke-CFESpinner {
    <#
    .SYNOPSIS
        Display an animated spinner for $Seconds seconds with a status message.
    .PARAMETER Message
        Label shown beside the spinner.
    .PARAMETER Seconds
        How long to spin (default: 3).
    #>
    param(
        [string]$Message  = "Initialising",
        [int]   $Seconds  = 3,
        [ConsoleColor]$Color = [ConsoleColor]::Cyan
    )

    $frames   = @('*','*','*','*','*','*','*','*','*','*')
    $deadline = (Get-Date).AddSeconds($Seconds)
    $i        = 0

    while ((Get-Date) -lt $deadline) {
        $frame = $frames[$i % $frames.Length]
        _CFE_WriteInline "`r  $frame  $Message   " $Color
        Start-Sleep -Milliseconds 80
        $i++
    }
    Write-Host "`r  ✓  $Message   " -ForegroundColor Green
}


# -- Pre-flight ----------------------------------------------------------------
function Test-CFEPreFlight {
    param([string]$OutputDir)

    $ok = $true

    # Python
    if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
        _CFE_Write "  [FAIL] Python not found in PATH." Red
        $ok = $false
    } else {
        $ver = python --version 2>&1
        _CFE_Write "  [OK]   $ver" Green
    }

    # Runner binary
    if (Test-Path $_CFE_Runner) {
        _CFE_Write "  [OK]   Runner: $_CFE_Runner" Green
    } else {
        _CFE_Write "  [WARN] Runner binary not found -- will use fallback pipeline." Yellow
    }

    # Output directory
    if (-not (Test-Path $OutputDir)) {
        New-Item -Path $OutputDir -ItemType Directory -Force | Out-Null
    }
    _CFE_Write "  [OK]   Output: $OutputDir" Green

    # matplotlib (quick check, non-blocking)
    $ml = python -c "import matplotlib" 2>&1
    if ($LASTEXITCODE -eq 0) {
        _CFE_Write "  [OK]   matplotlib available" Green
    } else {
        _CFE_Write "  [WARN] matplotlib missing -- charts disabled." Yellow
    }

    return $ok
}


# -- Info panel ----------------------------------------------------------------
function Show-CFEInfo {
    <#
    .SYNOPSIS
        Print current status from cfe_info.py.
    .PARAMETER OutputDir
        Results directory to inspect.
    .PARAMETER Top
        Number of top stable formulas to display.
    #>
    param(
        [string]$OutputDir = "continual_results",
        [int]   $Top       = 10
    )
    $fullDir = if ([System.IO.Path]::IsPathRooted($OutputDir)) {
        $OutputDir
    } else {
        Join-Path $_CFE_Root $OutputDir
    }
    python $script:_CFE_Info $fullDir --top $Top
}


# -- Live monitor (wired into engine loop) ------------------------------------
function Start-CFEMonitor {
    <#
    .SYNOPSIS
        Watch loop: calls cfe_info every N seconds until the process ends
        or a STOP file appears.
    #>
    param(
        [string]$OutputDir,
        [int]   $IntervalSec = 10,
        [int]   $ProcessId   = -1
    )

    $stopFile = Join-Path $OutputDir "STOP"
    _CFE_Write "" White
    _CFE_Write "  Live monitor active (Ctrl+C to stop the watch, engine continues)" Cyan
    _CFE_Write "  Refresh every ${IntervalSec}s   |   To stop engine: New-Item '$stopFile'" DarkGray
    _CFE_Write "" White

    try {
        while ($true) {
            # Clear and reprint info
            if ($Host.UI.RawUI.WindowSize.Width -gt 0) {
                Clear-Host
            }
            Show-CFEInfo -OutputDir $OutputDir

            # Health line
            try {
                $cpu  = (Get-CimInstance Win32_Processor -ErrorAction Stop |
                         Measure-Object -Property LoadPercentage -Average).Average
                $mem  = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
                $mPct = [math]::Round(
                    ($mem.TotalVisibleMemorySize - $mem.FreePhysicalMemory) /
                     $mem.TotalVisibleMemorySize * 100, 0)
                $disk = Get-PSDrive C -ErrorAction Stop
                $dfGB = [math]::Round($disk.Free / 1GB, 1)
                _CFE_Write ("  System   CPU {0,3}%   Mem {1,3}%   Disk free {2} GB" -f
                            $cpu, $mPct, $dfGB) DarkGray
            } catch {
                # WMI unavailable -- skip
            }

            # Check if the background process is still alive
            if ($ProcessId -gt 0) {
                $proc = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
                if (-not $proc) {
                    _CFE_Write "`n  Engine process exited." Yellow
                    break
                }
            }

            # Check STOP file
            if (Test-Path $stopFile) {
                _CFE_Write "`n  STOP file found -- engine will halt after this cycle." Yellow
                break
            }

            Start-Sleep -Seconds $IntervalSec
        }
    } catch {
        # Ctrl+C in the watch loop -- engine keeps running
        _CFE_Write "`n  Watch stopped. Engine continues in background." Cyan
    }
}


# -- Main exported function ----------------------------------------------------
function Start-CFE {
    <#
    .SYNOPSIS
        Single-call loader for the Continual Formation Engine.

    .DESCRIPTION
        Pre-flight → spinner → launch orchestrator → live monitor.
        Everything wired up in one call.
    #>
    param(
        [double]$Hours      = 24,
        [string]$OutputDir  = "continual_results",
        [int]   $BatchSize  = 30,
        [int]   $Seeds      = 3,
        [switch]$Resume,
        [switch]$Info,
        [switch]$Watch,
        [int]   $WatchInterval = 10
    )

    Set-Location $_CFE_Root

    $FullOutputDir = if ([System.IO.Path]::IsPathRooted($OutputDir)) {
        $OutputDir
    } else {
        Join-Path $_CFE_Root $OutputDir
    }

    # -- Info-only mode --------------------------------------------------------
    if ($Info) {
        Show-CFEInfo -OutputDir $FullOutputDir
        return
    }

    # -- Banner -----------------------------------------------------------------
    _CFE_Write "" White
    _CFE_Write "  +---------------------------------------------------------+" Cyan
    _CFE_Write "  |  Continual Formation Engine  --  Start-CFE               |" Cyan
    _CFE_Write "  |  Atomistic structure discovery, unattended               |" Cyan
    _CFE_Write "  +---------------------------------------------------------+" Cyan
    _CFE_Write "" White
    _CFE_Write ("  Hours: {0}   Batch: {1}   Seeds: {2}   Dir: {3}" -f
                $Hours, $BatchSize, $Seeds, $FullOutputDir) DarkGray
    _CFE_Write "" White

    # -- Pre-flight checks with spinner ----------------------------------------
    Invoke-CFESpinner -Message "Pre-flight checks" -Seconds 1

    $ok = Test-CFEPreFlight -OutputDir $FullOutputDir
    if (-not $ok) {
        _CFE_Write "  Pre-flight failed. Fix the issues above and retry." Red
        return
    }
    _CFE_Write "" White

    # -- Resume info ------------------------------------------------------------
    $ckptFile = Join-Path $FullOutputDir "checkpoint.json"
    if ($Resume -and (Test-Path $ckptFile)) {
        $ckpt = Get-Content $ckptFile | ConvertFrom-Json
        _CFE_Write ("  Resuming session {0}  (cycle {1},  {2} completions)" -f
                    $ckpt.session_id, $ckpt.cycle, $ckpt.total_completed) Yellow
        _CFE_Write "" White
    }

    # -- Build orchestrator args ------------------------------------------------
    $pyArgs = @(
        $script:_CFE_Orch,
        "--out",        $FullOutputDir,
        "--hours",      $Hours,
        "--batch-size", $BatchSize,
        "--seeds",      $Seeds,
        "--runner",     $script:_CFE_Runner
    )
    if ($Resume) { $pyArgs += "--resume" }

    # -- Launch spinner ---------------------------------------------------------
    Invoke-CFESpinner -Message "Launching formation engine" -Seconds 2

    # -- Start orchestrator as background process -------------------------------
    $stdoutLog = Join-Path $FullOutputDir "stdout.log"
    $stderrLog = Join-Path $FullOutputDir "stderr.log"

    $proc = Start-Process python `
        -ArgumentList $pyArgs `
        -NoNewWindow -PassThru `
        -RedirectStandardOutput $stdoutLog `
        -RedirectStandardError  $stderrLog

    _CFE_Write ("  Engine started  (PID {0})" -f $proc.Id) Green
    _CFE_Write ("  Logs → {0}" -f $stdoutLog) DarkGray
    _CFE_Write ("  Stop  → New-Item '{0}'" -f (Join-Path $FullOutputDir "STOP")) DarkGray
    _CFE_Write "" White

    # Give the first cycle a moment to produce initial ledger entries
    Invoke-CFESpinner -Message "Waiting for first cycle results" -Seconds 6

    # -- Wire into live monitor -------------------------------------------------
    if ($Watch -or $true) {
        # Always drop into live monitor -- user can Ctrl+C to exit watch
        # without killing the engine (it runs as a separate process).
        Start-CFEMonitor `
            -OutputDir    $FullOutputDir `
            -IntervalSec  $WatchInterval `
            -ProcessId    $proc.Id
    }
}


# -- Module export notice ------------------------------------------------------
_CFE_Write "  CFE loaded.  Call  Start-CFE  to begin." Cyan
_CFE_Write "  Quick info:  Start-CFE -Info" DarkGray
_CFE_Write "  Resume:      Start-CFE -Resume" DarkGray
_CFE_Write "" White
