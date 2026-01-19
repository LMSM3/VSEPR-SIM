#!/bin/bash
# XYZ Suite Test Runner
# Runs comprehensive I/O validation with resource monitoring

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║           XYZ Suite Tester - Build & Execute                   ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════════╝${NC}"

# Build test
echo -e "\n${YELLOW}[1/3] Building test suite...${NC}"
mkdir -p build
cd build

if ! cmake .. -DBUILD_VIS=OFF -DBUILD_APPS=ON 2>&1 | grep -q "Configuring done"; then
    echo -e "${RED}❌ CMake configuration failed${NC}"
    exit 1
fi

if ! cmake --build . --target xyz_suite_test 2>&1 | tail -5; then
    echo -e "${RED}❌ Build failed${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Build successful${NC}"

# Check for test data
cd ..
echo -e "\n${YELLOW}[2/3] Checking test data...${NC}"

TEST_FILES=""
if [ -d "benchmark_results" ]; then
    FILE_COUNT=$(find benchmark_results -name "*.xyz" | wc -l)
    echo -e "${GREEN}✓ Found ${FILE_COUNT} test molecules in benchmark_results/${NC}"
elif [ -f "test_water.xyz" ]; then
    TEST_FILES="test_water.xyz"
    echo -e "${YELLOW}⚠ Using local test file: test_water.xyz${NC}"
else
    echo -e "${YELLOW}⚠ No benchmark files found, creating sample molecule...${NC}"
    cat > test_h2o.xyz << EOF
3
Water molecule - Test data
O  0.000  0.000  0.117
H  0.000  0.757 -0.467
H  0.000 -0.757 -0.467
EOF
    TEST_FILES="test_h2o.xyz"
    echo -e "${GREEN}✓ Created test_h2o.xyz${NC}"
fi

# Run test
echo -e "\n${YELLOW}[3/3] Running test suite...${NC}"
echo -e "${BLUE}────────────────────────────────────────────────────────────────${NC}\n"

if [ -z "$TEST_FILES" ]; then
    # Auto-scan mode
    ./build/bin/xyz_suite_test
else
    # Explicit files
    ./build/bin/xyz_suite_test $TEST_FILES
fi

EXIT_CODE=$?

echo -e "\n${BLUE}────────────────────────────────────────────────────────────────${NC}"

if [ $EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✓ All tests passed!${NC}"
else
    echo -e "${RED}❌ Tests failed with exit code $EXIT_CODE${NC}"
fi

# Resource summary
echo -e "\n${YELLOW}System Resource Summary:${NC}"
if command -v free &> /dev/null; then
    echo -e "  Memory: $(free -h | awk '/^Mem:/ {print $3 "/" $2}')"
fi
if command -v uptime &> /dev/null; then
    echo -e "  Load:   $(uptime | awk -F'load average:' '{print $2}')"
fi

exit $EXIT_CODE
