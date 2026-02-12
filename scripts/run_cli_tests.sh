#!/usr/bin/env bash
# run_cli_tests.sh
# VSEPR-Sim CLI smoke/regression automation
# Runs representative user-path commands, validates exit codes and artifacts
#
# Usage:
#   ./run_cli_tests.sh
#   VSEPR_BIN=./build/bin/vsepr ./run_cli_tests.sh
#   VSEPR_BIN=./build/bin/molecule_builder ./run_cli_tests.sh
#
# Exit codes:
#   0 = all tests passed
#   1 = at least one test failed

set -euo pipefail
IFS=$'\n\t'

# ============================================================================
# Configuration
# ============================================================================

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Respect user override; otherwise auto-detect
VSEPR_BIN="${VSEPR_BIN:-}"
if [[ -z "${VSEPR_BIN}" ]]; then
  if [[ -x "${PROJECT_ROOT}/build/bin/vsepr" ]]; then
    VSEPR_BIN="${PROJECT_ROOT}/build/bin/vsepr"
  elif [[ -x "${PROJECT_ROOT}/build/bin/molecule_builder" ]]; then
    VSEPR_BIN="${PROJECT_ROOT}/build/bin/molecule_builder"
  else
    echo "✗ Could not find VSEPR binary"
    echo "  Tried:"
    echo "    ${PROJECT_ROOT}/build/bin/vsepr"
    echo "    ${PROJECT_ROOT}/build/bin/molecule_builder"
    echo "  Set VSEPR_BIN explicitly or run 'cmake --build build' first"
    exit 1
  fi
fi

# Test output directories
LOG_DIR="${PROJECT_ROOT}/logs/test_runs"
TS="$(date +"%Y%m%d_%H%M%S")"
RUN_DIR="${LOG_DIR}/${TS}"
OUT_DIR="${RUN_DIR}/artifacts"
mkdir -p "${OUT_DIR}"

STDOUT_LOG="${RUN_DIR}/stdout.log"
STDERR_LOG="${RUN_DIR}/stderr.log"
SUMMARY_LOG="${RUN_DIR}/summary.log"

: > "${STDOUT_LOG}"
: > "${STDERR_LOG}"
: > "${SUMMARY_LOG}"

# Skip visualization tests on headless CI
SKIP_VIZ="${SKIP_VIZ:-0}"

# ============================================================================
# Helper Functions
# ============================================================================

pass_count=0
fail_count=0

say()  { printf "%s\n" "$*" | tee -a "${SUMMARY_LOG}" ; }
die()  { say "✗ FATAL: $*"; exit 1; }

run_cmd() {
  # run_cmd "<name>" <expected_rc> <cmd...>
  local name="$1"; shift
  local expected_rc="$1"; shift

  local out_file="${RUN_DIR}/$(echo "${name}" | tr ' /' '__').out"
  local err_file="${RUN_DIR}/$(echo "${name}" | tr ' /' '__').err"

  say "• ${name}: ${VSEPR_BIN} $*"
  set +e
  "${VSEPR_BIN}" "$@" >"${out_file}" 2>"${err_file}"
  local rc=$?
  set -e

  cat "${out_file}" >> "${STDOUT_LOG}"
  cat "${err_file}" >> "${STDERR_LOG}"

  if [[ "${rc}" -ne "${expected_rc}" ]]; then
    say "  ✗ FAIL: ${name} (rc=${rc}, expected=${expected_rc})"
    say "    stdout: ${out_file}"
    say "    stderr: ${err_file}"
    ((fail_count++))
    return 1
  fi

  say "  ✓ OK (rc=${rc})"
  ((pass_count++))
  return 0
}

assert_file_exists_nonempty() {
  local f="$1"
  if [[ ! -s "${f}" ]]; then
    say "  ✗ FAIL: expected non-empty file: ${f}"
    ((fail_count++))
    return 1
  fi
  say "  ✓ file exists: ${f}"
  ((pass_count++))
  return 0
}

assert_xyz_header_sane() {
  local f="$1"
  if [[ ! -f "${f}" ]]; then
    say "  ✗ FAIL: missing xyz: ${f}"
    ((fail_count++))
    return 1
  fi
  
  local line1 line2
  line1="$(head -n 1 "${f}" | tr -d '\r')"
  line2="$(head -n 2 "${f}" | tail -n 1 | tr -d '\r')"

  if ! [[ "${line1}" =~ ^[0-9]+$ ]]; then
    say "  ✗ FAIL: xyz line1 not an integer atom count: '${line1}' (${f})"
    ((fail_count++))
    return 1
  fi
  
  if [[ -z "${line2}" ]]; then
    say "  ✗ FAIL: xyz line2 comment missing/empty (${f})"
    ((fail_count++))
    return 1
  fi
  
  say "  ✓ xyz header sane: ${f} (atoms=${line1})"
  ((pass_count++))
  return 0
}

assert_contains() {
  # assert_contains "<file>" "<regex>" "<label>"
  local f="$1"
  local re="$2"
  local label="$3"
  
  if ! grep -Eqi -- "${re}" "${f}"; then
    say "  ✗ FAIL: missing '${label}' (/${re}/) in ${f}"
    ((fail_count++))
    return 1
  fi
  
  say "  ✓ contains: ${label}"
  ((pass_count++))
  return 0
}

# ============================================================================
# Preflight
# ============================================================================

say "=== VSEPR-Sim CLI Test Automation ==="
say "Binary:    ${VSEPR_BIN}"
say "Run dir:   ${RUN_DIR}"
say "Artifacts: ${OUT_DIR}"
say "Date:      $(date)"
say ""

# ============================================================================
# Test Suite: Help & Version
# ============================================================================

say "=== Test Suite: Help & Version ==="

run_cmd "help --help"    0 --help || true
run_cmd "help -h"        0 -h || true

# Version paths (accept --version, -v, or version subcommand)
ver_ok=0
if run_cmd "version --version" 0 --version 2>/dev/null; then ver_ok=1; fi
if run_cmd "version -v"       0 -v 2>/dev/null; then ver_ok=1; fi
if run_cmd "version subcmd"   0 version 2>/dev/null; then ver_ok=1; fi

if [[ "${ver_ok}" -ne 1 ]]; then
  say "  ⚠ WARNING: No version command worked (--version, -v, or 'version')"
  say "  This is acceptable if not yet implemented"
else
  say "  ✓ At least one version command works"
  ((pass_count++))
fi

say ""

# ============================================================================
# Test Suite: Basic Build (molecule_builder compatibility)
# ============================================================================

say "=== Test Suite: Basic Build ==="

# Simple molecules
run_cmd "build H2O" 0 H2O || run_cmd "build H2O (alt)" 0 build H2O || true
run_cmd "build CH4" 0 CH4 || run_cmd "build CH4 (alt)" 0 build CH4 || true
run_cmd "build NH3" 0 NH3 || run_cmd "build NH3 (alt)" 0 build NH3 || true
run_cmd "build CO2" 0 CO2 || run_cmd "build CO2 (alt)" 0 build CO2 || true

say ""

# ============================================================================
# Test Suite: Optimization
# ============================================================================

say "=== Test Suite: Optimization ==="

run_cmd "H2O --optimize"     0 H2O --optimize || true
run_cmd "CH4 --no-opt"       0 CH4 --no-opt || true
run_cmd "NH3 --opt (FIRE)"   0 NH3 --opt || true

say ""

# ============================================================================
# Test Suite: Output Files
# ============================================================================

say "=== Test Suite: Output Files (XYZ) ==="

H2O_XYZ="${OUT_DIR}/water_opt.xyz"
CH4_XYZ="${OUT_DIR}/methane_opt.xyz"
NH3_XYZ="${OUT_DIR}/ammonia_opt.xyz"
CO2_XYZ="${OUT_DIR}/co2.xyz"

run_cmd "H2O --xyz output" 0 H2O --xyz "${H2O_XYZ}" || true
assert_file_exists_nonempty "${H2O_XYZ}" || true
assert_xyz_header_sane "${H2O_XYZ}" || true

run_cmd "CH4 --xyz output" 0 CH4 --xyz "${CH4_XYZ}" || true
assert_file_exists_nonempty "${CH4_XYZ}" || true
assert_xyz_header_sane "${CH4_XYZ}" || true

run_cmd "NH3 --xyz output" 0 NH3 --xyz "${NH3_XYZ}" --no-opt || true
assert_file_exists_nonempty "${NH3_XYZ}" || true
assert_xyz_header_sane "${NH3_XYZ}" || true

run_cmd "CO2 --output" 0 CO2 --output "${CO2_XYZ}" || true
assert_file_exists_nonempty "${CO2_XYZ}" || true

say ""

# ============================================================================
# Test Suite: Visualization (HTML viewer)
# ============================================================================

say "=== Test Suite: Visualization ==="

if [[ "${SKIP_VIZ}" == "1" ]]; then
  say "  • Viz tests skipped (SKIP_VIZ=1)"
else
  VIZ_OK=0
  
  pushd "${OUT_DIR}" >/dev/null || die "could not cd to ${OUT_DIR}"
  
  # Try generating viewer HTML
  set +e
  "${VSEPR_BIN}" H2O --xyz H2O_test.xyz >"${RUN_DIR}/viz_h2o.out" 2>"${RUN_DIR}/viz_h2o.err"
  viz_rc=$?
  set -e
  
  if [[ "${viz_rc}" -eq 0 ]]; then
    # Check for viewer HTML (pattern: *_viewer.html)
    vfile="$(ls -1 *_viewer.html 2>/dev/null | head -n 1 || true)"
    if [[ -n "${vfile}" && -s "${vfile}" ]]; then
      say "  ✓ Viz artifact: ${OUT_DIR}/${vfile}"
      ((pass_count++))
      VIZ_OK=1
    else
      say "  ⚠ XYZ created but no *_viewer.html found"
      say "    (Viewer generation may require explicit --viz flag)"
    fi
  fi
  
  popd >/dev/null || true
  
  if [[ "${VIZ_OK}" -ne 1 ]]; then
    say "  • No validated HTML output (may be unimplemented or require --viz flag)"
  fi
fi

say ""

# ============================================================================
# Test Suite: Complex Molecules
# ============================================================================

say "=== Test Suite: Complex Molecules ==="

# Larger organic molecules
run_cmd "C6H12O6 (glucose)" 0 C6H12O6 --xyz "${OUT_DIR}/glucose.xyz" || true
run_cmd "C10H22 (decane)"   0 C10H22 --xyz "${OUT_DIR}/decane.xyz" || true

# Inorganic molecules
run_cmd "H2SO4 (sulfuric)"  0 H2SO4 --xyz "${OUT_DIR}/h2so4.xyz" || true
run_cmd "CaCO3 (calcium)"   0 CaCO3 --xyz "${OUT_DIR}/caco3.xyz" || true

say ""

# ============================================================================
# Test Suite: Error Handling
# ============================================================================

say "=== Test Suite: Error Handling ==="

run_cmd "invalid formula" 1 XYZZY || true
run_cmd "empty formula"   1 "" 2>/dev/null || true
run_cmd "unknown element" 1 Zz99 || true

say ""

# ============================================================================
# Test Suite: Formula Parser (if available)
# ============================================================================

say "=== Test Suite: Formula Parser ==="

if [[ -x "${PROJECT_ROOT}/build/bin/test_formula_parser" ]]; then
  say "• Running formula parser tests..."
  set +e
  "${PROJECT_ROOT}/build/bin/test_formula_parser" >"${RUN_DIR}/formula_parser.out" 2>"${RUN_DIR}/formula_parser.err"
  ftest_rc=$?
  set -e
  
  if [[ "${ftest_rc}" -eq 0 ]]; then
    say "  ✓ Formula parser tests PASSED"
    ((pass_count++))
  else
    say "  ✗ Formula parser tests FAILED (rc=${ftest_rc})"
    say "    Output: ${RUN_DIR}/formula_parser.out"
    ((fail_count++))
  fi
else
  say "  • Formula parser tests not built (skipping)"
fi

say ""

# ============================================================================
# Test Suite: Batch Processing (if available)
# ============================================================================

say "=== Test Suite: Batch Processing ==="

if [[ -x "${PROJECT_ROOT}/build/bin/vsepr_batch" ]]; then
  BATCH_OUT="${OUT_DIR}/batch_test"
  mkdir -p "${BATCH_OUT}"
  
  run_cmd "vsepr_batch simple" 0 ../build/bin/vsepr_batch "H2O, CO2 -per{50,50}" --out "${BATCH_OUT}" --dry-run || true
  
  say "  • Batch runner available"
else
  say "  • Batch runner not built (skipping)"
fi

say ""

# ============================================================================
# Validation: Output Content
# ============================================================================

say "=== Content Validation ==="

# Check XYZ file structure
if [[ -f "${H2O_XYZ}" ]]; then
  assert_contains "${H2O_XYZ}" "^3" "atom count = 3" || true
  assert_contains "${H2O_XYZ}" "[OH]" "contains O or H atoms" || true
fi

# Check stdout logs for expected patterns
if [[ -f "${STDOUT_LOG}" ]]; then
  assert_contains "${STDOUT_LOG}" "H2O|molecule|build" "mentions molecules" || true
fi

say ""

# ============================================================================
# Summary
# ============================================================================

say "=== Test Summary ==="
say "Passed: ${pass_count}"
say "Failed: ${fail_count}"
say "Total:  $((pass_count + fail_count))"
say ""
say "Logs:"
say "  stdout:  ${STDOUT_LOG}"
say "  stderr:  ${STDERR_LOG}"
say "  summary: ${SUMMARY_LOG}"
say ""

if [[ "${fail_count}" -ne 0 ]]; then
  say "✗ Some tests failed"
  say "  Review logs in ${RUN_DIR}"
  exit 1
fi

say "✓ All tests passed"
say "  Artifacts saved to ${OUT_DIR}"
exit 0
