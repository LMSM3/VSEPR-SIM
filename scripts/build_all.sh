#!/usr/bin/env bash
################################################################################
# VSEPR-Sim - Universal Build Script
# One command to build everything consistently
################################################################################

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"
BUILD_TYPE="${BUILD_TYPE:-Release}"
VERBOSE="${VERBOSE:-0}"

source "$ROOT/scripts/env_check.sh" || {
    echo "Environment check failed. Cannot proceed."
    exit 1
}

echo ""
echo "════════════════════════════════════════════════════════════════"
echo "  VSEPR-Sim Universal Build System"
echo "  Build Type: $BUILD_TYPE"
echo "════════════════════════════════════════════════════════════════"
echo ""

# Clean build (optional)
if [[ "${CLEAN:-0}" == "1" ]]; then
    echo "[*] Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo "[*] Configuring CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_CXX_FLAGS_RELEASE="-O3 -march=native -DNDEBUG" \
    -DCMAKE_C_FLAGS_RELEASE="-O3 -march=native -DNDEBUG"

# Build
echo "[*] Building..."
NPROC=$(nproc 2>/dev/null || echo 4)
cmake --build . --config "$BUILD_TYPE" --parallel "$NPROC"

# Create manifest
echo "[*] Generating build manifest..."
cat > "$BUILD_DIR/manifest.json" << EOF
{
  "version": "2.3.1",
  "build_type": "$BUILD_TYPE",
  "timestamp": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "git_commit": "$(git rev-parse HEAD 2>/dev/null || echo 'unknown')",
  "cmake_version": "$(cmake --version | head -n1)",
  "compiler": "$(g++ --version | head -n1)",
  "binaries": [
    "bin/vsepr",
    "bin/vsepr-view",
    "bin/labelme"
  ]
}
EOF

echo ""
echo "════════════════════════════════════════════════════════════════"
echo "  Build Complete!"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "Binaries:"
ls -lh "$BUILD_DIR/bin/" 2>/dev/null || echo "  (none built)"
echo ""
echo "Manifest: $BUILD_DIR/manifest.json"
