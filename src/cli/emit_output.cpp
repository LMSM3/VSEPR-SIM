#include "cli/emit_output.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>

namespace vsepr {
namespace cli {

void write_xyz(
    const std::string& path,
    const std::vector<Atom>& atoms,
    const ParsedCommand& cmd,
    const RunContext& ctx
) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("Failed to open output file: " + path);
    }
    
    // Header: atom count
    file << atoms.size() << "\n";
    
    // Metadata-rich comment line for reproducibility
    file << "Generated: " << cmd.spec.formula() 
         << "@" << cmd.spec.mode_string()
         << " action=emit";
    
    // Preset info
    if (!cmd.action_params.preset.empty()) {
        file << " preset=" << cmd.action_params.preset;
    }
    
    // Cloud info
    if (cmd.action_params.cloud_size > 0) {
        file << " cloud=" << cmd.action_params.cloud_size;
    }
    
    // Domain parameters
    if (!ctx.cell_or_box.empty()) {
        if (ctx.has_cell) {
            file << " cell=";
        } else {
            file << " box=";
        }
        file << ctx.cell_or_box[0] << "," 
             << ctx.cell_or_box[1] << "," 
             << ctx.cell_or_box[2];
    }
    
    // RNG seed
    file << " seed=" << ctx.seed;
    
    // PBC status
    if (ctx.rules.pbc_enabled) {
        file << " pbc=ON";
    } else {
        file << " pbc=OFF";
    }
    
    file << "\n";
    
    // Atom data (6 decimal places for Ångström precision)
    for (const auto& atom : atoms) {
        file << std::left << std::setw(3) << atom.element << " "
             << std::fixed << std::setprecision(6)
             << std::setw(12) << atom.x << " "
             << std::setw(12) << atom.y << " "
             << std::setw(12) << atom.z << "\n";
    }
    
    file.close();
    
    std::cout << "Generated: " << path << "\n";
    std::cout << "  Atoms: " << atoms.size() << "\n";
    std::cout << "  Mode: " << cmd.spec.mode_string() << "\n";
    std::cout << "  PBC: " << (ctx.rules.pbc_enabled ? "ON" : "OFF") << "\n";
}

} // namespace cli
} // namespace vsepr
