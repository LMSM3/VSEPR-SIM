#!/usr/bin/env bash
# rebuild_tests.sh
# Clean rebuild with tests enabled for VSEPR-Sim
# Usage: ./scripts/rebuild_tests.sh

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "=== VSEPR-Sim Test Build Script ==="
echo "Project: ${PROJECT_ROOT}"
echo ""

# Clean old build artifacts
echo "• Cleaning build directory..."
rm -rf "${PROJECT_ROOT}/build"
mkdir -p "${PROJECT_ROOT}/build"

cd "${PROJECT_ROOT}/build"

# Configure with tests enabled
echo "• Configuring CMake with tests enabled..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=ON \
    -DBUILD_APPS=ON \
    -DBUILD_VIS=OFF

echo ""
echo "• Building all targets..."
cmake --build . -j$(nproc 2>/dev/null || echo 4)

echo ""
echo "=== Build Complete ==="
echo "Executables in: ${PROJECT_ROOT}/build/bin/"
ls -lh "${PROJECT_ROOT}/build/bin/" | grep -E "(test_formula_parser|formula_fuzz_tester|vsepr)" || true

echo ""
echo "✓ Test targets ready"
echo "  Run: ./build/bin/test_formula_parser"
echo "  Run: ./build/bin/formula_fuzz_tester --iterations 1000"
echo "  Run: ./scripts/run_cli_tests.sh"
