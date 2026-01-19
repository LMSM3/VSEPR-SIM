#!/bin/bash
# debug.sh
# Universal debugging script for VSEPR-Sim

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

echo -e "${MAGENTA}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${MAGENTA}║  VSEPR-Sim Debug & Diagnostic Tool                            ║${NC}"
echo -e "${MAGENTA}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Parse mode
MODE="${1:-info}"

case "$MODE" in
    info|--info|-i)
        echo -e "${CYAN}═══ System Information ═══${NC}"
        echo ""
        
        echo -e "${YELLOW}▶ OS:${NC}"
        uname -a
        echo ""
        
        echo -e "${YELLOW}▶ CMake:${NC}"
        cmake --version 2>/dev/null || echo "  Not found"
        echo ""
        
        echo -e "${YELLOW}▶ Compiler:${NC}"
        g++ --version 2>/dev/null | head -1 || echo "  g++ not found"
        clang++ --version 2>/dev/null | head -1 || echo "  clang++ not found"
        echo ""
        
        echo -e "${YELLOW}▶ Build directory:${NC}"
        if [ -d "build" ]; then
            echo "  ✓ Exists"
            ls -lh build/bin/ 2>/dev/null || echo "  No binaries yet"
        else
            echo "  ✗ Not found (run ./build_universal.sh)"
        fi
        echo ""
        
        echo -e "${YELLOW}▶ Binary status:${NC}"
        if [ -f "build/bin/vsepr" ]; then
            echo "  ✓ vsepr binary found"
            ls -lh build/bin/vsepr
        else
            echo "  ✗ vsepr binary not found"
        fi
        echo ""
        ;;
        
    build|--build|-b)
        echo -e "${CYAN}═══ Debug Build ═══${NC}"
        echo ""
        ./build_universal.sh --debug --verbose
        ;;
        
    test|--test|-t)
        echo -e "${CYAN}═══ Running Tests ═══${NC}"
        echo ""
        
        if [ ! -f "build/bin/vsepr" ]; then
            echo -e "${RED}✗ Binary not found, building first...${NC}"
            ./build_universal.sh --target vsepr
        fi
        
        echo -e "${YELLOW}▶ Testing vsepr command:${NC}"
        ./build/bin/vsepr --version
        echo ""
        
        echo -e "${YELLOW}▶ Testing build command:${NC}"
        ./build/bin/vsepr build H2O --output /tmp/test_water.xyz
        echo ""
        
        echo -e "${YELLOW}▶ Testing therm command:${NC}"
        if [ -f "/tmp/test_water.xyz" ]; then
            ./build/bin/vsepr therm /tmp/test_water.xyz --temperature 298.15
        else
            echo -e "${RED}  ✗ Test molecule not created${NC}"
        fi
        ;;
        
    clean|--clean|-c)
        echo -e "${CYAN}═══ Cleaning Build ═══${NC}"
        echo ""
        
        echo -e "${YELLOW}▶ Removing build directory...${NC}"
        rm -rf build/
        echo -e "${GREEN}✓ Cleaned${NC}"
        
        echo -e "${YELLOW}▶ Removing temp files...${NC}"
        find . -name "*.o" -delete 2>/dev/null
        find . -name "*.a" -delete 2>/dev/null
        echo -e "${GREEN}✓ Cleaned${NC}"
        echo ""
        ;;
        
    rebuild|--rebuild|-r)
        echo -e "${CYAN}═══ Clean Rebuild ═══${NC}"
        echo ""
        ./build_universal.sh --clean --verbose
        ;;
        
    thermal|--thermal|-th)
        echo -e "${CYAN}═══ Thermal System Debug ═══${NC}"
        echo ""
        ./test_thermal.sh
        ;;
        
    colocation|--colocation|-col)
        echo -e "${CYAN}═══ Colocation Validation Test ═══${NC}"
        echo ""
        
        if [ ! -f "build/bin/vsepr" ]; then
            echo -e "${RED}✗ Building first...${NC}"
            ./build_universal.sh --target vsepr
            echo ""
        fi
        
        echo -e "${YELLOW}▶ Testing colocation prevention:${NC}"
        echo "  This should fail with colocation error..."
        
        # Try to build Br2 which previously had colocation bug
        ./build/bin/vsepr build "Br2" --output /tmp/test_br2.xyz
        
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}✓ Br2 built successfully (2 atoms)${NC}"
            cat /tmp/test_br2.xyz
        else
            echo -e "${RED}✗ Build failed${NC}"
        fi
        echo ""
        ;;
        
    help|--help|-h)
        echo "Usage: ./debug.sh [mode]"
        echo ""
        echo "Modes:"
        echo "  info, -i        Show system information (default)"
        echo "  build, -b       Debug build with verbose output"
        echo "  test, -t        Run quick functionality tests"
        echo "  clean, -c       Clean build artifacts"
        echo "  rebuild, -r     Clean rebuild"
        echo "  thermal, -th    Test thermal properties system"
        echo "  colocation, -col Test colocation validation"
        echo "  help, -h        Show this help"
        echo ""
        echo "Examples:"
        echo "  ./debug.sh info"
        echo "  ./debug.sh build"
        echo "  ./debug.sh thermal"
        ;;
        
    *)
        echo -e "${RED}✗ Unknown mode: $MODE${NC}"
        echo "Run './debug.sh help' for usage"
        exit 1
        ;;
esac

echo ""
echo -e "${GREEN}═══ Debug session complete ═══${NC}"
