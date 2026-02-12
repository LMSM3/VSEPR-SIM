#!/bin/bash
# migrate_includes.sh - Update include paths for new structure

echo "=== Migrating Include Paths ==="

# Function to update includes in a file
update_file() {
    local file=$1
    echo "Processing: $file"
    
    # Core includes
    sed -i 's|"frame_snapshot.hpp"|"core/frame_snapshot.hpp"|g' "$file"
    sed -i 's|"frame_buffer.hpp"|"core/frame_buffer.hpp"|g' "$file"
    sed -i 's|"math_vec3.hpp"|"core/math_vec3.hpp"|g' "$file"
    
    # Simulation includes
    sed -i 's|"molecule.h"|"sim/molecule.hpp"|g' "$file"
    sed -i 's|"optimizer.hpp"|"sim/optimizer.hpp"|g' "$file"
    sed -i 's|"sim_state.hpp"|"sim/sim_state.hpp"|g' "$file"
    sed -i 's|"sim_thread.hpp"|"sim/sim_thread.hpp"|g' "$file"
    sed -i 's|"sim_command.hpp"|"sim/sim_command.hpp"|g' "$file"
    
    # Potential includes
    sed -i 's|"energy_model.hpp"|"pot/energy_model.hpp"|g' "$file"
    sed -i 's|"energy.hpp"|"pot/energy.hpp"|g' "$file"
    
    # Visualization includes
    sed -i 's|"renderer.hpp"|"vis/renderer.hpp"|g' "$file"
    sed -i 's|"window.hpp"|"vis/window.hpp"|g' "$file"
    sed -i 's|"ui_panels.hpp"|"vis/ui_panels.hpp"|g' "$file"
}

# Update apps
for file in apps/**/main.cpp; do
    [ -f "$file" ] && update_file "$file"
done

# Update tests
for file in tests/*.cpp; do
    [ -f "$file" ] && update_file "$file"
done

# Update module sources
for file in src/**/*.cpp; do
    [ -f "$file" ] && update_file "$file"
done

for file in src/**/*.hpp; do
    [ -f "$file" ] && update_file "$file"
done

echo "=== Migration Complete ==="
