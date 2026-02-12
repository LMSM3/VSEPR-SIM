#!/bin/bash
###############################################################################
# Comprehensive Molecule Test Suite
# Tests all common molecules across chemistry domains
###############################################################################

VSEPR_BIN="./build/bin/vsepr"
RESULTS_DIR="logs/comprehensive_test_$(date +%Y%m%d_%H%M%S)"
VERSION="VSEPR-Sim v1.0.0 | Formula Parser v0.5.0"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

mkdir -p "$RESULTS_DIR"

###############################################################################
# Test Categories
###############################################################################

# Simple/Fundamental
SIMPLE_FUNDAMENTAL=(
    "H2:Hydrogen"
    "O2:Oxygen"
    "N2:Nitrogen"
    "F2:Fluorine"
    "Cl2:Chlorine"
    "Br2:Bromine"
    "I2:Iodine"
    "O3:Ozone"
    "CO:Carbon Monoxide"
    "CO2:Carbon Dioxide"
)

# Small Inorganic
SMALL_INORGANIC=(
    "H2O:Water"
    "NH3:Ammonia"
    "H2S:Hydrogen Sulfide"
    "SO2:Sulfur Dioxide"
    "SO3:Sulfur Trioxide"
    "NO:Nitric Oxide"
    "NO2:Nitrogen Dioxide"
    "N2O:Nitrous Oxide"
    "HCl:Hydrogen Chloride"
    "HF:Hydrogen Fluoride"
)

# Acids, Bases, Salts
ACIDS_BASES_SALTS=(
    "H2SO4:Sulfuric Acid"
    "HNO3:Nitric Acid"
    "H3PO4:Phosphoric Acid"
    "HClO4:Perchloric Acid"
    "NaCl:Sodium Chloride"
    "KBr:Potassium Bromide"
    "CaCO3:Calcium Carbonate"
    "NaOH:Sodium Hydroxide"
    "KOH:Potassium Hydroxide"
    "NH4Cl:Ammonium Chloride"
)

# Simple Organics
SIMPLE_ORGANICS=(
    "CH4:Methane"
    "C2H6:Ethane"
    "C2H4:Ethene"
    "C2H2:Ethyne"
    "C3H8:Propane"
    "C4H10:Butane"
    "C6H6:Benzene"
    "C7H8:Toluene"
    "C8H10:Xylene"
    "C6H12:Cyclohexane"
)

# Alcohols & Ethers
ALCOHOLS_ETHERS=(
    "CH4O:Methanol"
    "C2H6O:Ethanol"
    "C3H8O:Propanol"
    "C4H10O:Butanol"
    "C6H14O:Hexanol"
)

# Carbonyls & Acids
CARBONYLS_ACIDS=(
    "CH2O:Formaldehyde"
    "C2H4O:Acetaldehyde"
    "C3H6O:Acetone"
    "CH2O2:Formic Acid"
    "C2H4O2:Acetic Acid"
)

# Nitrogen Organics
NITROGEN_ORGANICS=(
    "CH5N:Methylamine"
    "C2H7N:Ethylamine"
    "CH4N2O:Urea"
)

# Halogenated Organics
HALOGENATED=(
    "CH3Cl:Chloromethane"
    "CH2Cl2:Dichloromethane"
    "CHCl3:Chloroform"
    "CCl4:Carbon Tetrachloride"
    "CF4:Carbon Tetrafluoride"
)

# Biologically Relevant
BIOLOGICAL=(
    "C6H12O6:Glucose"
    "C5H10O5:Ribose"
    "C2H5NO2:Glycine"
    "C3H7NO2:Alanine"
)

# Industrial/Energetic
INDUSTRIAL=(
    "H2O2:Hydrogen Peroxide"
    "N2H4:Hydrazine"
)

# Inorganic Salts
INORGANIC_SALTS=(
    "Na2SO4:Sodium Sulfate"
    "KNO3:Potassium Nitrate"
    "CaCl2:Calcium Chloride"
    "MgSO4:Magnesium Sulfate"
    "Al2O3:Aluminum Oxide"
)

# Fluorides
FLUORIDES=(
    "UF6:Uranium Hexafluoride"
    "ThF4:Thorium Tetrafluoride"
    "LiF:Lithium Fluoride"
    "NaF:Sodium Fluoride"
    "BeF2:Beryllium Fluoride"
)

# Finish Strong
FINISH_STRONG=(
    "SiO2:Silicon Dioxide"
    "BF3:Boron Trifluoride"
    "PH3:Phosphine"
    "SF6:Sulfur Hexafluoride"
    "XeF2:Xenon Difluoride"
)

###############################################################################
# Testing Functions
###############################################################################

test_molecule() {
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
    elif echo "$output" | grep -q "Invalid formula"; then
        status="${RED}FAIL${NC}"
        reason="Parser error"
        echo "$formula,$name,$category,FAIL,Parser error" >> "$RESULTS_DIR/results.csv"
        return 1
    elif echo "$output" | grep -q "Multi-center\|Graph builder"; then
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
    local molecules=("$@")
    
    echo -e "\n${BLUE}═══ $category_name ═══${NC}"
    
    local total=0
    local passed=0
    local failed=0
    local todo=0
    
    for entry in "${molecules[@]}"; do
        IFS=':' read -r formula name <<< "$entry"
        ((total++))
        
        printf "  %-15s %-30s " "$formula" "$name"
        
        test_molecule "$formula" "$name" "$category_name"
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
echo -e "${CYAN}║  Comprehensive Molecule Test Suite                            ║${NC}"
echo -e "${CYAN}║  $VERSION${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════════╝${NC}"

# CSV Header
echo "Formula,Name,Category,Status,Reason" > "$RESULTS_DIR/results.csv"

# Run all test categories
test_category "Simple/Fundamental" "${SIMPLE_FUNDAMENTAL[@]}"
test_category "Small Inorganic" "${SMALL_INORGANIC[@]}"
test_category "Acids/Bases/Salts" "${ACIDS_BASES_SALTS[@]}"
test_category "Simple Organics" "${SIMPLE_ORGANICS[@]}"
test_category "Alcohols & Ethers" "${ALCOHOLS_ETHERS[@]}"
test_category "Carbonyls & Acids" "${CARBONYLS_ACIDS[@]}"
test_category "Nitrogen Organics" "${NITROGEN_ORGANICS[@]}"
test_category "Halogenated Organics" "${HALOGENATED[@]}"
test_category "Biological" "${BIOLOGICAL[@]}"
test_category "Industrial/Energetic" "${INDUSTRIAL[@]}"
test_category "Inorganic Salts" "${INORGANIC_SALTS[@]}"
test_category "Fluorides" "${FLUORIDES[@]}"
test_category "Finish Strong" "${FINISH_STRONG[@]}"

###############################################################################
# Generate Report
###############################################################################

echo ""
echo -e "${CYAN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║  Test Results Summary                                          ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════════╝${NC}"

total=$(wc -l < "$RESULTS_DIR/results.csv" | xargs)
total=$((total - 1))  # Subtract header
passed=$(grep -c ",PASS," "$RESULTS_DIR/results.csv" || echo 0)
failed=$(grep -c ",FAIL," "$RESULTS_DIR/results.csv" || echo 0)
todo=$(grep -c ",TODO," "$RESULTS_DIR/results.csv" || echo 0)

echo ""
echo -e "${CYAN}Total Tested:${NC} $total molecules"
echo -e "${GREEN}Passed:${NC}       $passed molecules ($(awk "BEGIN {printf \"%.1f\", $passed*100/$total}")%)"
echo -e "${RED}Failed:${NC}       $failed molecules ($(awk "BEGIN {printf \"%.1f\", $failed*100/$total}")%)"
echo -e "${YELLOW}Todo:${NC}         $todo molecules ($(awk "BEGIN {printf \"%.1f\", $todo*100/$total}")%)"

echo ""
echo -e "${CYAN}Results saved to: $RESULTS_DIR/results.csv${NC}"

# Generate detailed report
cat > "$RESULTS_DIR/REPORT.md" << EOF
# VSEPR-Sim Comprehensive Test Report

**Version:** $VERSION  
**Date:** $(date '+%Y-%m-%d %H:%M:%S')  
**Total Molecules Tested:** $total

## Summary

| Status | Count | Percentage |
|--------|-------|------------|
| ✓ Pass | $passed | $(awk "BEGIN {printf \"%.1f\", $passed*100/$total}")% |
| ✗ Fail | $failed | $(awk "BEGIN {printf \"%.1f\", $failed*100/$total}")% |
| ⧗ Todo | $todo | $(awk "BEGIN {printf \"%.1f\", $todo*100/$total}")% |

## Passing Molecules

$(grep ",PASS," "$RESULTS_DIR/results.csv" | awk -F, '{printf "- **%s** - %s\n", $1, $2}')

## Molecules Needing Implementation

$(grep ",TODO," "$RESULTS_DIR/results.csv" | awk -F, '{printf "- **%s** - %s (%s)\n", $1, $2, $5}')

## Failed Molecules

$(grep ",FAIL," "$RESULTS_DIR/results.csv" | awk -F, '{printf "- **%s** - %s (%s)\n", $1, $2, $5}')

---
*Generated by VSEPR-Sim Test Suite*
EOF

echo -e "${GREEN}Detailed report saved to: $RESULTS_DIR/REPORT.md${NC}"
echo ""
