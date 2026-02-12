#!/usr/bin/env bash
################################################################################
# VSEPR-Sim v2.3.1 - Minimal Build & Package (LabelMe only)
# Builds working components and creates distributable package
################################################################################

set -euo pipefail

VERSION="2.3.1"
BUILD_DATE="$(date +%Y%m%d)"
PKG_NAME="vsepr-sim-labelme-${VERSION}"
PKG_DIR="dist/${PKG_NAME}"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

GREEN='\033[0;32m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

log_step() { echo -e "${CYAN}${BOLD}[STEP]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }

echo -e "${CYAN}${BOLD}"
cat << 'EOF'
╔═══════════════════════════════════════════════════════════════╗
║  VSEPR-Sim LabelMe v2.3.1 - Distribution Package             ║
╚═══════════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

# Clean
log_step "Cleaning..."
rm -rf dist/
mkdir -p "$PKG_DIR"/{bin,data,tools,scripts,docs}

# Build LabelMe
log_step "Building LabelMe..."
./scripts/labelme.sh build

# Run tests
log_step "Running tests..."
./scripts/labelme_test.sh

# Copy files
log_step "Packaging..."
cp build/bin/labelme "$PKG_DIR/bin/"
cp -r data "$PKG_DIR/"
cp tools/labelme.c "$PKG_DIR/tools/"
cp scripts/labelme*.sh "$PKG_DIR/scripts/"
chmod +x "$PKG_DIR/scripts"/*.sh

# Documentation
cp LABELME_GUIDE.md LABELME_COMPLETE.md README.md "$PKG_DIR/docs/" 2>/dev/null || true

# Create installer
cat > "$PKG_DIR/install.sh" << 'INSTALL_EOF'
#!/usr/bin/env bash
INSTALL_DIR="${1:-$HOME/vsepr-sim-labelme}"
mkdir -p "$INSTALL_DIR"
cp -r bin data tools scripts docs "$INSTALL_DIR/"
chmod +x "$INSTALL_DIR"/scripts/*.sh
chmod +x "$INSTALL_DIR"/bin/*
echo "✅ Installed to: $INSTALL_DIR"
echo "Add to PATH: export PATH=\"\$PATH:$INSTALL_DIR/bin\""
echo "Test: $INSTALL_DIR/scripts/labelme.sh test"
INSTALL_EOF
chmod +x "$PKG_DIR/install.sh"

# Manifest
cat > "$PKG_DIR/README.txt" << README_EOF
VSEPR-Sim LabelMe v${VERSION}
Build Date: ${BUILD_DATE}

Molecular Phase State Labeling System

Quick Start:
  ./install.sh
  ./scripts/labelme.sh test
  ./scripts/labelme.sh label H2O 298.15

Documentation: docs/LABELME_GUIDE.md

Requirements:
  - Linux, WSL2, or macOS
  - GCC/Clang for rebuilding
  - No other dependencies

Components:
  - bin/labelme        (C binary, ~20 KB)
  - data/states_db.csv (34 molecules)
  - scripts/labelme.sh (CLI wrapper)
  - tools/labelme.c    (source code)
README_EOF

# Create archives
log_step "Creating archives..."
cd dist
tar czf "${PKG_NAME}.tar.gz" "$PKG_NAME"
if command -v zip &> /dev/null; then
    zip -r "${PKG_NAME}.zip" "$PKG_NAME" > /dev/null
fi
cd ..

echo ""
echo -e "${GREEN}${BOLD}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}${BOLD}║  ✅ Package Complete!                                         ║${NC}"
echo -e "${GREEN}${BOLD}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""
ls -lh "dist/${PKG_NAME}.tar.gz"
ls -lh "dist/${PKG_NAME}.zip" 2>/dev/null || true
echo ""
echo "Test install: cd dist/${PKG_NAME} && ./install.sh /tmp/labelme-test"
