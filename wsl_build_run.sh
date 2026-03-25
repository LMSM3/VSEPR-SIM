#!/usr/bin/env bash
# wsl_build_run.sh — Build and launch the animated CG ensemble proxy demo
#
# Usage:
#   bash wsl_build_run.sh               # configure + build + launch viewer
#   bash wsl_build_run.sh --build-only  # configure + build, do not launch
#   bash wsl_build_run.sh --headless    # build + run in headless (no display) mode
#   bash wsl_build_run.sh --clean       # wipe build dir, then build + launch
#   bash wsl_build_run.sh --help        # show this message
#
# Requires:
#   cmake, gcc/g++, GLFW, GLEW, OpenGL (mesa-dev), X11 server or WSLg
#   On WSLg: DISPLAY is set automatically. On VcXsrv: export DISPLAY=:0 first.
#
# The script always configures with:
#   BUILD_VIS=ON   — enable GLFW/ImGui viewer
#   BUILD_TESTS=OFF — skip test targets for faster build
#   BUILD_GUI=OFF  — skip prototype GUI
#   CMAKE_BUILD_TYPE=RelWithDebInfo

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build_wsl"
JOBS=$(nproc 2>/dev/null || echo 4)
MODE="run"

for arg in "$@"; do
    case "$arg" in
        --build-only) MODE="build"    ;;
        --headless)   MODE="headless" ;;
        --clean)      MODE="clean"    ;;
        --help|-h)
            sed -n "2,12p" "$0" | sed "s/^# //" | sed "s/^#//"
            exit 0
            ;;
    esac
done

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║   VSEPR-SIM  WSL Demo Build + Launch                        ║"
echo "║   Target: cg-anim-demo  (ensemble proxy animation)          ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

if [[ "${MODE}" == "clean" ]]; then
    echo "▸ Cleaning build directory: ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
    MODE="run"
fi

echo "▸ Configuring  (BUILD_VIS=ON  BUILD_TESTS=OFF  BUILD_GUI=OFF)"
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBUILD_APPS=ON \\\
    -DBUILD_VIS=ON  \\
    -DBUILD_TESTS=OFF \
    -DBUILD_GUI=OFF  \
    2>&1 | grep -E "^(--|Configuring|CMake Error|CMake Warning|Build type)" || true
echo ""

echo "▸ Building cg-anim-demo  (j${JOBS})"
cmake --build "${BUILD_DIR}" --target cg-anim-demo --parallel "${JOBS}"
echo ""

BINARY="${BUILD_DIR}/cg-anim-demo"
if [[ ! -f "${BINARY}" ]]; then
    echo "x Build failed — binary not found: ${BINARY}"
    exit 1
fi
echo "✓ Build complete: ${BINARY}"
echo ""

[[ "${MODE}" == "build" ]] && exit 0

if [[ "${MODE}" == "headless" ]]; then
    echo "▸ Running headless  (proxy table only, no viewer)"
    echo ""
    "${BINARY}" --headless
    exit $?
fi

if [[ -z "${DISPLAY:-}" ]]; then
    export DISPLAY=:0
    echo "  (DISPLAY not set — defaulting to :0; start VcXsrv or use WSLg)"
fi
echo "▸ Launching viewer  (DISPLAY=${DISPLAY})"
echo "  Controls: right-drag=orbit  scroll=zoom  O=cycle overlay  ESC=next scene"
echo ""
"${BINARY}"

