#!/usr/bin/env bash
# v4/run_core.sh — Launch the V4 continual formation engine
# ══════════════════════════════════════════════════════════
# Runs core_run in the foreground (Ctrl+C to stop cleanly).
# On exit: v4_final_heatmap.html + v4_final_correlation.csv are written.
#
# Usage (from WSL):
#   bash v4/run_core.sh
#
# Usage (from Windows PowerShell):
#   wsl -d AlmaLinux-10 -- bash /mnt/c/R/VSPER-SIM/v4/run_core.sh
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

# Rebuild if stale
if [ ! -f v4/core_run ] || [ v4/core_run.cpp -nt v4/core_run ]; then
    echo "[BUILD] Rebuilding v4/core_run..."
    g++ -std=c++23 -O3 -ftrivial-auto-var-init=pattern \
        -I. v4/core_run.cpp -o v4/core_run
    echo "[BUILD] Done."
fi

echo "[CORE]  Starting V4 continual formation engine..."
echo "[CORE]  Output files: v4_live_heatmap.html, v4_live_correlation.csv"
echo "[CORE]  Ctrl+C to stop cleanly."
echo ""

exec ./v4/core_run
