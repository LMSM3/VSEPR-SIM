#!/usr/bin/env bash
# =============================================================================
# deploy/build_wsl.sh
# -------------------
# One-shot build of the vsepr Linux binary inside WSL.
# Run once; the binary stays at build-linux/vsepr.
#
# Usage (from PowerShell, using your default WSL distro):
#   wsl -- bash deploy/build_wsl.sh
#
# Usage (from PowerShell, targeting a specific distro):
#   wsl -d <YourDistroName> -- bash deploy/build_wsl.sh
#
# Or from inside WSL (from anywhere, path is resolved automatically):
#   bash /path/to/repo/deploy/build_wsl.sh
#
# List installed distros with:  wsl -l -v
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
# NOTE: CMakeLists.txt enforces the Ninja generator (see the guard at the top
# of that file), so `ninja` is required here even though Linux distros often
# default to Unix Makefiles.
for tool in gcc g++ cmake ninja; do
    if ! command -v $tool &>/dev/null; then
        echo "[ERROR] $tool not found. Install with:"
        echo "        sudo dnf install -y gcc-c++ cmake ninja-build   (AlmaLinux/RHEL)"
        echo "        sudo apt install -y g++ cmake ninja-build        (Ubuntu/Debian)"
        exit 1
    fi
done

echo "[build] Configuring (generator: Ninja)..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja -Wno-dev

echo ""
echo "[build] Compiling vsepr (using $(nproc) cores)..."
ninja vsepr

echo ""
echo "  Build complete: $BUILD_DIR/vsepr"
echo "  Run the server:"
echo "    bash $SCRIPT_DIR/start_viz_server.sh Ar -T 300 -N 64 --verbose"
echo ""

# ── rc injection: theme/customization + one-time confirmation banner ────────
# Idempotently wires deploy/vseprrc into ~/.bashrc so every new WSL shell
# sources it. This is the easiest way to visually confirm the install
# actually ran: open a new shell and you should see a "[vsepr]" prompt
# prefix and a "[vseprrc] theme ... loaded" banner.
BASHRC="$HOME/.bashrc"
MARKER="# >>> vsepr-sim rc injection >>>"
if [ -f "$BASHRC" ] && grep -qF "$MARKER" "$BASHRC" 2>/dev/null; then
    echo "[rc] vseprrc already injected into $BASHRC"
else
    {
        echo ""
        echo "$MARKER"
        echo "export VSEPR_REPO_ROOT=\"$REPO_ROOT\""
        echo "source \"$REPO_ROOT/deploy/vseprrc\""
        echo "# <<< vsepr-sim rc injection <<<"
    } >> "$BASHRC"
    echo "[rc] Injected vseprrc into $BASHRC (open a new shell to see it take effect)"
fi
