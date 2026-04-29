#!/usr/bin/env bash
# tools/spinner.sh
# Formation Engine v4.1.0 -- optional CLI status animation wrapper
#
# Wraps a running C++ subprocess PID with a terminal spinner.
# The C++ binary (e.g. uff_smoketest) is the authoritative status source.
# This script only animates that something is still running.
#
# Usage:
#   ./tools/spinner.sh <pid> <label>
#
# Example:
#   ./uff_smoketest output &
#   ./tools/spinner.sh $! "Building UFF table"
#
# The spinner must NOT make decisions about solver state, validation state,
# or physics state.  It observes the PID exit code only.

set -euo pipefail

pid="${1:?Usage: spinner.sh <pid> <label>}"
label="${2:-Working}"

frames=("◐" "◓" "◑" "◒")
i=0

while kill -0 "$pid" 2>/dev/null; do
	printf "\r[UFX] %s %s  " "$label" "${frames[$i]}"
	i=$(( (i + 1) % ${#frames[@]} ))
	sleep 0.15
done

wait "$pid"
status=$?

if [ "$status" -eq 0 ]; then
	printf "\r[UFX] %s ✓\n" "$label"
else
	printf "\r[UFX] %s ✗  (exit %d)\n" "$label" "$status"
fi

exit "$status"
