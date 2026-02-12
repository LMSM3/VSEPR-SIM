#include "xyz_parser.hpp"
#include <stdexcept>
#include <map>

namespace atomistic {
namespace parsers {

static vsepr::PeriodicTable& get_periodic_table() {
    static vsepr::PeriodicTable pt = []() {
        try {
            return vsepr::PeriodicTable::load_from_json_file("data/PeriodicTableJSON.json");
        } catch (...) {
            try {
                return vsepr::PeriodicTable::load_from_json_file("data/elements.physics.json");
            } catch (...) {
                return vsepr::PeriodicTable();
            }
        }
    }();
    return pt;
}

[[maybe_unused]] static uint8_t get_atomic_number(const std::string& symbol) {
    const auto* elem = get_periodic_table().by_symbol(symbol);
    return elem ? elem->Z : 0;
}

static double get_atomic_mass(const std::string& symbol) {
    const auto* elem = get_periodic_table().by_symbol(symbol);
    return elem ? elem->atomic_mass : 1.0;
}

State from_xyz(const vsepr::io::XYZMolecule& mol) {
    State s;
    s.N = static_cast<uint32_t>(mol.atoms.size());
    
    s.X.reserve(s.N);
    s.V.reserve(s.N);
    s.Q.reserve(s.N);
    s.M.reserve(s.N);
    s.type.reserve(s.N);
    s.F.resize(s.N);

    // Build type map (element symbol → type ID)
    std::map<std::string, uint32_t> type_map;
    uint32_t next_type = 0;

    for (const auto& atom : mol.atoms) {
        auto pos = atom.position;
        s.X.push_back({pos[0], pos[1], pos[2]});
        s.V.push_back({0, 0, 0}); // No velocity in plain XYZ
        s.Q.push_back(0.0);       // No charge in plain XYZ
        
        double mass = get_atomic_mass(atom.element);
        s.M.push_back(mass);

        if (type_map.find(atom.element) == type_map.end()) {
            type_map[atom.element] = next_type++;
        }
        s.type.push_back(type_map[atom.element]);
    }

    // Convert bonds
    s.B.reserve(mol.bonds.size());
    for (const auto& bond : mol.bonds) {
        s.B.push_back({static_cast<uint32_t>(bond.atom_i), static_cast<uint32_t>(bond.atom_j)});
    }

    return s;
}

State from_xyza(const vsepr::io::XYZMolecule& mol) {
    // Start with basic XYZ parsing
    State s = from_xyz(mol);

    // XYZA can have extended attributes; for now just basic
    // Future: parse velocities/charges from metadata if present
    
    return s;
}

} // namespace parsers
} // namespace atomistic
