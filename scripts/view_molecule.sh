#!/usr/bin/env bash
################################################################################
# Auto-launch molecular viewer
# Usage: ./view_molecule.sh molecule.xyz
################################################################################

if [ $# -eq 0 ]; then
    echo "Usage: $0 <molecule.xyz>"
    echo ""
    echo "Example: $0 water.xyz"
    exit 1
fi

XYZ_FILE="$1"

# Check if Python is available
if ! command -v python3 &> /dev/null; then
    echo "Error: Python 3 not found. Please install Python 3."
    exit 1
fi

# Generate and auto-open viewer
echo "Generating molecular viewer for $XYZ_FILE..."
python3 scripts/viewer_generator.py "$XYZ_FILE" --open

if [ $? -ne 0 ]; then
    echo ""
    echo "Generation failed!"
    exit 1
fi

echo ""
echo "Done! Viewer opened in browser."
