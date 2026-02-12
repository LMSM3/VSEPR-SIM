#!/usr/bin/env bash
################################################################################
# Build VSEPR OpenGL Viewer with Batch Processing
# Demonstrates 10,000 random molecule generation with visualization updates
################################################################################

set -e

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  VSEPR OpenGL Viewer - Batch Processing Demo                  ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# Check for g++
if ! command -v g++ &> /dev/null; then
    echo "Error: g++ not found. Please install build tools."
    echo ""
    echo "Installation:"
    echo "  Ubuntu/Debian: sudo apt install build-essential"
    echo "  macOS:         xcode-select --install"
    echo "  Fedora:        sudo dnf install gcc-c++"
    exit 1
fi

echo "[1/3] Compiling vsepr_opengl_viewer.cpp..."
echo ""

g++ -std=c++17 -O2 \
    vsepr_opengl_viewer.cpp \
    -o vsepr_opengl_viewer \
    -I../include \
    -I../third_party/glm \
    -pthread

echo ""
echo "✓ Compilation successful!"
echo ""
echo "[2/3] Executable created: vsepr_opengl_viewer"
echo ""
echo "[3/3] Usage:"
echo "  ./vsepr_opengl_viewer [batch_size] [visualization_mode]"
echo ""
echo "Examples:"
echo "  ./vsepr_opengl_viewer              (Demo mode with 10,000 molecules)"
echo "  ./vsepr_opengl_viewer 5000         (5,000 molecules, every other visualized)"
echo "  ./vsepr_opengl_viewer 10000 all    (10,000 molecules, all visualized)"
echo ""
echo "════════════════════════════════════════════════════════════════"
echo ""

# Ask if user wants to run now
read -p "Run viewer now? (y/n): " RUN
if [[ "$RUN" =~ ^[Yy]$ ]]; then
    echo ""
    echo "Starting VSEPR OpenGL Viewer..."
    echo ""
    ./vsepr_opengl_viewer
fi

echo ""
echo "Done!"
