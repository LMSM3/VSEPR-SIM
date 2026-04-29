#!/bin/bash
# Setup and build script for WSL (Windows Subsystem for Linux)

set -e

echo "===================================="
echo "ImGui Abstraction - WSL/Linux Build"
echo "===================================="
echo ""

# Check if we're in WSL
if grep -qi microsoft /proc/version; then
    echo "Running in WSL environment"
    IS_WSL=1
else
    echo "Running in native Linux environment"
    IS_WSL=0
fi

echo ""

# Install dependencies
echo "Checking dependencies..."

if ! command -v g++ &> /dev/null; then
    echo "Installing build essentials..."
    sudo apt-get update
    sudo apt-get install -y build-essential
fi

if ! command -v cmake &> /dev/null; then
    echo "Installing CMake..."
    sudo apt-get install -y cmake
fi

# For graphical applications in WSL, we need X11
if [ $IS_WSL -eq 1 ]; then
    echo ""
    echo "Note: To run graphical applications in WSL, you need:"
    echo "  1. An X Server on Windows (e.g., VcXsrv, X410)"
    echo "  2. Set DISPLAY environment variable"
    echo ""
    echo "Install X11 development libraries..."
    sudo apt-get install -y libx11-dev libgl1-mesa-dev libglu1-mesa-dev
fi

# Check if running headless
if [ -z "$DISPLAY" ]; then
    echo ""
    echo "WARNING: DISPLAY not set. Graphical applications won't run."
    echo "For WSL, install an X Server on Windows and set:"
    echo "  export DISPLAY=:0"
    echo "or for WSL2:"
    echo "  export DISPLAY=\$(cat /etc/resolv.conf | grep nameserver | awk '{print \$2}'):0"
    echo ""
fi

# Create build directory
BUILD_DIR="build_wsl"
if [ ! -d "$BUILD_DIR" ]; then
    mkdir "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Configure with CMake
echo "Configuring project with CMake..."
cmake ..

# Build
echo "Building project..."
cmake --build . -j$(nproc)

cd ..

echo ""
echo "===================================="
echo "Build successful!"
echo "===================================="
echo "Executable: $BUILD_DIR/bin/imgui_abstraction_example"
echo ""
echo "To run:"
echo "  cd $BUILD_DIR/bin"
echo "  ./imgui_abstraction_example"
echo ""

if [ $IS_WSL -eq 1 ]; then
    echo "WSL Graphics Setup:"
    echo "  1. Install X Server on Windows (VcXsrv recommended)"
    echo "  2. Launch X Server with 'Disable access control' checked"
    echo "  3. Set DISPLAY: export DISPLAY=:0 (WSL1) or export DISPLAY=\$(cat /etc/resolv.conf | grep nameserver | awk '{print \$2}'):0 (WSL2)"
    echo "  4. Run the application"
fi
