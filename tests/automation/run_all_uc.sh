#!/usr/bin/env bash
# =============================================================================
# run_all_uc.sh  --  VSEPR-SIM Use-Case Automation Master Harness
# =============================================================================
# VSEPR-SIM | v5.0.14 | WO-MF-01
#
# Runs all workflow use-case automation scripts in sequence and produces:
#   1. Live console output (per UC)
#   2. A human-readable text summary  (tests/automation/reports/uc_report.txt)
#   3. A machine-readable JSON report (tests/automation/reports/uc_report.json)
#
# The JSON report is suitable for CI dashboards, pandas, or jq pipelines.
#
# Exit code:
#   0  — all UCs completed with FAIL=0 (PENDING is not a failure)
#   1  — one or more UCs had FAIL > 0
#
# Usage:
#   bash tests/automation/run_all_uc.sh [OPTIONS]
#
# Options:
#   --vsepr PATH     Path to vsepr binary  (default: build/vsepr.exe)
#   --only UC        Run only one UC: uc1 | uc2 | uc3 | uc6 | uc7
#   --no-report      Skip writing the report files
#   --help
# =============================================================================

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# --------------- defaults -----------------------------------------------------
VSEPR="${VSEPR:-$REPO_ROOT/build/vsepr.exe}"
REPORT_DIR="$SCRIPT_DIR/reports"
REPORT_TXT="$REPORT_DIR/uc_report.txt"
REPORT_JSON="$REPORT_DIR/uc_report.json"
RUN_UC1=true; RUN_UC2=true; RUN_UC3=true; RUN_UC6=true; RUN_UC7=true
WRITE_REPORT=true
START_TS=$(date -u +%Y-%m-%dT%H:%M:%SZ)

# --------------- arg parse ----------------------------------------------------
while [[ $# -gt 0 ]]; do
	case $1 in
		--vsepr)
			VSEPR="$2"; shift 2 ;;
		--only)
			RUN_UC1=false; RUN_UC2=false; RUN_UC3=false; RUN_UC6=false; RUN_UC7=false
			case $2 in
				uc1) RUN_UC1=true ;;
				uc2) RUN_UC2=true ;;
				uc3) RUN_UC3=true ;;
				uc6) RUN_UC6=true ;;
				uc7) RUN_UC7=true ;;
				*) echo "Unknown UC: $2 (use uc1|uc2|uc3|uc6|uc7)"; exit 1 ;;
			esac
			shift 2 ;;
		--no-report)
			WRITE_REPORT=false; shift ;;
		--help|-h)
			sed -n '/^# Usage/,/^#---/p' "$0" | sed 's/^# \?//'
			exit 0 ;;
		*) echo "Unknown arg: $1"; exit 1 ;;
	esac
done

# --------------- helpers ------------------------------------------------------
_stamp() { date '+%H:%M:%S'; }
_now()   { date -u +%Y-%m-%dT%H:%M:%SZ; }
_banner() {
	echo ""
	echo "################################################################"
	echo "  $1"
	echo "################################################################"
}

# Run one UC script and capture its exit code + output
# run_uc LABEL SCRIPT_PATH
# Sets global: UC_PASS, UC_FAIL, UC_PENDING, UC_STATUS, UC_LOG
run_uc() {
	local label="$1" script="$2"
	local logfile; logfile=$(mktemp)
	local uc_start; uc_start=$(_now)

	_banner "$label"

	set +e
	bash "$script" --vsepr "$VSEPR" 2>&1 | tee "$logfile"
	local exit_code=${PIPESTATUS[0]}
	set -e

	# Parse counters from the script's own summary block
	UC_PASS=$(grep -oP 'PASS:\s+\K[0-9]+' "$logfile" | tail -1 || echo 0)
	UC_FAIL=$(grep -oP 'FAIL:\s+\K[0-9]+' "$logfile" | tail -1 || echo 0)
	UC_PENDING=$(grep -oP 'PENDING:\s+\K[0-9]+' "$logfile" | tail -1 || echo 0)
	UC_LOG=$(cat "$logfile")
	UC_STATUS=$( [[ $UC_FAIL -eq 0 ]] && echo "OK" || echo "FAILED" )
	UC_END=$(_now)

	rm -f "$logfile"

	echo ""
	echo "  --> $label: $UC_STATUS  (PASS=$UC_PASS  PENDING=$UC_PENDING  FAIL=$UC_FAIL)"
}

# --------------- main ---------------------------------------------------------
mkdir -p "$REPORT_DIR"

# Accumulators
TOTAL_PASS=0; TOTAL_FAIL=0; TOTAL_PENDING=0
declare -a UC_RESULTS_JSON=()

_banner "VSEPR-SIM Use-Case Automation Harness  [$(_stamp)]"
echo "  vsepr binary:  $VSEPR"
echo "  report dir:    $REPORT_DIR"
echo "  UCs:           $(${RUN_UC1} && echo UC-1 || true) $(${RUN_UC2} && echo UC-2 || true) $(${RUN_UC3} && echo UC-3 || true) $(${RUN_UC6} && echo UC-6 || true) $(${RUN_UC7} && echo UC-7 || true)"
echo ""

# Ensure vsepr exists
if [[ ! -f "$VSEPR" ]]; then
	echo "[ERROR] vsepr binary not found: $VSEPR"
	echo "        Build with: cmake --preset release && ninja -C build vsepr"
	exit 1
fi

# ---- UC-1 ----
if $RUN_UC1; then
	run_uc "UC-1: Desktop Script-to-Results" "$SCRIPT_DIR/uc1_run.sh"
	TOTAL_PASS=$((TOTAL_PASS + UC_PASS))
	TOTAL_FAIL=$((TOTAL_FAIL + UC_FAIL))
	TOTAL_PENDING=$((TOTAL_PENDING + UC_PENDING))
	UC_RESULTS_JSON+=("$(cat <<JSONEOF
  {
	"id": "UC-1",
	"name": "Desktop Script-to-Results",
	"status": "$UC_STATUS",
	"pass": $UC_PASS,
	"fail": $UC_FAIL,
	"pending": $UC_PENDING,
	"started": "$uc_start",
	"finished": "$UC_END"
  }
JSONEOF
)")
fi

# ---- UC-2 ----
if $RUN_UC2; then
	run_uc "UC-2: Batch Crystal Sweep" "$SCRIPT_DIR/uc2_run.sh"
	TOTAL_PASS=$((TOTAL_PASS + UC_PASS))
	TOTAL_FAIL=$((TOTAL_FAIL + UC_FAIL))
	TOTAL_PENDING=$((TOTAL_PENDING + UC_PENDING))
	UC_RESULTS_JSON+=("$(cat <<JSONEOF
  {
	"id": "UC-2",
	"name": "Batch Crystal Sweep",
	"status": "$UC_STATUS",
	"pass": $UC_PASS,
	"fail": $UC_FAIL,
	"pending": $UC_PENDING,
	"started": "$uc_start",
	"finished": "$UC_END"
  }
JSONEOF
)")
fi

# ---- UC-3 ----
if $RUN_UC3; then
	run_uc "UC-3: Isomer Discovery Pipeline" "$SCRIPT_DIR/uc3_run.sh"
	TOTAL_PASS=$((TOTAL_PASS + UC_PASS))
	TOTAL_FAIL=$((TOTAL_FAIL + UC_FAIL))
	TOTAL_PENDING=$((TOTAL_PENDING + UC_PENDING))
	UC_RESULTS_JSON+=("$(cat <<JSONEOF
  {
	"id": "UC-3",
	"name": "Isomer Discovery Pipeline",
	"status": "$UC_STATUS",
	"pass": $UC_PASS,
	"fail": $UC_FAIL,
	"pending": $UC_PENDING,
	"started": "$uc_start",
	"finished": "$UC_END"
  }
JSONEOF
)")
fi

# ---- UC-6 ----
if $RUN_UC6; then
	run_uc "UC-6: Crystallographic Regression Suite" "$SCRIPT_DIR/uc6_run.sh"
	TOTAL_PASS=$((TOTAL_PASS + UC_PASS))
	TOTAL_FAIL=$((TOTAL_FAIL + UC_FAIL))
	TOTAL_PENDING=$((TOTAL_PENDING + UC_PENDING))
	UC_RESULTS_JSON+=("$(cat <<JSONEOF
  {
	"id": "UC-6",
	"name": "Crystallographic Regression Suite",
	"status": "$UC_STATUS",
	"pass": $UC_PASS,
	"fail": $UC_FAIL,
	"pending": $UC_PENDING,
	"started": "$uc_start",
	"finished": "$UC_END"
  }
JSONEOF
)")
fi

# ---- UC-7 ----
if $RUN_UC7; then
	run_uc "UC-7: SiO2 Pipe Simulation" "$SCRIPT_DIR/uc7_run.sh"
	TOTAL_PASS=$((TOTAL_PASS + UC_PASS))
	TOTAL_FAIL=$((TOTAL_FAIL + UC_FAIL))
	TOTAL_PENDING=$((TOTAL_PENDING + UC_PENDING))
	UC_RESULTS_JSON+=("$(cat <<JSONEOF
  {
	"id": "UC-7",
	"name": "SiO2 Pipe Simulation",
	"status": "$UC_STATUS",
	"pass": $UC_PASS,
	"fail": $UC_FAIL,
	"pending": $UC_PENDING,
	"started": "$uc_start",
	"finished": "$UC_END"
  }
JSONEOF
)")
fi

# --------------- text summary -------------------------------------------------
END_TS=$(_now)
OVERALL=$( [[ $TOTAL_FAIL -eq 0 ]] && echo "OK" || echo "FAILED" )

TXT_REPORT=$(cat <<TXTEOF
================================================================
 VSEPR-SIM UC Automation Report
 Started:  $START_TS
 Finished: $END_TS
 Binary:   $VSEPR
================================================================

 TOTAL PASS:    $TOTAL_PASS
 TOTAL PENDING: $TOTAL_PENDING  (expected — see docs/wo/WO-MF-01-missing-features.md)
 TOTAL FAIL:    $TOTAL_FAIL
 OVERALL:       $OVERALL

 PENDING reminder:
   A PENDING result means the feature is in WO-MF-01 and is not
   yet implemented.  It is NOT a test failure.  To convert a
   PENDING to a PASS: implement the feature, swap expect_pending
   to expect_file in the corresponding uc*_run.sh script, and
   update WO-MF-01-missing-features.md.

================================================================
TXTEOF
)

echo ""
echo "$TXT_REPORT"

# --------------- write reports ------------------------------------------------
if $WRITE_REPORT; then
	echo "$TXT_REPORT" > "$REPORT_TXT"
	echo "  Report (txt):  $REPORT_TXT"

	# Build JSON report
	IFS=$'\n'
	UC_JSON_ARR=$(printf '%s,\n' "${UC_RESULTS_JSON[@]}")
	UC_JSON_ARR="${UC_JSON_ARR%,}"   # trim trailing comma
	unset IFS

	cat > "$REPORT_JSON" <<JSONEOF
{
  "schema": "vsepr-uc-report-v1",
  "started": "$START_TS",
  "finished": "$END_TS",
  "binary": "$VSEPR",
  "overall": "$OVERALL",
  "totals": {
	"pass": $TOTAL_PASS,
	"fail": $TOTAL_FAIL,
	"pending": $TOTAL_PENDING
  },
  "use_cases": [
$UC_JSON_ARR
  ],
  "ledger": "docs/wo/WO-MF-01-missing-features.md"
}
JSONEOF
	echo "  Report (json): $REPORT_JSON"
fi

echo ""
[[ $TOTAL_FAIL -eq 0 ]] && echo "ALL UCs: OK" || echo "HARNESS: FAILED ($TOTAL_FAIL failures)"
exit "$TOTAL_FAIL"
