#!/usr/bin/env bash
# =============================================================================
# pipeline/run_pipeline.sh — VSEPR-SIM full test + post-process + host pipeline
#
# Stages (each is independently re-runnable via --stage=N):
#   1  build        cmake configure + build (incremental by default)
#   2  ctest        run all CTest suites, emit JUnit XML + JSON summary
#   3  pytest       run all pykernel tests, emit JSON + coverage
#   4  figures      regenerate overview PNG figures
#   5  postprocess  Python post-processor: merge results → HTML / JSON / CSV
#   6  host         start FastAPI server at http://localhost:8765
#
# Usage:
#   ./pipeline/run_pipeline.sh                    # all stages
#   ./pipeline/run_pipeline.sh --stage=1          # only build
#   ./pipeline/run_pipeline.sh --stage=2,3        # ctest + pytest
#   ./pipeline/run_pipeline.sh --stage=5,6        # post-process + host
#   ./pipeline/run_pipeline.sh --no-build         # skip stage 1
#   ./pipeline/run_pipeline.sh --watch            # re-run + live-reload host
#   ./pipeline/run_pipeline.sh --port=9000        # custom port
#   ./pipeline/run_pipeline.sh --jobs=8           # parallel cmake jobs
#
# Outputs (under out/pipeline/):
#   results/ctest_results.json
#   results/pytest_results.json
#   results/pipeline_summary.json
#   reports/index.html            ← hosted root
#   reports/figures/              ← copied PNG figures
#
# VSEPR-SIM 3.0.0
# =============================================================================

set -euo pipefail

# ── Colours ─────────────────────────────────────────────────────────────────
RED='\033[0;31m'  GREEN='\033[0;32m'  YELLOW='\033[1;33m'
BLUE='\033[0;34m' CYAN='\033[0;36m'   BOLD='\033[1m'    NC='\033[0m'

TICK="${GREEN}✔${NC}"  CROSS="${RED}✘${NC}"  ARROW="${CYAN}→${NC}"

# ── Paths ───────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT/build"
OUT_DIR="$ROOT/out/pipeline"
RESULTS_DIR="$OUT_DIR/results"
REPORTS_DIR="$OUT_DIR/reports"
FIG_SRC="$ROOT/docs/figures"
FIG_DST="$REPORTS_DIR/figures"
LOG_FILE="$OUT_DIR/pipeline.log"

PYTHON="${ROOT}/.venv/Scripts/python"
[[ ! -x "$PYTHON" ]] && PYTHON="python"

# ── Defaults ────────────────────────────────────────────────────────────────
STAGES="1,2,3,4,5,6"
PORT=8765
JOBS="${NUMBER_OF_PROCESSORS:-4}"
BUILD_TYPE="Release"
WATCH=false
NO_BUILD=false

# ── Argument parsing ────────────────────────────────────────────────────────
for arg in "$@"; do
    case "$arg" in
        --stage=*)  STAGES="${arg#--stage=}" ;;
        --port=*)   PORT="${arg#--port=}"    ;;
        --jobs=*)   JOBS="${arg#--jobs=}"    ;;
        --watch)    WATCH=true               ;;
        --no-build) NO_BUILD=true; STAGES="${STAGES//1,/}"; STAGES="${STAGES//,1/}"; STAGES="${STAGES//^1$/}" ;;
        --help|-h)
            sed -n '3,30p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,2\}//'
            exit 0 ;;
    esac
done

# Expand --stage=1 into a set for membership testing
declare -A ACTIVE_STAGES=()
IFS=',' read -ra _s <<< "$STAGES"
for s in "${_s[@]}"; do ACTIVE_STAGES[$s]=1; done

# ── Utilities ───────────────────────────────────────────────────────────────
log() { echo -e "$*" | tee -a "$LOG_FILE"; }
section() { log ""; log "${BOLD}${BLUE}══════════════════════════════════════════${NC}"; log "${BOLD}${BLUE}  Stage $1: $2${NC}"; log "${BOLD}${BLUE}══════════════════════════════════════════${NC}"; }
ok()   { log "  ${TICK} $*"; }
fail() { log "  ${CROSS} $*"; }
info() { log "  ${ARROW} $*"; }

skip_unless() {
    local stage="$1"
    [[ "${ACTIVE_STAGES[$stage]+_}" ]] || return 1
    return 0
}

# ── Init ────────────────────────────────────────────────────────────────────
mkdir -p "$RESULTS_DIR" "$REPORTS_DIR" "$FIG_DST"
: > "$LOG_FILE"   # reset log

log "${BOLD}VSEPR-SIM Pipeline${NC}  stages=${STAGES}  port=${PORT}  jobs=${JOBS}"
log "$(date '+%Y-%m-%d %H:%M:%S')"
log ""

PIPELINE_START=$(date +%s)
STAGE_RESULTS=()   # "stage:name:pass|fail:elapsed"

record_stage() {
    local stage="$1" name="$2" status="$3" elapsed="$4"
    STAGE_RESULTS+=("${stage}:${name}:${status}:${elapsed}")
}

# =============================================================================
# STAGE 1 — CMake Build
# =============================================================================
if skip_unless 1; then
    section 1 "CMake Build"
    T0=$(date +%s)
    info "Root: $ROOT"
    info "Build dir: $BUILD_DIR"
    info "Config: $BUILD_TYPE  Jobs: $JOBS"

    cmake -S "$ROOT" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DBUILD_TESTS=ON -DBUILD_APPS=ON -DBUILD_VIS=OFF \
        2>&1 | tee -a "$LOG_FILE" | grep -E "^(--|  |\-\- )" | sed 's/^/    /' || true

    cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j "$JOBS" \
        2>&1 | tee -a "$LOG_FILE" | tail -5 | sed 's/^/    /'

    T1=$(date +%s); ELAPSED=$((T1-T0))
    ok "Build completed in ${ELAPSED}s"
    record_stage 1 "cmake_build" "pass" "${ELAPSED}s"
fi

# =============================================================================
# STAGE 2 — CTest
# =============================================================================
if skip_unless 2; then
    section 2 "CTest Suites"
    T0=$(date +%s)
    CTEST_XML="$RESULTS_DIR/ctest_junit.xml"
    CTEST_JSON="$RESULTS_DIR/ctest_results.json"

    # Run ctest — capture output, don't abort on failure
    set +e
    ctest --test-dir "$BUILD_DIR" \
          --build-config "$BUILD_TYPE" \
          --output-on-failure \
          --no-compress-output \
          -T Test \
          2>&1 | tee -a "$LOG_FILE" | tail -20 | sed 's/^/    /'
    CTEST_EXIT=$?
    set -e

    # Parse ctest LastTest.log for pass/fail counts
    LAST_TEST_LOG="$BUILD_DIR/Testing/Temporary/LastTest.log"
    TOTAL_TESTS=0; PASS_TESTS=0; FAIL_TESTS=0
    if [[ -f "$LAST_TEST_LOG" ]]; then
        TOTAL_TESTS=$(grep -c "^Test " "$LAST_TEST_LOG" 2>/dev/null || echo 0)
    fi

    # Parse CTestTestfile output summary line "N tests passed"
    SUMMARY_LINE=$(grep -E "[0-9]+ tests? (passed|failed)" "$LOG_FILE" 2>/dev/null | tail -1 || echo "")
    if [[ -n "$SUMMARY_LINE" ]]; then
        PASS_TESTS=$(echo "$SUMMARY_LINE" | grep -oP '\d+(?= tests? passed)' || echo 0)
        FAIL_TESTS=$(echo "$SUMMARY_LINE" | grep -oP '\d+(?= tests? failed)' || echo 0)
        TOTAL_TESTS=$((PASS_TESTS + FAIL_TESTS))
    fi

    # Emit JSON summary
    CTEST_STATUS="pass"; [[ $CTEST_EXIT -ne 0 ]] && CTEST_STATUS="fail"
    cat > "$CTEST_JSON" <<ENDJSON
{
  "suite": "ctest",
  "timestamp": "$(date -Iseconds)",
  "total": $TOTAL_TESTS,
  "passed": $PASS_TESTS,
  "failed": $FAIL_TESTS,
  "exit_code": $CTEST_EXIT,
  "status": "$CTEST_STATUS",
  "build_dir": "$BUILD_DIR",
  "log": "$LOG_FILE"
}
ENDJSON

    T1=$(date +%s); ELAPSED=$((T1-T0))
    if [[ $CTEST_EXIT -eq 0 ]]; then
        ok "CTest: ${PASS_TESTS}/${TOTAL_TESTS} passed in ${ELAPSED}s"
        record_stage 2 "ctest" "pass" "${ELAPSED}s"
    else
        fail "CTest: ${FAIL_TESTS} failed (${PASS_TESTS}/${TOTAL_TESTS}) in ${ELAPSED}s"
        record_stage 2 "ctest" "fail" "${ELAPSED}s"
    fi
fi

# =============================================================================
# STAGE 3 — pytest
# =============================================================================
if skip_unless 3; then
    section 3 "pytest (pykernel)"
    T0=$(date +%s)
    PYTEST_JSON="$RESULTS_DIR/pytest_results.json"

    set +e
    "$PYTHON" -m pytest pykernel/tests/ \
        --tb=short -q \
        --json-report --json-report-file="$PYTEST_JSON" \
        2>&1 | tee -a "$LOG_FILE" | tail -15 | sed 's/^/    /'
    PYTEST_EXIT=$?
    set -e

    # Fallback: --json-report may not be installed; use plain pytest + grep
    if [[ ! -f "$PYTEST_JSON" ]]; then
        set +e
        PYTEST_OUT=$("$PYTHON" -m pytest pykernel/tests/ --tb=short -q 2>&1)
        PYTEST_EXIT=$?
        set -e
        echo "$PYTEST_OUT" | tee -a "$LOG_FILE" | tail -10 | sed 's/^/    /'
        PYTEST_PASS=$(echo "$PYTEST_OUT" | grep -oP '\d+(?= passed)' || echo 0)
        PYTEST_FAIL=$(echo "$PYTEST_OUT" | grep -oP '\d+(?= failed)' || echo 0)
        PYTEST_STATUS="pass"; [[ $PYTEST_EXIT -ne 0 ]] && PYTEST_STATUS="fail"
        cat > "$PYTEST_JSON" <<ENDJSON
{
  "suite": "pytest",
  "timestamp": "$(date -Iseconds)",
  "total": $((PYTEST_PASS + PYTEST_FAIL)),
  "passed": $PYTEST_PASS,
  "failed": $PYTEST_FAIL,
  "exit_code": $PYTEST_EXIT,
  "status": "$PYTEST_STATUS"
}
ENDJSON
    fi

    # Read counts from JSON for display
    PY_PASS=$("$PYTHON" -c "import json,sys; d=json.load(open('$PYTEST_JSON')); print(d.get('passed',d.get('summary',{}).get('passed',0)))" 2>/dev/null || echo "?")
    PY_TOTAL=$("$PYTHON" -c "import json,sys; d=json.load(open('$PYTEST_JSON')); print(d.get('total',d.get('summary',{}).get('total',0)))" 2>/dev/null || echo "?")

    T1=$(date +%s); ELAPSED=$((T1-T0))
    if [[ $PYTEST_EXIT -eq 0 ]]; then
        ok "pytest: ${PY_PASS}/${PY_TOTAL} passed in ${ELAPSED}s"
        record_stage 3 "pytest" "pass" "${ELAPSED}s"
    else
        fail "pytest: some failed (${PY_PASS}/${PY_TOTAL}) in ${ELAPSED}s"
        record_stage 3 "pytest" "fail" "${ELAPSED}s"
    fi
fi

# =============================================================================
# STAGE 4 — Regenerate figures
# =============================================================================
if skip_unless 4; then
    section 4 "Figure Generation"
    T0=$(date +%s)

    info "Overview figures..."
    "$PYTHON" scripts/render_overview_figures.py 2>&1 | tee -a "$LOG_FILE" | sed 's/^/    /'

    info "Document figures (if XYZ examples exist)..."
    if [[ -d "$ROOT/examples/my_molecules" ]] || [[ -f "$ROOT/examples/xyz_output/molecules.xyz" ]]; then
        "$PYTHON" scripts/render_document_figures.py 2>&1 | tee -a "$LOG_FILE" | sed 's/^/    /' || true
    else
        info "  Skipping render_document_figures (no example XYZ found)"
    fi

    # Copy figures to reports dir
    cp -r "$FIG_SRC"/. "$FIG_DST/"
    FIG_COUNT=$(ls "$FIG_DST"/*.png 2>/dev/null | wc -l)

    T1=$(date +%s); ELAPSED=$((T1-T0))
    ok "${FIG_COUNT} figures ready in ${ELAPSED}s"
    record_stage 4 "figures" "pass" "${ELAPSED}s"
fi

# =============================================================================
# STAGE 5 — Post-processing
# =============================================================================
if skip_unless 5; then
    section 5 "Post-processing"
    T0=$(date +%s)

    "$PYTHON" pipeline/postprocess.py \
        --results-dir "$RESULTS_DIR" \
        --reports-dir "$REPORTS_DIR" \
        --fig-dir     "$FIG_DST" \
        --root        "$ROOT" \
        2>&1 | tee -a "$LOG_FILE" | sed 's/^/    /'

    T1=$(date +%s); ELAPSED=$((T1-T0))
    ok "Post-process complete in ${ELAPSED}s"
    record_stage 5 "postprocess" "pass" "${ELAPSED}s"
fi

# =============================================================================
# Write pipeline summary JSON (read by web host)
# =============================================================================
PIPELINE_END=$(date +%s)
TOTAL_ELAPSED=$((PIPELINE_END - PIPELINE_START))

{
    echo "{"
    echo "  \"pipeline\": \"vsepr-sim\","
    echo "  \"version\": \"3.0.0\","
    echo "  \"timestamp\": \"$(date -Iseconds)\","
    echo "  \"total_elapsed_s\": $TOTAL_ELAPSED,"
    echo "  \"stages\": ["
    FIRST=true
    for entry in "${STAGE_RESULTS[@]}"; do
        IFS=':' read -r sid sname sstatus selapsed <<< "$entry"
        $FIRST && FIRST=false || echo ","
        printf '    {"id": %s, "name": "%s", "status": "%s", "elapsed": "%s"}' \
            "$sid" "$sname" "$sstatus" "$selapsed"
    done
    echo ""
    echo "  ]"
    echo "}"
} > "$RESULTS_DIR/pipeline_summary.json"

log ""
log "${BOLD}Pipeline summary (${TOTAL_ELAPSED}s):${NC}"
for entry in "${STAGE_RESULTS[@]}"; do
    IFS=':' read -r sid sname sstatus selapsed <<< "$entry"
    if [[ "$sstatus" == "pass" ]]; then
        log "  ${TICK}  Stage ${sid} ${sname}  (${selapsed})"
    else
        log "  ${CROSS}  Stage ${sid} ${sname}  (${selapsed})"
    fi
done

# =============================================================================
# STAGE 6 — Host
# =============================================================================
if skip_unless 6; then
    section 6 "Web Host"
    info "Starting server at http://localhost:${PORT}"
    info "Press Ctrl+C to stop"
    log ""

    if $WATCH; then
        # watchfiles-backed reload
        "$PYTHON" -m uvicorn pipeline.host:app \
            --host 0.0.0.0 --port "$PORT" \
            --reload --reload-dir "$OUT_DIR" \
            --reload-dir "$ROOT/docs/figures" \
            2>&1 | tee -a "$LOG_FILE"
    else
        "$PYTHON" -m uvicorn pipeline.host:app \
            --host 0.0.0.0 --port "$PORT" \
            2>&1 | tee -a "$LOG_FILE"
    fi
fi
