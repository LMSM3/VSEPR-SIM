#!/usr/bin/env bash
# tests/run_uc7_pipe_validation.sh
# UC-7 -- Resolved SiO2 Pipe Bridge validation
# Runs the resolved SiO2 pipe fixture and asserts the resolved-pipe report
# contains the expected material/geometry/bridge/validation tokens, with no
# real NaN/Inf in the output.
# Exit 0 = pass. Exit 1 = missing pattern or NaN/Inf.
# UC doc:  docs/uc/UC-7-resolved-sio2-pipe-bridge.md
# Fixture: scripts/uc7_resolved_sio2_pipe.vsim

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

echo "[UC-7] Resolved SiO2 Pipe Validation"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# On Windows the binary is build/vsepr.exe; fall back to build/vsepr on POSIX.
if [ -n "${VSIM_BIN:-}" ]; then
  BIN="$VSIM_BIN"
elif [ -x "./build/vsepr.exe" ]; then
  BIN="./build/vsepr.exe"
else
  BIN="./build/vsepr"
fi

SCRIPT="scripts/uc7_resolved_sio2_pipe.vsim"
OUT="exports/uc7_resolved_sio2_pipe"
LOG="$OUT/uc7_pipe_validation.log"

mkdir -p "$OUT"

# The fixture opens a lightweight viewer and completes with exit 0; capture all
# output. Do not let a non-zero exit abort silently -- report it explicitly.
if ! "$BIN" run "$SCRIPT" > "$LOG" 2>&1; then
  echo "[UC-7][FAIL] runtime failed (non-zero exit): $SCRIPT"
  echo "[UC-7] see $LOG"
  exit 1
fi

required_patterns=(
  "SiO2"
  "pipe"
  "linear"
  "bridge"
  "validation"
)

for pattern in "${required_patterns[@]}"; do
  if ! grep -Eiq "$pattern" "$LOG"; then
	echo "[UC-7][FAIL] missing pattern: $pattern"
	echo "[UC-7] see $LOG"
	exit 1
  fi
done

# Reject only real numeric NaN/Inf tokens -- word-boundary match so we do not
# trip on words like "inference" or "info".
if grep -Eqiw 'nan|inf|infinity' "$LOG"; then
  echo "[UC-7][FAIL] NaN/Inf detected"
  grep -Eniw 'nan|inf|infinity' "$LOG" | head -n 3
  echo "[UC-7] see $LOG"
  exit 1
fi

echo "[UC-7] artifacts in: $OUT"
echo "[UC-7] PASS"
