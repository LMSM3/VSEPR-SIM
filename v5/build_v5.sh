#!/usr/bin/env bash
# build_v5.sh — Build V5 Environment-Responsive Bead Transport Demos
# VSEPR-SIM V5.0 | 2026-04-16
#
# Usage (from project root):
#   bash v5/build_v5.sh           # build both demo binaries
#   bash v5/build_v5.sh run       # build + run v5_demo (phases A-C)
#   bash v5/build_v5.sh run3d     # build + run v5_3d_demos (phases D-H)
#   bash v5/build_v5.sh runall    # build + run both

set -euo pipefail
cd "$(dirname "$0")/.."

CXX_FLAGS="-std=c++23 -O2 -Wall -Wextra -ftrivial-auto-var-init=pattern -I."

echo "═══════════════════════════════════════════"
echo "  Building V5 Demo (C++23, GCC)"
echo "═══════════════════════════════════════════"

g++ ${CXX_FLAGS} v5/v5_demo.cpp      -o v5/v5_demo
echo "  → v5/v5_demo built OK"

g++ ${CXX_FLAGS} v5/v5_3d_demos.cpp  -o v5/v5_3d_demos
echo "  → v5/v5_3d_demos built OK"

echo ""

case "${1:-}" in
  run)
	echo "── Running v5_demo (Phases A–C) ──"
	./v5/v5_demo
	;;
  run3d)
	echo "── Running v5_3d_demos (Phases D–H) ──"
	./v5/v5_3d_demos
	;;
  runall)
	echo "── Running v5_demo (Phases A–C) ──"
	./v5/v5_demo
	echo ""
	echo "── Running v5_3d_demos (Phases D–H) ──"
	./v5/v5_3d_demos
	;;
esac
