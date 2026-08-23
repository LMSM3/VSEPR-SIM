#!/usr/bin/env bash
# tests/run_uc6_golden_suite.sh
# UC-6 -- Golden Suite Dispatcher (format-aware regression gate)
# Runs the Day-85 core golden scripts, captures stdout+stderr per script,
# and gates on: exit code 0, no real NaN/Inf, and energy output present.
# Exit 0 = all pass. Exit 1 = at least one failure.
# UC doc:  docs/uc/UC-6-golden-suite-dispatcher.md
# Fixture: scripts/uc6_golden_suite_dispatcher.vsim

# When launched by CTest via an absolute bash path (MSYS2/Git), the interpreter
# directory may not be on PATH, so coreutils (dirname, mkdir, grep) are missing.
# Prepend the running bash's own bin dir using only shell built-ins.
_bash_dir="${BASH%/*}"
case ":$PATH:" in
  *":$_bash_dir:"*) : ;;
  *) PATH="$_bash_dir:$PATH" ;;
esac
export PATH

set -euo pipefail

echo "[UC-6] Golden Suite Dispatcher"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

OUT="exports/uc6_golden_suite"
mkdir -p "$OUT"

# On Windows the binary is build/vsepr.exe; fall back to build/vsepr on POSIX.
if [ -n "${VSIM_BIN:-}" ]; then
  BIN="$VSIM_BIN"
elif [ -x "./build/vsepr.exe" ]; then
  BIN="./build/vsepr.exe"
else
  BIN="./build/vsepr"
fi

SCRIPTS=(
  "scripts/demo_stress_format_a_small.vsim"
  "scripts/demo_stress_format_b_medium.vsim"
  "scripts/demo_stress_format_d_large.vsim"
  "scripts/demo_stress_gas_injection.vsim"
)

failures=0

for script in "${SCRIPTS[@]}"; do
  name="$(basename "$script" .vsim)"
  log="$OUT/${name}.log"

  echo "[UC-6] run $script"

  # require_exit_zero
  if ! "$BIN" run "$script" > "$log" 2>&1; then
	echo "[UC-6][FAIL] runtime failed (non-zero exit): $script"
	failures=$((failures + 1))
	continue
  fi

  # reject_nan / reject_inf -- word-boundary match so we do NOT trip on words
  # like "inference" or "info" that legitimately contain "inf".
  if grep -Eqiw 'nan|inf|infinity' "$log"; then
	echo "[UC-6][FAIL] NaN/Inf detected: $script"
	grep -Eniw 'nan|inf|infinity' "$log" | head -n 3
	failures=$((failures + 1))
	continue
  fi

  # require_energy -- the step loop prints energy as "E=" columns; also accept
  # the literal word "energy" for scripts that spell it out.
  if ! grep -Eqi 'E=|energy' "$log"; then
	echo "[UC-6][FAIL] missing energy output: $script"
	failures=$((failures + 1))
	continue
  fi

  echo "[UC-6][PASS] $script"
done

echo "[UC-6] artifacts in: $OUT"

if (( failures > 0 )); then
  echo "[UC-6] FAILED with $failures failure(s)"
  exit 1
fi

echo "[UC-6] PASS"
