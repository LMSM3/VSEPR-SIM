#pragma once
#include "atomistic/core/state.hpp"
#include <string>
#include <vector>
#include <memory>

namespace atomistic {
namespace predict {

/**
 * Property prediction from VSEPR topology and electronic structure
 * 
 * Leverages existing vsepr-sim codebase to predict:
 * - Partial charges (electronegativity equilibration)
 * - Bond dipoles and molecular dipole moment
 * - Polarizability estimates
 * - HOMO-LUMO gap estimates (from valence orbital energies)
 * - Reactivity indices (Fukui functions, electrophilicity)
 * 
 * These predictions are fast (no QM calculation) and suitable for
 * high-throughput screening before expensive DFT/CCSD(T) calculations.
 */

struct ElectronicProperties {
    std::vector<double> partial_charges;  // Mulliken-like charges (e)
    double dipole_moment;                 // Total dipole (Debye)
    Vec3 dipole_vector;                   // Dipole direction
    double polarizability;                // Isotropic α (Å³)
    double ionization_potential;          // Estimated IP (eV)
    double electron_affinity;             // Estimated EA (eV)
    double electronegativity;             // Mulliken χ = (IP+EA)/2
    double hardness;                      // Chemical hardness η = (IP-EA)/2
    double electrophilicity;              // ω = χ²/(2η)
};

struct ReactivityIndices {
    std::vector<double> fukui_plus;       // f+ = q(N) - q(N-1), nucleophilic attack
    std::vector<double> fukui_minus;      // f- = q(N+1) - q(N), electrophilic attack
    std::vector<double> fukui_zero;       // f0 = [f+ + f-]/2, radical attack
    std::vector<double> local_softness;   // s = S·f where S = 1/(2η)
};

struct GeometryPrediction {
    std::vector<Vec3> positions;          // Predicted 3D coordinates
    std::vector<double> bond_orders;      // Pauling bond orders
    double strain_energy;                 // Ring/torsional strain (kcal/mol)
    std::string vsepr_class;              // AX_nE_m notation
    double predicted_barrier;             // Rotation barrier (kcal/mol)
};

/**
 * Predict electronic properties from molecular topology
 * Uses electronegativity equilibration (QEq method) for charges
 */
ElectronicProperties predict_electronic_properties(const State& s);

/**
 * Predict reactivity indices for each atom
 * Requires electronic properties (charges at N, N±1 electrons)
 */
ReactivityIndices predict_reactivity(const State& s, 
                                     const ElectronicProperties& props);

/**
 * Predict 3D geometry from molecular formula and connectivity
 * Uses VSEPR theory + steric effects
 */
GeometryPrediction predict_geometry_from_vsepr(const State& s);

/**
 * Predict reaction energy (ΔE) for A + B → C + D
 * Fast estimate using bond energies and strain corrections
 */
double predict_reaction_energy(const State& reactants_A,
                               const State& reactants_B,
                               const State& products_C,
                               const State& products_D);

/**
 * Predict activation barrier for reaction
 * Uses Bell-Evans-Polanyi principle: Ea = Ea0 + α·ΔH
 */
double predict_activation_barrier(const State& reactant,
                                  const State& product,
                                  double intrinsic_barrier = 10.0);

} // namespace predict
} // namespace atomistic
