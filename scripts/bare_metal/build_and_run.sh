#!/usr/bin/env bash
# =============================================================================
# scripts/bare_metal/build_and_run.sh
# ------------------------------------
# Build and run both C++23/26 bare-metal demo scripts in WSL.
#
# Usage (from PowerShell):
#   wsl -d AlmaLinux-10 -- bash /mnt/c/R/VSPER-SIM/scripts/bare_metal/build_and_run.sh
#
# Or from inside WSL:
#   cd /mnt/c/R/VSPER-SIM/scripts/bare_metal && bash build_and_run.sh
# =============================================================================

set -e
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

echo ""
echo "  ╔══════════════════════════════════════════════════════════╗"
echo "  ║    VSEPR-SIM  C++23/26 Bare Metal Scripts               ║"
echo "  ║    AlmaLinux 10 · GCC $(gcc --version | head -1 | grep -oP '\d+\.\d+\.\d+')                        ║"
echo "  ╚══════════════════════════════════════════════════════════╝"
echo ""

FLAGS="-std=c++23 -O2 -ftrivial-auto-var-init=pattern -Wall -Wextra"

echo "[1/4] Building matrix_demo..."
g++ $FLAGS matrix_demo.cpp -o matrix_demo
echo "       OK"

echo "[2/4] Building contracts_demo..."
g++ $FLAGS contracts_demo.cpp -lstdc++exp -o contracts_demo
echo "       OK"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "[3/4] Running matrix_demo..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
./matrix_demo

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "[4/4] Running contracts_demo..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
./contracts_demo

echo ""
echo "  All done."
