#!/bin/bash
# Quick Demo: 50 molecules with 1-second delay using continuous generation
# This uses the vsepr_opengl_viewer for actual molecular generation

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="$SCRIPT_DIR/xyz_output"
WATCH_FILE="$OUTPUT_DIR/realtime_50.xyz"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  Real-Time Generation: 50 Molecules (1 sec delay each)       ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"
rm -f "$WATCH_FILE"

# Compile the continuous generation tool if needed
if [ ! -f "./vsepr_opengl_viewer" ]; then
    echo "Compiling vsepr_opengl_viewer..."
    g++ -std=c++17 -O2 examples/vsepr_opengl_viewer.cpp \
        -o vsepr_opengl_viewer \
        -Iinclude \
        -Ithird_party/glm \
        -pthread
    
    if [ $? -ne 0 ]; then
        echo "Compilation failed!"
        exit 1
    fi
fi

echo -e "${GREEN}Starting real-time generation...${NC}"
echo ""
echo "Open in another terminal for live viewing:"
echo -e "${YELLOW}  avogadro $WATCH_FILE${NC}"
echo ""
echo "The visualization tool will auto-refresh as molecules are added"
echo ""
sleep 2

# Generate 50 molecules with progress updates
# Using a wrapper to add delays between molecules
{
    for i in {1..50}; do
        echo "y"  # Auto-confirm
        sleep 1  # 1 second delay between molecules
    done
} | ./vsepr_opengl_viewer 50 every-other --watch realtime_50.xyz

echo ""
echo -e "${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  ✓ Generation Complete!                                      ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

if [ -f "$WATCH_FILE" ]; then
    FILE_SIZE=$(du -h "$WATCH_FILE" | awk '{print $1}')
    echo "Output: $WATCH_FILE"
    echo "Size: $FILE_SIZE"
    echo ""
    echo "View with:"
    echo "  avogadro $WATCH_FILE"
    echo "  vmd $WATCH_FILE"
    echo "  pymol $WATCH_FILE"
else
    echo "Warning: Output file not created"
fi
