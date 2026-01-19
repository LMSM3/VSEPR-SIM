#!/bin/bash
# build_multiscale.sh
# Build and test the multiscale integration module

set -e  # Exit on error

echo ""
echo "╔═══════════════════════════════════════════════════════════╗"
echo "║  MULTISCALE INTEGRATION BUILD                             ║"
echo "║  GPU Resource Manager + MD↔FEA Bridge                    ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Create build directory
echo -e "${BLUE}[1/4]${NC} Creating build directory..."
mkdir -p build
cd build

# Configure CMake
echo -e "${BLUE}[2/4]${NC} Configuring CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build multiscale_demo
echo -e "${BLUE}[3/4]${NC} Building multiscale_demo..."
make multiscale_demo -j8

# Check if build succeeded
if [ -f "multiscale_demo" ]; then
    echo ""
    echo -e "${GREEN}✓ Build successful!${NC}"
    echo ""
    echo "╔═══════════════════════════════════════════════════════════╗"
    echo "║  READY TO RUN                                             ║"
    echo "╠═══════════════════════════════════════════════════════════╣"
    echo "║  Run all demos:                                           ║"
    echo "║    ./multiscale_demo                                      ║"
    echo "║                                                           ║"
    echo "║  Run specific demo:                                       ║"
    echo "║    ./multiscale_demo 1  # GPU conflict prevention         ║"
    echo "║    ./multiscale_demo 2  # Property extraction             ║"
    echo "║    ./multiscale_demo 3  # Safe transition                 ║"
    echo "║    ./multiscale_demo 4  # Status monitoring               ║"
    echo "║    ./multiscale_demo 5  # Automated workflow              ║"
    echo "╚═══════════════════════════════════════════════════════════╝"
    echo ""
    
    # Run demo 1 automatically
    echo -e "${BLUE}[4/4]${NC} Running Demo 1: GPU Conflict Prevention..."
    echo ""
    ./multiscale_demo 1
    
else
    echo ""
    echo -e "${YELLOW}⚠ Build completed but multiscale_demo not found${NC}"
    exit 1
fi
