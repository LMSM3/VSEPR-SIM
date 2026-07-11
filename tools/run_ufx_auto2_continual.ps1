# tools/run_ufx_auto2_continual.ps1
# UFX_AUTO_2 Phase 10 -- Continual Run Harness
# VSEPR-SIM v5 beta9+
#
# Purpose:
#   Runs the full UFX_AUTO_2 Phase 6-10 generation pipeline repeatedly until
#   the SQLite database reaches TargetBytes (WAL + SHM sidecar files included).
#
# Cycle order (Phase 10 full pipeline):
#   randomfill -> fill-molecular -> fill-thermo -> fill-crystal
#   -> fill-transport -> fill-mechanical -> validate -> [validate-web]
#   -> score -> promote -> audit
#   -> optional snapshot -> check DB size -> sleep -> repeat
#
# Exit condition:  database size >= TargetBytes
#
# Usage:
#   .\tools\run_ufx_auto2_continual.ps1 -TargetBytes 100MB -BatchSize 500 -ValidateBatch 100 -SleepSeconds 2
#   .\tools\run_ufx_auto2_continual.ps1 -TargetBytes 10GB  -BatchSize 2000 -ValidateBatch 500 -SleepSeconds 5

param(
	[string]$Root = "C:\R\VSPER-SIM",
	[string]$Exe  = "C:\R\VSPER-SIM\build\vsepr-ufx.exe",
	[string]$Db   = "C:\R\VSPER-SIM\runs\ufx_auto2\ufx_auto2.sqlite",

	[int]$BatchSize     = 500,
	[int]$ValidateBatch = 100,
	[double]$MinScore   = 0.92,
	[int]$SleepSeconds  = 5,

	[long]$TargetBytes    = 10GB,
	[long]$MinFreeBytes   = 20GB,

	[int]$SnapshotEveryCycles = 25,
	[long]$SnapshotEveryBytes = 512MB,

	[switch]$WebValidate,
	[int]$WebBatch = 25,

	[switch]$EmergencyStopOnFail
)

$ErrorActionPreference = "Stop"

$RunRoot      = Join-Path $Root "runs\ufx_auto2"
$SnapshotRoot = Join-Path $RunRoot "snapshots"
$Summary      = Join-Path $RunRoot "summary.csv"
$LockFile     = Join-Path $RunRoot ".ufx_auto2_continual.lock"
$WebCacheDir  = Join-Path $RunRoot "web_cache"

New-Item -ItemType Directory -Force -Path $RunRoot      | Out-Null
New-Item -ItemType Directory -Force -Path $SnapshotRoot | Out-Null
New-Item -ItemType Directory -Force -Path $WebCacheDir  | Out-Null

# ─────────────────────────────────────────────────────────────
# Preflight checks
# ─────────────────────────────────────────────────────────────

if (!(Test-Path $Exe)) {
	Write-Host "[UFX_AUTO_2] Missing executable: $Exe" -ForegroundColor Red
	exit 1
}

# ─────────────────────────────────────────────────────────────
# Lockfile guard — prevent duplicate runs
# ─────────────────────────────────────────────────────────────

if (Test-Path $LockFile) {
	$OldPid = Get-Content $LockFile -ErrorAction SilentlyContinue
	if ($OldPid -and (Get-Process -Id ([int]$OldPid) -ErrorAction SilentlyContinue)) {
		Write-Host "[UFX_AUTO_2] Already running as PID $OldPid" -ForegroundColor Red
		exit 1
	}
	Write-Host "[UFX_AUTO_2] Stale lockfile found (PID $OldPid not running). Removing." -ForegroundColor Yellow
	Remove-Item $LockFile -Force
}

"$PID" | Out-File $LockFile -Encoding ascii

# ─────────────────────────────────────────────────────────────
# Cleanup
# ─────────────────────────────────────────────────────────────

function Cleanup {
	Remove-Item $LockFile -Force -ErrorAction SilentlyContinue
}

# ─────────────────────────────────────────────────────────────
# SafeField — strip characters that break CSV
# ─────────────────────────────────────────────────────────────

function SafeField($s) {
	if ($null -eq $s) { return "" }
	return ($s.ToString() `
		-replace "`r", " " `
		-replace "`n", " " `
		-replace ",",  ";" `
		-replace '"',  "'").Trim()
}

# ─────────────────────────────────────────────────────────────
# Invoke-ToLog — run vsepr.exe command, redirect stdout+stderr to log
# Returns exit code.
# ─────────────────────────────────────────────────────────────

function Invoke-ToLog {
	param(
		[string]$ExePath,
		[string[]]$Arguments,
		[string]$LogPath
	)

	& $ExePath @Arguments > $LogPath 2>&1
	return $LASTEXITCODE
}

# ─────────────────────────────────────────────────────────────
# Get-DbBytes — WAL-aware total database size
# Counts: .sqlite + .sqlite-wal + .sqlite-shm
# ─────────────────────────────────────────────────────────────

function Get-DbBytes {
	$total = 0
	foreach ($path in @($Db, "$Db-wal", "$Db-shm")) {
		if (Test-Path $path) {
			$total += (Get-Item $path).Length
		}
	}
	return $total
}

# ─────────────────────────────────────────────────────────────
# Get-FreeBytes — free space on the drive containing $RunRoot
# ─────────────────────────────────────────────────────────────

function Get-FreeBytes {
	$drive = (Get-Item $RunRoot).PSDrive
	return $drive.Free
}

# ─────────────────────────────────────────────────────────────
# Write-SizeBar — progress indicator
# ─────────────────────────────────────────────────────────────

function Write-SizeBar {
	param([long]$Current, [long]$Target)

	$pct = [math]::Min(100, [int](($Current / $Target) * 100))
	$gb  = "{0:F3}" -f ($Current / 1GB)
	$tgb = "{0:F2}" -f ($Target  / 1GB)

	Write-Host ("[UFX_AUTO_2] DB size: {0} GB / {1} GB  ({2}%)" `
		-f $gb, $tgb, $pct) -ForegroundColor DarkCyan
}

# ─────────────────────────────────────────────────────────────
# Main loop
# ─────────────────────────────────────────────────────────────

try {
	# Write summary header if file does not exist
	if (!(Test-Path $Summary)) {
		"cycle,run_id,timestamp,status,randomfill_status,fill_molecular_status,fill_thermo_status,fill_crystal_status,fill_transport_status,fill_mechanical_status,validate_status,web_validate_status,score_status,promote_status,audit_status,elapsed_seconds,db_bytes,notes" |
			Out-File $Summary -Encoding utf8
	}

	# Auto-init DB if missing
	if (!(Test-Path $Db)) {
		Write-Host "[UFX_AUTO_2] DB missing; initializing..." -ForegroundColor Yellow

		$InitStatus = Invoke-ToLog `
			-ExePath $Exe `
			-Arguments @("ufx","auto2","init","--db","$Db") `
			-LogPath (Join-Path $RunRoot "init.log")

		if ($InitStatus -ne 0) {
			throw "auto2 init failed with exit code $InitStatus"
		}

		Write-Host "[UFX_AUTO_2] Database initialized." -ForegroundColor Green
	}

	$cycle             = 0
	$LastSnapshotBytes = Get-DbBytes

	Write-Host ("[UFX_AUTO_2] Starting continual run. Target: {0:F2} GB" `
		-f ($TargetBytes / 1GB)) -ForegroundColor Cyan

	while ((Get-DbBytes) -lt $TargetBytes) {

		# Disk-space guardrail
		if ((Get-FreeBytes) -lt $MinFreeBytes) {
			throw ("Free disk space below safety threshold ({0:F1} GB required)" `
				-f ($MinFreeBytes / 1GB))
		}

		$cycle++
		$Start  = Get-Date
		$RunId  = "run_{0}_{1:D6}" -f $Start.ToString("yyyyMMdd_HHmmss"), $cycle
		$RunDir = Join-Path $RunRoot $RunId

		New-Item -ItemType Directory -Force -Path $RunDir | Out-Null

		$RandomLog          = Join-Path $RunDir "randomfill.log"
		$FillMolLog         = Join-Path $RunDir "fill_molecular.log"
		$FillThermoLog      = Join-Path $RunDir "fill_thermo.log"
		$FillCrystalLog     = Join-Path $RunDir "fill_crystal.log"
		$FillTransportLog   = Join-Path $RunDir "fill_transport.log"
		$FillMechanicalLog  = Join-Path $RunDir "fill_mechanical.log"
		$ValidateLog        = Join-Path $RunDir "validate.log"
		$WebValidateLog     = Join-Path $RunDir "validate_web.log"
		$ScoreLog           = Join-Path $RunDir "score.log"
		$PromoteLog         = Join-Path $RunDir "promote.log"
		$AuditLog           = Join-Path $RunDir "audit.log"

		$Status              = "PASS"
		$Notes               = "ok"

		$RandomStatus        = 0
		$FillMolStatus       = 0
		$FillThermoStatus    = 0
		$FillCrystalStatus   = 0
		$FillTransportStatus = 0
		$FillMechStatus      = 0
		$ValidateStatus      = 0
		$WebValStatus        = 0
		$ScoreStatus         = 0
		$PromoteStatus       = 0
		$AuditStatus         = 0

		Write-Host ""
		Write-Host "[UFX_AUTO_2] Cycle $cycle  $RunId" -ForegroundColor Cyan
		Write-SizeBar (Get-DbBytes) $TargetBytes

		try {
			# ── 1. randomfill ────────────────────────────────────────────────
			$RandomStatus = Invoke-ToLog `
				-ExePath $Exe `
				-Arguments @("ufx","auto2","randomfill",
							 "--count", "$BatchSize",
							 "--db",    "$Db") `
				-LogPath $RandomLog

			if ($RandomStatus -ne 0) { throw "randomfill failed: exit $RandomStatus" }

			# ── 2. fill-molecular ────────────────────────────────────────────
			$FillMolStatus = Invoke-ToLog `
				-ExePath $Exe `
				-Arguments @("ufx","auto2","fill-molecular",
							 "--batch", "$BatchSize",
							 "--db",    "$Db") `
				-LogPath $FillMolLog

			if ($FillMolStatus -ne 0) { throw "fill-molecular failed: exit $FillMolStatus" }

			# ── 3. fill-thermo ───────────────────────────────────────────────
			$FillThermoStatus = Invoke-ToLog `
				-ExePath $Exe `
				-Arguments @("ufx","auto2","fill-thermo",
							 "--batch", "$BatchSize",
							 "--db",    "$Db") `
				-LogPath $FillThermoLog

			if ($FillThermoStatus -ne 0) { throw "fill-thermo failed: exit $FillThermoStatus" }

			# ── 4. fill-crystal ──────────────────────────────────────────────
			$FillCrystalStatus = Invoke-ToLog `
				-ExePath $Exe `
				-Arguments @("ufx","auto2","fill-crystal",
							 "--batch", "$BatchSize",
							 "--db",    "$Db") `
				-LogPath $FillCrystalLog

			if ($FillCrystalStatus -ne 0) { throw "fill-crystal failed: exit $FillCrystalStatus" }

			# ── 5. fill-transport ────────────────────────────────────────────
			$FillTransportStatus = Invoke-ToLog `
				-ExePath $Exe `
				-Arguments @("ufx","auto2","fill-transport",
							 "--batch", "$BatchSize",
							 "--db",    "$Db") `
				-LogPath $FillTransportLog

			if ($FillTransportStatus -ne 0) { throw "fill-transport failed: exit $FillTransportStatus" }

			# ── 6. fill-mechanical ───────────────────────────────────────────
			$FillMechStatus = Invoke-ToLog `
				-ExePath $Exe `
				-Arguments @("ufx","auto2","fill-mechanical",
							 "--batch", "$BatchSize",
							 "--db",    "$Db") `
				-LogPath $FillMechanicalLog

			if ($FillMechStatus -ne 0) { throw "fill-mechanical failed: exit $FillMechStatus" }

			# ── 7. validate (local) ──────────────────────────────────────────
			$ValidateStatus = Invoke-ToLog `
				-ExePath $Exe `
				-Arguments @("ufx","auto2","validate",
							 "--batch", "$ValidateBatch",
							 "--db",    "$Db") `
				-LogPath $ValidateLog

			if ($ValidateStatus -ne 0) { throw "validate failed: exit $ValidateStatus" }

			# ── 8. validate-web (optional) ───────────────────────────────────
			if ($WebValidate) {
				$WebValStatus = Invoke-ToLog `
					-ExePath $Exe `
					-Arguments @("ufx","auto2","validate-web",
								 "--batch", "$WebBatch",
								 "--db",    "$Db",
								 "--cache", "$WebCacheDir") `
					-LogPath $WebValidateLog

				if ($WebValStatus -ne 0) {
					# Web validate failure is non-fatal (external data availability != model error)
					Write-Host "[UFX_AUTO_2] validate-web returned $WebValStatus (non-fatal)" `
						-ForegroundColor Yellow
					$WebValStatus = 0
				}
			}

			# ── 9. score ─────────────────────────────────────────────────────
			# Runs after validate so scores account for validation outcomes.
			$ScoreStatus = Invoke-ToLog `
				-ExePath $Exe `
				-Arguments @("ufx","auto2","score",
							 "--batch", "$BatchSize",
							 "--db",    "$Db") `
				-LogPath $ScoreLog

			if ($ScoreStatus -ne 0) { throw "score failed: exit $ScoreStatus" }

			# ── 10. promote ──────────────────────────────────────────────────
			$PromoteStatus = Invoke-ToLog `
				-ExePath $Exe `
				-Arguments @("ufx","auto2","promote",
							 "--min-score", "$MinScore",
							 "--db",        "$Db") `
				-LogPath $PromoteLog

			if ($PromoteStatus -ne 0) { throw "promote failed: exit $PromoteStatus" }

			# ── 11. audit ────────────────────────────────────────────────────
			$AuditStatus = Invoke-ToLog `
				-ExePath $Exe `
				-Arguments @("ufx","auto2","audit",
							 "--db", "$Db") `
				-LogPath $AuditLog

			if ($AuditStatus -ne 0) { throw "audit failed: exit $AuditStatus" }

			# Print last 20 lines of audit to console
			Get-Content $AuditLog | Select-Object -Last 20
		}
		catch {
			$Status = "FAIL"
			$Notes  = SafeField $_.Exception.Message
			Write-Host "[UFX_AUTO_2] FAIL: $Notes" -ForegroundColor Red
		}

		# ── Snapshot logic ──────────────────────────────────────────────────
		$DbBytes = Get-DbBytes

		$ShouldSnapshot =
			($cycle % $SnapshotEveryCycles -eq 0) -or
			(($DbBytes - $LastSnapshotBytes) -ge $SnapshotEveryBytes) -or
			($DbBytes -ge $TargetBytes)

		if ($ShouldSnapshot -and (Test-Path $Db)) {
			$SnapName = "ufx_auto2_{0}_{1:D6}.sqlite" `
				-f $Start.ToString("yyyyMMdd_HHmmss"), $cycle
			$SnapPath = Join-Path $SnapshotRoot $SnapName

			Copy-Item $Db $SnapPath -Force
			Write-Host "[UFX_AUTO_2] Snapshot: $SnapName" -ForegroundColor DarkGray
			$LastSnapshotBytes = $DbBytes
		}

		# ── Write summary row ───────────────────────────────────────────────
		$End     = Get-Date
		$Elapsed = [int]($End - $Start).TotalSeconds

		"$cycle,$RunId,$($Start.ToString("yyyy-MM-dd HH:mm:ss")),$Status,$RandomStatus,$FillMolStatus,$FillThermoStatus,$FillCrystalStatus,$FillTransportStatus,$FillMechStatus,$ValidateStatus,$WebValStatus,$ScoreStatus,$PromoteStatus,$AuditStatus,$Elapsed,$DbBytes,$(SafeField $Notes)" |
			Out-File $Summary -Append -Encoding utf8

		# ── Emergency stop ──────────────────────────────────────────────────
		if (($Status -eq "FAIL") -and $EmergencyStopOnFail) {
			Write-Host "[UFX_AUTO_2] EmergencyStopOnFail triggered. Halting." -ForegroundColor Red
			break
		}

		# ── Sleep between cycles (skip on last cycle) ────────────────────────
		if ((Get-DbBytes) -lt $TargetBytes) {
			Start-Sleep -Seconds $SleepSeconds
		}
	}

	# ─────────────────────────────────────────────────────────
	# Final report
	# ─────────────────────────────────────────────────────────

	$FinalBytes = Get-DbBytes
	Write-Host ""
	Write-Host ("[UFX_AUTO_2] Done. Final DB: {0:F3} GB after {1} cycle(s)." `
		-f ($FinalBytes / 1GB), $cycle) -ForegroundColor Green

	Write-Host "[UFX_AUTO_2] Last 20 summary rows:" -ForegroundColor DarkGray
	Import-Csv $Summary | Select-Object -Last 20 | Format-Table -AutoSize
}
finally {
	Cleanup
}
