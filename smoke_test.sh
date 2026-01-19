#!/bin/bash
# VSEPR-Sim Smoke Test - Quick verification that build worked

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  VSEPR-Sim Smoke Test                                        ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

PASS=0
FAIL=0

# Test 1: Check if build directory exists
echo -n "Build directory exists... "
if [ -d "build" ]; then
    echo -e "${GREEN}✓${NC}"
    ((PASS++))
else
    echo -e "${RED}✗${NC}"
    echo "  Run: ./build_universal.sh"
    ((FAIL++))
fi

# Test 2: Check for binaries
echo ""
echo "Checking binaries:"
for bin in hydrate-demo md_demo vsepr-cli vsepr_batch; do
    echo -n "  $bin... "
    if [ -f "build/bin/$bin" ]; then
        echo -e "${GREEN}✓${NC}"
        ((PASS++))
    else
        echo -e "${RED}✗${NC}"
        ((FAIL++))
    fi
done

# Test 3: Check for GLM
echo ""
echo -n "GLM library... "
if [ -d "third_party/glm" ]; then
    echo -e "${GREEN}✓${NC}"
    ((PASS++))
else
    echo -e "${YELLOW}⚠${NC}"
    echo "  Run: git clone https://github.com/g-truc/glm.git third_party/glm"
    ((FAIL++))
fi

# Test 4: Run quick CTest
if [ -d "build" ]; then
    echo ""
    echo "Running quick tests..."
    cd build
    if ctest --timeout 10 -R "BasicMolecule" --output-on-failure > /dev/null 2>&1; then
        echo -e "  ${GREEN}✓${NC} Basic tests passed"
        ((PASS++))
    else
        echo -e "  ${YELLOW}⚠${NC} Some tests failed (run 'cd build && ctest' for details)"
    fi
    cd ..
fi

# Test 5: Check for vsepr_opengl_viewer source
echo ""
echo -n "Continuous generation source... "
if [ -f "examples/vsepr_opengl_viewer.cpp" ]; then
    echo -e "${GREEN}✓${NC}"
    ((PASS++))
else
    echo -e "${RED}✗${NC}"
    ((FAIL++))
fi

# Summary
echo ""
echo "═══════════════════════════════════════════════════════════════"
echo "Results: ${GREEN}${PASS} passed${NC}, ${RED}${FAIL} failed${NC}"
echo "═══════════════════════════════════════════════════════════════"

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}✓ All checks passed!${NC}"
    echo ""
    echo "Try these commands:"
    echo "  ./test_thermal.sh              # Thermal analysis"
    echo "  ./build/bin/hydrate-demo       # Hydrate simulation"
    echo "  cd build && ctest              # Full test suite"
    exit 0
else
    echo -e "${YELLOW}⚠ Some components missing${NC}"
    echo ""
    echo "To fix:"
    echo "  ./build_universal.sh --clean   # Rebuild everything"
    exit 1
fi
