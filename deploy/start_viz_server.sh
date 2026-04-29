#!/usr/bin/env bash
# =============================================================================
# deploy/start_viz_server.sh
# --------------------------
# Launch the vsepr viz stream server inside WSL and optionally start nginx.
#
# Usage (inside WSL):
#   bash deploy/start_viz_server.sh [FORMULA] [EXTRA_ARGS...]
#
# Examples:
#   bash deploy/start_viz_server.sh Ar
#   bash deploy/start_viz_server.sh N2 -T 300 -N 125 --verbose
#   bash deploy/start_viz_server.sh CO2 --T-start 200 --T-end 800 -N 64
#
# The script:
#   1. Optionally starts nginx as the TCP proxy (--nginx flag)
#   2. Launches vsepr viz on ports 9999 (atomic) and 10001 (analysis)
#   3. Windows Python viewers connect to localhost:9999 / localhost:10001
#      (WSL2 auto-forwards these ports to the Windows host)
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BINARY="$REPO_ROOT/build-linux/vsepr"

# ── Defaults ─────────────────────────────────────────────────────────────────
FORMULA="${1:-Ar}"
shift 2>/dev/null || true
EXTRA_ARGS="$@"
USE_NGINX=0

# ── Parse --nginx flag ────────────────────────────────────────────────────────
FILTERED_ARGS=""
for arg in $EXTRA_ARGS; do
    if [ "$arg" = "--nginx" ]; then
        USE_NGINX=1
    else
        FILTERED_ARGS="$FILTERED_ARGS $arg"
    fi
done
EXTRA_ARGS="$FILTERED_ARGS"

# ── Banner ────────────────────────────────────────────────────────────────────
echo ""
echo "  ╔══════════════════════════════════════════════════════════╗"
echo "  ║       VSEPR-SIM  Dual-Port 3D Live Render Server        ║"
echo "  ╠══════════════════════════════════════════════════════════╣"
echo "  ║  Port  9999  →  Atomic View     (15 fps NDJSON)         ║"
echo "  ║  Port 10001  →  Analysis View    (2 fps NDJSON)         ║"
echo "  ╠══════════════════════════════════════════════════════════╣"
echo "  ║  Formula : $FORMULA"
echo "  ║  Args    : $EXTRA_ARGS"
echo "  ╚══════════════════════════════════════════════════════════╝"
echo ""

# ── Check binary ──────────────────────────────────────────────────────────────
if [ ! -f "$BINARY" ]; then
    echo "[ERROR] Binary not found: $BINARY"
    echo "        Run: cd $REPO_ROOT && mkdir -p build-linux && cd build-linux"
    echo "             cmake .. -DCMAKE_BUILD_TYPE=Release && make vsepr -j\$(nproc)"
    exit 1
fi

# ── Nginx (optional) ─────────────────────────────────────────────────────────
if [ "$USE_NGINX" = "1" ]; then
    NGINX_CONF="$SCRIPT_DIR/nginx-viz.conf"
    echo "[nginx] Starting with $NGINX_CONF ..."
    # Stop any existing viz nginx instance
    nginx -s stop -c "$NGINX_CONF" 2>/dev/null || true
    sleep 0.5
    nginx -c "$NGINX_CONF"
    echo "[nginx] Running (PID: $(cat /tmp/nginx-viz.pid 2>/dev/null || echo unknown))"
    echo ""
fi

# ── Launch server ─────────────────────────────────────────────────────────────
echo "[vsepr] Starting: vsepr viz $FORMULA $EXTRA_ARGS"
echo "[vsepr] Viewers:"
echo "          python tools/viz_atomic.py        (Windows terminal)"
echo "          python tools/viz_analysis.py      (Windows terminal)"
echo ""
exec "$BINARY" viz "$FORMULA" $EXTRA_ARGS
