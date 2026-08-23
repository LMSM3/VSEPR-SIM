#!/usr/bin/env bash
# tests/automation/uc7_run.sh
# UC-7 -- SiO2 Pipe Simulation
# Validates the resolved uc7_sio2_pipe.vsim fixture.
# Exit 0 = pass (PENDING items are not failures). Exit 1 = at least one FAIL.

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BIN="$REPO_ROOT/build/vsepr.exe"
FIXTURE="$SCRIPT_DIR/vsim/uc7_sio2_pipe.vsim"
OUTDIR="$REPO_ROOT/out/uc7_sio2_pipe"
RECORDS="$OUTDIR/pipeline_records.json"
PASS=0; FAIL=0; PENDING=0

expect_file() {
    local f="$1" label="$2"
    if [ -f "$f" ]; then echo "  PASS    $label"; PASS=$((PASS+1))
    else echo "  FAIL    $label (missing: $f)"; FAIL=$((FAIL+1)); fi
}

expect_pending() {
    local f="$1" tag="$2" label="$3"
    if [ -f "$f" ]; then echo "  PASS    $label"; PASS=$((PASS+1))
    else echo "  PENDING [$tag] $label"; PENDING=$((PENDING+1)); fi
}

validate_json() {
    local f="$1"
    if python3 -c "import json,sys; json.load(open(sys.argv[1]))" "$f" 2>/dev/null; then
        echo "  PASS    JSON valid: $(basename "$f")"; PASS=$((PASS+1))
    else echo "  FAIL    JSON invalid: $f"; FAIL=$((FAIL+1)); fi
}

json_field() {
    python3 - "$RECORDS" "$1" <<-PYEOF
import json, sys
try:
    data = json.load(open(sys.argv[1]))
    keys = sys.argv[2].split(".")
    v = data
    for k in keys:
        v = v[k]
    print(v)
except Exception:
    print("MISSING")
PYEOF
}

check_flux_recorded() {
    [ ! -f "$RECORDS" ] && { echo "  PENDING [MF-G04] flux recorded (no records)"; PENDING=$((PENDING+1)); return; }
    local v; v=$(json_field outlet.record_flux)
    case "$v" in
        true|True|1) echo "  PASS    outlet flux recorded"; PASS=$((PASS+1)) ;;
        MISSING)     echo "  PENDING [MF-G04] outlet.record_flux absent"; PENDING=$((PENDING+1)) ;;
        *)           echo "  FAIL    outlet flux not recorded (v=$v)"; FAIL=$((FAIL+1)) ;;
    esac
}

check_energy_decreasing() {
    [ ! -f "$RECORDS" ] && { echo "  PENDING [MF-G04] energy trace"; PENDING=$((PENDING+1)); return; }
    local ok
    ok=$(python3 - "$RECORDS" <<-PYEOF
import json, sys
data = json.load(open(sys.argv[1]))
t = data.get("energy_trace", data.get("energy", None))
if isinstance(t, list) and len(t)>=2:
    print("yes" if float(t[-1])<float(t[0]) else "no")
elif t is not None:
    print("scalar")
else:
    print("MISSING")
PYEOF
)
    case "$ok" in
        yes)    echo "  PASS    energy decreasing"; PASS=$((PASS+1)) ;;
        no)     echo "  FAIL    energy increasing (diverging)"; FAIL=$((FAIL+1)) ;;
        scalar) echo "  PASS    energy scalar present"; PASS=$((PASS+1)) ;;
        *)      echo "  PENDING [MF-G04] energy_trace absent"; PENDING=$((PENDING+1)) ;;
    esac
}

check_wall_regions() {
    [ ! -f "$RECORDS" ] && { echo "  PENDING [MF-G04] wall regions"; PENDING=$((PENDING+1)); return; }
    local v; v=$(json_field wall.region_count)
    case "$v" in
        MISSING) echo "  PENDING [MF-G04] wall.region_count absent"; PENDING=$((PENDING+1)) ;;
        16)      echo "  PASS    wall region count = 16"; PASS=$((PASS+1)) ;;
        *)       echo "  FAIL    wall region count = $v (expected 16)"; FAIL=$((FAIL+1)) ;;
    esac
}

echo "=============================================="; echo " UC-7: SiO2 Pipe Simulation"; echo " Fixture: tests/automation/vsim/uc7_sio2_pipe.vsim"; echo ""
echo "-- STEP 1: Run fixture"
cd "$REPO_ROOT"; "$BIN" run "$FIXTURE" || true

echo ""; echo "-- STEP 2-5: Core output files"
expect_file "$OUTDIR/uc7_sio2_pipe.xyz"          "particle trajectory XYZ"
expect_file "$RECORDS"                            "pipeline_records.json"
expect_file "$OUTDIR/pipeline_dashboard.md"       "pipeline_dashboard.md"
expect_file "$OUTDIR/run_manifest.json"           "run_manifest.json"

echo ""; echo "-- STEP 6-8: Pending visual outputs (MF-G01..G03)"
expect_pending "$OUTDIR/figures/pipe/energy_trace.svg"    "MF-G01" "energy_trace.svg"
expect_pending "$OUTDIR/figures/pipe/html_dashboard.html" "MF-G02" "html_dashboard.html"
expect_pending "$OUTDIR/pipeline_audit.jsonl"             "MF-G03" "pipeline_audit.jsonl"

echo ""; echo "-- STEP 9-11: Physics checks"
check_flux_recorded
check_energy_decreasing
check_wall_regions

echo ""; echo "-- STEP 12-13: JSON integrity"
[ -f "$RECORDS" ]                  && validate_json "$RECORDS"
[ -f "$OUTDIR/run_manifest.json" ] && validate_json "$OUTDIR/run_manifest.json"

echo ""; echo "=============================================="
echo " UC-7 SUMMARY"
printf "  PASS:    %d\n"                               "$PASS"
printf "  PENDING: %d  (MF-G01..G05 per WO-MF-01)\n"  "$PENDING"
printf "  FAIL:    %d\n"                               "$FAIL"
echo "=============================================="
if [ "$FAIL" -gt 0 ]; then echo "  RESULT: FAIL"; exit 1; fi
if [ "$PENDING" -gt 0 ]; then echo "  RESULT: PASS (with $PENDING pending items)"; else echo "  RESULT: PASS -- SiO2 pipe fully green"; fi
exit 0
