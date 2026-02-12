#pragma once
/**
 * chemistry.hpp
 * ==============
 * Centralized chemical typing and valence rules for realistic molecular modeling.
 * 
 * Key Features:
 * - Atom hybridization detection (sp3, sp2, sp)
 * - Bond order support with valence accounting
 * - Geometry-aware angle equilibria
 * - Temperature parameter for Boltzmann weighting
 * 
 * Design Principles:
 * - NO hardcoded 298 K anywhere
 * - NO mixing of energy units (kB matches kcal/mol)
 * - NO circular dependencies (typing before geometry)
 * - NO valence violations
 */

#include "types.hpp"
#include <cmath>
#include <vector>
#include <unordered_set>
#include <algorithm>

namespace vsepr {

//=============================================================================
// Global Thermodynamic Parameter
//=============================================================================

/**
 * Global temperature configuration for Boltzmann weighting.
 * 
 * Usage:
 *   ThermalConfig config(300.0);  // 300 K
 *   double beta = config.beta();  // Returns 1/(kB*T)
 *   
 *   ThermalConfig pure_energy;    // T_K = None (no Boltzmann factors)
 *   if (pure_energy.is_zero_kelvin()) { ... }
 */
struct ThermalConfig {
    // Boltzmann constant in kcal/(mol·K)
    // Value: R/N_A = 1.987204259e-3 kcal/(mol·K)
    static constexpr double kB = 1.987204259e-3;
    
    double T_K = 0.0;  // Temperature in Kelvin (0 = pure energy mode)
    
    ThermalConfig() : T_K(0.0) {}
    explicit ThermalConfig(double temperature_K) : T_K(temperature_K) {}
    
    // Get inverse temperature beta = 1/(kB*T) for Boltzmann weighting
    // Returns infinity for T=0 (pure energy mode)
    double beta() const {
        if (T_K <= 0.0) return std::numeric_limits<double>::infinity();
        return 1.0 / (kB * T_K);
    }
    
    // Check if in pure energy mode (no temperature effects)
    bool is_zero_kelvin() const { return T_K <= 0.0; }
    
    // Boltzmann factor: exp(-beta * E)
    double boltzmann_factor(double energy) const {
        if (is_zero_kelvin()) return (energy <= 0.0) ? 1.0 : 0.0;
        return std::exp(-beta() * energy);
    }
    
    // Free energy from partition function: -kT ln(Z)
    // For conformer ensemble: F = -kT ln(sum_i exp(-E_i/kT))
    double free_energy_from_energies(const std::vector<double>& energies) const {
        if (is_zero_kelvin()) {
            // T=0: free energy = minimum energy
            return *std::min_element(energies.begin(), energies.end());
        }
        
        if (energies.empty()) return 0.0;
        
        // Numerical stability: shift by minimum energy
        double E_min = *std::min_element(energies.begin(), energies.end());
        double Z = 0.0;
        for (double E : energies) {
            Z += std::exp(-beta() * (E - E_min));
        }
        
        return E_min - (1.0 / beta()) * std::log(Z);
    }
};

//=============================================================================
// Atom Hybridization and Typing
//=============================================================================

enum class Hybridization : uint8_t {
    UNKNOWN = 0,
    SP3     = 1,  // Tetrahedral (109.5°)
    SP2     = 2,  // Trigonal planar (120°)
    SP      = 3,  // Linear (180°)
    SP3D    = 4,  // Trigonal bipyramidal
    SP3D2   = 5   // Octahedral
};

/**
 * Infer hybridization from element, degree, and bond orders.
 * 
 * Rules:
 * - Total valence = sum(bond_order) + lone_pairs
 * - sp3: 4 electron domains (e.g., CH4, NH3, H2O)
 * - sp2: 3 electron domains (e.g., C=C, carbonyl)
 * - sp:  2 electron domains (e.g., C≡C, CO2)
 */
inline Hybridization infer_hybridization(
    uint8_t Z,
    const std::vector<uint8_t>& bond_orders,
    uint8_t lone_pairs = 0)
{
    // Count total electron domains
    // Each bond (regardless of order) is ONE domain
    // Each lone pair is ONE domain
    int num_bonds = bond_orders.size();
    int num_domains = num_bonds + lone_pairs;
    
    // Check for multiple bonds (indicates sp2 or sp)
    int max_order = 0;
    int total_bond_electrons = 0;
    for (uint8_t order : bond_orders) {
        max_order = std::max(max_order, (int)order);
        total_bond_electrons += order;
    }
    
    // Carbon-specific rules (most common case)
    if (Z == 6) {
        if (max_order >= 3) return Hybridization::SP;   // Triple bond
        if (max_order == 2) return Hybridization::SP2;  // Double bond
        if (num_domains == 4) return Hybridization::SP3; // Four single bonds
        if (num_domains == 3) return Hybridization::SP2; // Three bonds (carbocation, radical)
        if (num_domains == 2) return Hybridization::SP;  // Two bonds (carbene)
    }
    
    // General heuristic by electron domain count
    switch (num_domains) {
        case 2: return Hybridization::SP;
        case 3: return Hybridization::SP2;
        case 4: return Hybridization::SP3;
        case 5: return Hybridization::SP3D;
        case 6: return Hybridization::SP3D2;
        default: return Hybridization::UNKNOWN;
    }
}

/**
 * Get ideal bond angle for hybridization.
 * Returns angle in radians.
 */
inline double ideal_angle_for_hybridization(Hybridization hyb) {
    constexpr double pi = 3.14159265358979323846;
    switch (hyb) {
        case Hybridization::SP:     return pi;           // 180°
        case Hybridization::SP2:    return 2.0*pi/3.0;   // 120°
        case Hybridization::SP3:    return 1.910633236;  // 109.471° (tetrahedral)
        case Hybridization::SP3D:   return pi/2.0;       // 90° (approx, geometry-dependent)
        case Hybridization::SP3D2:  return pi/2.0;       // 90° (octahedral)
        default:                    return 2.0*pi/3.0;   // Default to 120° if unknown
    }
}

//=============================================================================
// Valence Checking
//=============================================================================

/**
 * Get maximum valence (total bonds) for an element.
 */
inline int max_valence(uint8_t Z) {
    // Common valences (expand as needed)
    switch (Z) {
        case 1:  return 1;  // H
        case 6:  return 4;  // C
        case 7:  return 3;  // N (can be 4 with formal charge)
        case 8:  return 2;  // O (can be 3 with formal charge)
        case 9:  return 1;  // F
        case 15: return 5;  // P (3-5 depending on oxidation)
        case 16: return 6;  // S (2-6 depending on oxidation)
        case 17: return 1;  // Cl (can be higher with hypervalency)
        default: return 8;  // Default upper bound
    }
}

/**
 * Validate that total bond order doesn't exceed valence.
 * Counts: single=1, double=2, triple=3
 */
inline bool check_valence(uint8_t Z, const std::vector<uint8_t>& bond_orders) {
    int total = 0;
    for (uint8_t order : bond_orders) {
        total += order;
    }
    return total <= max_valence(Z);
}

//=============================================================================
// Bond Order Utilities
//=============================================================================

/**
 * Get bonds connected to atom i from bond list.
 * Returns pairs of (neighbor_index, bond_order).
 */
inline std::vector<std::pair<uint32_t, uint8_t>> get_atom_bonds(
    uint32_t atom_idx,
    const std::vector<Bond>& bonds)
{
    std::vector<std::pair<uint32_t, uint8_t>> result;
    for (const auto& bond : bonds) {
        if (bond.i == atom_idx) {
            result.emplace_back(bond.j, bond.order);
        } else if (bond.j == atom_idx) {
            result.emplace_back(bond.i, bond.order);
        }
    }
    return result;
}

/**
 * Get hybridization for an atom given its bonds.
 */
inline Hybridization get_atom_hybridization(
    const Atom& atom,
    const std::vector<Bond>& all_bonds)
{
    auto atom_bonds = get_atom_bonds(atom.id, all_bonds);
    std::vector<uint8_t> orders;
    for (const auto& [neighbor, order] : atom_bonds) {
        orders.push_back(order);
    }
    return infer_hybridization(atom.Z, orders, atom.lone_pairs);
}

//=============================================================================
// Angle Force Constants
//=============================================================================

/**
 * Get angle force constant based on central atom hybridization.
 * Returns k in kcal/mol/rad^2.
 * 
 * Design: Stronger constants for more rigid geometries.
 */
inline double angle_force_constant_from_hybridization(Hybridization hyb) {
    switch (hyb) {
        case Hybridization::SP:     return 100.0;  // Very rigid (linear)
        case Hybridization::SP2:    return 80.0;   // Rigid (planar)
        case Hybridization::SP3:    return 60.0;   // Standard tetrahedral
        case Hybridization::SP3D:   return 40.0;   // More flexible
        case Hybridization::SP3D2:  return 40.0;   // Octahedral
        default:                    return 50.0;   // Default
    }
}

} // namespace vsepr
