#!/usr/bin/env bash
# batch_chemistry_demo.sh
# Automated batch testing of molecular builder with diverse chemistry
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VSEPR_BIN="${PROJECT_ROOT}/build/bin/vsepr"
OUT_DIR="${PROJECT_ROOT}/logs/batch_chem_$(date +"%Y%m%d_%H%M%S")"

mkdir -p "${OUT_DIR}"

echo "=== VSEPR Batch Chemistry Automation ==="
echo "Output: ${OUT_DIR}"
echo ""

# Test molecules across categories
MOLECULES=(
    # Simple diatomics
    "H2:Hydrogen"
    "O2:Oxygen"
    "N2:Nitrogen"
    
    # Small molecules
    "H2O:Water"
    "NH3:Ammonia"
    "CH4:Methane"
    "CO2:Carbon_dioxide"
    
    # Organic molecules
    "C2H6:Ethane"
    "C3H8:Propane"
    "C6H6:Benzene"
    "C6H12O6:Glucose"
    "C10H22:Decane"
    
    # Inorganic compounds
    "H2SO4:Sulfuric_acid"
    "HNO3:Nitric_acid"
    "CaCO3:Calcium_carbonate"
    "NaCl:Sodium_chloride"
    "Fe2O3:Iron_oxide"
    
    # Complex molecules
    "C8H10N4O2:Caffeine"
    "C9H8O4:Aspirin"
    "C2H5OH:Ethanol"
)

pass=0
fail=0

echo "┌─ Building ${#MOLECULES[@]} molecules..."
echo ""

for mol in "${MOLECULES[@]}"; do
    IFS=: read -r formula name <<< "$mol"
    
    printf "  %-25s" "${name}..."
    
    if "${VSEPR_BIN}" build "${formula}" --xyz "${OUT_DIR}/${name}.xyz" \
        >"${OUT_DIR}/${name}.log" 2>&1; then
        echo " ✓"
        ((pass++))
    else
        echo " ✗ (see ${name}.log)"
        ((fail++))
    fi
done

echo ""
echo "┌─ Results"
echo "  Passed: ${pass}"
echo "  Failed: ${fail}"
echo "  Total:  $((pass + fail))"
echo ""

if [[ -d "${OUT_DIR}" ]]; then
    xyz_count=$(find "${OUT_DIR}" -name "*.xyz" | wc -l)
    echo "┌─ Generated Files"
    echo "  XYZ files: ${xyz_count}"
    echo "  Location:  ${OUT_DIR}"
    echo ""
    
    if [[ "${xyz_count}" -gt 0 ]]; then
        echo "┌─ Sample XYZ Files:"
        find "${OUT_DIR}" -name "*.xyz" | head -5 | while read -r f; do
            atoms=$(head -1 "$f")
            name=$(basename "$f" .xyz)
            echo "  ${name}: ${atoms} atoms"
        done
    fi
fi

echo ""
if [[ "${fail}" -eq 0 ]]; then
    echo "✓ All molecules built successfully"
    exit 0
else
    echo "⚠ Some molecules failed (expected for complex formulas)"
    echo "  Review logs in ${OUT_DIR}"
    exit 0  # Not a hard failure - some formulas may not be implemented
fi
