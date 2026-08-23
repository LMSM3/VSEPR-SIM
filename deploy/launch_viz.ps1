# launch_viz.ps1
# --------------
# Windows launcher: starts the vsepr viz server in WSL and opens
# both Python viewers in separate windows.
#
# Usage (PowerShell, from the repo root):
#   .\deploy\launch_viz.ps1
#   .\deploy\launch_viz.ps1 -Formula N2 -Args "-T 400 -N 125 --verbose"
#   .\deploy\launch_viz.ps1 -Formula CO2 -Args "--T-start 200 --T-end 800"
#   .\deploy\launch_viz.ps1 -Distro Ubuntu   # target a specific WSL distro
#
# The repo path is auto-detected from this script's location and mapped to
# its /mnt/<drive>/... equivalent inside WSL, so it works from any clone
# location/drive.
#
# The WSL server runs inside Linux (no Device Guard restriction).
# Python viewers run on Windows and connect to localhost:9999 / localhost:10001
# via WSL2 automatic port forwarding.

param(
    [string]$Formula = "Ar",
    [string]$Args    = "-T 300 -N 64 --verbose",
    [string]$Distro  = ""
)

$repoRoot  = Split-Path -Parent $PSScriptRoot

# Convert C:\path\to\repo -> /mnt/c/path/to/repo for use inside WSL.
$driveLetter = $repoRoot.Substring(0,1).ToLower()
$wslPath     = "/mnt/$driveLetter" + ($repoRoot.Substring(2) -replace "\\","/")
$serverSh    = "$wslPath/deploy/start_viz_server.sh"
$binary      = "$wslPath/build-linux/vsepr"

# Only pass -d <Distro> if the caller specified one; otherwise `wsl.exe`
# falls back to the user's configured default distro.
$distroArgs  = @()
$distroLabel = "(default)"
if ($Distro -ne "") {
    $distroArgs  = @("-d", $Distro)
    $distroLabel = $Distro
}

Write-Host ""
Write-Host "  VSEPR-SIM Dual-Port 3D Viz Launcher" -ForegroundColor Cyan
Write-Host "  ─────────────────────────────────────" -ForegroundColor Cyan
Write-Host "  Formula  : $Formula" -ForegroundColor White
Write-Host "  Args     : $Args"    -ForegroundColor White
Write-Host "  WSL      : $distroLabel"  -ForegroundColor White
Write-Host "  Port 9999  -> Atomic View"    -ForegroundColor Green
Write-Host "  Port 10001 -> Analysis View"  -ForegroundColor Yellow
Write-Host ""

# ── 1. Start WSL viz server in a new window ───────────────────────────────────
Write-Host "[1/3] Starting vsepr viz server in WSL..." -ForegroundColor Cyan
$wslArgs = $distroArgs + @("--", "bash", $serverSh, $Formula, $Args)
Start-Process "wsl.exe" -ArgumentList $wslArgs -WindowStyle Normal

# Wait for server to bind ports
Write-Host "      Waiting for ports to open..." -ForegroundColor Gray
$timeout  = 15
$interval = 1
$elapsed  = 0
$port9999Ready = $false
while ($elapsed -lt $timeout) {
    Start-Sleep -Seconds $interval
    $elapsed += $interval
    try {
        $tcp = New-Object System.Net.Sockets.TcpClient
        $tcp.Connect("127.0.0.1", 9999)
        $tcp.Close()
        $port9999Ready = $true
        break
    } catch { }
}

if (-not $port9999Ready) {
    Write-Host "[WARN] Port 9999 not responding after ${timeout}s." -ForegroundColor Yellow
    Write-Host "       The server may still be starting — launching viewers anyway." -ForegroundColor Yellow
} else {
    Write-Host "      Port 9999 ready." -ForegroundColor Green
}

# ── 2. Launch Atomic Viewer (Port 9999) ───────────────────────────────────────
Write-Host "[2/3] Launching Atomic Viewer  (port 9999)..." -ForegroundColor Cyan
Start-Process "python" -ArgumentList "$repoRoot\tools\viz_atomic.py" -WindowStyle Normal

Start-Sleep -Milliseconds 800

# ── 3. Launch Analysis Viewer (Port 10001) ────────────────────────────────────
Write-Host "[3/3] Launching Analysis Viewer (port 10001)..." -ForegroundColor Cyan
Start-Process "python" -ArgumentList "$repoRoot\tools\viz_analysis.py" -WindowStyle Normal

Write-Host ""
Write-Host "  All three processes launched." -ForegroundColor Green
Write-Host "  Close the WSL window to stop the server." -ForegroundColor Gray
Write-Host ""
