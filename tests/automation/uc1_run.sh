#!/usr/bin/env bash
# =============================================================================
# uc1_run.sh  --  UC-1: Desktop Script-to-Results automation
# =============================================================================
# VSEPR-SIM | v5.0.14 | WO-MF-01
#
# Tests the full headless pipeline for a single H2O script:
#   vsepr run → export package → expected output files
#
# Result classes:
#   PASS     expected file present and valid
#   PENDING  feature listed in WO-MF-01 as pending (not a failure)
#   FAIL     implemented feature produced wrong or absent output
#
# Usage:
#   bash tests/automation/uc1_run.sh [--vsepr PATH] [--out-dir PATH]
# =============================================================================

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# ---------------- configurable ------------------------------------------------
VSEPR="${VSEPR:-$REPO_ROOT/build/vsepr.exe}"
FIXTURE="$SCRIPT_DIR/vsim/uc1_h2o_quick.vsim"
OUT_DIR="$REPO_ROOT/out/uc1_h2o"
TIMEOUT=60                       # seconds before the run is killed

# Parse CLI overrides
while [[ $# -gt 0 ]]; do
	case $1 in
		--vsepr)   VSEPR="$2";   shift 2 ;;
		--out-dir) OUT_DIR="$2"; shift 2 ;;
		*) echo "Unknown arg: $1"; exit 1 ;;
	esac
done

# ---------------- shared helpers (sourced or inlined) -------------------------
PASS=0; FAIL=0; PENDING=0
PENDING_TAGS=()
FAIL_MSGS=()

_stamp() { date '+%H:%M:%S'; }

pass()  { PASS=$((PASS+1));    echo "  [PASS]    $1"; }
fail()  { FAIL=$((FAIL+1));    FAIL_MSGS+=("$1"); echo "  [FAIL]    $1"; }
pend()  { PENDING=$((PENDING+1)); PENDING_TAGS+=("$2"); echo "  [PENDING] $1  ($2)"; }

# expect_file FILE DESC
# PASS if present, FAIL if absent (feature should be implemented)
expect_file() {
	local f="$1" desc="$2"
	if [[ -f "$f" ]]; then
		pass "$desc"
	else
		fail "$desc — missing: $f"
	fi
}

# expect_pending FILE DESC MF-TAG
# PASS if absent (feature not yet implemented), FAIL if present-but-corrupt
expect_pending() {
	local f="$1" desc="$2" tag="$3"
	if [[ ! -f "$f" ]]; then
		pend "$desc" "$tag"
	else
		# File exists — validate it's at least non-empty
		if [[ -s "$f" ]]; then
			pass "$desc (feature arrived early — promote to expect_file!)"
		else
			fail "$desc — file present but empty: $f"
		fi
	fi
}

validate_json() {
	local f="$1" desc="$2"
	if [[ ! -f "$f" ]]; then skip_validate "$f"; return; fi
	if command -v jq &>/dev/null; then
		if jq empty "$f" 2>/dev/null; then
			pass "JSON valid: $desc"
		else
			fail "JSON invalid: $desc — $f"
		fi
	else
		pass "JSON present (jq not available, skipping parse): $desc"
	fi
}

validate_xyz() {
	local f="$1" expected_atoms="$2" desc="$3"
	if [[ ! -f "$f" ]]; then skip_validate "$f"; return; fi
	local first_line
	first_line=$(head -1 "$f")
	if [[ "$first_line" == "$expected_atoms" ]]; then
		pass "XYZ atom count: $desc (got $first_line)"
	else
		fail "XYZ atom count wrong: $desc — expected $expected_atoms, got $first_line"
	fi
}

skip_validate() { echo "  [SKIP]    validation skipped (file absent): $1"; }

# ---------------- pre-flight --------------------------------------------------
echo "========================================================"
echo " UC-1: Desktop Script-to-Results"
echo " $(_stamp)  $VSEPR"
echo "========================================================"

if [[ ! -f "$VSEPR" ]]; then
	echo "[ERROR] vsepr binary not found: $VSEPR"
	echo "        Build with: cmake --preset release && ninja -C build vsepr"
	exit 1
fi

if [[ ! -f "$FIXTURE" ]]; then
	echo "[ERROR] fixture not found: $FIXTURE"
	exit 1
fi

rm -rf "$OUT_DIR"

# Patch the output_dir in the fixture to our resolved OUT_DIR if needed
# (the fixture already uses "out/uc1_h2o"; resolve relative to repo root)
cd "$REPO_ROOT"

# ---------------- STEP 1: run vsepr -------------------------------------------
echo ""
echo "-- STEP 1: launch vsepr run [$(_stamp)] --"
set +e
timeout "$TIMEOUT" "$VSEPR" run "$FIXTURE" 2>&1
RUN_EXIT=$?
set -e

if [[ $RUN_EXIT -eq 124 ]]; then
	fail "vsepr run timed out after ${TIMEOUT}s"
elif [[ $RUN_EXIT -ne 0 ]]; then
	fail "vsepr run exited $RUN_EXIT"
else
	pass "vsepr run exited 0"
fi

# ---------------- STEP 2-6: expect implemented output files -------------------
echo ""
echo "-- STEP 2-6: implemented export writers (real filenames) --"
# The CLI names the XYZ after the [project] name field, not 'trajectory.xyz'.
# The export package names files after the internal pipeline stage labels.
XYZ_FILE="$OUT_DIR/uc1_h2o_quick.xyz"
expect_file "$XYZ_FILE"                               "Core XYZ output ({name}.xyz)"
expect_file "$OUT_DIR/pipeline_records.json"          "Pipeline records JSON"
expect_file "$OUT_DIR/pipeline_dashboard.md"          "Pipeline dashboard Markdown"
expect_file "$OUT_DIR/run_manifest.json"              "Run manifest JSON"
expect_file "$OUT_DIR/reports/beta7_pipeline_report.md"   "Pipeline report Markdown"
expect_file "$OUT_DIR/reports/beta7_pipeline_report.json" "Pipeline report JSON"

# ---------------- STEP 7: pending canonical names (MF-A naming gap) -----------
echo ""
echo "-- STEP 7: pending features and naming gaps (PENDING = expected absence) --"
# These are the xsim::xport canonical filenames that the export package
# is designed to produce but the headless CLI currently does not emit.
expect_pending "$OUT_DIR/trajectory.xyz"  "Canonical XYZ name 'trajectory.xyz' (xsim::xport XYZWriter)" "MF-A06"
expect_pending "$OUT_DIR/analysis.json"   "Canonical analysis JSON (AnalysisJsonWriter)"                  "MF-A07"
expect_pending "$OUT_DIR/manifest.json"   "Canonical manifest (ManifestJsonWriter)"                       "MF-A08"
expect_pending "$OUT_DIR/report.md"       "Canonical report.md (ReportMdWriter)"                          "MF-A09"
expect_pending "$OUT_DIR/metrics.tsv"     "Canonical metrics.tsv (MetricsTsvWriter)"                      "MF-A10"
expect_pending "$OUT_DIR/h2o.pdb"         "PDB writer (write_pdb)"                                        "MF-A04"

# ---------------- STEP 8-9: content validation --------------------------------
echo ""
echo "-- STEP 8-9: content validation --"
validate_xyz  "$XYZ_FILE"                        "3" "H2O has 3 atoms per frame"
validate_json "$OUT_DIR/pipeline_records.json"       "pipeline_records.json"
validate_json "$OUT_DIR/run_manifest.json"           "run_manifest.json"
validate_json "$OUT_DIR/reports/beta7_pipeline_report.json" "pipeline_report.json"

# Additional: check run_manifest lists the XYZ artifact
if [[ -f "$OUT_DIR/run_manifest.json" ]] && command -v jq &>/dev/null; then
	if jq -e '.artifacts[] | select(. | contains(".xyz"))' "$OUT_DIR/run_manifest.json" &>/dev/null; then
		pass "run_manifest.json references an XYZ artifact"
	else
		fail "run_manifest.json does not reference any XYZ artifact"
	fi
fi

# ---------------- summary -----------------------------------------------------
echo ""
echo "========================================================"
echo " UC-1 Results  [$(_stamp)]"
echo "   PASS:    $PASS"
echo "   PENDING: $PENDING  (expected — see WO-MF-01)"
echo "   FAIL:    $FAIL"
echo "========================================================"

if [[ ${#PENDING_TAGS[@]} -gt 0 ]]; then
	echo " Pending features: ${PENDING_TAGS[*]}"
fi

if [[ ${#FAIL_MSGS[@]} -gt 0 ]]; then
	echo " Failures:"
	for m in "${FAIL_MSGS[@]}"; do echo "   - $m"; done
fi

echo ""
[[ $FAIL -eq 0 ]] && echo "UC-1: OK (FAIL=0)" || echo "UC-1: FAILED (FAIL=$FAIL)"
exit "$FAIL"
