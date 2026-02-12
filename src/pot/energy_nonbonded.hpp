#pragma once
/*
energy_nonbonded.hpp - Nonbonded van der Waals (VSEPR MODE)
------------------------------------------------------------
Nonbonded van der Waals interactions using Lennard-Jones 12-6 potential model.
https://en.wikipedia.org/wiki/Lennard-Jones_potential

Purpose: Soft repulsion for VSEPR geometry optimization

Related Files:
- src/pot/uff_params.hpp: Shared parameter database (can be used here too)
- meso/models/lj_coulomb.cpp: MD mode (full LJ + Coulomb)

Key Differences from MD Mode:
- WCA (repulsion-only) vs full LJ
- No Coulomb (geometry-driven only)
- Soft parameters (ε = 0.001-0.01 kcal/mol)
- Different scaling (s13 = 0.0-0.3 for VSEPR)

Energy function:
  Full LJ:         E_LJ = 4ε[(σ/r)^12 - (σ/r)^6]
  Repulsion-only (WCA):  E = 4ε[(σ/r)^12 - (σ/r)^6] + ε  for r < 2^(1/6)σ
                         E = 0                           for r ≥ 2^(1/6)σ
                         (preferred for VSEPR - purely repulsive, no attraction)

Note: This file uses its own parameter loading from vdw_radii.hpp and
      lj_epsilon_params.hpp. Could be unified with src/pot/uff_params.hpp
      if needed, but VSEPR-specific scaling makes that low priority.
  
where:
  r   = distance between atoms i and j
  σ   = VDW_radius_i + VDW_radius_j (collision diameter)
  ε   = energy scale (VSEPR: 0.001-0.01 kcal/mol, MD: 0.1+ kcal/mol)

Pair exclusions:
  1-2 (bonded):    excluded (s12 = 0.0)
  1-3 (angle):     scaled (s13 = 0.0-0.5, VSEPR mode uses low values)
  1-4 (torsion):   scaled (s14 = 0.0-0.8, typically 0 until torsions exist)
  1-5+ (nonbonded): full interaction (s15 = 1.0)

Cutoff modes:
  None:          No cutoff (small systems or testing)
  Hard:          Truncate at rcut (force discontinuous - use only for testing)

Parameter Optimization Strategy:
════════════════════════════════════════════════════════════════════════════
Goal: Select defaults that are:
  - Structurally invariant (geometry stable under parameter perturbation)
  - Topology-correct (scaled pairs behave as intended)
  - Numerically stable (no singularities, no discontinuities unless chosen)

Principle: Each parameter class has ONE JOB
  - ε must NOT set geometry in VSEPR mode
  - σ must NOT encode bonding/angles
  - s13/s14 must NOT repair chemistry; only prevent double-counting
  - cutoff must NOT change structure beyond tolerance

Mode Guidance:
  VSEPR mode:
    - s13 ~ 0.0-0.3 (reduce strongly)
    - s14 ~ 0.0-0.2 (or 0 until torsions implemented)
    - epsilon very small (0.001-0.01 kcal/mol)
    - no cutoff or very large (> 50 Å)
    
  MD mode:
    - Scaling from force field specification
    - ε, σ from force field parameters
    - Cutoff for performance: rcut >= 2.5 * sigma_max
    - Use Hard cutoff (ShiftedForce not yet implemented)

Validation Procedure (first-pass):
  1. Freeze base geometry: bonds + VSEPR stable without LJ
  2. ε-insensitivity sweep: vary ε by 100×; angles must change < 0.5°
  3. σ hard-core sanity: nonbonded approach → repulsion, no collapse
  4. s13 sweep (key): disable angles; sweep s13; geometry matches s13=0
  5. Cutoff convergence: compare no-cutoff vs rcut; choose smallest
     with acceptable ΔE/N and stable forces
════════════════════════════════════════════════════════════════════════════
*/

#include "pot/energy.hpp"
#include "../core/geom_ops.hpp"
#include "pot/vdw_radii.hpp"
#include "pot/lj_epsilon_params.hpp"
#include "../core/types.hpp"
#include <vector>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <algorithm>

namespace vsepr {

// ============================================================================
// Nonbonded Pair
// ============================================================================
struct NonbondedPair {
    uint32_t i, j;     // Atom indices
    double scale;      // Scaling factor (0.0 = excluded, 1.0 = full)
};

// ============================================================================
// Nonbonded Configuration (Modular Parameter Structure)
// ============================================================================

// Pair topology scaling factors
// Principle: Control double-counting artifacts, NOT chemistry
struct NonbondedScaling {
    double s12 = 0.0;   // 1-2 (bonded): typically excluded
    double s13 = 0.5;   // 1-3 (angle): reduce strongly in VSEPR mode (0.0-0.3 typical)
    double s14 = 0.8;   // 1-4 (torsion): only meaningful once torsions exist (0-0.2 for VSEPR)
    double s15 = 1.0;   // 1-5+: full nonbonded
};

// Cutoff mode selection
enum class CutoffMode {
    None,          // No cutoff (use for small systems or testing)
    Hard           // Truncate at rcut (force discontinuous - use only for testing)
    // ShiftedEnergy and ShiftedForce: NOT YET IMPLEMENTED
};

// Lennard-Jones potential shape parameters
// Principle: ε must not set geometry in VSEPR mode; σ is excluded volume scale
struct LennardJonesParams {
    double epsilon = 0.01;  // Well depth (kcal/mol) - VSEPR: "small enough to be irrelevant"
    double sigma = 1.0;     // Collision diameter scale (Å) - excluded volume
    bool repulsion_only = true;  // Use WCA (purely repulsive, no attraction)
    bool use_element_specific = true;  // Use element-specific epsilon values (recommended)
    MixingRule mixing_rule = MixingRule::LorentzBerthelot;  // Mixing rule for pairs
    
    // Short-range damping to soften repulsion at very close distances
    bool use_damping = false;  // Enable softer short-range potential
    double damping_coefficient = 1.5;  // Controls damping strength (β in Tang-Toennies)
};

// Cutoff policy parameters
// Rule of thumb: rcut >= 2.5 * sigma_max for LJ convergence
struct CutoffParams {
    CutoffMode mode = CutoffMode::None;  // Default: no cutoff (safest)
    double rcut = 12.0;  // Cutoff distance (Å) - choose by convergence, not chemistry
    double rskin = 0.0;  // Skin distance for neighbor lists (future optimization)
};

// Unified nonbonded configuration
// Goal: Structurally invariant, topology-correct, numerically stable
struct NonbondedConfig {
    NonbondedScaling scaling;
    LennardJonesParams lj;
    CutoffParams cutoff;
    
    // Numerics and safety
    double rmin = 0.5;      // Minimum distance clamp to avoid singularities (Å)
    // Note: PBC/MIC must be applied to coordinates BEFORE calling evaluate()
    // This class operates on already-unwrapped coordinates
};

// Legacy parameter structure (DEPRECATED - use NonbondedConfig instead)
struct NonbondedParams {
    double epsilon = 0.1;      // Well depth (kcal/mol)
    double scale_13 = 0.5;     // 1-3 interaction scaling
    double scale_14 = 0.8;     // 1-4 interaction scaling
    double cutoff = 12.0;      // Cutoff distance (Å), 0 = no cutoff
    bool repulsion_only = true; // Use WCA (recommended for VSEPR)
    
    // Convert to new config format
    NonbondedConfig to_config() const {
        NonbondedConfig cfg;
        cfg.scaling.s13 = scale_13;
        cfg.scaling.s14 = scale_14;
        cfg.lj.epsilon = epsilon;
        cfg.lj.repulsion_only = repulsion_only;
        cfg.cutoff.mode = (cutoff > 0.0) ? CutoffMode::Hard : CutoffMode::None;
        cfg.cutoff.rcut = (cutoff > 0.0) ? cutoff : 1000.0;  // Large value if no cutoff
        return cfg;
    }
};

// ============================================================================
// Lennard-Jones Energy
// ============================================================================
class NonbondedEnergy {
public:
    // Constructor with new config format (preferred)
    NonbondedEnergy(const std::vector<NonbondedPair>& pairs,
                    const std::vector<Atom>& atoms,
                    const NonbondedConfig& config)
        : pairs_(pairs), atoms_(atoms), config_(config) {
        init_sigma_values();
    }
    
    // Constructor with legacy params (DEPRECATED - for backward compatibility)
    NonbondedEnergy(const std::vector<NonbondedPair>& pairs,
                    const std::vector<Atom>& atoms,
                    const NonbondedParams& params)
        : pairs_(pairs), atoms_(atoms), config_(params.to_config()) {
        init_sigma_values();
    }

    double evaluate(EnergyContext& ctx) const {
        if (!ctx.coords) {
            throw std::runtime_error("NonbondedEnergy: null coords");
        }
        
        const auto& coords = *ctx.coords;
        double energy = 0.0;

        for (size_t p = 0; p < pairs_.size(); ++p) {
            const NonbondedPair& pair = pairs_[p];
            
            // Skip if fully excluded
            if (pair.scale < 1e-6) continue;

            // Get positions from flat coordinate array
            uint32_t i = pair.i;
            uint32_t j = pair.j;
            Vec3 ri(coords[3*i], coords[3*i+1], coords[3*i+2]);
            Vec3 rj(coords[3*j], coords[3*j+1], coords[3*j+2]);
            Vec3 rij = rj - ri;
            
            // PBC/MIC must be applied to coords BEFORE calling evaluate()
            
            double r = rij.norm();

            // Apply user-requested cutoff
            if (config_.cutoff.mode != CutoffMode::None && 
                r > config_.cutoff.rcut) {
                continue;
            }

            // Clamp minimum distance for numerical stability
            if (r < config_.rmin) r = config_.rmin;

            double sigma = sigma_values_[p];
            double epsilon = epsilon_values_[p];
            double s_r = sigma / r;
            double s_r6 = s_r * s_r * s_r * s_r * s_r * s_r;
            double s_r12 = s_r6 * s_r6;

            double E_pair;

            if (config_.lj.repulsion_only) {
                // WCA (Weeks-Chandler-Andersen): purely repulsive
                // Cutoff at r_wca = 2^(1/6) * sigma ≈ 1.122σ
                double r_wca = 1.12246204830937 * sigma;  // 2^(1/6)
                
                if (r < r_wca) {
                    // E_WCA = 4ε[(σ/r)^12 - (σ/r)^6] + ε
                    E_pair = 4.0 * epsilon * (s_r12 - s_r6) + epsilon;
                    
                    // Apply damping if requested (softens very short-range repulsion)
                    if (config_.lj.use_damping) {
                        double damp = tang_toennies_damping(r / sigma, 
                                                           config_.lj.damping_coefficient, 12);
                        E_pair *= damp;
                    }
                } else {
                    E_pair = 0.0;  // Beyond WCA cutoff: no interaction
                }
            } else {
                // Full LJ: E = 4ε[(σ/r)^12 - (σ/r)^6]
                E_pair = 4.0 * epsilon * (s_r12 - s_r6);
                
                // Damping for full LJ (affects both repulsion and attraction)
                if (config_.lj.use_damping) {
                    double damp_rep = tang_toennies_damping(r / sigma, 
                                                           config_.lj.damping_coefficient, 12);
                    double damp_att = tang_toennies_damping(r / sigma, 
                                                           config_.lj.damping_coefficient, 6);
                    // Apply separate damping to repulsive and attractive terms
                    E_pair = 4.0 * epsilon * (s_r12 * damp_rep - s_r6 * damp_att);
                }
            }

            // Apply scaling
            energy += pair.scale * E_pair;
            
            // Gradient computation would go here (future enhancement)
            // When gradient is needed, compute dE/dr and accumulate forces
        }

        return energy;
    }

    size_t num_pairs() const { return pairs_.size(); }
    
    const NonbondedConfig& config() const { return config_; }

private:
    void init_sigma_values() {
        // Precompute sigma and epsilon values for each pair
        sigma_values_.reserve(pairs_.size());
        epsilon_values_.reserve(pairs_.size());
        
        for (const auto& pair : pairs_) {
            double r_i = get_vdw_radius(atoms_[pair.i].Z);
            double r_j = get_vdw_radius(atoms_[pair.j].Z);

            // Compute sigma using mixing rule
            // double sigma_raw = r_i + r_j;  // Combined VDW radii (unused, kept for reference)
            double sigma_mixed = mix_sigma(r_i, r_j, config_.lj.mixing_rule);

            // Apply sigma scale factor
            sigma_values_.push_back(sigma_mixed * config_.lj.sigma);

            // Compute epsilon
            double epsilon;
            if (config_.lj.use_element_specific) {
                // Element-specific epsilon with mixing
                double eps_i = get_lj_epsilon(atoms_[pair.i].Z);
                double eps_j = get_lj_epsilon(atoms_[pair.j].Z);
                epsilon = mix_epsilon(eps_i, eps_j, config_.lj.mixing_rule);
            } else {
                // Uniform epsilon (legacy)
                epsilon = config_.lj.epsilon;
            }
            
            epsilon_values_.push_back(epsilon);
        }
    }

    const std::vector<NonbondedPair>& pairs_;
    const std::vector<Atom>& atoms_;
    NonbondedConfig config_;
    std::vector<double> sigma_values_;
    std::vector<double> epsilon_values_;
};

// ============================================================================
// Build Exclusion Lists from Topology
// ============================================================================

// Build neighbor connectivity up to N bonds away
inline std::vector<std::unordered_set<uint32_t>> build_connectivity(
    size_t num_atoms,
    const std::vector<Bond>& bonds,
    int max_separation)
{
    // connectivity[i] = set of atoms within max_separation bonds of i
    std::vector<std::unordered_set<uint32_t>> connectivity(num_atoms);
    
    // Initialize: each atom connected to itself
    for (size_t i = 0; i < num_atoms; ++i) {
        connectivity[i].insert(i);
    }
    
    // Build adjacency lists
    std::vector<std::vector<uint32_t>> neighbors(num_atoms);
    for (const auto& bond : bonds) {
        neighbors[bond.i].push_back(bond.j);
        neighbors[bond.j].push_back(bond.i);
    }
    
    // Breadth-first propagation up to max_separation
    for (int sep = 1; sep <= max_separation; ++sep) {
        std::vector<std::unordered_set<uint32_t>> new_neighbors(num_atoms);
        
        for (size_t i = 0; i < num_atoms; ++i) {
            for (uint32_t j : connectivity[i]) {
                for (uint32_t k : neighbors[j]) {
                    if (connectivity[i].find(k) == connectivity[i].end()) {
                        new_neighbors[i].insert(k);
                    }
                }
            }
        }
        
        // Add new neighbors
        for (size_t i = 0; i < num_atoms; ++i) {
            connectivity[i].insert(new_neighbors[i].begin(), 
                                  new_neighbors[i].end());
        }
    }
    
    return connectivity;
}

// Generate nonbonded pairs with appropriate scaling
// New version using NonbondedScaling
inline std::vector<NonbondedPair> build_nonbonded_pairs(
    size_t num_atoms,
    const std::vector<Bond>& bonds,
    const NonbondedScaling& scaling)
{
    std::vector<NonbondedPair> pairs;
    
    // Build connectivity maps
    auto conn_1 = build_connectivity(num_atoms, bonds, 1);  // 1-2 (bonded)
    auto conn_2 = build_connectivity(num_atoms, bonds, 2);  // 1-3 (angle)
    auto conn_3 = build_connectivity(num_atoms, bonds, 3);  // 1-4 (torsion)
    auto conn_4 = build_connectivity(num_atoms, bonds, 4);  // 1-5 (check for s15)
    
    // Generate all pairs i < j
    for (uint32_t i = 0; i < num_atoms; ++i) {
        for (uint32_t j = i + 1; j < num_atoms; ++j) {
            NonbondedPair pair;
            pair.i = i;
            pair.j = j;
            
            // Determine separation and scaling
            // Priority: 1-2 > 1-3 > 1-4 > 1-5+
            if (conn_1[i].find(j) != conn_1[i].end() && i != j) {
                // 1-2 bonded: typically excluded
                pair.scale = scaling.s12;
            }
            else if (conn_2[i].find(j) != conn_2[i].end()) {
                // 1-3 (angle): reduce strongly in VSEPR mode
                pair.scale = scaling.s13;
            }
            else if (conn_3[i].find(j) != conn_3[i].end()) {
                // 1-4 (torsion): only meaningful once torsions exist
                pair.scale = scaling.s14;
            }
            else {
                // 1-5+ or nonbonded: full interaction
                pair.scale = scaling.s15;
            }
            
            pairs.push_back(pair);
        }
    }
    
    return pairs;
}

// Legacy version (DEPRECATED - use NonbondedScaling version)
inline std::vector<NonbondedPair> build_nonbonded_pairs(
    size_t num_atoms,
    const std::vector<Bond>& bonds,
    double scale_13 = 0.5,
    double scale_14 = 0.8)
{
    NonbondedScaling scaling;
    scaling.s13 = scale_13;
    scaling.s14 = scale_14;
    return build_nonbonded_pairs(num_atoms, bonds, scaling);
}

} // namespace vsepr
