#!/usr/bin/env bash
################################################################################
# VSEPR-Sim - Environment Check Script
# Verifies all required dependencies for building
################################################################################

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${BLUE}${BOLD}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}${BOLD}║  VSEPR-Sim Environment Check                                  ║${NC}"
echo -e "${BLUE}${BOLD}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""

ERRORS=0
WARNINGS=0

check_required() {
    local name="$1"
    local cmd="$2"
    local min_version="$3"
    
    if command -v "$cmd" &> /dev/null; then
        local version=$($cmd --version 2>&1 | head -n1 || echo "unknown")
        echo -e "${GREEN}[✓]${NC} $name: $version"
        return 0
    else
        echo -e "${RED}[✗]${NC} $name: NOT FOUND (required: $min_version+)"
        ((ERRORS++))
        return 1
    fi
}

check_optional() {
    local name="$1"
    local cmd="$2"
    
    if command -v "$cmd" &> /dev/null; then
        local version=$($cmd --version 2>&1 | head -n1 || echo "unknown")
        echo -e "${GREEN}[✓]${NC} $name: $version"
        return 0
    else
        echo -e "${YELLOW}[!]${NC} $name: NOT FOUND (optional)"
        ((WARNINGS++))
        return 1
    fi
}

echo "Required Tools:"
check_required "CMake" "cmake" "3.16"
check_required "C++ Compiler" "g++" "10.0"
check_required "C Compiler" "gcc" "10.0"
check_required "Make" "make" "4.0"

echo ""
echo "Optional Tools:"
check_optional "Python3" "python3"
check_optional "Git" "git"
check_optional "Valgrind" "valgrind"

echo ""
echo "System Libraries:"
if ldconfig -p | grep -q libGL; then
    echo -e "${GREEN}[✓]${NC} OpenGL libraries found"
else
    echo -e "${YELLOW}[!]${NC} OpenGL libraries not found (needed for visualization)"
    ((WARNINGS++))
fi

echo ""
echo "═══════════════════════════════════════════════════════════════"
if [[ $ERRORS -eq 0 ]]; then
    echo -e "${GREEN}${BOLD}Environment Check PASSED${NC}"
    echo "Warnings: $WARNINGS (non-critical)"
    exit 0
else
    echo -e "${RED}${BOLD}Environment Check FAILED${NC}"
    echo "Errors: $ERRORS (must fix)"
    echo "Warnings: $WARNINGS (non-critical)"
    exit 1
fi
