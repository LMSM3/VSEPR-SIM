#!/bin/bash
# Demo script for improved MD methods
# Shows different thermostats and mass handling

echo "================================"
echo "MD Improvements Demonstration"
echo "================================"
echo ""

BIN=./build/bin/vsepr_batch

# Test 1: NVE (Energy-conserving dynamics)
echo "Test 1: NVE Dynamics (No Thermostat)"
echo "-------------------------------------"
$BIN << 'EOF'
spawn argon 64 --box 20 --lattice fcc --T 300
set mode md
set md.thermostat none
set md.timestep 0.001
advance 1000
energy
summary
exit
EOF

echo ""
echo "Test 2: Berendsen Thermostat (Fast Equilibration)"
echo "-------------------------------------------------"
$BIN << 'EOF'
spawn argon 64 --box 20 --lattice fcc --T 100
set mode md
set md.thermostat berendsen
set md.temperature 300
set md.tau_thermostat 0.1
set md.timestep 0.001
advance 1000
energy
summary
exit
EOF

echo ""
echo "Test 3: Temperature Control Comparison"
echo "--------------------------------------"
echo "Starting at 100K, heating to 500K with different thermostats..."

# Berendsen
echo "  - Berendsen (fast but non-canonical):"
$BIN << 'EOF'
spawn argon 27 --box 15 --lattice fcc --T 100
set mode md
set md.thermostat berendsen
set md.temperature 500
set md.tau_thermostat 0.05
set md.timestep 0.001
advance 500
exit
EOF

# Langevin  
echo "  - Langevin (stochastic, canonical):"
$BIN << 'EOF'
spawn argon 27 --box 15 --lattice fcc --T 100
set mode md
set md.thermostat langevin
set md.temperature 500
set md.damping 1.0
set md.timestep 0.001
advance 500
exit
EOF

echo ""
echo "Test 4: Small Molecule MD (Water-like)"
echo "--------------------------------------"
$BIN << 'EOF'
build H2O
set mode md
set md.thermostat berendsen
set md.temperature 300
set md.timestep 0.0005
advance 100
energy
exit
EOF

echo ""
echo "================================"
echo "Demo Complete!"
echo "================================"
echo ""
echo "Key improvements demonstrated:"
echo "  ✓ Multiple thermostat options (none, berendsen, langevin)"
echo "  ✓ Proper mass handling (F = ma)"
echo "  ✓ Maxwell-Boltzmann velocity initialization"
echo "  ✓ Flexible temperature control"
echo ""
echo "See docs/MD_IMPROVEMENTS.md for full documentation"
