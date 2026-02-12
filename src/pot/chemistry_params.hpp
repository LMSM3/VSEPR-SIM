#pragma once
/**
 * chemistry_params.hpp
 * ====================
 * Chemistry-aware parameter assignment for molecular force fields.
 * 
 * Uses hybridization and bond order information for:
 * - Geometry-aware equilibrium angles
 * - Hybridization-dependent force constants
 * - Torsion parameter selection
 * 
 * REPLACES: Element-only and VSEPR-only heuristics
 */

#include "pot/energy.hpp"
#include "core/chemistry.hpp"
#include "core/types.hpp"
#include <vector>
#include <unordered_map>

namespace vsepr {

//=============================================================================
// Chemistry-Aware Angle Parameters
//=============================================================================

/**
 * Assign angle parameters using hybridization and bond orders.
 * 
 * Key improvements over VSEPR-only approach:
 * - sp3 C: 109.5° (tetrahedral) with k=60
 * - sp2 C: 120.0° (trigonal) with k=80
 * - sp C:  180.0° (linear) with k=100
 * 
 * This KILLS star topologies because angles are strongly enforced!
 */
inline std::vector<AngleParams> assign_angle_parameters_chemistry(
    const std::vector<Angle>& angles,
    const std::vector<Atom>& atoms,
    const std::vector<Bond>& bonds)
{
    std::vector<AngleParams> params;
    params.reserve(angles.size());
    
    for (const auto& angle : angles) {
        const Atom& central = atoms[angle.j];
        
        // Get hybridization for central atom
        Hybridization hyb = get_atom_hybridization(central, bonds);
        
        // Get ideal angle and force constant from hybridization
        double theta0 = ideal_angle_for_hybridization(hyb);
        double k = angle_force_constant_from_hybridization(hyb);
        
        params.push_back({theta0, k});
    }
    
    return params;
}

//=============================================================================
// Chemistry-Aware Torsion Parameters
//=============================================================================

/**
 * Torsion parameter selection based on bond types.
 * 
 * Rules:
 * - sp3-sp3: n=3, V=1.4 kcal/mol (ethane-like)
 * - sp2-sp3: n=3, V=0.5 kcal/mol (weaker)
 * - sp2-sp2: n=2, V=3.0 kcal/mol (planar preference)
 * - sp-sp3:  n=1, V=0.1 kcal/mol (very weak)
 */
struct ChemistryTorsionParams {
    int n;           // Periodicity
    double V;        // Barrier height (kcal/mol)
    double phi0;     // Phase offset (radians)
};

inline ChemistryTorsionParams get_torsion_params_chemistry(
    const Atom& atom_j,
    const Atom& atom_k,
    const std::vector<Bond>& bonds,
    uint8_t central_bond_order = 1)
{
    ChemistryTorsionParams params;
    
    // Get hybridizations
    Hybridization hyb_j = get_atom_hybridization(atom_j, bonds);
    Hybridization hyb_k = get_atom_hybridization(atom_k, bonds);
    
    // Double/triple bonds: restricted rotation
    if (central_bond_order >= 2) {
        params.n = 2;
        params.V = 20.0;  // Very high barrier (essentially rigid)
        params.phi0 = 0.0;
        return params;
    }
    
    // Single bond torsions by hybridization
    if (hyb_j == Hybridization::SP3 && hyb_k == Hybridization::SP3) {
        // sp3-sp3: Classic ethane-like
        params.n = 3;
        params.V = 1.4;
        params.phi0 = 0.0;  // Staggered at 60°, 180°, 300°
    }
    else if (hyb_j == Hybridization::SP2 && hyb_k == Hybridization::SP2) {
        // sp2-sp2: Prefer planar (e.g., biphenyl)
        params.n = 2;
        params.V = 3.0;
        params.phi0 = M_PI;  // Planar at 0° and 180°
    }
    else if ((hyb_j == Hybridization::SP2 && hyb_k == Hybridization::SP3) ||
             (hyb_j == Hybridization::SP3 && hyb_k == Hybridization::SP2)) {
        // sp2-sp3: Moderate barrier
        params.n = 3;
        params.V = 0.5;
        params.phi0 = 0.0;
    }
    else if (hyb_j == Hybridization::SP || hyb_k == Hybridization::SP) {
        // sp-X: Very weak (near-free rotation)
        params.n = 1;
        params.V = 0.1;
        params.phi0 = 0.0;
    }
    else {
        // Default: weak 3-fold
        params.n = 3;
        params.V = 0.2;
        params.phi0 = 0.0;
    }
    
    return params;
}

/**
 * Assign torsion parameters for all torsions using chemistry.
 * Requires bond order information.
 */
inline std::vector<TorsionParams> assign_torsion_parameters_chemistry(
    const std::vector<Torsion>& torsions,
    const std::vector<Atom>& atoms,
    const std::vector<Bond>& bonds)
{
    std::vector<TorsionParams> params;
    params.reserve(torsions.size());
    
    // Build bond lookup map for central bond order
    std::unordered_map<uint64_t, uint8_t> bond_orders;
    for (const auto& bond : bonds) {
        uint64_t key = (static_cast<uint64_t>(std::min(bond.i, bond.j)) << 32) | 
                       std::max(bond.i, bond.j);
        bond_orders[key] = bond.order;
    }
    
    for (const auto& torsion : torsions) {
        const Atom& atom_j = atoms[torsion.j];
        const Atom& atom_k = atoms[torsion.k];
        
        // Get central bond order
        uint64_t key = (static_cast<uint64_t>(std::min(torsion.j, torsion.k)) << 32) | 
                       std::max(torsion.j, torsion.k);
        uint8_t bond_order = 1;
        if (bond_orders.find(key) != bond_orders.end()) {
            bond_order = bond_orders[key];
        }
        
        // Get chemistry-aware parameters
        auto chem_params = get_torsion_params_chemistry(atom_j, atom_k, bonds, bond_order);
        
        TorsionParams p;
        p.n = chem_params.n;
        p.V = chem_params.V;
        p.phi0 = chem_params.phi0;
        params.push_back(p);
    }
    
    return params;
}

//=============================================================================
// Torsion Generation with Deduplication
//=============================================================================

/**
 * Generate all unique torsions from bonds.
 * 
 * CRITICAL: Deduplicates i-j-k-l and l-k-j-i (same dihedral).
 * Uses canonical ordering: min(i,l) comes first.
 */
inline std::vector<Torsion> generate_torsions_deduplicated(
    const std::vector<Bond>& bonds,
    size_t num_atoms)
{
    // Build adjacency list
    std::vector<std::vector<uint32_t>> neighbors(num_atoms);
    for (const auto& bond : bonds) {
        neighbors[bond.i].push_back(bond.j);
        neighbors[bond.j].push_back(bond.i);
    }
    
    std::vector<Torsion> torsions;
    std::unordered_set<uint64_t> seen;
    
    // For each bond j-k (central)
    for (const auto& bond : bonds) {
        uint32_t j = bond.i;
        uint32_t k = bond.j;
        
        // Find neighbors of j (excluding k) and neighbors of k (excluding j)
        for (uint32_t i : neighbors[j]) {
            if (i == k) continue;
            for (uint32_t l : neighbors[k]) {
                if (l == j) continue;
                
                // Canonical ordering: ensure min(i,l) comes first
                uint32_t a = i, b = j, c = k, d = l;
                if (d < a) {
                    std::swap(a, d);
                    std::swap(b, c);
                }
                
                // Hash for deduplication
                uint64_t hash = (static_cast<uint64_t>(a) << 48) |
                               (static_cast<uint64_t>(b) << 32) |
                               (static_cast<uint64_t>(c) << 16) |
                               static_cast<uint64_t>(d);
                
                if (seen.insert(hash).second) {
                    torsions.push_back({a, b, c, d});
                }
            }
        }
    }
    
    return torsions;
}

} // namespace vsepr
