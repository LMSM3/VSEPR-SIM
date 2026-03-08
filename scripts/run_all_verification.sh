#!/usr/bin/env bash
# run_all_verification.sh
# =======================
# One-command full verification suite for the atomistic formation engine.
#
# Usage:
#   wsl bash scripts/run_all_verification.sh            # quick local run
#   wsl bash scripts/run_all_verification.sh --full     # includes Suite P (downloaded refs)
#   wsl bash scripts/run_all_verification.sh --verbose  # print individual check results
#
# Tiered mode:
#   default   -- all static + randomised suites (A-N, Q, I-M); Suite P skipped.
#                Suitable for rapid local kernel checks during development.
#   --full    -- also runs fetch_empirical.py then deep_verification (Suite P active).
#                Required before release tags and for Milestone verification records.
#
# Exit code 0 = all executed checks pass.

set -euo pipefail

FULL_MODE=false
VERBOSE=false
for arg in "$@"; do
    case "$arg" in
        --full)    FULL_MODE=true  ;;
        --verbose) VERBOSE=true    ;;
    esac
done

BUILD_DIR="wsl-build"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$ROOT_DIR"

GREEN='\033[0;32m'
RED='\033[0;31m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo ""
echo -e "${CYAN}============================================================="
echo "  Formation Engine — Verification Suite"
if $FULL_MODE; then
    echo "  Mode: FULL (includes empirical-reference Suite P)"
else
    echo "  Mode: QUICK (Suite P skipped — run with --full for release)"
fi
echo -e "=============================================================${NC}"
echo ""

# ---------------------------------------------------------------------------
# Step 0 (full mode only): fetch downloaded references
# ---------------------------------------------------------------------------
if $FULL_MODE; then
    echo -e "${CYAN}[fetch]${NC}  Fetching empirical references (PubChem + NIST)..."
    if python3 scripts/fetch_empirical.py > /tmp/fetch_log.txt 2>&1; then
        echo -e "  ${GREEN}[OK]${NC}   fetch_empirical.py succeeded"
    else
        echo -e "  ${RED}[FAIL]${NC} fetch_empirical.py failed — Suite P will skip"
        cat /tmp/fetch_log.txt | tail -10
    fi
    echo ""
fi

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
echo "Building verification executables..."
cd "$BUILD_DIR"
cmake .. -DBUILD_VIS=OFF -DBUILD_TESTS=ON > /dev/null 2>&1
make -j"$(nproc)" \
    phase1_kernel_audit phase2_structural_energy phase3_relaxation \
    phase4_formation_priors phase5_crystal_validation \
    phase6_7_verification_sessions phase8_9_10_environment \
    phase11_12_13_final phase14_milestones deep_verification \
    > /dev/null 2>&1
cd "$ROOT_DIR"
echo ""

# ---------------------------------------------------------------------------
# Run phases
# ---------------------------------------------------------------------------
declare -a NAMES RESULTS PASS_COUNTS FAIL_COUNTS
TOTAL_PASS=0
TOTAL_FAIL=0
ALL_OK=true

run_phase() {
    local name="$1"
    local cmd="$2"
    local output
    output=$(eval "$cmd" 2>&1) || true

    local pline
    pline=$(echo "$output" | grep -E '(Audit:|Checks:|Deep Verification:)' | tail -1)
    local p f
    p=$(echo "$pline" | grep -oP '\d+(?= passed)' || echo "0")
    f=$(echo "$pline" | grep -oP '\d+(?= failed)' || echo "0")

    NAMES+=("$name")
    PASS_COUNTS+=("$p")
    FAIL_COUNTS+=("$f")
    TOTAL_PASS=$((TOTAL_PASS + p))
    TOTAL_FAIL=$((TOTAL_FAIL + f))

    if [ "$f" -gt 0 ]; then
        RESULTS+=("FAIL")
        ALL_OK=false
        echo -e "  ${RED}[FAIL]${NC}  $name  ($p passed, $f failed)"
    else
        RESULTS+=("PASS")
        echo -e "  ${GREEN}[PASS]${NC}  $name  ($p passed)"
    fi

    if $VERBOSE; then
        echo "$output" | grep -E '(FAIL|PASS)' | head -40
    fi
}

echo "Running verification phases..."
echo ""

run_phase "Phase 1  (kernel)"      "cd $BUILD_DIR && ./phase1_kernel_audit"
run_phase "Phase 2  (structural)"  "cd $BUILD_DIR && ./phase2_structural_energy"
run_phase "Phase 3  (relaxation)"  "cd $BUILD_DIR && ./phase3_relaxation"
run_phase "Phase 4  (presets)"     "cd $BUILD_DIR && ./phase4_formation_priors"
run_phase "Phase 5  (crystals)"    "cd $BUILD_DIR && ./phase5_crystal_validation"
run_phase "Phase 6/7 (sessions)"   "cd $BUILD_DIR && ./phase6_7_verification_sessions"
run_phase "Phase 8-10 (env)"       "cd $BUILD_DIR && ./phase8_9_10_environment"
run_phase "Phase 11-13 (comp)"     "cd $BUILD_DIR && ./phase11_12_13_final"
run_phase "Phase 14 (milestones)"  "cd $BUILD_DIR && ./phase14_milestones"
run_phase "Deep Verification"      "cd $BUILD_DIR && ./deep_verification"

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo ""
echo -e "${CYAN}============================================================="
echo "  Verification Summary"
echo -e "=============================================================${NC}"
echo ""
printf "  %-26s  %6s  %6s  %s\n" "Phase" "Pass" "Fail" "Status"
printf "  %-26s  %6s  %6s  %s\n" "--------------------------" "------" "------" "------"
for i in "${!NAMES[@]}"; do
    st="${RESULTS[$i]}"
    color=$GREEN; [ "$st" = "FAIL" ] && color=$RED
    printf "  %-26s  %6s  %6s  ${color}%s${NC}\n" \
        "${NAMES[$i]}" "${PASS_COUNTS[$i]}" "${FAIL_COUNTS[$i]}" "$st"
done
printf "  %-26s  %6s  %6s\n" "--------------------------" "------" "------"
printf "  %-26s  %6d  %6d\n" "TOTAL" "$TOTAL_PASS" "$TOTAL_FAIL"
echo ""

if ! $FULL_MODE; then
    echo -e "  ${YELLOW}NOTE:${NC} Suite P (downloaded empirical refs) was skipped."
    echo -e "        Run with --full to include it before a release tag."
    echo ""
fi

if $ALL_OK; then
    echo -e "  ${GREEN}ALL VERIFICATION PASSED${NC}  ($TOTAL_PASS checks)"
    exit 0
else
    echo -e "  ${RED}VERIFICATION FAILED${NC}  ($TOTAL_FAIL failures)"
    exit 1
fi


# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m'

echo ""
echo -e "${CYAN}============================================================="
echo "  Formation Engine — Full Verification Suite"
echo -e "=============================================================${NC}"
echo ""

# Build
echo "Building all phase executables..."
cd "$BUILD_DIR"
cmake .. -DBUILD_VIS=OFF -DBUILD_TESTS=ON > /dev/null 2>&1
make -j$(nproc) phase1_kernel_audit phase2_structural_energy phase3_relaxation \
    phase4_formation_priors phase5_crystal_validation phase6_7_verification_sessions \
    phase8_9_10_environment phase11_12_13_final phase14_milestones deep_verification \
    > /dev/null 2>&1
cd "$ROOT_DIR"

# Run phases and collect results
declare -a NAMES
declare -a RESULTS
declare -a PASS_COUNTS
declare -a FAIL_COUNTS
TOTAL_PASS=0
TOTAL_FAIL=0
ALL_OK=true

run_phase() {
    local name="$1"
    local cmd="$2"
    local output
    output=$(eval "$cmd" 2>&1) || true

    # Extract pass/fail from last "Audit:" or "Checks:" or "Deep Verification:" line
    local pline
    pline=$(echo "$output" | grep -E '(Audit:|Checks:|Deep Verification:)' | tail -1)
    local p f
    p=$(echo "$pline" | grep -oP '\d+(?= passed)' || echo "0")
    f=$(echo "$pline" | grep -oP '\d+(?= failed)' || echo "0")

    NAMES+=("$name")
    PASS_COUNTS+=("$p")
    FAIL_COUNTS+=("$f")
    TOTAL_PASS=$((TOTAL_PASS + p))
    TOTAL_FAIL=$((TOTAL_FAIL + f))

    if [ "$f" -gt 0 ]; then
        RESULTS+=("FAIL")
        ALL_OK=false
        echo -e "  ${RED}[FAIL]${NC}  $name  ($p passed, $f failed)"
    else
        RESULTS+=("PASS")
        echo -e "  ${GREEN}[PASS]${NC}  $name  ($p passed)"
    fi

    if [ "$VERBOSE" = "--verbose" ]; then
        echo "$output" | grep -E '(FAIL|PASS)' | head -40
    fi
}

echo "Running verification phases..."
echo ""

run_phase "Phase 1  (kernel)"      "cd $BUILD_DIR && ./phase1_kernel_audit"
run_phase "Phase 2  (structural)"  "cd $BUILD_DIR && ./phase2_structural_energy"
run_phase "Phase 3  (relaxation)"  "cd $BUILD_DIR && ./phase3_relaxation"
run_phase "Phase 4  (presets)"     "cd $BUILD_DIR && ./phase4_formation_priors"
run_phase "Phase 5  (crystals)"    "cd $BUILD_DIR && ./phase5_crystal_validation"
run_phase "Phase 6/7 (sessions)"   "cd $BUILD_DIR && ./phase6_7_verification_sessions"
run_phase "Phase 8-10 (env)"       "cd $BUILD_DIR && ./phase8_9_10_environment"
run_phase "Phase 11-13 (comp)"     "cd $BUILD_DIR && ./phase11_12_13_final"
run_phase "Phase 14 (milestones)"  "cd $BUILD_DIR && ./phase14_milestones"
run_phase "Deep Verification"      "cd $BUILD_DIR && ./deep_verification"

# Summary
echo ""
echo -e "${CYAN}============================================================="
echo "  Verification Summary"
echo "=============================================================${NC}"
echo ""
printf "  %-25s  %6s  %6s  %s\n" "Phase" "Pass" "Fail" "Status"
printf "  %-25s  %6s  %6s  %s\n" "-------------------------" "------" "------" "------"
for i in "${!NAMES[@]}"; do
    local_status="${RESULTS[$i]}"
    color=$GREEN
    [ "$local_status" = "FAIL" ] && color=$RED
    printf "  %-25s  %6s  %6s  ${color}%s${NC}\n" \
        "${NAMES[$i]}" "${PASS_COUNTS[$i]}" "${FAIL_COUNTS[$i]}" "$local_status"
done
printf "  %-25s  %6s  %6s\n" "-------------------------" "------" "------"
printf "  %-25s  %6d  %6d\n" "TOTAL" "$TOTAL_PASS" "$TOTAL_FAIL"
echo ""

if $ALL_OK; then
    echo -e "  ${GREEN}ALL VERIFICATION PASSED${NC}  ($TOTAL_PASS checks)"
    exit 0
else
    echo -e "  ${RED}VERIFICATION FAILED${NC}  ($TOTAL_FAIL failures)"
    exit 1
fi
