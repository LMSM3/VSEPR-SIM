#!/usr/bin/env bash
# VSEPR-SIM — bead simulation test orchestration layer
# Usage: ./scripts/test_orchestration.sh [OPTIONS]
#
# Options:
#   -n, --particles N       number of particles          (default: 1200000)
#   -t, --time T            simulation time in seconds   (default: 8)
#   -l, --lmax L            max spherical harmonic order (default: 4)
#   -r, --rcut R            neighbour cutoff in Angstrom (default: 12)
#   -d, --dt DT             timestep in femtoseconds     (default: 5)
#   -j, --jobs J            parallel test jobs           (default: nproc)
#   -o, --output DIR        output directory             (default: ./test_output)
#   -s, --suite SUITE       suite: all|unit|integration|perf|validate
#                           (default: all)
#       --no-cleanup        keep intermediate files on failure
#       --ref DIR           reference data directory for validation
#   -v, --verbose           verbose output
#   -h, --help              show this message

set -euo pipefail

# ── defaults ──────────────────────────────────────────────────────────────────
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly SCRIPT_NAME="$(basename "$0")"

N_PARTICLES=1200000
T_SIM=8
LMAX=4
RCUT=12
DT=5
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
OUTPUT_DIR="./test_output"
SUITE="all"
CLEANUP=1
REF_DIR=""
VERBOSE=0

BINARY="${VSEPR_BIN:-./build/vsepr_sim}"
CTEST_BIN="${CTEST_BIN:-ctest}"
BUILD_DIR="${BUILD_DIR:-./out/build/x64-Debug}"

# ── colours (disabled when not a tty) ────────────────────────────────────────
if [[ -t 1 ]]; then
    RED='\033[0;31m' YEL='\033[0;33m' GRN='\033[0;32m'
    CYN='\033[0;36m' DIM='\033[2m'    RST='\033[0m' BOLD='\033[1m'
else
    RED='' YEL='' GRN='' CYN='' DIM='' RST='' BOLD=''
fi

# ── logging ───────────────────────────────────────────────────────────────────
pass() { printf "${GRN}[PASS]${RST} %s\n" "$*"; }
fail() { printf "${RED}[FAIL]${RST} %s\n" "$*" >&2; }
warn() { printf "${YEL}[WARN]${RST} %s\n" "$*" >&2; }
info() { printf "${CYN}[INFO]${RST} %s\n" "$*"; }
step() { printf "\n${BOLD}── %s${RST}\n" "$*"; }
dbg()  { [[ $VERBOSE -eq 1 ]] && printf "${DIM}[DBG]  %s${RST}\n" "$*" >&2 || true; }

# ── argument parsing ──────────────────────────────────────────────────────────
usage() {
    sed -n '/^# Usage:/,/^[^#]/{ /^#/{ s/^# \{0,1\}//; p }; /^[^#]/q }' "$0"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -n|--particles) N_PARTICLES=$2; shift 2 ;;
        -t|--time)      T_SIM=$2;       shift 2 ;;
        -l|--lmax)      LMAX=$2;        shift 2 ;;
        -r|--rcut)      RCUT=$2;        shift 2 ;;
        -d|--dt)        DT=$2;          shift 2 ;;
        -j|--jobs)      JOBS=$2;        shift 2 ;;
        -o|--output)    OUTPUT_DIR=$2;  shift 2 ;;
        -s|--suite)     SUITE=$2;       shift 2 ;;
        --no-cleanup)   CLEANUP=0;      shift   ;;
        --ref)          REF_DIR=$2;     shift 2 ;;
        -v|--verbose)   VERBOSE=1;      shift   ;;
        -h|--help)      usage ;;
        *) printf '%s: unknown option: %s\n' "$SCRIPT_NAME" "$1" >&2; exit 1 ;;
    esac
done

# ── state tracking ────────────────────────────────────────────────────────────
PASS_COUNT=0
FAIL_COUNT=0
WARN_COUNT=0
FAILED_TESTS=()

record_pass() { PASS_COUNT=$(( PASS_COUNT + 1 )); pass "$1"; }
record_fail() { FAIL_COUNT=$(( FAIL_COUNT + 1 )); FAILED_TESTS+=("$1"); fail "$1"; }
record_warn() { WARN_COUNT=$(( WARN_COUNT + 1 )); warn "$1"; }

# ── helpers ───────────────────────────────────────────────────────────────────
require_bin() {
    local bin=$1
    command -v "$bin" >/dev/null 2>&1 || {
        fail "Required binary not found: $bin"
        exit 1
    }
}

# Run a command, print elapsed ms to stdout.  Returns the command's exit code.
run_timed() {
    local label=$1; shift
    local start end elapsed rc=0
    start=$(date +%s%3N)
    "$@" || rc=$?
    end=$(date +%s%3N)
    elapsed=$(( end - start ))
    dbg "$label: ${elapsed} ms (exit $rc)"
    echo "$elapsed"
    return $rc
}

assert_file_exists() {
    if [[ ! -f "$1" ]]; then
        record_fail "Missing expected output: $1"
        return 1
    fi
}

# assert_json_key FILE PYTHON_KEY EXPECTED_VALUE
assert_json_key() {
    local file=$1 key=$2 expected=$3
    local actual
    actual=$(python3 -c "import json,sys; d=json.load(open('$file')); print(d$key)" 2>/dev/null) || {
        record_fail "JSON parse error in $file (key $key)"
        return 1
    }
    if [[ "$actual" == "$expected" ]]; then
        record_pass "$file: $key == $expected"
    else
        record_fail "$file: $key expected '$expected', got '$actual'"
    fi
}

# assert_numeric_lt LABEL VALUE THRESHOLD
assert_numeric_lt() {
    local label=$1 value=$2 threshold=$3
    if python3 -c "import sys; sys.exit(0 if float('$value') < float('$threshold') else 1)" 2>/dev/null; then
        record_pass "$label: $value < $threshold"
    else
        record_fail "$label: $value >= $threshold (threshold: $threshold)"
    fi
}

# assert_numeric_approx LABEL VALUE REFERENCE [TOL=0.01]
assert_numeric_approx() {
    local label=$1 value=$2 reference=$3 tol=${4:-0.01}
    if python3 - <<EOF 2>/dev/null
import sys
v, r, t = float('$value'), float('$reference'), float('$tol')
sys.exit(0 if abs(v - r) / (abs(r) + 1e-30) <= t else 1)
EOF
    then
        record_pass "$label (within ${tol})"
    else
        record_fail "$label: $value vs reference $reference (tol $tol)"
    fi
}

# ── setup ─────────────────────────────────────────────────────────────────────
setup() {
    step "Setup"

    require_bin python3

    if [[ ! -x "$BINARY" ]]; then
        warn "Simulation binary not found at $BINARY — perf/integration tests will be skipped"
    fi

    mkdir -p "$OUTPUT_DIR"/{unit,integration,perf,validate,logs}

    cat > "$OUTPUT_DIR/run_config.env" <<ENV
N_PARTICLES=$N_PARTICLES
T_SIM=$T_SIM
LMAX=$LMAX
RCUT=$RCUT
DT=$DT
JOBS=$JOBS
SUITE=$SUITE
REF_DIR=$REF_DIR
ENV

    info "Output dir  : $OUTPUT_DIR"
    info "Particles   : $N_PARTICLES"
    info "Sim time    : ${T_SIM} s"
    info "ℓ_max       : $LMAX"
    info "r_cut       : ${RCUT} Å"
    info "Δt          : ${DT} fs"
    info "Jobs        : $JOBS"
    info "Suite       : $SUITE"
}

# ── unit tests (via ctest) ────────────────────────────────────────────────────
run_unit_tests() {
    step "Unit tests"

    if [[ ! -d "$BUILD_DIR" ]]; then
        warn "Build dir not found ($BUILD_DIR) — skipping unit tests"
        return 0
    fi

    local log="$OUTPUT_DIR/logs/unit_ctest.log"

    local -a suites=(
        BeadStateTest
        DescriptorTest
        FragmentViewTest
        WignerDTest
        InteractionKernelTest
        AdaptiveTruncationTest
        BeadFIREReportTest
        coarse_grain
        reporting
    )

    for suite in "${suites[@]}"; do
        dbg "ctest -R $suite"
        if "$CTEST_BIN" --test-dir "$BUILD_DIR" \
               -R "$suite" --output-on-failure -j "$JOBS" \
               >> "$log" 2>&1; then
            record_pass "ctest: $suite"
        else
            record_fail "ctest: $suite (see $log)"
        fi
    done
}

# ── integration tests ─────────────────────────────────────────────────────────
run_integration_tests() {
    step "Integration tests"

    local dir="$OUTPUT_DIR/integration"
    local ilog="$OUTPUT_DIR/logs/integration.log"

    # Helper: skip gracefully when binary is absent
    require_binary_or_skip() { [[ -x "$BINARY" ]] || { warn "Binary missing — skipping $1"; return 1; }; return 0; }

    # ── benzene descriptor round-trip ─────────────────────────────────────────
    local benzene_out="$dir/benzene_roundtrip.json"
    if require_binary_or_skip "benzene round-trip"; then
        if "$BINARY" descriptor \
               --molecule benzene --lmax "$LMAX" --output "$benzene_out" \
               >> "$ilog" 2>&1; then
            assert_file_exists "$benzene_out"
            assert_json_key "$benzene_out" "['convergence']['converged']" "True"
            assert_json_key "$benzene_out" "['descriptor']['channels']"   "3"
        else
            record_fail "benzene descriptor generation"
        fi
    fi

    # ── fragment view scale boundary ──────────────────────────────────────────
    local fv_out="$dir/fragment_view.json"
    if require_binary_or_skip "fragment view"; then
        if "$BINARY" fragment-view \
               --molecule naphthalene --output "$fv_out" \
               >> "$ilog" 2>&1; then
            assert_file_exists "$fv_out"
            assert_json_key "$fv_out" "['status']"      "ok"
            assert_json_key "$fv_out" "['frame_valid']" "True"
        else
            record_fail "fragment view boundary test"
        fi
    fi

    # ── bead FIRE minimisation convergence ────────────────────────────────────
    local fire_out="$dir/fire_minimise.md"
    local fire_json="$dir/fire_minimise.json"
    if require_binary_or_skip "FIRE minimisation"; then
        if "$BINARY" minimise \
               --n-beads 2 --lmax "$LMAX" \
               --output-md "$fire_out" --output-json "$fire_json" \
               >> "$ilog" 2>&1; then
            assert_file_exists "$fire_out"
            assert_file_exists "$fire_json"
            assert_json_key "$fire_json" "['converged']" "True"
        else
            record_fail "FIRE minimisation convergence"
        fi
    fi

    # ── adaptive truncation promotes ℓ_max ────────────────────────────────────
    local adapt_out="$dir/adaptive_truncation.json"
    if require_binary_or_skip "adaptive truncation"; then
        if "$BINARY" descriptor \
               --molecule ferrocene --lmax 1 --adaptive --tol 0.01 \
               --output "$adapt_out" >> "$ilog" 2>&1; then
            assert_file_exists "$adapt_out"
            local promoted
            promoted=$(python3 -c \
                "import json; print(json.load(open('$adapt_out'))['descriptor']['lmax_final'])" \
                2>/dev/null || echo "0")
            if [[ "$promoted" -gt 1 ]]; then
                record_pass "Adaptive truncation promoted ℓ_max: 1 → $promoted"
            else
                record_fail "Adaptive truncation did not promote ℓ_max for ferrocene"
            fi
        else
            record_fail "Adaptive truncation test"
        fi
    fi

    # ── charge conservation across mapping ────────────────────────────────────
    local charge_out="$dir/charge_conservation.json"
    if require_binary_or_skip "charge conservation"; then
        if "$BINARY" map \
               --molecule benzene_dimer --output "$charge_out" \
               >> "$ilog" 2>&1; then
            local q_atom q_cg
            q_atom=$(python3 -c "import json; print(json.load(open('$charge_out'))['atomistic_total_charge'])" 2>/dev/null || echo "ERR")
            q_cg=$(python3   -c "import json; print(json.load(open('$charge_out'))['cg_total_charge'])"        2>/dev/null || echo "ERR")
            assert_numeric_approx "charge conservation" "$q_cg" "$q_atom" 1e-6
        else
            record_fail "charge conservation mapping"
        fi
    fi
}

# ── performance tests ─────────────────────────────────────────────────────────
run_perf_tests() {
    step "Performance tests"

    if [[ ! -x "$BINARY" ]]; then
        warn "Binary not found — skipping all perf tests"
        return 0
    fi

    local dir="$OUTPUT_DIR/perf"
    local plog="$OUTPUT_DIR/logs/perf.log"
    # Minimum GFLOP/s floor — tune for your hardware
    local PERF_FLOOR_GFLOPS="${PERF_FLOOR_GFLOPS:-50}"

    # ── neighbour list build time ─────────────────────────────────────────────
    local nbr_out="$dir/nbr_list.json"
    if "$BINARY" benchmark nbr-list \
           --n "$N_PARTICLES" --rcut "$RCUT" --output "$nbr_out" \
           >> "$plog" 2>&1; then
        assert_file_exists "$nbr_out"
        local nbr_ms
        nbr_ms=$(python3 -c "import json; print(json.load(open('$nbr_out'))['build_ms'])" \
                 2>/dev/null || echo "9999")
        assert_numeric_lt "neighbour list build (ms)" "$nbr_ms" 500
    else
        record_fail "neighbour list benchmark"
    fi

    # ── single-step interaction throughput ────────────────────────────────────
    local kern_out="$dir/kernel_throughput.json"
    if "$BINARY" benchmark interaction \
           --n "$N_PARTICLES" --lmax "$LMAX" --rcut "$RCUT" --output "$kern_out" \
           >> "$plog" 2>&1; then
        assert_file_exists "$kern_out"
        local gflops
        gflops=$(python3 -c "import json; print(json.load(open('$kern_out'))['gflops'])" \
                 2>/dev/null || echo "0")
        assert_numeric_lt "interaction kernel GFLOP/s floor" "$PERF_FLOOR_GFLOPS" "$gflops"
    else
        record_fail "interaction kernel benchmark"
    fi

    # ── Wigner-D cache hit rate ───────────────────────────────────────────────
    local wd_out="$dir/wigner_cache.json"
    if "$BINARY" benchmark wigner \
           --n "$N_PARTICLES" --lmax "$LMAX" --output "$wd_out" \
           >> "$plog" 2>&1; then
        assert_file_exists "$wd_out"
        local hit_rate miss_rate
        hit_rate=$(python3 -c "import json; print(json.load(open('$wd_out'))['cache_hit_rate'])" \
                   2>/dev/null || echo "0")
        miss_rate=$(python3 -c "print(1.0 - $hit_rate)" 2>/dev/null || echo "1")
        assert_numeric_lt "Wigner-D cache miss rate" "$miss_rate" 0.05
    else
        record_warn "Wigner-D cache benchmark not available"
    fi

    # ── full step wall-time projection (small-N proxy) ────────────────────────
    local proxy_n=50000
    local step_out="$dir/full_step_proxy.json"
    if "$BINARY" benchmark step \
           --n "$proxy_n" --lmax "$LMAX" --rcut "$RCUT" --dt "$DT" \
           --output "$step_out" >> "$plog" 2>&1; then
        assert_file_exists "$step_out"
        local step_ms projected_s
        step_ms=$(python3 -c "import json; print(json.load(open('$step_out'))['step_ms'])" \
                  2>/dev/null || echo "0")
        projected_s=$(python3 - <<EOF
n_steps    = int($T_SIM / ($DT * 1e-15))
step_full  = $step_ms * ($N_PARTICLES / $proxy_n)
print(round(step_full * n_steps / 1000, 1))
EOF
)
        info "Projected wall-clock for ${N_PARTICLES} particles / ${T_SIM} s sim: ${projected_s} s"
        printf 'projected_wall_s=%s\n' "$projected_s" >> "$dir/projection.env"
    else
        record_warn "Full step proxy benchmark not available"
    fi
}

# ── validation against reference data ─────────────────────────────────────────
run_validation_tests() {
    step "Validation tests"

    local dir="$OUTPUT_DIR/validate"
    local vlog="$OUTPUT_DIR/logs/validate.log"

    if [[ -z "$REF_DIR" ]]; then
        warn "No --ref directory provided — skipping validation suite"
        return 0
    fi
    if [[ ! -d "$REF_DIR" ]]; then
        warn "Reference dir '$REF_DIR' not found — skipping validation suite"
        return 0
    fi
    if [[ ! -x "$BINARY" ]]; then
        warn "Binary not found — skipping validation suite"
        return 0
    fi

    # Helper: run one validation case
    # validate_case REF_JSON SIM_JSON SIM_ARGS... --- LABEL REF_KEY SIM_KEY TOL
    # (Kept inline per case for clarity)

    # ── benzene pi-stacking reference energies ────────────────────────────────
    local ref_benz="$REF_DIR/benzene_dimer_energies.json"
    local sim_benz="$dir/benzene_dimer_sim.json"
    if [[ -f "$ref_benz" ]]; then
        if "$BINARY" energy --system benzene_dimer --lmax "$LMAX" \
               --output "$sim_benz" >> "$vlog" 2>&1; then
            local ref_e sim_e
            ref_e=$(python3 -c "import json; print(json.load(open('$ref_benz'))['face_to_face_kcal'])" 2>/dev/null || echo "ERR")
            sim_e=$(python3 -c "import json; print(json.load(open('$sim_benz'))['face_to_face_kcal'])" 2>/dev/null || echo "ERR")
            assert_numeric_approx "benzene face-to-face energy (kcal/mol)" "$sim_e" "$ref_e" 0.05
        else
            record_fail "benzene energy calculation"
        fi
    else
        warn "benzene_dimer_energies.json not in ref dir — skipping"
    fi

    # ── naphthalene crystal packing geometry ──────────────────────────────────
    local ref_naph="$REF_DIR/naphthalene_crystal.json"
    local sim_naph="$dir/naphthalene_crystal_sim.json"
    if [[ -f "$ref_naph" ]]; then
        if "$BINARY" crystal --system naphthalene --lmax "$LMAX" \
               --output "$sim_naph" >> "$vlog" 2>&1; then
            local ref_a sim_a
            ref_a=$(python3 -c "import json; print(json.load(open('$ref_naph'))['lattice_a_ang'])" 2>/dev/null || echo "ERR")
            sim_a=$(python3 -c "import json; print(json.load(open('$sim_naph'))['lattice_a_ang'])" 2>/dev/null || echo "ERR")
            assert_numeric_approx "naphthalene lattice a (Å)" "$sim_a" "$ref_a" 0.02
        else
            record_fail "naphthalene crystal calculation"
        fi
    else
        warn "naphthalene_crystal.json not in ref dir — skipping"
    fi

    # ── organometallic coordination geometry ──────────────────────────────────
    local ref_om="$REF_DIR/ferrocene_geometry.json"
    local sim_om="$dir/ferrocene_sim.json"
    if [[ -f "$ref_om" ]]; then
        if "$BINARY" energy --system ferrocene --lmax "$LMAX" \
               --output "$sim_om" >> "$vlog" 2>&1; then
            local ref_fe_cp sim_fe_cp
            ref_fe_cp=$(python3 -c "import json; print(json.load(open('$ref_om'))['fe_cp_dist_ang'])"  2>/dev/null || echo "ERR")
            sim_fe_cp=$(python3 -c "import json; print(json.load(open('$sim_om'))['fe_cp_dist_ang'])"  2>/dev/null || echo "ERR")
            assert_numeric_approx "ferrocene Fe–Cp distance (Å)" "$sim_fe_cp" "$ref_fe_cp" 0.03
        else
            record_fail "ferrocene geometry calculation"
        fi
    else
        warn "ferrocene_geometry.json not in ref dir — skipping"
    fi
}

# ── cleanup ───────────────────────────────────────────────────────────────────
cleanup_on_fail() {
    if [[ $CLEANUP -eq 0 ]]; then
        warn "Keeping intermediate files (--no-cleanup set)"
    elif [[ $FAIL_COUNT -gt 0 ]]; then
        warn "Keeping $OUTPUT_DIR for post-mortem (${FAIL_COUNT} failure(s))"
    else
        dbg "All passed — output retained in $OUTPUT_DIR"
    fi
}

# ── summary ───────────────────────────────────────────────────────────────────
print_summary() {
    local total=$(( PASS_COUNT + FAIL_COUNT ))
    step "Summary"
    printf '  Passed : %b%d%b\n' "$GRN" "$PASS_COUNT" "$RST"
    printf '  Failed : %b%d%b\n' "$RED" "$FAIL_COUNT" "$RST"
    printf '  Warned : %b%d%b\n' "$YEL" "$WARN_COUNT" "$RST"
    printf '  Total  : %d\n'     "$total"

    if [[ ${#FAILED_TESTS[@]} -gt 0 ]]; then
        printf '\n%bFailed tests:%b\n' "$RED" "$RST"
        printf '  - %s\n' "${FAILED_TESTS[@]}"
    fi

    local proj_env="$OUTPUT_DIR/perf/projection.env"
    if [[ -f "$proj_env" ]]; then
        local projected_wall_s
        # shellcheck disable=SC1090
        source "$proj_env"
        printf '\n%bWall-clock projection%b: ~%s s for %d particles / %s s sim\n' \
            "$CYN" "$RST" "${projected_wall_s:-?}" "$N_PARTICLES" "$T_SIM"
    fi

    printf '\nLogs: %s/logs/\n' "$OUTPUT_DIR"
}

# ── main ──────────────────────────────────────────────────────────────────────
main() {
    setup

    case "$SUITE" in
        all)
            run_unit_tests
            run_integration_tests
            run_perf_tests
            run_validation_tests
            ;;
        unit)        run_unit_tests ;;
        integration) run_integration_tests ;;
        perf)        run_perf_tests ;;
        validate)    run_validation_tests ;;
        *)
            printf '%s: unknown suite: %s\n' "$SCRIPT_NAME" "$SUITE" >&2
            exit 1
            ;;
    esac

    cleanup_on_fail
    print_summary

    [[ $FAIL_COUNT -eq 0 ]]
}

trap 'printf "\n%bInterrupted%b\n" "$RED" "$RST"; exit 130' INT TERM
main "$@"
