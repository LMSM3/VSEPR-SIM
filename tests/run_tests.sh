#!/bin/bash
# ============================================================================
# Run All VSEPR-Sim Tests
# ============================================================================

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║           VSEPR-Sim Test Suite                                 ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# ============================================================================
# Check Prerequisites
# ============================================================================

echo "▶ Checking prerequisites..."

# Check for BATS
if ! command -v bats &> /dev/null; then
    echo "✗ BATS not found"
    echo ""
    echo "Install BATS:"
    echo "  Ubuntu/Debian: sudo apt-get install bats"
    echo "  macOS: brew install bats-core"
    echo "  npm: npm install -g bats"
    echo ""
    exit 1
fi

echo "✓ BATS available: $(bats --version)"

# Check for executable
if [ ! -f "build/bin/vsepr" ] && [ ! -f "build/bin/vsepr.exe" ]; then
    echo "✗ VSEPR executable not found"
    echo ""
    echo "Build first:"
    echo "  cmake -B build"
    echo "  cmake --build build"
    echo ""
    exit 1
fi

echo "✓ Executable found"
echo ""

# ============================================================================
# Run Integration Tests (BATS)
# ============================================================================

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  Integration Tests (BATS)                                      ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

bats tests/integration_tests.bats

BATS_EXIT=$?

echo ""

# ============================================================================
# Run Unit Tests (if built with tests)
# ============================================================================

if [ -f "build/CTestTestfile.cmake" ]; then
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║  Unit Tests (CTest)                                            ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo ""
    
    cd build
    ctest --output-on-failure --parallel 4
    CTEST_EXIT=$?
    cd ..
    
    echo ""
else
    echo "ℹ Unit tests not built (use -DBUILD_TESTS=ON)"
    CTEST_EXIT=0
fi

# ============================================================================
# Summary
# ============================================================================

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  Test Summary                                                  ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

if [ $BATS_EXIT -eq 0 ]; then
    echo "✓ Integration tests: PASSED"
else
    echo "✗ Integration tests: FAILED"
fi

if [ -f "build/CTestTestfile.cmake" ]; then
    if [ $CTEST_EXIT -eq 0 ]; then
        echo "✓ Unit tests: PASSED"
    else
        echo "✗ Unit tests: FAILED"
    fi
fi

echo ""

# Exit with error if any tests failed
if [ $BATS_EXIT -ne 0 ] || [ $CTEST_EXIT -ne 0 ]; then
    echo "✗ Some tests failed"
    exit 1
else
    echo "✓ All tests passed!"
    exit 0
fi
