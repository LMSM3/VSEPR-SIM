#!/usr/bin/env bash
# ============================================================================
# VSEPR-SIM Modular Test Runner (bash)
# ============================================================================
#
# Usage:
#   ./scripts/run_tests.sh [OPTIONS] [SUITE]
#
# Suites:
#   all          Run every registered test (default)
#   quick        Fast smoke tests only
#   core         Core geometry / VSEPR
#   atomistic    Atomistic simulation domain
#   cg           Coarse-grained tests
#   suites       CG suites 1-8 + Track2
#   pbc          Periodic boundary conditions
#   chemistry    Chemistry / typing
#   parsers      Spec / formula parsers
#   thermal      Thermal / animation
#   crystal      Crystal pipeline
#   nuclear      Nuclear stability
#   polarization Polarization tests
#   fundamental  Fundamental two/three-body
#
# Options:
#   --build       Force rebuild before running tests
#   --clean       Remove build directory and rebuild from scratch
#   --release     Build in Release mode (default)
#   --debug       Build in Debug mode
#   --vis         Enable BUILD_VIS=ON
#   --list        List matching tests without running them
#   --verbose     Full CTest output (--verbose --output-on-failure)
#   --jobs N      Parallel build jobs (default: nproc)
#   --help        Show this help message
#
# Examples:
#   ./scripts/run_tests.sh                   # build + run all
#   ./scripts/run_tests.sh quick             # fast smoke tests
#   ./scripts/run_tests.sh cg                # coarse-grained
#   ./scripts/run_tests.sh suites            # CG suites 1-8 + Track2
#   ./scripts/run_tests.sh atomistic         # atomistic domain
#   ./scripts/run_tests.sh --list cg         # list CG tests (no run)
#   ./scripts/run_tests.sh --clean --verbose # clean rebuild + verbose
# ============================================================================

set -euo pipefail

# --- Resolve paths ----------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build_wsl"

# --- Defaults ---------------------------------------------------------------
SUITE="all"
BUILD_TYPE="Release"
DO_BUILD=false
DO_CLEAN=false
DO_LIST=false
DO_VIS=false
DO_VERBOSE=false
JOBS="$(nproc 2>/dev/null || echo 4)"

# --- Parse arguments --------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --build)       DO_BUILD=true ;;
        --clean)       DO_CLEAN=true; DO_BUILD=true ;;
        --release)     BUILD_TYPE="Release" ;;
        --debug)       BUILD_TYPE="Debug" ;;
        --vis)         DO_VIS=true ;;
        --list)        DO_LIST=true ;;
        --verbose)     DO_VERBOSE=true ;;
        --jobs)        shift; JOBS="$1" ;;
        --help|-h)
            head -n 48 "$0" | tail -n +2 | sed 's/^# \?//'
            exit 0
            ;;
        *)
            SUITE="$1"
            ;;
    esac
    shift
done

# --- Banner -----------------------------------------------------------------
echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║           VSEPR-SIM Modular Test Runner                      ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# --- Suite → CTest label mapping --------------------------------------------
declare -A LABEL_MAP=(
    [all]=""
    [quick]="quick"
    [core]="core"
    [atomistic]="atomistic"
    [cg]="coarse_grain"
    [suites]="cg_suites"
    [pbc]="pbc"
    [chemistry]="chemistry"
    [parsers]="parsers"
    [thermal]="thermal"
    [crystal]="crystal"
    [nuclear]="nuclear"
    [polarization]="polarization"
    [fundamental]="fundamental"
)

LABEL="${LABEL_MAP[$SUITE]:-INVALID}"
if [[ "$LABEL" == "INVALID" ]]; then
    echo "✗ Unknown suite: $SUITE" >&2
    echo "  Valid suites: ${!LABEL_MAP[*]}" >&2
    exit 1
fi

# --- Tool checks ------------------------------------------------------------
if ! command -v cmake &>/dev/null; then
    echo "✗ cmake not found in PATH" >&2
    exit 1
fi
if ! command -v ctest &>/dev/null; then
    echo "✗ ctest not found in PATH" >&2
    exit 1
fi

# --- Clean ------------------------------------------------------------------
if $DO_CLEAN && [[ -d "$BUILD_DIR" ]]; then
    echo "  Removing $BUILD_DIR ..."
    rm -rf "$BUILD_DIR"
fi

# --- Determine whether to build ---------------------------------------------
if $DO_BUILD || $DO_CLEAN || [[ ! -d "$BUILD_DIR" ]]; then
    NEEDS_BUILD=true
else
    NEEDS_BUILD=false
fi

# --- Build ------------------------------------------------------------------
if $NEEDS_BUILD; then
    VIS_FLAG="OFF"
    if $DO_VIS; then VIS_FLAG="ON"; fi

    echo "  Configuring ($BUILD_TYPE, VIS=$VIS_FLAG) ..."
    mkdir -p "$BUILD_DIR"
    cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
          -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
          -DBUILD_TESTS=ON \
          -DBUILD_VIS="$VIS_FLAG" \
          2>&1 | sed 's/^/    /'

    echo "  Building (j=$JOBS) ..."
    cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j "$JOBS" 2>&1 | sed 's/^/    /'

    echo "✓ Build succeeded"
    echo ""
fi

# --- Assemble ctest arguments -----------------------------------------------
CTEST_ARGS=(--test-dir "$BUILD_DIR" --build-config "$BUILD_TYPE")

if [[ -n "$LABEL" ]]; then
    CTEST_ARGS+=(-L "$LABEL")
fi

if $DO_VERBOSE; then
    CTEST_ARGS+=(--verbose --output-on-failure)
else
    CTEST_ARGS+=(--output-on-failure)
fi

# --- List mode --------------------------------------------------------------
if $DO_LIST; then
    echo "  Tests matching suite '$SUITE':"
    LIST_ARGS=(--test-dir "$BUILD_DIR" --build-config "$BUILD_TYPE" -N)
    if [[ -n "$LABEL" ]]; then
        LIST_ARGS+=(-L "$LABEL")
    fi
    ctest "${LIST_ARGS[@]}" 2>&1 | sed 's/^/    /'
    exit 0
fi

# --- Run tests --------------------------------------------------------------
echo "  Running tests: suite=$SUITE$(if [[ -n "$LABEL" ]]; then echo " (label=$LABEL)"; else echo " (all)"; fi)"
echo ""

ctest "${CTEST_ARGS[@]}" 2>&1 | sed 's/^/  /'
EXIT_CODE=${PIPESTATUS[0]}

echo ""
if [[ $EXIT_CODE -eq 0 ]]; then
    echo "╔═══════════════════════════════════════════════════════════════╗"
    printf "║  ✓ All tests passed (suite: %-33s ║\n" "$SUITE)"
    echo "╚═══════════════════════════════════════════════════════════════╝"
else
    echo "╔═══════════════════════════════════════════════════════════════╗"
    printf "║  ✗ Some tests failed (suite: %-30s ║\n" "$SUITE)"
    echo "╚═══════════════════════════════════════════════════════════════╝"
fi

exit $EXIT_CODE
