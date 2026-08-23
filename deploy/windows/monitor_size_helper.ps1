# monitor_size_helper.ps1
# =============================================================================
# 72-floating-13  |  VSEPR-SIM V5.1.4
#
# Monitor / window sizing helper.
#
# Reports connected monitor geometry and recommends viewer window placement for
# VSEPR-SIM.  Can be called by the C++ launcher via PowerShell IPC, or run
# standalone for diagnostics.
#
# Output (stdout, one line per key):
#   MONITOR_COUNT   <N>
#   PRIMARY_W       <px>
#   PRIMARY_H       <px>
#   SECONDARY_W     <px>        (only if a second monitor exists)
#   SECONDARY_H     <px>
#   VIEWER_X        <px>        recommended viewer left edge
#   VIEWER_Y        <px>        recommended viewer top edge
#   VIEWER_W        <px>        recommended viewer width  (default 800)
#   VIEWER_H        <px>        recommended viewer height (default 1200)
#   PLACEMENT       <string>    primary | secondary | primary_right_half
#
# Placement rules:
#   2+ monitors  -> place on secondary, centred
#   1 wide (>=2560 px) -> place on right half of primary
#   otherwise    -> centre on primary
# =============================================================================
param(
	[int]$DefaultW = 800,
	[int]$DefaultH = 1200
)

Add-Type -AssemblyName System.Windows.Forms

$screens = [System.Windows.Forms.Screen]::AllScreens
$primary = [System.Windows.Forms.Screen]::PrimaryScreen
$count   = $screens.Count
$primW   = $primary.Bounds.Width
$primH   = $primary.Bounds.Height

Write-Output "MONITOR_COUNT   $count"
Write-Output "PRIMARY_W       $primW"
Write-Output "PRIMARY_H       $primH"

$secW = 0; $secH = 0; $secX = 0; $secY = 0

foreach ($s in $screens) {
	if (-not $s.Primary) {
		$secW = $s.Bounds.Width
		$secH = $s.Bounds.Height
		$secX = $s.Bounds.X
		$secY = $s.Bounds.Y
		Write-Output "SECONDARY_W     $secW"
		Write-Output "SECONDARY_H     $secH"
		break
	}
}

$vx = 0; $vy = 0; $vw = $DefaultW; $vh = $DefaultH; $placement = "primary"

if ($count -ge 2 -and $secW -gt 0) {
	$vx = $secX + [int](($secW - $vw) / 2)
	$vy = $secY + [int](($secH - $vh) / 2)
	if ($vy -lt $secY) { $vy = $secY }
	if ($vh -gt $secH) { $vh = $secH }
	$placement = "secondary"
} elseif ($primW -ge 2560) {
	$vx = $primW - $vw - 20
	$vy = [int](($primH - $vh) / 2)
	if ($vy -lt 0) { $vy = 0 }
	$placement = "primary_right_half"
} else {
	$vx = [int](($primW - $vw) / 2)
	$vy = [int](($primH - $vh) / 2)
	if ($vy -lt 0) { $vy = 0 }
}

Write-Output "VIEWER_X        $vx"
Write-Output "VIEWER_Y        $vy"
Write-Output "VIEWER_W        $vw"
Write-Output "VIEWER_H        $vh"
Write-Output "PLACEMENT       $placement"
