#!/bin/bash
# VSEPR-Sim XYZ Export Demo
# Demonstrates 3D molecule visualization export

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  VSEPR-Sim XYZ Export Demo                                     ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# Clean up previous runs
rm -rf xyz_output my_molecules

echo "Demo 1: Individual XYZ files (default)"
echo "─────────────────────────────────────────"
echo "Command: ./vsepr_opengl_viewer 20 all --viz .xyz"
echo ""
echo "y" | ./vsepr_opengl_viewer 20 all --viz .xyz 2>&1 | grep -E "(XYZ Export|Visualized|exported to)" | head -8
echo ""
echo "Files created: $(ls xyz_output/*.xyz 2>/dev/null | wc -l)"
echo "Example: $(ls xyz_output/*.xyz 2>/dev/null | head -1)"
echo ""

# Clean for next demo
rm -rf xyz_output

echo ""
echo "Demo 2: Watch mode (streaming to single file)"
echo "─────────────────────────────────────────"
echo "Command: ./vsepr_opengl_viewer 30 all --watch molecules.xyz"
echo ""
echo "y" | ./vsepr_opengl_viewer 30 all --watch molecules.xyz 2>&1 | grep -E "(XYZ Export|Visualized|exported to|Open with)" | head -8
echo ""
echo "Watch file size: $(wc -l xyz_output/molecules.xyz 2>/dev/null | awk '{print $1}') lines"
echo ""
cat xyz_output/molecules.xyz 2>/dev/null | head -12
echo "... (showing first 2 molecules)"
echo ""

# Clean for next demo
rm -rf xyz_output

echo ""
echo "Demo 3: Custom directory"
echo "─────────────────────────────────────────"
echo "Command: ./vsepr_opengl_viewer 15 all --viz ./my_molecules"
echo ""
echo "y" | ./vsepr_opengl_viewer 15 all --viz ./my_molecules 2>&1 | grep -E "(XYZ Export|Visualized|exported to)" | head -8
echo ""
echo "Files in my_molecules/: $(ls my_molecules/*.xyz 2>/dev/null | wc -l)"
echo ""

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  XYZ Files Ready for 3D Visualization!                         ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "Recommended viewers:"
echo "  • Avogadro:   https://avogadro.cc/"
echo "  • VMD:        https://www.ks.uiuc.edu/Research/vmd/"
echo "  • PyMOL:      https://pymol.org/"
echo "  • JMol:       http://jmol.sourceforge.net/"
echo "  • ChemDoodle: https://www.chemdoodle.com/"
echo ""
echo "Quick view with Avogadro:"
echo "  avogadro xyz_output/*.xyz"
echo "  avogadro my_molecules/*.xyz"
echo ""
