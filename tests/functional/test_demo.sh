#!/bin/bash
# Test script for demo mode

cd /mnt/c/Users/Liam/Desktop/vsepr-sim/build

echo "Building vsepr with demo mode..."
cmake --build . --target vsepr -j4

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Build successful!"
    echo ""
    echo "To test the demo mode, run:"
    echo "  cd /mnt/c/Users/Liam/Desktop/vsepr-sim"
    echo "  ./build/bin/vsepr --viz sim --demo"
    echo ""
    echo "This will automatically cycle through molecules:"
    echo "  H2O → CH4 → NH3 → CO2 → H2S → SF6 → PCl5 → XeF4"
    echo ""
else
    echo "✗ Build failed"
    exit 1
fi
