#!/usr/bin/env bash
################################################################################
# VSEPR-Sim Master Test Runner
# Runs all tests with proper organization and reporting
################################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

# Test results
PASSED=0
FAILED=0
SKIPPED=0

# Parse arguments
RUN_UNIT=false
RUN_INTEGRATION=false
RUN_FUNCTIONAL=false
RUN_REGRESSION=false
RUN_ALL=false
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --unit|-u)
            RUN_UNIT=true
            shift
            ;;
        --integration|-i)
            RUN_INTEGRATION=true
            shift
            ;;
        --functional|-f)
            RUN_FUNCTIONAL=true
            shift
            ;;
        --regression|-r)
            RUN_REGRESSION=true
            shift
            ;;
        --all|-a)
            RUN_ALL=true
            shift
            ;;
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --help|-h)
            echo "VSEPR-Sim Test Runner"
            echo ""
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --unit, -u           Run C++ unit tests (ctest)"
            echo "  --integration, -i    Run integration tests"
            echo "  --functional, -f     Run functional tests"
            echo "  --regression, -r     Run regression tests"
            echo "  --all, -a            Run all test categories"
            echo "  --verbose, -v        Verbose output"
            echo "  --help, -h           Show this help"
            echo ""
            echo "Examples:"
            echo "  $0 --all                  # Run everything"
            echo "  $0 --unit --functional    # Run unit and functional only"
            echo "  $0 --integration -v       # Run integration tests with verbose output"
            echo ""
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# If no options specified, default to all
if [ "$RUN_UNIT" = false ] && [ "$RUN_INTEGRATION" = false ] && \
   [ "$RUN_FUNCTIONAL" = false ] && [ "$RUN_REGRESSION" = false ] && \
   [ "$RUN_ALL" = false ]; then
    RUN_ALL=true
fi

# Set all flags if --all
if [ "$RUN_ALL" = true ]; then
    RUN_UNIT=true
    RUN_INTEGRATION=true
    RUN_FUNCTIONAL=true
    RUN_REGRESSION=true
fi

header() {
    echo ""
    echo -e "${MAGENTA}╔═══════════════════════════════════════════════════════════════╗${NC}"
    printf "${MAGENTA}║  %-61s║${NC}\n" "$1"
    echo -e "${MAGENTA}╚═══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

section() {
    echo ""
    echo -e "${CYAN}▶ $1${NC}"
    echo ""
}

success() {
    echo -e "${GREEN}✓ $1${NC}"
}

failure() {
    echo -e "${RED}✗ $1${NC}"
}

warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

run_test() {
    local test_name="$1"
    local test_cmd="$2"
    
    echo -n "  Running $test_name... "
    
    if [ "$VERBOSE" = true ]; then
        echo ""
        if eval "$test_cmd"; then
            success "  $test_name PASSED"
            ((PASSED++))
        else
            failure "  $test_name FAILED"
            ((FAILED++))
        fi
    else
        if eval "$test_cmd" > /dev/null 2>&1; then
            success "PASSED"
            ((PASSED++))
        else
            failure "FAILED"
            ((FAILED++))
            echo "    (run with --verbose to see details)"
        fi
    fi
}

# ============================================================================
# Main Test Execution
# ============================================================================

header "VSEPR-Sim Test Suite"

# Unit Tests (C++)
if [ "$RUN_UNIT" = true ]; then
    section "Unit Tests (C++)"
    
    if [ -d "$PROJECT_ROOT/build" ] && [ -f "$PROJECT_ROOT/build/Makefile" ]; then
        cd "$PROJECT_ROOT/build"
        run_test "C++ Unit Tests" "ctest --output-on-failure"
        cd "$PROJECT_ROOT"
    else
        warning "Build directory not found - skipping unit tests"
        warning "Run ./build.sh first to build the project"
        ((SKIPPED++))
    fi
fi

# Integration Tests
if [ "$RUN_INTEGRATION" = true ]; then
    section "Integration Tests"
    
    for test in "$PROJECT_ROOT"/tests/integration/test_*.sh; do
        if [ -f "$test" ]; then
            test_name=$(basename "$test" .sh)
            run_test "$test_name" "bash '$test'"
        fi
    done
    
    if [ ! -f "$PROJECT_ROOT"/tests/integration/test_*.sh ]; then
        warning "No integration tests found"
        ((SKIPPED++))
    fi
fi

# Functional Tests
if [ "$RUN_FUNCTIONAL" = true ]; then
    section "Functional Tests"
    
    for test in "$PROJECT_ROOT"/tests/functional/test_*.sh; do
        if [ -f "$test" ]; then
            test_name=$(basename "$test" .sh)
            run_test "$test_name" "bash '$test'"
        fi
    done
    
    if [ ! -f "$PROJECT_ROOT"/tests/functional/test_*.sh ]; then
        warning "No functional tests found"
        ((SKIPPED++))
    fi
fi

# Regression Tests
if [ "$RUN_REGRESSION" = true ]; then
    section "Regression Tests"
    
    for test in "$PROJECT_ROOT"/tests/regression/test_*.sh; do
        if [ -f "$test" ]; then
            test_name=$(basename "$test" .sh)
            run_test "$test_name" "bash '$test'"
        fi
    done
    
    if [ ! -f "$PROJECT_ROOT"/tests/regression/test_*.sh ]; then
        warning "No regression tests found"
        ((SKIPPED++))
    fi
fi

# ============================================================================
# Summary
# ============================================================================

header "Test Summary"

TOTAL=$((PASSED + FAILED + SKIPPED))

echo ""
echo -e "  Total Tests: $TOTAL"
echo -e "  ${GREEN}Passed:  $PASSED${NC}"
echo -e "  ${RED}Failed:  $FAILED${NC}"
echo -e "  ${YELLOW}Skipped: $SKIPPED${NC}"
echo ""

if [ $FAILED -eq 0 ]; then
    success "All tests passed! 🎉"
    echo ""
    exit 0
else
    failure "Some tests failed"
    echo ""
    echo "  Run with --verbose to see detailed output"
    echo "  Or check logs in logs/ directory"
    echo ""
    exit 1
fi
