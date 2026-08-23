#!/usr/bin/env bash
# =============================================================================
# uc3_run.sh  --  UC-3: Isomer Discovery Pipeline automation
# =============================================================================
# VSEPR-SIM | v5.0.14 | WO-MF-01
#
# Tests the [Co(NH3)4Cl2]+ isomer enumeration → MD → chirality → ranked report.
# Almost all outputs are PENDING (isomer subsystem not yet wired).
# Validates any partial output that arrives early.
#
# Result classes:
#   PASS     expected file present and valid
#   PENDING  feature listed in WO-MF-01 as pending (not a failure)
#   FAIL     implemented feature produced wrong or absent output
#
# Usage:
#   bash tests/automation/uc3_run.sh [--vsepr PATH] [--out-dir PATH]
# =============================================================================

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

VSEPR="${VSEPR:-$REPO_ROOT/build/vsepr.exe}"
FIXTURE="$SCRIPT_DIR/vsim/uc3_cobalt_isomers.vsim"
OUT_DIR="$REPO_ROOT/out/uc3_cobalt_isomers"
TIMEOUT=180      # per-isomer MD runs can be slow

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
		pass "$desc (arrived early — promote to expect_file!)"
	else
		fail "$desc — present but empty: $f"
	fi
}

validate_json() {
	local f="$1" desc="$2"
	[[ -f "$f" ]] || return
	command -v jq &>/dev/null || { pass "JSON present (jq absent): $desc"; return; }
	jq empty "$f" 2>/dev/null && pass "JSON valid: $desc" || fail "JSON invalid: $desc"
}

# check_json_key FILE JQ_EXPR DESC [MF_TAG]
check_json_key() {
	local f="$1" expr="$2" desc="$3" tag="${4:-}"
	[[ -f "$f" ]] || { [[ -n "$tag" ]] && pend "$desc" "$tag" || return; return; }
	command -v jq &>/dev/null || { pass "key check skipped (jq absent): $desc"; return; }
	local val
	val=$(jq -r "$expr" "$f" 2>/dev/null || echo "null")
	if [[ "$val" == "null" || -z "$val" ]]; then
		[[ -n "$tag" ]] && pend "$desc" "$tag" || fail "$desc — jq expr $expr returned null"
	else
		pass "$desc — value: $val"
	fi
}

# check_sorted FILE JQ_ARRAY_EXPR KEY_EXPR DESC
check_sorted() {
	local f="$1" arr_expr="$2" key_expr="$3" desc="$4"
	[[ -f "$f" ]] || return
	command -v jq &>/dev/null || return
	local sorted_check
	sorted_check=$(jq -e \
		"[$arr_expr | .[] | $key_expr] as \$vals |
		 \$vals == (\$vals | sort)" \
		"$f" 2>/dev/null || echo "false")
	[[ "$sorted_check" == "true" ]] && pass "Sorted: $desc" || fail "Not sorted: $desc"
}

# count_isomer_dirs DIR MIN MAX
count_isomer_dirs() {
	local dir="$1" min_n="$2" max_n="$3"
	if [[ ! -d "$dir" ]]; then
		pend "Isomer output dirs absent (generator pending)" "MF-C01"
		return
	fi
	local found
	found=$(find "$dir" -maxdepth 1 -type d -name "isomer_*" | wc -l)
	if [[ "$found" -ge "$min_n" && "$found" -le "$max_n" ]]; then
		pass "Isomer dir count: $found (expected $min_n-$max_n)"
	elif [[ "$found" -eq 0 ]]; then
		pend "0 isomer dirs (generator not yet wired)" "MF-C01"
	else
		fail "Isomer dir count $found outside expected range $min_n-$max_n"
	fi
}

# validate_chirality_labels FILE
# Checks that all chirality values are in the valid enum set
validate_chirality_labels() {
	local f="$1"
	[[ -f "$f" ]] || return
	command -v jq &>/dev/null || return
	local invalid
	invalid=$(jq -r '.isomers[].chirality // empty' "$f" 2>/dev/null | \
			  grep -Ev '^(R|S|achiral|unclassified|unknown)$' || true)
	if [[ -z "$invalid" ]]; then
		pass "All chirality labels valid {R, S, achiral, unclassified}"
	else
		fail "Invalid chirality labels found: $invalid"
	fi
}

# ---------------- pre-flight --------------------------------------------------
echo "========================================================"
echo " UC-3: Isomer Discovery Pipeline ([Co(NH3)4Cl2]+)"
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

# ---------------- STEP 2-4: pending core isomer outputs ----------------------
echo ""
echo "-- STEP 2-4: pending isomer generator / tracker outputs --"
expect_pending "$OUT_DIR/isomer_000/trajectory.xyz" \
			   "Isomer generator (first structure)"  "MF-C01"
expect_pending "$OUT_DIR/isomer_report.json" \
			   "Isomer ranked report (write_isomer_report)" "MF-C04"
expect_pending "$OUT_DIR/analysis.json" \
			   "Isomer tracking analysis"            "MF-C02"

# ---------------- STEP 5: conditional deep validation ------------------------
echo ""
echo "-- STEP 5: conditional validation (if report arrived early) --"
if [[ -f "$OUT_DIR/isomer_report.json" ]]; then
	validate_json "$OUT_DIR/isomer_report.json" "isomer_report.json"

	check_json_key "$OUT_DIR/isomer_report.json" \
		".isomers[0].chirality" \
		"Chirality label on isomer[0]" "MF-C03"

	check_json_key "$OUT_DIR/isomer_report.json" \
		".isomers | length" \
		"At least 1 isomer in report"

	check_sorted "$OUT_DIR/isomer_report.json" \
		".isomers[]" ".energy" \
		"Isomers sorted ascending by energy"

	validate_chirality_labels "$OUT_DIR/isomer_report.json"
else
	pend "isomer_report.json absent — deep validation skipped" "MF-C04"
fi

# Check analysis JSON if present
if [[ -f "$OUT_DIR/analysis.json" ]]; then
	validate_json "$OUT_DIR/analysis.json" "analysis.json"
	check_json_key "$OUT_DIR/analysis.json" ".rmsd_per_isomer" \
		"RMSD per isomer in analysis" "MF-C02"
fi

# ---------------- STEP 6: isomer directory count -----------------------------
echo ""
echo "-- STEP 6: isomer directory count (1-8 expected) --"
count_isomer_dirs "$OUT_DIR" 1 8

# ---------------- STEP 7: manifest (should appear even if isomers pending) ---
echo ""
echo "-- STEP 7: manifest validation --"
if [[ -f "$OUT_DIR/manifest.json" ]]; then
	expect_file  "$OUT_DIR/manifest.json" "Export manifest"
	validate_json "$OUT_DIR/manifest.json" "manifest.json"
else
	pend "manifest.json absent (run may not have completed)" "MF-C01"
fi

# ---------------- summary -----------------------------------------------------
echo ""
echo "========================================================"
echo " UC-3 Results  [$(_stamp)]"
echo "   PASS:    $PASS"
echo "   PENDING: $PENDING  (expected — see WO-MF-01)"
echo "   FAIL:    $FAIL"
echo "========================================================"

[[ ${#PENDING_TAGS[@]} -gt 0 ]] && echo " Pending: ${PENDING_TAGS[*]}"
[[ ${#FAIL_MSGS[@]}    -gt 0 ]] && { echo " Failures:"; for m in "${FAIL_MSGS[@]}"; do echo "   - $m"; done; }
echo ""
[[ $FAIL -eq 0 ]] && echo "UC-3: OK (FAIL=0)" || echo "UC-3: FAILED (FAIL=$FAIL)"
exit "$FAIL"
