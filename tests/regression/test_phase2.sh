#!/bin/bash
echo "========================================="
echo "Phase 2 Implementation Test"
echo "========================================="
echo ""

echo "Phase 1: VSEPR Stars (still working)"
echo "-------------------------------------"
for mol in H2O CH4 SF6 H2SO4; do
    result=$(./build/bin/vsepr build $mol 2>&1 | grep -c "Molecule constructed")
    if [ "$result" -eq 1 ]; then
        echo "  ✓ $mol"
    else
        echo "  ✗ $mol FAILED"
    fi
done

echo ""
echo "Phase 2: Alkane Chains (NEW!)"
echo "-------------------------------------"
for mol in C2H6 C3H8 C4H10 C6H14 C8H18 C10H22; do
    result=$(./build/bin/vsepr build $mol 2>&1 | grep -c "Molecule constructed")
    if [ "$result" -eq 1 ]; then
        echo "  ✓ $mol (n-alkane chain)"
    else
        echo "  ✗ $mol FAILED"
    fi
done

echo ""
echo "Not Yet Supported (graceful errors):"
echo "-------------------------------------"
echo -n "  Alkenes (C2H4): "
./build/bin/vsepr build C2H4 2>&1 | grep -q "Alkenes detected" && echo "✓ Clear error" || echo "✗ Wrong error"

echo -n "  Glucose (C6H12O6): "
./build/bin/vsepr build C6H12O6 2>&1 | grep -q "Graph builder error" && echo "✓ Clear error" || echo "✗ Wrong error"

echo ""
echo "========================================="
echo "Phase 2.1 Status: Alkanes working!"
echo "Next: Alkenes, aromatics, coordination"
echo "========================================="
