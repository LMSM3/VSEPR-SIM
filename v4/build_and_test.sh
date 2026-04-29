#!/usr/bin/env bash
# v4/build_and_test.sh — Build + run V4 Beta score validation suite
# ═══════════════════════════════════════════════════════════════════
# Usage:  bash v4/build_and_test.sh
#
# C++26 features:
#   -ftrivial-auto-var-init=pattern  (erroneous-behaviour trapping)
#   -std=c++23                       (GCC 14 C++23 mode, contracts emulated)
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

echo "═══════════════════════════════════════════════════════════════"
echo "  V4 Beta Build — VSEPR-SIM Day #47"
echo "  Compiler: $(g++ --version | head -1)"
echo "═══════════════════════════════════════════════════════════════"

g++ -std=c++23 -O2 -ftrivial-auto-var-init=pattern \
    -Wall -Wextra -Wpedantic \
    -I. \
    v4/v4_scores_test.cpp \
    -o v4/v4_scores_test

echo "  [BUILD] v4_scores_test OK"
echo ""

./v4/v4_scores_test
