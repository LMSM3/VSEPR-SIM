#!/bin/bash
# ============================================================================
# Quick Integration Tests (without BATS dependency)
# ============================================================================

set -e

cd "$(dirname "$0")"

VSEPR="./build/bin/vsepr"
PASS=0
FAIL=0

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║        VSEPR-Sim Quick Integration Tests                       ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# Test function
test_cmd() {
    local name="$1"
    shift
    local cmd="$@"
    
    printf "  %-50s " "$name"
    
    if eval "$cmd" > /dev/null 2>&1; then
        echo "✓ PASS"
        ((PASS++))
        return 0
    else
        echo "✗ FAIL"
        ((FAIL++))
        return 1
    fi
}

# Test with output check
test_output() {
    local name="$1"
    local cmd="$2"
    local expected="$3"
    
    printf "  %-50s " "$name"
    
    output=$(eval "$cmd" 2>&1)
    if echo "$output" | grep -qi "$expected"; then
        echo "✓ PASS"
        ((PASS++))
        return 0
    else
        echo "✗ FAIL (expected: $expected)"
        ((FAIL++))
        return 1
    fi
}

# ============================================================================
# Run Tests
# ============================================================================

echo "▶ Basic Commands"
test_output "version command" "$VSEPR version" "version"
test_output "help command" "$VSEPR help" "build"
test_output "help alias" "$VSEPR --help" "Commands"

echo ""
echo "▶ Simple Molecules"
test_output "build H2O" "$VSEPR build H2O" "H2O"
test_output "build CH4" "$VSEPR build CH4" "CH4"
test_output "build NH3" "$VSEPR build NH3" "NH3"
test_output "build CO2" "$VSEPR build CO2" "CO2"

echo ""
echo "▶ Optimization"
test_output "optimize H2O" "$VSEPR build H2O --optimize" "converged"
test_output "optimize CH4" "$VSEPR build CH4 --optimize" "converged"

echo ""
echo "▶ File Output"
test_cmd "create XYZ file" "$VSEPR build H2O --output /tmp/test_h2o.xyz && [ -f /tmp/test_h2o.xyz ]"
test_cmd "XYZ file has content" "[ -s /tmp/test_h2o.xyz ]"

echo ""
echo "▶ Complex Molecules"
test_output "build C2H6" "$VSEPR build C2H6" "C2H6"
test_output "build H2SO4" "$VSEPR build H2SO4" "H2SO4"

echo ""
echo "▶ Error Handling"
! $VSEPR build "" > /dev/null 2>&1 && echo "  Invalid empty formula                             ✓ PASS" && ((PASS++)) || (echo "  Invalid empty formula                             ✗ FAIL" && ((FAIL++)))

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  Results                                                       ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "  Passed: $PASS"
echo "  Failed: $FAIL"
echo "  Total:  $((PASS + FAIL))"
echo ""

if [ $FAIL -eq 0 ]; then
    echo "✓ All tests passed!"
    exit 0
else
    echo "✗ Some tests failed"
    exit 1
fi
