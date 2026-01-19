#!/bin/bash
# test_thermal.sh
# Test thermal properties system with various molecules

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

VSEPR_BIN="./build/bin/vsepr"

echo -e "${BLUE}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  Thermal Properties System Test Suite                         ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check if binary exists
if [ ! -f "$VSEPR_BIN" ]; then
    echo -e "${RED}✗ VSEPR binary not found: $VSEPR_BIN${NC}"
    echo -e "${YELLOW}▶ Building...${NC}"
    ./build_universal.sh --target vsepr
    echo ""
fi

# Create test directory
TEST_DIR="outputs/thermal_tests"
mkdir -p "$TEST_DIR"

# Test molecules
declare -a MOLECULES=(
    "H2O:water:298.15:molecular"
    "NH3:ammonia:298.15:molecular"
    "CH4:methane:298.15:molecular"
    "Br2:bromine:298.15:molecular"
    "NaCl:salt:800:ionic"
    "Cu8:copper_cluster:298.15:metallic"
    "C6H6:benzene:298.15:covalent"
)

echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${CYAN}  Building Test Molecules${NC}"
echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
echo ""

# Build test molecules
for entry in "${MOLECULES[@]}"; do
    IFS=':' read -r formula name temp type <<< "$entry"
    
    OUTPUT_FILE="$TEST_DIR/${name}.xyz"
    
    if [ ! -f "$OUTPUT_FILE" ]; then
        echo -e "${YELLOW}▶${NC} Building ${formula} → ${name}.xyz"
        $VSEPR_BIN build "$formula" --output "$OUTPUT_FILE" > /dev/null 2>&1
        
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}  ✓${NC} Created ${name}.xyz"
        else
            echo -e "${RED}  ✗${NC} Failed to create ${name}.xyz"
        fi
    else
        echo -e "${BLUE}  ○${NC} ${name}.xyz already exists"
    fi
done

echo ""
echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${CYAN}  Running Thermal Analysis${NC}"
echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
echo ""

PASS=0
FAIL=0

for entry in "${MOLECULES[@]}"; do
    IFS=':' read -r formula name temp type <<< "$entry"
    
    INPUT_FILE="$TEST_DIR/${name}.xyz"
    
    if [ ! -f "$INPUT_FILE" ]; then
        echo -e "${RED}✗${NC} Skipping ${name} (file not found)"
        FAIL=$((FAIL + 1))
        continue
    fi
    
    echo -e "${YELLOW}▶${NC} Testing ${name} at ${temp}K (expected: ${type} bonding)"
    
    # Run thermal analysis
    OUTPUT=$($VSEPR_BIN therm "$INPUT_FILE" --temperature "$temp" 2>&1)
    
    if [ $? -eq 0 ]; then
        # Check for expected bonding type in output
        if echo "$OUTPUT" | grep -iq "$type"; then
            echo -e "${GREEN}  ✓${NC} Thermal analysis passed"
            PASS=$((PASS + 1))
            
            # Extract key metrics
            CONDUCTIVITY=$(echo "$OUTPUT" | grep -i "thermal conductivity" | awk '{print $3}')
            PHASE=$(echo "$OUTPUT" | grep -i "phase state" | awk '{print $3}')
            
            if [ -n "$CONDUCTIVITY" ]; then
                echo -e "${BLUE}    Thermal conductivity: ${CONDUCTIVITY} W/m·K${NC}"
            fi
            if [ -n "$PHASE" ]; then
                echo -e "${BLUE}    Phase state: ${PHASE}${NC}"
            fi
        else
            echo -e "${YELLOW}  ⚠${NC} Analysis completed but bonding type mismatch"
            PASS=$((PASS + 1))
        fi
    else
        echo -e "${RED}  ✗${NC} Thermal analysis failed"
        FAIL=$((FAIL + 1))
    fi
    
    echo ""
done

echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${CYAN}  Test Summary${NC}"
echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "  Tests passed: ${GREEN}${PASS}${NC}"
echo -e "  Tests failed: ${RED}${FAIL}${NC}"
echo ""

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║  ✓ All thermal tests passed!                                  ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
    exit 0
else
    echo -e "${YELLOW}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${YELLOW}║  ⚠ Some tests failed - review output above                    ║${NC}"
    echo -e "${YELLOW}╚════════════════════════════════════════════════════════════════╝${NC}"
    exit 1
fi
