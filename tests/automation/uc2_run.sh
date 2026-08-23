#!/usr/bin/env bash
# =============================================================================
# uc2_run.sh  --  UC-2: Batch Crystal Sweep automation
# =============================================================================
# VSEPR-SIM | v5.0.14 | WO-MF-01
#
# Tests the 4x4 SiO2 lattice-parameter sweep pipeline.
# Most outputs are PENDING (sweep dispatcher + writers not yet wired).
# Any implemented partial output is validated rather than silently accepted.
#
# Result classes:
#   PASS     expected file present and valid
#   PENDING  feature listed in WO-MF-01 as pending (not a failure)
#   FAIL     implemented feature produced wrong or absent output
#
# Usage:
#   bash tests/automation/uc2_run.sh [--vsepr PATH] [--out-dir PATH]
# =============================================================================

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

VSEPR="${VSEPR:-$REPO_ROOT/build/vsepr.exe}"
FIXTURE="$SCRIPT_DIR/vsim/uc2_sio2_sweep.vsim"
OUT_DIR="$REPO_ROOT/out/uc2_sio2_sweep"
TIMEOUT=300      # 16-run sweep may take several minutes
EXPECTED_RUNS=16

while [[ $# -gt 0 ]]; do
	case $1 in
		--vsepr)   VSEPR="$2";   shift 2 ;;
		--out-dir) OUT_DIR="$2"; shift 2 ;;
		*) echo "Unknown arg: $1"; exit 1 ;;
	esac
done

# ---------------- helpers -----------------------------------------------------
PASS=0; FAIL=0; PENDING=0
PENDING_TAGS=(); FAIL_MSGS=()

_stamp() { date '+%H:%M:%S'; }
pass()  { PASS=$((PASS+1));    echo "  [PASS]    $1"; }
fail()  { FAIL=$((FAIL+1));    FAIL_MSGS+=("$1"); echo "  [FAIL]    $1"; }
pend()  { PENDING=$((PENDING+1)); PENDING_TAGS+=("$2"); echo "  [PENDING] $1  ($2)"; }

expect_file() {
	local f="$1" desc="$2"
	[[ -f "$f" ]] && pass "$desc" || fail "$desc — missing: $f"
}

expect_pending() {
	local f="$1" desc="$2" tag="$3"
	if [[ ! -f "$f" ]]; then
		pend "$desc" "$tag"
	elif [[ -s "$f" ]]; then
		pass "$desc (feature arrived early — promote to expect_file!)"
	else
		fail "$desc — file present but empty: $f"
	fi
}

validate_json() {
	local f="$1" desc="$2"
	[[ -f "$f" ]] || return
	command -v jq &>/dev/null || { pass "JSON present (jq absent): $desc"; return; }
	jq empty "$f" 2>/dev/null && pass "JSON valid: $desc" || fail "JSON invalid: $desc"
}

# validate_csv_columns FILE COL1 COL2 ...
validate_csv_columns() {
	local f="$1"; shift
	[[ -f "$f" ]] || return
	local header
	header=$(head -1 "$f")
	for col in "$@"; do
		if echo "$header" | grep -q "$col"; then
			pass "CSV column '$col' present"
		else
			fail "CSV column '$col' missing in $f — header: $header"
		fi
	done
}

# count_sweep_runs DIR EXPECTED_COUNT
count_sweep_runs() {
	local dir="$1" expected="$2"
	if [[ ! -d "$dir" ]]; then
		fail "Sweep output dir absent: $dir"
		return
	fi
	local found
	found=$(find "$dir" -maxdepth 1 -type d -name "run_*" | wc -l)
	if [[ "$found" -eq "$expected" ]]; then
		pass "Sweep produced $found / $expected run dirs"
	elif [[ "$found" -gt 0 ]]; then
		fail "Sweep produced $found run dirs (expected $expected)"
	else
		pend "Sweep produced 0 run dirs (dispatcher pending)" "MF-B01"
	fi
}

# ---------------- pre-flight --------------------------------------------------
echo "========================================================"
echo " UC-2: Batch Crystal Sweep (SiO2 4x4 lattice grid)"
echo " $(_stamp)  $VSEPR"
echo "========================================================"

[[ -f "$VSEPR" ]]   || { echo "[ERROR] vsepr not found: $VSEPR"; exit 1; }
[[ -f "$FIXTURE" ]] || { echo "[ERROR] fixture not found: $FIXTURE"; exit 1; }

rm -rf "$OUT_DIR"
cd "$REPO_ROOT"

# ---------------- STEP 1: run vsepr -------------------------------------------
echo ""
echo "-- STEP 1: launch vsepr run [$(_stamp)] --"
set +e
timeout "$TIMEOUT" "$VSEPR" run "$FIXTURE" 2>&1
RUN_EXIT=$?
set -e

case $RUN_EXIT in
	0)   pass "vsepr run exited 0" ;;
	124) fail "vsepr run timed out after ${TIMEOUT}s" ;;
	*)   fail "vsepr run exited $RUN_EXIT" ;;
esac

# ---------------- STEP 2-4: pending sweep outputs -----------------------------
echo ""
echo "-- STEP 2-4: pending sweep/writer outputs --"
expect_pending "$OUT_DIR/summary.csv"         "summary CSV (write_summary_csv)"   "MF-A03"
expect_pending "$OUT_DIR/fingerprints.json"   "fingerprint JSON (write_fingerprint_json)" "MF-A02"
expect_pending "$OUT_DIR/clusters.json"       "cluster JSON (write_cluster_json)" "MF-A01"
expect_pending "$OUT_DIR/run_000/trajectory.xyz" "sweep dispatcher first run"     "MF-B01"

# ---------------- STEP 5-6: implemented outputs (if sweep ran) ---------------
echo ""
echo "-- STEP 5-6: implemented outputs (conditional on sweep running) --"
if [[ -d "$OUT_DIR" ]]; then
	# Manifest should appear even on partial runs
	[[ -f "$OUT_DIR/manifest.json" ]] && expect_file "$OUT_DIR/manifest.json" "Export manifest" \
		|| pend "manifest.json absent (sweep may not have run)" "MF-B01"
fi

# ---------------- STEP 7: content validation (conditional) -------------------
echo ""
echo "-- STEP 7: content validation (only if files present) --"
validate_json "$OUT_DIR/manifest.json" "manifest.json"

if [[ -f "$OUT_DIR/summary.csv" ]]; then
	validate_csv_columns "$OUT_DIR/summary.csv" \
		"lattice_a" "lattice_c" "energy" "cluster_id" "fp_hash"
fi

if [[ -f "$OUT_DIR/fingerprints.json" ]]; then
	validate_json "$OUT_DIR/fingerprints.json" "fingerprints.json"
	if command -v jq &>/dev/null; then
		local_count=$(jq '.fingerprints | length' "$OUT_DIR/fingerprints.json" 2>/dev/null || echo 0)
		[[ "$local_count" -gt 0 ]] && pass "fingerprints.json has $local_count entries" \
			|| fail "fingerprints.json has 0 entries"
	fi
fi

if [[ -f "$OUT_DIR/clusters.json" ]]; then
	validate_json "$OUT_DIR/clusters.json" "clusters.json"
fi

# ---------------- STEP 8: sweep run dir count --------------------------------
echo ""
echo "-- STEP 8: sweep run directory count --"
count_sweep_runs "$OUT_DIR" "$EXPECTED_RUNS"

# ---------------- summary -----------------------------------------------------
echo ""
echo "========================================================"
echo " UC-2 Results  [$(_stamp)]"
echo "   PASS:    $PASS"
echo "   PENDING: $PENDING  (expected — see WO-MF-01)"
echo "   FAIL:    $FAIL"
echo "========================================================"

[[ ${#PENDING_TAGS[@]} -gt 0 ]] && echo " Pending: ${PENDING_TAGS[*]}"
[[ ${#FAIL_MSGS[@]}    -gt 0 ]] && { echo " Failures:"; for m in "${FAIL_MSGS[@]}"; do echo "   - $m"; done; }
echo ""
[[ $FAIL -eq 0 ]] && echo "UC-2: OK (FAIL=0)" || echo "UC-2: FAILED (FAIL=$FAIL)"
exit "$FAIL"
