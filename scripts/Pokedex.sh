#!/bin/bash
###############################################################################
# Molecular Pokedex - Interactive Molecule Database and Testing Tool
###############################################################################
# 
# Features:
# - Interactive molecule catalog browser
# - Automated testing of all supported molecules
# - Detailed stats and success rates
# - Search and filter by phase/category
# - Generate test reports
#
# Usage:
#   ./Pokedex.sh                 # Interactive menu
#   ./Pokedex.sh test            # Run all tests
#   ./Pokedex.sh test phase1     # Test specific phase
#   ./Pokedex.sh list            # List all molecules
#   ./Pokedex.sh search alkane   # Search molecules
#   ./Pokedex.sh stats           # Show statistics
#
###############################################################################

VSEPR_BIN="./build/bin/vsepr"
OUTPUT_DIR="pokedex_results"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

###############################################################################
# Molecule Database
###############################################################################

# Phase 1: VSEPR Stars
declare -A PHASE1_BINARY_HYDRIDES=(
    ["H2"]="Hydrogen"
    ["H2O"]="Water"
    ["NH3"]="Ammonia"
    ["CH4"]="Methane"
    ["H2S"]="Hydrogen Sulfide"
    ["HCl"]="Hydrogen Chloride"
    ["HF"]="Hydrogen Fluoride"
)

declare -A PHASE1_HYPERVALENT=(
    ["SF6"]="Sulfur Hexafluoride"
    ["PF5"]="Phosphorus Pentafluoride"
    ["PCl5"]="Phosphorus Pentachloride"
    ["IF7"]="Iodine Heptafluoride"
)

declare -A PHASE1_INTERHALOGENS=(
    ["ClF3"]="Chlorine Trifluoride"
    ["BrF5"]="Bromine Pentafluoride"
    ["XeF2"]="Xenon Difluoride"
    ["XeF4"]="Xenon Tetrafluoride"
    ["XeF6"]="Xenon Hexafluoride"
)

declare -A PHASE1_OXOACIDS=(
    ["H2SO4"]="Sulfuric Acid"
    ["H3PO4"]="Phosphoric Acid"
    ["HNO3"]="Nitric Acid"
)

# Phase 2.1: Alkanes
declare -A PHASE21_ALKANES=(
    ["C2H6"]="Ethane"
    ["C3H8"]="Propane"
    ["C4H10"]="n-Butane"
    ["C5H12"]="n-Pentane"
    ["C6H14"]="n-Hexane"
    ["C8H18"]="n-Octane"
    ["C10H22"]="n-Decane"
    ["C20H42"]="n-Eicosane"
)

# Expected failures (for validation)
declare -A INVALID_FORMULAS=(
    ["H2Xyz"]="Invalid element"
    ["X99Y"]="Invalid element"
)

declare -A UNSUPPORTED=(
    ["C2H4"]="Alkene (not yet)"
    ["C6H6"]="Aromatic (not yet)"
    ["C6H12O6"]="Complex organic"
)

###############################################################################
# Helper Functions
###############################################################################

print_header() {
    echo -e "${CYAN}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║${NC}  $1"
    echo -e "${CYAN}╚════════════════════════════════════════════════════════════════╝${NC}"
}

print_section() {
    echo -e "\n${BLUE}═══ $1 ═══${NC}"
}

test_molecule() {
    local formula=$1
    local name=$2
    
    # Run build and capture output
    local output=$($VSEPR_BIN build "$formula" 2>&1)
    
    # Check if molecule was constructed
    if echo "$output" | grep -q "Molecule constructed"; then
        echo -e "  ${GREEN}✓${NC} $formula - $name"
        return 0
    else
        echo -e "  ${RED}✗${NC} $formula - $name ${RED}FAILED${NC}"
        return 1
    fi
}

test_should_fail() {
    local formula=$1
    local reason=$2
    
    # Run build and capture output
    local output=$($VSEPR_BIN build "$formula" 2>&1)
    
    # Check if it failed as expected
    if echo "$output" | grep -q "Build failed\|Invalid formula"; then
        echo -e "  ${GREEN}✓${NC} $formula - ${YELLOW}Correctly rejected${NC} ($reason)"
        return 0
    else
        echo -e "  ${RED}✗${NC} $formula - ${RED}Should have failed${NC} ($reason)"
        return 1
    fi
}

###############################################################################
# Testing Functions
###############################################################################

test_phase1() {
    local total=0
    local passed=0
    
    print_section "Phase 1: VSEPR Stars"
    
    echo ""
    echo "Binary Hydrides:"
    for formula in "${!PHASE1_BINARY_HYDRIDES[@]}"; do
        if test_molecule "$formula" "${PHASE1_BINARY_HYDRIDES[$formula]}"; then
            ((passed++))
        fi
        ((total++))
    done
    
    echo ""
    echo "Hypervalent Compounds:"
    for formula in "${!PHASE1_HYPERVALENT[@]}"; do
        if test_molecule "$formula" "${PHASE1_HYPERVALENT[$formula]}"; then
            ((passed++))
        fi
        ((total++))
    done
    
    echo ""
    echo "Interhalogen Compounds:"
    for formula in "${!PHASE1_INTERHALOGENS[@]}"; do
        if test_molecule "$formula" "${PHASE1_INTERHALOGENS[$formula]}"; then
            ((passed++))
        fi
        ((total++))
    done
    
    echo ""
    echo "Oxoacids:"
    for formula in "${!PHASE1_OXOACIDS[@]}"; do
        if test_molecule "$formula" "${PHASE1_OXOACIDS[$formula]}"; then
            ((passed++))
        fi
        ((total++))
    done
    
    echo ""
    echo -e "${CYAN}Phase 1 Results: $passed/$total passed${NC}"
    return $((total - passed))
}

test_phase21() {
    local total=0
    local passed=0
    
    print_section "Phase 2.1: Alkane Chains"
    
    echo ""
    echo "Linear Alkanes (CnH2n+2):"
    for formula in "${!PHASE21_ALKANES[@]}"; do
        if test_molecule "$formula" "${PHASE21_ALKANES[$formula]}"; then
            ((passed++))
        fi
        ((total++))
    done
    
    echo ""
    echo -e "${CYAN}Phase 2.1 Results: $passed/$total passed${NC}"
    return $((total - passed))
}

test_invalid() {
    local total=0
    local passed=0
    
    print_section "Validation Tests (Expected Failures)"
    
    echo ""
    echo "Invalid Formulas:"
    for formula in "${!INVALID_FORMULAS[@]}"; do
        if test_should_fail "$formula" "${INVALID_FORMULAS[$formula]}"; then
            ((passed++))
        fi
        ((total++))
    done
    
    echo ""
    echo "Unsupported Molecules:"
    for formula in "${!UNSUPPORTED[@]}"; do
        if test_should_fail "$formula" "${UNSUPPORTED[$formula]}"; then
            ((passed++))
        fi
        ((total++))
    done
    
    echo ""
    echo -e "${CYAN}Validation Results: $passed/$total correct${NC}"
    return $((total - passed))
}

run_all_tests() {
    print_header "VSEPR-Sim Molecular Pokedex - Full Test Suite"
    
    local total_failed=0
    
    test_phase1
    total_failed=$((total_failed + $?))
    
    test_phase21
    total_failed=$((total_failed + $?))
    
    test_invalid
    total_failed=$((total_failed + $?))
    
    # Final summary
    echo ""
    print_header "Test Suite Complete"
    
    if [ $total_failed -eq 0 ]; then
        echo -e "${GREEN}ALL TESTS PASSED! 🎉${NC}"
        return 0
    else
        echo -e "${RED}$total_failed tests failed${NC}"
        return 1
    fi
}

###############################################################################
# List Functions
###############################################################################

list_all() {
    print_header "VSEPR-Sim Molecule Catalog"
    
    print_section "Phase 1: VSEPR Stars (${#PHASE1_BINARY_HYDRIDES[@]} + ${#PHASE1_HYPERVALENT[@]} + ${#PHASE1_INTERHALOGENS[@]} + ${#PHASE1_OXOACIDS[@]} molecules)"
    
    echo ""
    echo "Binary Hydrides:"
    for formula in "${!PHASE1_BINARY_HYDRIDES[@]}"; do
        echo "  • $formula - ${PHASE1_BINARY_HYDRIDES[$formula]}"
    done | sort
    
    echo ""
    echo "Hypervalent Compounds:"
    for formula in "${!PHASE1_HYPERVALENT[@]}"; do
        echo "  • $formula - ${PHASE1_HYPERVALENT[$formula]}"
    done | sort
    
    echo ""
    echo "Interhalogen Compounds:"
    for formula in "${!PHASE1_INTERHALOGENS[@]}"; do
        echo "  • $formula - ${PHASE1_INTERHALOGENS[$formula]}"
    done | sort
    
    echo ""
    echo "Oxoacids:"
    for formula in "${!PHASE1_OXOACIDS[@]}"; do
        echo "  • $formula - ${PHASE1_OXOACIDS[$formula]}"
    done | sort
    
    print_section "Phase 2.1: Alkane Chains (${#PHASE21_ALKANES[@]} molecules)"
    
    echo ""
    for formula in "${!PHASE21_ALKANES[@]}"; do
        echo "  • $formula - ${PHASE21_ALKANES[$formula]}"
    done | sort
}

show_stats() {
    print_header "VSEPR-Sim Statistics"
    
    local phase1_count=$((${#PHASE1_BINARY_HYDRIDES[@]} + ${#PHASE1_HYPERVALENT[@]} + ${#PHASE1_INTERHALOGENS[@]} + ${#PHASE1_OXOACIDS[@]}))
    local phase21_count=${#PHASE21_ALKANES[@]}
    local total_supported=$((phase1_count + phase21_count))
    
    echo ""
    echo -e "${CYAN}Supported Molecules:${NC} $total_supported"
    echo -e "  ${GREEN}✓${NC} Phase 1 (VSEPR Stars): $phase1_count"
    echo -e "  ${GREEN}✓${NC} Phase 2.1 (Alkanes): $phase21_count"
    echo ""
    echo -e "${YELLOW}In Development:${NC}"
    echo -e "  ${YELLOW}⧗${NC} Phase 2.2 (Alkenes/Alkynes): Planned"
    echo -e "  ${YELLOW}⧗${NC} Phase 2.3 (Aromatics): Planned"
    echo -e "  ${YELLOW}⧗${NC} Phase 3 (Coordination): Future"
    echo -e "  ${YELLOW}⧗${NC} Phase 4 (Crystals): Future"
    echo ""
}

###############################################################################
# Main Menu
###############################################################################

show_menu() {
    clear
    print_header "VSEPR-Sim Molecular Pokedex"
    echo ""
    echo "  1) Run all tests"
    echo "  2) Test Phase 1 only (VSEPR Stars)"
    echo "  3) Test Phase 2.1 only (Alkanes)"
    echo "  4) List all molecules"
    echo "  5) Show statistics"
    echo "  6) Validation tests"
    echo "  q) Quit"
    echo ""
    echo -n "Select option: "
}

interactive_menu() {
    while true; do
        show_menu
        read -r choice
        
        case $choice in
            1) run_all_tests; echo ""; read -p "Press Enter to continue..." ;;
            2) test_phase1; echo ""; read -p "Press Enter to continue..." ;;
            3) test_phase21; echo ""; read -p "Press Enter to continue..." ;;
            4) list_all; echo ""; read -p "Press Enter to continue..." ;;
            5) show_stats; echo ""; read -p "Press Enter to continue..." ;;
            6) test_invalid; echo ""; read -p "Press Enter to continue..." ;;
            q|Q) echo "Goodbye!"; exit 0 ;;
            *) echo "Invalid option"; sleep 1 ;;
        esac
    done
}

###############################################################################
# Main Entry Point
###############################################################################

# Check if vsepr binary exists
if [ ! -f "$VSEPR_BIN" ]; then
    echo -e "${RED}Error: VSEPR binary not found at $VSEPR_BIN${NC}"
    echo "Please build the project first: cd build && make"
    exit 1
fi

# Parse command line arguments
case "${1:-menu}" in
    test)
        case "${2:-all}" in
            all) run_all_tests ;;
            phase1) test_phase1 ;;
            phase2|phase21) test_phase21 ;;
            invalid) test_invalid ;;
            *) echo "Unknown test: $2"; exit 1 ;;
        esac
        ;;
    list) list_all ;;
    stats) show_stats ;;
    menu) interactive_menu ;;
    help|--help|-h)
        echo "Usage: $0 [command] [args]"
        echo ""
        echo "Commands:"
        echo "  test [all|phase1|phase21|invalid]  Run tests"
        echo "  list                                List all molecules"
        echo "  stats                               Show statistics"
        echo "  menu                                Interactive menu (default)"
        echo "  help                                Show this help"
        ;;
    *)
        echo "Unknown command: $1"
        echo "Use '$0 help' for usage information"
        exit 1
        ;;
esac
