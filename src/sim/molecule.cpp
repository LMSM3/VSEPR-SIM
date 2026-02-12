#include "molecule.hpp"
#include <stdexcept>
#include <cmath>
#include <sstream>

namespace vsepr {

// ============================================================================
// Atom Management with Colocation Prevention
// ============================================================================
void Molecule::add_atom(uint8_t Z, double x, double y, double z, uint32_t flags) {
    // CRITICAL: Validate no colocation with existing atoms
    constexpr double COLOCATION_TOLERANCE = 1e-6;  // 0.000001 Angstrom
    
    for (size_t i = 0; i < num_atoms(); ++i) {
        double xi, yi, zi;
        get_position(i, xi, yi, zi);
        
        double dx = x - xi;
        double dy = y - yi;
        double dz = z - zi;
        double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        if (dist < COLOCATION_TOLERANCE) {
            std::ostringstream err;
            err << "FATAL: Attempt to add colocated atom at ("
                << x << ", " << y << ", " << z << "). "
                << "Atom " << i << " (Z=" << (int)atoms[i].Z << ") already exists at ("
                << xi << ", " << yi << ", " << zi << "). "
                << "Distance: " << dist << " Å (tolerance: " << COLOCATION_TOLERANCE << " Å)";
            throw std::runtime_error(err.str());
        }
    }
    
    Atom atom;
    atom.id = next_id++;
    atom.Z = Z;
    atom.mass = 0.0;  // TODO: Look up from periodic table
    atom.lone_pairs = 0;  // Default: no explicit lone pairs (will be computed by VSEPR)
    atom.flags = flags;
    
    atoms.push_back(atom);
    coords.push_back(x);
    coords.push_back(y);
    coords.push_back(z);
}

// ============================================================================
// Topology Management
// ============================================================================
void Molecule::add_bond(uint32_t i, uint32_t j, uint8_t order) {
    if (i >= num_atoms() || j >= num_atoms()) {
        throw std::out_of_range("Bond atom indices out of range");
    }
    bonds.push_back({i, j, order});
}

void Molecule::add_angle(uint32_t i, uint32_t j, uint32_t k) {
    if (i >= num_atoms() || j >= num_atoms() || k >= num_atoms()) {
        throw std::out_of_range("Angle atom indices out of range");
    }
    angles.push_back({i, j, k});
}

void Molecule::add_torsion(uint32_t i, uint32_t j, uint32_t k, uint32_t l) {
    if (i >= num_atoms() || j >= num_atoms() || 
        k >= num_atoms() || l >= num_atoms()) {
        throw std::out_of_range("Torsion atom indices out of range");
    }
    torsions.push_back({i, j, k, l});
}

void Molecule::add_improper(uint32_t i, uint32_t j, uint32_t k, uint32_t l) {
    if (i >= num_atoms() || j >= num_atoms() || 
        k >= num_atoms() || l >= num_atoms()) {
        throw std::out_of_range("Improper atom indices out of range");
    }
    impropers.push_back({i, j, k, l});
}

// ============================================================================
// Coordinate Access
// ============================================================================
void Molecule::get_position(uint32_t i, double& x, double& y, double& z) const {
    if (i >= num_atoms()) {
        throw std::out_of_range("Atom index out of range");
    }
    size_t idx = 3 * i;
    x = coords[idx];
    y = coords[idx + 1];
    z = coords[idx + 2];
}

void Molecule::set_position(uint32_t i, double x, double y, double z) {
    if (i >= num_atoms()) {
        throw std::out_of_range("Atom index out of range");
    }
    size_t idx = 3 * i;
    coords[idx]     = x;
    coords[idx + 1] = y;
    coords[idx + 2] = z;
}

// ============================================================================
// Angle Generation from Bonds
// ============================================================================
void Molecule::generate_angles_from_bonds() {
    // Clear existing angles
    angles.clear();
    
    // Build neighbor lists from bonds
    std::vector<std::vector<uint32_t>> neighbors(num_atoms());
    for (const auto& bond : bonds) {
        neighbors[bond.i].push_back(bond.j);
        neighbors[bond.j].push_back(bond.i);
    }
    
    // For each atom as potential vertex
    for (uint32_t j = 0; j < num_atoms(); ++j) {
        const auto& nbrs = neighbors[j];
        
        // Need at least 2 neighbors to form an angle
        if (nbrs.size() < 2) continue;
        
        // Generate all pairs of neighbors
        for (size_t a = 0; a < nbrs.size(); ++a) {
            for (size_t b = a + 1; b < nbrs.size(); ++b) {
                uint32_t i = nbrs[a];
                uint32_t k = nbrs[b];
                
                // Add angle i-j-k (j is vertex)
                add_angle(i, j, k);
            }
        }
    }
}

// ============================================================================
// Torsion Generation from Bonds
// ============================================================================
void Molecule::generate_torsions_from_bonds() {
    // Clear existing torsions
    torsions.clear();
    
    // Build neighbor lists from bonds
    std::vector<std::vector<uint32_t>> neighbors(num_atoms());
    for (const auto& bond : bonds) {
        neighbors[bond.i].push_back(bond.j);
        neighbors[bond.j].push_back(bond.i);
    }
    
    // For each bond j-k, generate all i-j-k-l torsions
    for (const auto& bond_jk : bonds) {
        uint32_t j = bond_jk.i;
        uint32_t k = bond_jk.j;
        
        // Find all neighbors of j (excluding k) for position i
        for (uint32_t i : neighbors[j]) {
            if (i == k) continue;  // Skip the central bond
            
            // Find all neighbors of k (excluding j) for position l
            for (uint32_t l : neighbors[k]) {
                if (l == j || l == i) continue;  // Avoid back-tracking
                
                add_torsion(i, j, k, l);
            }
        }
    }
}

// ============================================================================
// Validation Infrastructure - Centralized Atom Tracking
// ============================================================================

double Molecule::distance(uint32_t i, uint32_t j) const {
    if (i >= num_atoms() || j >= num_atoms()) {
        throw std::out_of_range("Atom index out of range in distance calculation");
    }
    
    double xi, yi, zi, xj, yj, zj;
    get_position(i, xi, yi, zi);
    get_position(j, xj, yj, zj);
    
    double dx = xj - xi;
    double dy = yj - yi;
    double dz = zj - zi;
    
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

bool Molecule::has_colocated_atoms(double tolerance) const {
    for (size_t i = 0; i < num_atoms(); ++i) {
        for (size_t j = i + 1; j < num_atoms(); ++j) {
            if (distance(i, j) < tolerance) {
                return true;
            }
        }
    }
    return false;
}

std::vector<std::pair<uint32_t, uint32_t>> Molecule::find_colocated_atoms(double tolerance) const {
    std::vector<std::pair<uint32_t, uint32_t>> colocated;
    
    for (size_t i = 0; i < num_atoms(); ++i) {
        for (size_t j = i + 1; j < num_atoms(); ++j) {
            if (distance(i, j) < tolerance) {
                colocated.push_back({i, j});
            }
        }
    }
    
    return colocated;
}

void Molecule::validate_structure(double colocation_tolerance) const {
    // Check for colocated atoms
    auto colocated = find_colocated_atoms(colocation_tolerance);
    
    if (!colocated.empty()) {
        std::ostringstream err;
        err << "Structure validation FAILED: Found " << colocated.size() 
            << " colocated atom pair(s):\n";
        
        for (const auto& pair : colocated) {
            double dist = distance(pair.first, pair.second);
            double x1, y1, z1, x2, y2, z2;
            get_position(pair.first, x1, y1, z1);
            get_position(pair.second, x2, y2, z2);
            
            err << "  Atoms " << pair.first << " (Z=" << (int)atoms[pair.first].Z 
                << ") and " << pair.second << " (Z=" << (int)atoms[pair.second].Z 
                << ") at distance " << dist << " Å\n"
                << "    Atom " << pair.first << ": (" << x1 << ", " << y1 << ", " << z1 << ")\n"
                << "    Atom " << pair.second << ": (" << x2 << ", " << y2 << ", " << z2 << ")\n";
        }
        
        throw std::runtime_error(err.str());
    }
    
    // Additional validations can be added here:
    // - Bond length sanity checks
    // - Angle range checks
    // - Duplicate bond detection
    // - Connectivity validation
}

} // namespace vsepr
