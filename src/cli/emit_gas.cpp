#include "cli/emit_gas.hpp"
#include "cli/emit_output.hpp"
#include <iostream>
#include <random>
#include <stdexcept>

namespace vsepr {
namespace cli {

// Expand formula to atom list matching target count
// Example: H2O + cloud=300 → [H,H,O, H,H,O, ...] (200 H, 100 O)
std::vector<std::string> expand_formula(
    const std::vector<std::pair<std::string, int>>& composition,
    int target_atoms
) {
    // Calculate total atoms per "molecule unit"
    int ratio_sum = 0;
    for (const auto& [elem, count] : composition) {
        ratio_sum += count;
    }
    
    if (ratio_sum == 0) {
        throw std::runtime_error("Invalid composition: no atoms");
    }
    
    // Calculate how many complete units we can fit
    int complete_units = target_atoms / ratio_sum;
    int remainder = target_atoms % ratio_sum;
    
    if (remainder != 0) {
        std::cerr << "WARNING: --cloud " << target_atoms 
                  << " is not divisible by formula ratio (" << ratio_sum << ")\n";
        std::cerr << "Generating " << complete_units * ratio_sum 
                  << " atoms (" << complete_units << " units)\n";
    }
    
    // Generate atom list
    std::vector<std::string> atoms;
    atoms.reserve(complete_units * ratio_sum);
    
    for (int unit = 0; unit < complete_units; ++unit) {
        for (const auto& [elem, count] : composition) {
            for (int i = 0; i < count; ++i) {
                atoms.push_back(elem);
            }
        }
    }
    
    return atoms;
}

int emit_gas(const ParsedCommand& cmd, RunContext& ctx) {
    // Validate --cloud provided
    if (cmd.action_params.cloud_size == 0) {
        std::cerr << "ERROR: Gas emission requires --cloud <N>\n";
        std::cerr << "Example: --cloud 300 (generates 300 atoms)\n";
        return 1;
    }
    
    // Validate box dimensions
    if (ctx.cell_or_box.size() != 3) {
        std::cerr << "ERROR: Gas emission requires --box x,y,z\n";
        return 1;
    }
    
    double box_x = ctx.cell_or_box[0];
    double box_y = ctx.cell_or_box[1];
    double box_z = ctx.cell_or_box[2];
    
    if (box_x <= 0 || box_y <= 0 || box_z <= 0) {
        std::cerr << "ERROR: Box dimensions must be positive\n";
        return 1;
    }
    
    std::cout << "Generating gas cloud:\n";
    std::cout << "  Formula: " << cmd.spec.formula() << "\n";
    std::cout << "  Target atoms: " << cmd.action_params.cloud_size << "\n";
    std::cout << "  Box: " << box_x << " × " << box_y << " × " << box_z << " Å\n";
    std::cout << "  Seed: " << ctx.seed << "\n";
    std::cout << "  PBC: " << (ctx.rules.pbc_enabled ? "ON" : "OFF") << "\n";
    
    // Expand formula to atom list
    std::vector<std::string> atom_types = expand_formula(
        cmd.spec.composition.elements,
        cmd.action_params.cloud_size
    );
    
    std::cout << "  Actual atoms: " << atom_types.size() << "\n";
    
    // Count element occurrences
    std::map<std::string, int> counts;
    for (const auto& elem : atom_types) {
        counts[elem]++;
    }
    std::cout << "  Composition: ";
    bool first = true;
    for (const auto& [elem, count] : counts) {
        if (!first) std::cout << ", ";
        std::cout << elem << ": " << count;
        first = false;
    }
    std::cout << "\n";
    
    // Generate random positions (uniform distribution in box)
    std::uniform_real_distribution<double> dist_x(0.0, box_x);
    std::uniform_real_distribution<double> dist_y(0.0, box_y);
    std::uniform_real_distribution<double> dist_z(0.0, box_z);
    
    std::vector<Atom> atoms;
    atoms.reserve(atom_types.size());
    
    for (const auto& elem : atom_types) {
        double x = dist_x(ctx.rng);
        double y = dist_y(ctx.rng);
        double z = dist_z(ctx.rng);
        atoms.push_back({elem, x, y, z});
    }
    
    // Write XYZ
    write_xyz(ctx.output_path, atoms, cmd, ctx);
    
    std::cout << "\nNote: Positions are randomly generated.\n";
    std::cout << "No overlap checking or minimum distance enforcement.\n";
    std::cout << "Consider running 'relax' to optimize geometry.\n";
    
    return 0;
}

} // namespace cli
} // namespace vsepr
