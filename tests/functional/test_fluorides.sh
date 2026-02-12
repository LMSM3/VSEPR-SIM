#!/bin/bash
set -Eeuo pipefail
IFS=$'\n\t'
# Pipefall experimental script to test fluoride compounds
###############################################################################
# Fluoride Compound Test Suite
# Tests comprehensive fluoride chemistry across the periodic table
###############################################################################

trap 'echo -e "${RED}Fatal:${NC} line $LINENO: $BASH_COMMAND" >&2' ERR

VSEPR_BIN="./build/bin/vsepr"
RESULTS_DIR="logs/fluoride_test_$(date +%Y%m%d_%H%M%S)"
VERSION="VSEPR-Sim v1.0.0 | Formula Parser v0.5.0"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

# Check that binary exists and is executable
if [[ ! -x "$VSEPR_BIN" ]]; then
  echo -e "${RED}Missing binary:${NC} $VSEPR_BIN (build it first)" >&2
  exit 127
fi

mkdir -p "$RESULTS_DIR"

###############################################################################
# CSV Helper Functions , only one 
###############################################################################

csv_escape() {
    local s=${1//\"/\"\"}
    printf '"%s"' "$s"
}

###############################################################################
# Fluoride Test Categories
###############################################################################

# Alkali Metal Fluorides
ALKALI_FLUORIDES=(
    "LiF:Lithium Fluoride"
    "NaF:Sodium Fluoride"
    "KF:Potassium Fluoride"
    "RbF:Rubidium Fluoride"
    "CsF:Cesium Fluoride"
)

# Alkaline Earth Fluorides
ALKALINE_EARTH_FLUORIDES=(
    "BeF2:Beryllium Fluoride"
    "MgF2:Magnesium Fluoride"
    "CaF2:Calcium Fluoride"
    "SrF2:Strontium Fluoride"
    "BaF2:Barium Fluoride"
)

# Transition Metal Fluorides
TRANSITION_FLUORIDES=(
    "ScF3:Scandium(III) Fluoride"
    "TiF4:Titanium(IV) Fluoride"
    "VF3:Vanadium(III) Fluoride"
    "VF5:Vanadium(V) Fluoride"
    "CrF3:Chromium(III) Fluoride"
    "MnF2:Manganese(II) Fluoride"
    "FeF2:Iron(II) Fluoride"
    "FeF3:Iron(III) Fluoride"
    "CoF2:Cobalt(II) Fluoride"
    "NiF2:Nickel(II) Fluoride"
    "CuF2:Copper(II) Fluoride"
    "ZnF2:Zinc Fluoride"
    "YF3:Yttrium Fluoride"
    "ZrF4:Zirconium Fluoride"
    "NbF5:Niobium Fluoride"
    "MoF6:Molybdenum Hexafluoride"
    "RuF5:Ruthenium Pentafluoride"
    "RhF3:Rhodium Trifluoride"
    "PdF2:Palladium Difluoride"
    "AgF:Silver(I) Fluoride"
    "AgF2:Silver(II) Fluoride"
    "CdF2:Cadmium Fluoride"
    "HfF4:Hafnium Fluoride"
    "TaF5:Tantalum Fluoride"
    "WF6:Tungsten Hexafluoride"
)

# Post-Transition Metal Fluorides
POST_TRANSITION_FLUORIDES=(
    "AlF3:Aluminum Fluoride"
    "GaF3:Gallium Fluoride"
    "InF3:Indium Fluoride"
    "SnF2:Tin(II) Fluoride"
    "SnF4:Tin(IV) Fluoride"
    "PbF2:Lead(II) Fluoride"
    "PbF4:Lead(IV) Fluoride"
    "BiF3:Bismuth Fluoride"
)

# Lanthanide Fluorides
LANTHANIDE_FLUORIDES=(
    "LaF3:Lanthanum Fluoride"
    "CeF3:Cerium Fluoride"
    "NdF3:Neodymium Fluoride"
    "SmF3:Samarium Fluoride"
    "EuF2:Europium(II) Fluoride"
    "GdF3:Gadolinium Fluoride"
    "DyF3:Dysprosium Fluoride"
    "YbF3:Ytterbium Fluoride"
)

# Actinide Fluorides
ACTINIDE_FLUORIDES=(
    "ThF4:Thorium Tetrafluoride"
    "UF4:Uranium Tetrafluoride"
    "UF6:Uranium Hexafluoride"
    "NpF6:Neptunium Hexafluoride"
    "PuF4:Plutonium Tetrafluoride"
)

# Complex & Double Fluoride Salts
COMPLEX_FLUORIDES=(
    "Na3AlF6:Cryolite (Sodium Aluminum Fluoride)"
    "K2TiF6:Potassium Hexafluorotitanate"
    "N2H8SiF6:Ammonium Hexafluorosilicate"
    "Na2ZrF6:Sodium Hexafluorozirconate"
    "K2ZrF6:Potassium Hexafluorozirconate"
    "Cs2UF6:Cesium Hexafluorouranate"
)

###############################################################################
# Testing Functions
###############################################################################

test_fluoride() {
    local formula=$1
    local name=$2
    local category=$3
    
    local output=$($VSEPR_BIN build "$formula" 2>&1)
    local status=""
    local reason=""
    
    if echo "$output" | grep -q "Molecule constructed"; then
        status="${GREEN}PASS${NC}"
        echo "$formula,$name,$category,PASS,Built successfully" >> "$RESULTS_DIR/results.csv"
        return 0
    elif echo "$output" | grep -q "Invalid formula\|Unknown element"; then
        status="${RED}FAIL${NC}"
        reason="Parser error"
        echo "$formula,$name,$category,FAIL,Parser error" >> "$RESULTS_DIR/results.csv"
        return 1
    elif echo "$output" | grep -q "Multi-center\|Graph builder\|construction failed"; then
        status="${YELLOW}TODO${NC}"
        reason="Unsupported topology"
        echo "$formula,$name,$category,TODO,Unsupported topology" >> "$RESULTS_DIR/results.csv"
        return 2
    else
        status="${RED}FAIL${NC}"
        reason="Unknown error"
        echo "$formula,$name,$category,FAIL,Unknown error" >> "$RESULTS_DIR/results.csv"
        return 1
    fi
}

test_category() {
    local category_name=$1
    shift
    local fluorides=("$@")
    
    echo -e "\n${MAGENTA}═══ $category_name ═══${NC}"
    
    local total=0
    local passed=0
    local failed=0
    local todo=0
    
    for entry in "${fluorides[@]}"; do
        IFS=':' read -r formula name <<< "$entry"
        ((total++))
        
        printf "  %-20s %-40s " "$formula" "$name"
        
        test_fluoride "$formula" "$name" "$category_name"
        local result=$?
        
        case $result in
            0) echo -e "${GREEN}✓ PASS${NC}"; ((passed++)) ;;
            1) echo -e "${RED}✗ FAIL${NC}"; ((failed++)) ;;
            2) echo -e "${YELLOW}⧗ TODO${NC}"; ((todo++)) ;;
        esac
    done
    
    echo -e "${CYAN}  Summary: $passed pass, $failed fail, $todo todo (total: $total)${NC}"
}

###############################################################################
# Main Test Execution
###############################################################################

echo -e "${CYAN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║  Fluoride Compound Test Suite                                 ║${NC}"
echo -e "${CYAN}║  $VERSION${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════════╝${NC}"

# CSV Header
echo "Formula,Name,Category,Status,Reason" > "$RESULTS_DIR/results.csv"

# Run all test categories
test_category "Alkali Metal Fluorides" "${ALKALI_FLUORIDES[@]}"
test_category "Alkaline Earth Fluorides" "${ALKALINE_EARTH_FLUORIDES[@]}"
test_category "Transition Metal Fluorides" "${TRANSITION_FLUORIDES[@]}"
test_category "Post-Transition Metal Fluorides" "${POST_TRANSITION_FLUORIDES[@]}"
test_category "Lanthanide Fluorides" "${LANTHANIDE_FLUORIDES[@]}"
test_category "Actinide Fluorides" "${ACTINIDE_FLUORIDES[@]}"
test_category "Complex & Double Fluoride Salts" "${COMPLEX_FLUORIDES[@]}"

###############################################################################
# Generate Report
###############################################################################

echo ""
echo -e "${CYAN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║  Fluoride Test Results Summary                                 ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════════╝${NC}"

total=$(wc -l < "$RESULTS_DIR/results.csv" | xargs)
total=$((total - 1))  # Subtract header
passed=$(grep -c ",PASS," "$RESULTS_DIR/results.csv" || echo 0)
failed=$(grep -c ",FAIL," "$RESULTS_DIR/results.csv" || echo 0)
todo=$(grep -c ",TODO," "$RESULTS_DIR/results.csv" || echo 0)

echo ""
echo -e "${CYAN}Total Tested:${NC} $total fluoride compounds"
echo -e "${GREEN}Passed:${NC}       $passed compounds ($(awk "BEGIN {printf \"%.1f\", $passed*100/$total}")%)"
echo -e "${RED}Failed:${NC}       $failed compounds ($(awk "BEGIN {printf \"%.1f\", $failed*100/$total}")%)"
echo -e "${YELLOW}Todo:${NC}         $todo compounds ($(awk "BEGIN {printf \"%.1f\", $todo*100/$total}")%)"

echo ""
echo -e "${CYAN}Results saved to: $RESULTS_DIR/results.csv${NC}"

# Generate detailed report
cat > "$RESULTS_DIR/FLUORIDE_REPORT.md" << EOF
# Fluoride Compound Test Report

**Version:** $VERSION  
**Date:** $(date '+%Y-%m-%d %H:%M:%S')  
**Total Fluorides Tested:** $total

## Summary

| Status | Count | Percentage |
|--------|-------|------------|
| ✓ Pass | $passed | $(awk "BEGIN {printf \"%.1f\", $passed*100/$total}")% |
| ✗ Fail | $failed | $(awk "BEGIN {printf \"%.1f\", $failed*100/$total}")% |
| ⧗ Todo | $todo | $(awk "BEGIN {printf \"%.1f\", $todo*100/$total}")% |

## Passing Fluorides

$(grep ",PASS," "$RESULTS_DIR/results.csv" | awk -F, '{printf "- **%s** - %s\n", $1, $2}')

## Fluorides Needing Implementation

$(grep ",TODO," "$RESULTS_DIR/results.csv" | awk -F, '{printf "- **%s** - %s (%s)\n", $1, $2, $5}')

## Failed Fluorides

$(grep ",FAIL," "$RESULTS_DIR/results.csv" | awk -F, '{printf "- **%s** - %s (%s)\n", $1, $2, $5}')

---

## Category Breakdown

### Alkali Metal Fluorides
$(grep "Alkali Metal Fluorides,PASS" "$RESULTS_DIR/results.csv" | wc -l || echo 0) / 5 passing

### Alkaline Earth Fluorides
$(grep "Alkaline Earth Fluorides,PASS" "$RESULTS_DIR/results.csv" | wc -l || echo 0) / 5 passing

### Transition Metal Fluorides
$(grep "Transition Metal Fluorides,PASS" "$RESULTS_DIR/results.csv" | wc -l || echo 0) / 25 passing

### Post-Transition Metal Fluorides
$(grep "Post-Transition Metal Fluorides,PASS" "$RESULTS_DIR/results.csv" | wc -l || echo 0) / 8 passing

### Lanthanide Fluorides
$(grep "Lanthanide Fluorides,PASS" "$RESULTS_DIR/results.csv" | wc -l || echo 0) / 8 passing

### Actinide Fluorides
$(grep "Actinide Fluorides,PASS" "$RESULTS_DIR/results.csv" | wc -l || echo 0) / 5 passing

### Complex & Double Fluoride Salts
$(grep "Complex & Double Fluoride Salts,PASS" "$RESULTS_DIR/results.csv" | wc -l || echo 0) / 6 passing

---

*Generated by VSEPR-Sim Fluoride Test Suite*
EOF

echo -e "${GREEN}Detailed report saved to: $RESULTS_DIR/FLUORIDE_REPORT.md${NC}"

# Show quick stats by category
echo ""
echo -e "${MAGENTA}═══ Results by Category ═══${NC}"
for cat in "Alkali Metal" "Alkaline Earth" "Transition Metal" "Post-Transition" "Lanthanide" "Actinide" "Complex"; do
    count=$(grep "$cat" "$RESULTS_DIR/results.csv" | grep ",PASS," | wc -l || echo 0)
    total_cat=$(grep "$cat" "$RESULTS_DIR/results.csv" | wc -l || echo 0)
    if [ $total_cat -gt 0 ]; then
        pct=$(awk "BEGIN {printf \"%.0f\", $count*100/$total_cat}")
        echo -e "  ${CYAN}$cat:${NC} $count/$total_cat (${pct}%)"
    fi
done

echo ""
