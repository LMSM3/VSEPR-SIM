#!/usr/bin/env bash
# =============================================================================
# deploy/build_wsl.sh
# -------------------
# One-shot build of the vsepr Linux binary inside WSL.
# Run once; the binary stays at build-linux/vsepr.
#
# Usage (from PowerShell):
#   wsl -d AlmaLinux-10 -- bash /mnt/c/R/VSPER-SIM/deploy/build_wsl.sh
#
# Or from inside WSL:
#   bash ~/vsper-sim/deploy/build_wsl.sh
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build-linux"

echo ""
echo "  VSEPR-SIM WSL Linux Build"
echo "  ─────────────────────────"
echo "  Repo  : $REPO_ROOT"
echo "  Build : $BUILD_DIR"
echo ""

# Dependency check
for tool in gcc g++ cmake make; do
    if ! command -v $tool &>/dev/null; then
        echo "[ERROR] $tool not found. Install with:"
        echo "        sudo dnf install -y gcc-c++ cmake make   (AlmaLinux/RHEL)"
        echo "        sudo apt install -y g++ cmake make        (Ubuntu/Debian)"
        exit 1
    fi
done

echo "[build] Configuring..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" -Wno-dev 2>&1 | grep -E "^--.*target|error|warning|Configuring done|Generating done" || true

echo ""
echo "[build] Compiling vsepr (using $(nproc) cores)..."
make vsepr -j$(nproc)

echo ""
echo "  Build complete: $BUILD_DIR/vsepr"
echo "  Run the server:"
echo "    bash $SCRIPT_DIR/start_viz_server.sh Ar -T 300 -N 64 --verbose"
echo ""
