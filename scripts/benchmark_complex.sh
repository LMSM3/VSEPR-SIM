#!/bin/bash
# Benchmark Complex and Noble Gas Compounds

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  VSEPR-Sim Complex Molecule Benchmarking Suite              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

VSEPR_BIN="./build/bin/vsepr"
OUTPUT_DIR="benchmark_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

mkdir -p "$OUTPUT_DIR"

# Results summary file
SUMMARY="${OUTPUT_DIR}/benchmark_summary_${TIMESTAMP}.txt"

echo "Benchmark Run: $TIMESTAMP" > "$SUMMARY"
echo "═══════════════════════════════════════════════════════════" >> "$SUMMARY"
echo "" >> "$SUMMARY"

printf "%-20s %-25s %-12s %-12s %-15s %s\n" "Formula" "Geometry" "Atoms" "Energy" "Converged" "Time" >> "$SUMMARY"
echo "────────────────────────────────────────────────────────────────────────────────────────────" >> "$SUMMARY"

# Test molecules
MOLECULES=(
    # Noble gas compounds
    "XeF2:Linear"
    "XeF4:Square_Planar"
    "XeF6:Distorted_Octahedral"
    "KrF2:Linear"
    
    # Hypervalent
    "SF6:Octahedral"
    "PF5:Trigonal_Bipyramidal"
    "ClF3:T-shaped"
    "IF5:Square_Pyramidal"
    "BrF5:Square_Pyramidal"
    
    # Oxoacids
    "H2SO4:Tetrahedral"
    "H3PO4:Tetrahedral"
    "HNO3:Trigonal_Planar"
    "HClO4:Tetrahedral"
    
    # Complex
    "PCl5:Trigonal_Bipyramidal"
    "AsF5:Trigonal_Bipyramidal"
    "SbCl5:Trigonal_Bipyramidal"
    "XeOF4:Square_Pyramidal"
)

for entry in "${MOLECULES[@]}"; do
    IFS=':' read -r formula expected_geom <<< "$entry"
    
    echo ""
    echo "▶ Testing: $formula (Expected: $expected_geom)"
    
    output_file="${OUTPUT_DIR}/${formula}_${TIMESTAMP}.xyz"
    
    # Time the build
    start_time=$(date +%s.%N)
    
    # Run build with optimization
    result=$($VSEPR_BIN build "$formula" --optimize --energy --output "$output_file" 2>&1)
    
    end_time=$(date +%s.%N)
    elapsed=$(echo "$end_time - $start_time" | bc)
    
    # Extract info from output
    atoms=$(echo "$result" | grep -A1 "Building 3D structure" | grep "Atoms" | awk '{print $2}')
    geometry=$(echo "$result" | grep "Geometry" | awk -F'Geometry' '{print $2}' | xargs)
    energy=$(echo "$result" | grep "Final energy" | awk '{print $3}')
    converged=$(echo "$result" | grep -q "did not fully converge" ; echo "NO" ; echo "YES")
    
    # Handle empty values
    [ -z "$atoms" ] && atoms="?"
    [ -z "$geometry" ] ; geometry="?"
    [ -z "$energy" ] && energy="?"
    
    # Print to summary
    printf "%-20s %-25s %-12s %-12s %-15s %.3fs\n" \
        "$formula" "$geometry" "$atoms" "$energy" "$converged" "$elapsed" >> "$SUMMARY"
    
    # Print to console
    if [ "$converged" = "YES" ]; then
        echo "  ✓ $formula: $geometry ($energy kcal/mol, ${elapsed}s)"
    else
        echo "  ⚠ $formula: $geometry ($energy kcal/mol, not converged, ${elapsed}s)"
    fi
done

echo ""
echo "═══════════════════════════════════════════════════════════" >> "$SUMMARY"
echo "" >> "$SUMMARY"

echo ""
echo "✓ Benchmarking complete!"
echo "  Results: $SUMMARY"
echo ""

cat "$SUMMARY"
