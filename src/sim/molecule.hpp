#ifndef VSEPR_MOLECULE_H
#define VSEPR_MOLECULE_H

#include "core/types.hpp"
#include <vector>

namespace vsepr {

// ============================================================================
// Molecule: Container for atoms, topology, and coordinates
// ============================================================================
class Molecule {
public:
    // Atom data
    std::vector<Atom>   atoms;
    std::vector<double> coords;  // Flat array: [x0,y0,z0, x1,y1,z1, ...]
    
    // Topology
    std::vector<Bond>     bonds;
    std::vector<Angle>    angles;
    std::vector<Torsion>  torsions;
    std::vector<Improper> impropers;
    
    // Optional cell (for periodic systems, MOFs, crystals)
    Cell cell;
    
    // ========================================================================
    // Construction and Initialization
    // ========================================================================
    Molecule() = default;
    
    // Add an atom with initial position
    void add_atom(uint8_t Z, double x, double y, double z, uint32_t flags = 0);
    
    // Add topology terms
    void add_bond(uint32_t i, uint32_t j, uint8_t order = 1);
    void add_angle(uint32_t i, uint32_t j, uint32_t k);
    void add_torsion(uint32_t i, uint32_t j, uint32_t k, uint32_t l);
    void add_improper(uint32_t i, uint32_t j, uint32_t k, uint32_t l);
    
    // Auto-generate angles from bond topology
    void generate_angles_from_bonds();
    
    // Auto-generate torsions from bond topology
    void generate_torsions_from_bonds();
    
    // ========================================================================
    // Accessors
    // ========================================================================
    size_t num_atoms() const { return atoms.size(); }
    size_t num_bonds() const { return bonds.size(); }
    
    // Get position of atom i
    void get_position(uint32_t i, double& x, double& y, double& z) const;
    
    // Set position of atom i
    void set_position(uint32_t i, double x, double y, double z);
    
    // ========================================================================
    // Validation and Quality Control
    // ========================================================================
    
    // Check if any atoms are colocated (within tolerance)
    bool has_colocated_atoms(double tolerance = 1e-6) const;
    
    // Get list of colocated atom pairs
    std::vector<std::pair<uint32_t, uint32_t>> find_colocated_atoms(double tolerance = 1e-6) const;
    
    // Validate molecule structure (throws on error)
    void validate_structure(double colocation_tolerance = 1e-6) const;
    
private:
    uint32_t next_id = 0;  // Auto-increment atom IDs
    
    // Calculate distance between two atoms
    double distance(uint32_t i, uint32_t j) const;
};

} // namespace vsepr

#endif // VSEPR_MOLECULE_H
