#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LABELME="$ROOT/scripts/labelme.sh"
DB="$ROOT/data/states_db.csv"

# ---------- tiny helpers ----------
fail() { echo "[FAIL] $*" >&2; exit 1; }
ok()   { echo "[ OK ] $*"; }

require_file() { [[ -f "$1" ]] || fail "missing file: $1"; }
require_exec() { [[ -x "$1" ]] || fail "not executable: $1"; }

trim() { sed -e 's/[[:space:]]*$//' -e 's/^[[:space:]]*//'; }

assert_eq() {
  local got="$1" exp="$2" msg="${3:-}"
  [[ "$got" == "$exp" ]] || fail "${msg} expected='$exp' got='$got'"
}

assert_contains() {
  local hay="$1" needle="$2" msg="${3:-}"
  grep -Fq "$needle" <<<"$hay" || fail "${msg} missing='$needle'"
}

csv_state_field() {
  # expects a single CSV data line (no header)
  # molecule,tempK,state,meltK,boilK,plasmaK
  awk -F',' '{print $3}' <<<"$1" | trim
}

csv_header_ok() {
  local hdr="$1"
  assert_eq "$hdr" "molecule,tempK,state,meltK,boilK,plasmaK" "bad header:"
}

# ---------- tests ----------
test_build() {
  "$LABELME" build >/dev/null
  require_file "$ROOT/build/bin/labelme"
  require_exec "$ROOT/build/bin/labelme"
  ok "build produces build/bin/labelme"
}

test_db_sanity() {
  require_file "$DB"

  # Skip header; ensure 4 columns
  local bad
  bad="$(tail -n +2 "$DB" | awk -F',' 'NF!=4 {print NR ":" $0}')"
  [[ -z "$bad" ]] || fail "DB schema invalid (expected 4 cols):\n$bad"

  # No duplicate molecule keys
  local dups
  dups="$(tail -n +2 "$DB" | awk -F',' '{print $1}' | sort | uniq -d)"
  [[ -z "$dups" ]] || fail "duplicate molecule keys in DB:\n$dups"

  ok "database sanity (cols, duplicates)"
}

test_single_labels() {
  # H2O @ 298.15 -> LIQUID
  local out line hdr state
  out="$("$LABELME" label H2O 298.15)"
  hdr="$(head -n 1 <<<"$out" | trim)"
  line="$(tail -n 1 <<<"$out" | trim)"
  csv_header_ok "$hdr"
  state="$(csv_state_field "$line")"
  assert_eq "$state" "LIQUID" "H2O 298.15 state mismatch:"
  ok "H2O 298.15 -> LIQUID"

  # H2O @ 250 -> SOLID
  out="$("$LABELME" label H2O 250)"
  line="$(tail -n 1 <<<"$out" | trim)"
  state="$(csv_state_field "$line")"
  assert_eq "$state" "SOLID" "H2O 250 state mismatch:"
  ok "H2O 250 -> SOLID"

  # H2O @ 450 -> GAS
  out="$("$LABELME" label H2O 450)"
  line="$(tail -n 1 <<<"$out" | trim)"
  state="$(csv_state_field "$line")"
  assert_eq "$state" "GAS" "H2O 450 state mismatch:"
  ok "H2O 450 -> GAS"
}

test_range_basic() {
  local out hdr nrows
  out="$("$LABELME" range I2 300 600 50)"
  hdr="$(head -n 1 <<<"$out" | trim)"
  csv_header_ok "$hdr"

  # should be 7 data rows for 300..600 step 50 (inclusive)
  nrows="$(tail -n +2 <<<"$out" | wc -l | tr -d ' ')"
  assert_eq "$nrows" "7" "range row count mismatch:"
  ok "range row count ok (I2 300..600 step 50)"

  # ensure we see SOLID, LIQUID, GAS at least once
  assert_contains "$out" ",SOLID," "range missing SOLID:"
  assert_contains "$out" ",LIQUID," "range missing LIQUID:"
  assert_contains "$out" ",GAS," "range missing GAS:"
  ok "range contains expected phase states"
}

test_unknown_molecule_fails() {
  set +e
  "$LABELME" label NOT_A_REAL_MOL 300 >/dev/null 2>&1
  local rc=$?
  set -e
  [[ $rc -ne 0 ]] || fail "expected non-zero exit for unknown molecule"
  ok "unknown molecule returns non-zero exit"
}

# ---------- main ----------
main() {
  echo "[INFO] LabelMe automated test suite"
  require_file "$LABELME"
  require_file "$DB"

  test_build
  test_db_sanity
  test_single_labels
  test_range_basic
  test_unknown_molecule_fails

  echo "[SUCCESS] All tests passed."
}

main "$@"
