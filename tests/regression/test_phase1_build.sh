#!/bin/bash
echo "========================================="
echo "Phase 1 Formula Builder Test Suite"
echo "========================================="
echo ""

echo "✓ Star-like molecules (should succeed):"
echo "----------------------------------------"
for mol in H2O CH4 NH3 H2S HCl HF SF6 PF5 PCl5 ClF3 BrF5 XeF2 XeF4 XeF6 H2SO4 H3PO4 IF7; do
    result=$(./build/bin/vsepr build $mol 2>&1 | grep -c "Molecule constructed")
    if [ "$result" -eq 1 ]; then
        echo "  ✓ $mol"
    else
        echo "  ✗ $mol FAILED"
    fi
done

echo ""
echo "✗ Multi-center molecules (should reject):"
echo "----------------------------------------"
for mol in C2H6 C3H8 C6H12O6 C2H4 C2H2; do
    result=$(./build/bin/vsepr build $mol 2>&1 | grep -c "Multi-center topology not yet supported")
    if [ "$result" -eq 1 ]; then
        echo "  ✓ $mol (correctly rejected)"
    else
        echo "  ✗ $mol FAILED (should have been rejected)"
    fi
done

echo ""
echo "========================================="
echo "Test complete!"
echo "========================================="
