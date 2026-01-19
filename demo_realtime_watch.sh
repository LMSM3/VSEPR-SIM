#!/bin/bash
# Demo: Generate 50 molecules with 1-second delay for real-time visualization
# Usage: ./demo_realtime_watch.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VSEPR_BIN="$SCRIPT_DIR/build/bin/vsepr"
OUTPUT_DIR="$SCRIPT_DIR/xyz_output"
WATCH_FILE="$OUTPUT_DIR/realtime_demo.xyz"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  Real-Time Molecule Generation Demo                         ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo "This demo generates 50 molecules with 1-second delays"
echo "Perfect for watching in real-time with Avogadro or VMD!"
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"
rm -f "$WATCH_FILE"  # Clear previous run

# Check if vsepr binary exists
if [ ! -f "$VSEPR_BIN" ]; then
    echo -e "${YELLOW}Warning: vsepr binary not found at $VSEPR_BIN${NC}"
    echo "Building project first..."
    "$SCRIPT_DIR/build_universal.sh"
fi

# Array of diverse molecular formulas
FORMULAS=(
    "H2O"      # Water
    "NH3"      # Ammonia
    "CH4"      # Methane
    "CO2"      # Carbon dioxide
    "H2O2"     # Hydrogen peroxide
    "N2O"      # Nitrous oxide
    "SO2"      # Sulfur dioxide
    "H2S"      # Hydrogen sulfide
    "HCN"      # Hydrogen cyanide
    "HCl"      # Hydrogen chloride
    "C2H6"     # Ethane
    "C2H4"     # Ethylene
    "C2H2"     # Acetylene
    "CH3OH"    # Methanol
    "CH2O"     # Formaldehyde
    "C6H6"     # Benzene
    "C3H8"     # Propane
    "C4H10"    # Butane
    "CCl4"     # Carbon tetrachloride
    "SF6"      # Sulfur hexafluoride
    "PCl5"     # Phosphorus pentachloride
    "XeF4"     # Xenon tetrafluoride
    "IF5"      # Iodine pentafluoride
    "BrF5"     # Bromine pentafluoride
    "ClF3"     # Chlorine trifluoride
    "NF3"      # Nitrogen trifluoride
    "PF5"      # Phosphorus pentafluoride
    "AsF5"     # Arsenic pentafluoride
    "SbCl5"    # Antimony pentachloride
    "XeF2"     # Xenon difluoride
    "KrF2"     # Krypton difluoride
    "BF3"      # Boron trifluoride
    "AlCl3"    # Aluminum chloride
    "SiH4"     # Silane
    "PH3"      # Phosphine
    "H2Se"     # Hydrogen selenide
    "H2Te"     # Hydrogen telluride
    "CS2"      # Carbon disulfide
    "NO2"      # Nitrogen dioxide
    "N2O4"     # Dinitrogen tetroxide
    "O3"       # Ozone
    "SO3"      # Sulfur trioxide
    "Cl2O"     # Dichlorine monoxide
    "F2O"      # Difluorine monoxide
    "Br2"      # Bromine
    "I2"       # Iodine
    "HBr"      # Hydrogen bromide
    "HI"       # Hydrogen iodide
    "HF"       # Hydrogen fluoride
    "NaCl"     # Sodium chloride
)

echo -e "${GREEN}Starting generation...${NC}"
echo ""
echo "In another terminal, run:"
echo -e "${YELLOW}  avogadro $WATCH_FILE${NC}"
echo "or"
echo -e "${YELLOW}  vmd $WATCH_FILE${NC}"
echo ""
echo "Press Ctrl+C to stop early"
echo ""
sleep 3

for i in {0..49}; do
    FORMULA="${FORMULAS[$i]}"
    
    # Progress indicator
    PERCENT=$(( (i + 1) * 100 / 50 ))
    printf "\r${GREEN}[%3d/50]${NC} %-10s ${BLUE}Progress: %3d%%${NC}" "$((i+1))" "$FORMULA" "$PERCENT"
    
    # Build molecule and append to watch file
    if "$VSEPR_BIN" build "$FORMULA" --xyz temp_molecule.xyz &>/dev/null; then
        # Append to watch file
        cat temp_molecule.xyz >> "$WATCH_FILE"
        rm -f temp_molecule.xyz
    else
        # Fallback: use dummy molecule if build fails
        echo "3" >> "$WATCH_FILE"
        echo "$FORMULA - Build Failed" >> "$WATCH_FILE"
        echo "H 0.0 0.0 0.0" >> "$WATCH_FILE"
        echo "H 1.0 0.0 0.0" >> "$WATCH_FILE"
        echo "O 0.5 0.5 0.5" >> "$WATCH_FILE"
    fi
    
    # Sleep 1 second between molecules
    sleep 1
done

echo ""
echo ""
echo -e "${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  ✓ Generation Complete!                                      ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo "Output file: $WATCH_FILE"
echo ""

# Count molecules in file
MOLECULE_COUNT=$(grep -c "^[0-9]\+$" "$WATCH_FILE")
FILE_SIZE=$(du -h "$WATCH_FILE" | awk '{print $1}')

echo "Statistics:"
echo "  Molecules generated: $MOLECULE_COUNT"
echo "  File size: $FILE_SIZE"
echo "  Total time: ~50 seconds"
echo ""
echo "View with:"
echo "  avogadro $WATCH_FILE"
echo "  vmd $WATCH_FILE"
echo "  pymol $WATCH_FILE"
echo ""
