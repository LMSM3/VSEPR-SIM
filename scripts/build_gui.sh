#!/usr/bin/env bash
################################################################################
# VSEPR-Sim Elevated GUI - Quick Build & Run
# Builds and launches the visual GUI application
################################################################################

set -euo pipefail

GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${CYAN}${BOLD}"
cat << 'EOF'
╔════════════════════════════════════════════════════════════════╗
║  VSEPR-Sim Elevated GUI - Build & Launch                     ║
╚════════════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

# Check dependencies
echo -e "${CYAN}[1/4]${NC} Checking dependencies..."

check_dep() {
    if command -v "$1" &> /dev/null; then
        echo -e "  ${GREEN}✓${NC} $1 found"
        return 0
    else
        echo -e "  ${RED}✗${NC} $1 not found"
        return 1
    fi
}

MISSING=0
check_dep cmake || ((MISSING++))
check_dep g++ || ((MISSING++))
check_dep pkg-config || ((MISSING++))

if ! pkg-config --exists glfw3; then
    echo -e "  ${RED}✗${NC} GLFW3 not found"
    echo -e "  ${YELLOW}Install:${NC} sudo apt-get install libglfw3-dev"
    ((MISSING++))
else
    echo -e "  ${GREEN}✓${NC} GLFW3 found"
fi

if ! pkg-config --exists gl; then
    echo -e "  ${RED}✗${NC} OpenGL not found"
    echo -e "  ${YELLOW}Install:${NC} sudo apt-get install libgl1-mesa-dev"
    ((MISSING++))
else
    echo -e "  ${GREEN}✓${NC} OpenGL found"
fi

if [[ $MISSING -gt 0 ]]; then
    echo ""
    echo -e "${RED}Missing $MISSING required dependencies${NC}"
    echo ""
    echo "Install all dependencies:"
    echo "  sudo apt-get install cmake g++ libglfw3-dev libgl1-mesa-dev"
    exit 1
fi

# Check ImGui
echo ""
echo -e "${CYAN}[2/4]${NC} Checking ImGui..."

if [[ ! -d "third_party/imgui" ]]; then
    echo -e "  ${RED}✗${NC} ImGui not found at third_party/imgui"
    echo ""
    echo "Clone ImGui:"
    echo "  git submodule update --init --recursive"
    echo "OR:"
    echo "  cd third_party"
    echo "  git clone https://github.com/ocornut/imgui"
    exit 1
else
    echo -e "  ${GREEN}✓${NC} ImGui found"
fi

# Configure CMake
echo ""
echo -e "${CYAN}[3/4]${NC} Configuring build..."

mkdir -p build
cd build

if cmake .. -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_CXX_FLAGS="-O3 -march=native" \
            > /dev/null 2>&1; then
    echo -e "  ${GREEN}✓${NC} CMake configuration successful"
else
    echo -e "  ${RED}✗${NC} CMake configuration failed"
    echo ""
    echo "Try manual configuration:"
    echo "  cmake -B build"
    exit 1
fi

# Build
echo ""
echo -e "${CYAN}[4/4]${NC} Building..."

if cmake --build . --parallel $(nproc 2>/dev/null || echo 4) > /dev/null 2>&1; then
    echo -e "  ${GREEN}✓${NC} Build successful"
else
    echo -e "  ${RED}✗${NC} Build failed"
    echo ""
    echo "Check build errors:"
    echo "  cmake --build build"
    exit 1
fi

cd ..

# Success
echo ""
echo -e "${GREEN}${BOLD}"
cat << 'EOF'
╔════════════════════════════════════════════════════════════════╗
║  ✅ Build Complete!                                            ║
╚════════════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

echo "Binaries built:"
ls -lh build/bin/vsepr_gui_* 2>/dev/null || echo "  (check build directory)"

echo ""
echo "Launch GUI:"
echo "  ./build/bin/vsepr_gui_elevated"
echo ""
echo "Run simple demo:"
echo "  ./build/bin/vsepr_gui_demo"
echo ""

# Auto-launch if requested
if [[ "${1:-}" == "--launch" || "${1:-}" == "-l" ]]; then
    echo "Launching GUI..."
    ./build/bin/vsepr_gui_elevated
fi
