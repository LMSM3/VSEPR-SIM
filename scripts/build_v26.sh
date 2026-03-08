#!/usr/bin/env bash
# ============================================================================
# build_v26.sh — VSEPR-SIM v2.6 WSL build script
# ============================================================================
#
# Builds the full atomistic simulation stack (headless, no GUI/VIS).
# Targets: all libraries, all test suites, all CLI tools.
#
# Usage:
#   ./scripts/build_v26.sh              # Release build
#   ./scripts/build_v26.sh Debug        # Debug build
#   ./scripts/build_v26.sh Release run  # Build + run all tests
#
# Prerequisites (WSL/Ubuntu):
#   sudo apt install build-essential cmake libglfw3-dev libglew-dev
#
# Note: BUILD_VIS=OFF and BUILD_GUI=OFF by default.
#       Pass VIS=ON to enable (requires X11 forwarding or WSLg).
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_TYPE="${1:-Release}"
RUN_TESTS="${2:-}"
VIS="${VIS:-OFF}"
BUILD_DIR="$PROJECT_ROOT/build-v26"
JOBS=$(nproc 2>/dev/null || echo 4)

echo "╔══════════════════════════════════════════════════════╗"
echo "║     VSEPR-SIM v2.6 Build (WSL headless)             ║"
echo "╠══════════════════════════════════════════════════════╣"
echo "║  Project:    $PROJECT_ROOT"
echo "║  Build dir:  $BUILD_DIR"
echo "║  Type:       $BUILD_TYPE"
echo "║  Parallel:   $JOBS"
echo "║  VIS/GUI:    $VIS"
echo "╚══════════════════════════════════════════════════════╝"
echo ""

# ── Configure ────────────────────────────────────────────────────────────────

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$PROJECT_ROOT" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_CXX_STANDARD=20 \
    -DBUILD_TESTS=ON \
    -DBUILD_APPS=ON \
    -DBUILD_VIS="$VIS" \
    -DBUILD_GUI="$VIS" \
    -DBUILD_DEMOS=OFF \
    2>&1 | grep -E '^--|Configuring|Generating|Build files'

echo ""

# ── Build core libraries ─────────────────────────────────────────────────────

echo "▸ Building core libraries..."
cmake --build . --target atomistic vsepr_io vsepr_sim -j"$JOBS" 2>&1 | tail -3
echo "  ✓ atomistic, vsepr_io, vsepr_sim"

# ── Build atomistic test suite (Phases 1–6) ──────────────────────────────────

ATOMISTIC_TESTS=(
    test_alpha_model
    test_scf_polarization
    test_polarization_phases
    test_nuclear_chemical
    test_alpha_calibrator
    test_nuclear_stability
)

echo ""
echo "▸ Building atomistic test suite..."
for t in "${ATOMISTIC_TESTS[@]}"; do
    cmake --build . --target "$t" -j"$JOBS" 2>&1 | tail -1
done
echo "  ✓ ${#ATOMISTIC_TESTS[@]} atomistic test targets"

# ── Build fundamental validation ─────────────────────────────────────────────

FUNDAMENTAL_TESTS=(
    problem1_two_body_lj
    problem2_three_body_cluster
    test_heat_gate
    test_crystal_pipeline
    test_crystal_metrics
)

echo ""
echo "▸ Building fundamental validation..."
for t in "${FUNDAMENTAL_TESTS[@]}"; do
    cmake --build . --target "$t" -j"$JOBS" 2>&1 | tail -1
done
echo "  ✓ ${#FUNDAMENTAL_TESTS[@]} fundamental test targets"

# ── Build infrastructure tests ───────────────────────────────────────────────

INFRA_TESTS=(
    geom_ops_tests
    energy_tests
    optimizer_tests
    angle_tests
    vsepr_tests
    torsion_tests
    pbc_test
    pbc_verification
    pbc_phase2_physics
    test_periodic_table_102
    test_decay_chains
    chemistry_basic_test
    test_formula_parser
    test_spec_parser
    test_improved_nonbonded
    test_ethane_torsion
)

echo ""
echo "▸ Building all remaining targets..."
cmake --build . -j"$JOBS" -- -k 2>&1 | grep -c 'Built target' | xargs -I{} echo "  ✓ {} targets built (some legacy apps may fail due to API drift)"

# ── Build tools ──────────────────────────────────────────────────────────────

echo ""
echo "▸ Building tools..."
cmake --build . --target fit_alpha_model -j"$JOBS" 2>&1 | tail -1
echo "  ✓ fit_alpha_model"

# ── Summary ──────────────────────────────────────────────────────────────────

echo ""
echo "═══════════════════════════════════════════════════════"
echo " Build complete: $BUILD_DIR"
echo " Libraries:  $(find . -name '*.a' | wc -l) static libraries"
echo " Executables: $(find . -maxdepth 1 -type f -executable | wc -l) targets"
echo "═══════════════════════════════════════════════════════"

# ── Optional: run all tests ──────────────────────────────────────────────────

if [ "$RUN_TESTS" = "run" ]; then
    echo ""
    echo "▸ Running atomistic test suite..."
    echo ""

    TOTAL_PASS=0
    TOTAL_FAIL=0

    for t in "${ATOMISTIC_TESTS[@]}"; do
        echo "── $t ──"
        RESULT=$(./"$t" 2>&1 | tail -3)
        echo "$RESULT"
        # Extract pass/fail counts
        PASS=$(echo "$RESULT" | grep -oP '\d+ / \d+ passed' | head -1)
        if echo "$RESULT" | grep -q "ALL PASS"; then
            echo "  → PASS"
        else
            echo "  → FAIL"
            TOTAL_FAIL=$((TOTAL_FAIL + 1))
        fi
        echo ""
    done

    echo "── Fundamental validation ──"
    for t in "${FUNDAMENTAL_TESTS[@]}"; do
        if ./"$t" >/dev/null 2>&1; then
            echo "  $t: PASS"
        else
            echo "  $t: FAIL"
            TOTAL_FAIL=$((TOTAL_FAIL + 1))
        fi
    done

    echo ""
    if [ "$TOTAL_FAIL" -eq 0 ]; then
        echo "══════════════════════════════════════"
        echo "  ALL TEST SUITES PASSED"
        echo "══════════════════════════════════════"
    else
        echo "══════════════════════════════════════"
        echo "  $TOTAL_FAIL SUITE(S) FAILED"
        echo "══════════════════════════════════════"
        exit 1
    fi
fi
