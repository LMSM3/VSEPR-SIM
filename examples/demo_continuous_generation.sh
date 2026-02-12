#!/bin/bash
# VSEPR-Sim Continuous Generation Demo
# Demonstrates C++'s power for large-scale molecular discovery

echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║  VSEPR-Sim Continuous Generation - C++ Power Demonstration      ║"
echo "╚══════════════════════════════════════════════════════════════════╝"
echo ""
echo "This demonstrates how C++ enables continuous molecular discovery:"
echo "  • Generates N molecules (or unlimited)"
echo "  • Real-time statistics tracking"
echo "  • Checkpoint saving for resume capability"
echo "  • Streaming XYZ output for live visualization"
echo "  • Performance metrics (molecules/sec, molecules/hour)"
echo ""

# Clean up previous runs
rm -rf xyz_output final_discovery_checkpoint.txt

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Demo 1: Medium-scale generation (10,000 molecules)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Command: ./vsepr_opengl_viewer 10000 every-other --continue --watch molecules.xyz --checkpoint 2000"
echo ""
echo "y" | timeout 20 ./vsepr_opengl_viewer 10000 every-other --continue --watch molecules.xyz --checkpoint 2000 2>&1 | tail -80

echo ""
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Results:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if [ -f xyz_output/molecules.xyz ]; then
    LINES=$(wc -l xyz_output/molecules.xyz | awk '{print $1}')
    SIZE=$(du -h xyz_output/molecules.xyz | awk '{print $1}')
    echo "✓ XYZ file created:"
    echo "    Path: xyz_output/molecules.xyz"
    echo "    Size: $SIZE ($LINES lines)"
    echo ""
    echo "  First molecule:"
    head -8 xyz_output/molecules.xyz
    echo ""
fi

if [ -f final_discovery_checkpoint.txt ]; then
    echo "✓ Checkpoint saved:"
    echo "    Path: final_discovery_checkpoint.txt"
    head -6 final_discovery_checkpoint.txt
    echo ""
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Performance Characteristics:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Expected throughput:"
echo "  • ~200-300 molecules/sec (standard mode)"
echo "  • ~400-600 molecules/sec (every-other visualization)"
echo "  • ~720,000 - 1,080,000 molecules/hour"
echo ""
echo "Scalability examples:"
echo "  100,000 molecules  → ~5-8 minutes"
echo "  1,000,000 molecules → ~50-80 minutes"
echo "  10,000,000 molecules → ~8-13 hours"
echo ""
echo "Memory efficiency:"
echo "  • Streaming mode: Constant memory (~10-20 MB)"
echo "  • Statistics tracking: O(unique_formulas)"
echo "  • XYZ output: Appended to disk (not held in RAM)"
echo ""

echo ""
echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║  Try These Commands:                                             ║"
echo "╚══════════════════════════════════════════════════════════════════╝"
echo ""
echo "# Generate 100,000 molecules with statistics every 5000:"
echo "  ./vsepr_opengl_viewer 100000 every-other -c --watch all.xyz --checkpoint 5000"
echo ""
echo "# Generate 1 million molecules (takes ~1 hour):"
echo "  ./vsepr_opengl_viewer 1000000 every-other -c --watch million.xyz --checkpoint 10000"
echo ""
echo "# Visualize in Avogadro while generating (in separate terminal):"
echo "  tail -f xyz_output/molecules.xyz | avogadro -"
echo ""
echo "# View statistics:"
echo "  cat final_discovery_checkpoint.txt"
echo ""
