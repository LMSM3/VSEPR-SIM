#pragma once
/**
 * energy.hpp - Common energy types and result structures
 */

#include <cstdint>
#include <cstddef>
#include <vector>

namespace vsepr {

// ============================================================================
// Energy Parameters
// ============================================================================

struct BondParams {
    double r0;  // Equilibrium bond length (Å)
    double k;   // Force constant (kcal/mol/Å²)
};

struct AngleParams {
    double theta0;  // Equilibrium angle (radians)
    double k;       // Force constant (kcal/mol/rad²)
};

struct TorsionParams {
    double V;   // Barrier height (kcal/mol)
    int n;      // Periodicity
    double phi0;  // Phase shift (radians)
    int multiplicity = 1;  // Degeneracy factor (e.g., 9 for ethane H-C-C-H)
};

// Context for energy evaluations (holds coordinates, gradient, etc.)
struct EnergyContext {
    const std::vector<double>* coords;     // Flat array [x0,y0,z0, x1,y1,z1, ...]
    std::vector<double>* gradient;         // Optional gradient output
    const uint8_t* Z;                      // Atomic numbers (optional)
    size_t n_atoms;                        // Number of atoms
    
    bool compute_gradient() const {
        return gradient != nullptr;
    }
};

// ============================================================================
// Energy Results
// ============================================================================

// Energy breakdown result
struct EnergyResult {
    double total_energy = 0.0;
    double bond_energy = 0.0;
    double angle_energy = 0.0;
    double torsion_energy = 0.0;
    double nonbonded_energy = 0.0;
    double vsepr_energy = 0.0;
    double vdw_energy = 0.0;
    double coulomb_energy = 0.0;
    
    // Per-term counts (for diagnostics)
    int n_bonds = 0;
    int n_angles = 0;
    int n_torsions = 0;
    int n_nonbonded = 0;
    int n_vsepr = 0;
};

} // namespace vsepr
