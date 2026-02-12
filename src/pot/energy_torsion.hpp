#pragma once
/*
energy_torsion.hpp
------------------
Torsional (dihedral) energy term using periodic cosine potential.

Energy function:
  E(φ) = V/2 * [1 + cos(nφ - δ)]

where:
  φ = dihedral angle (i-j-k-l)
  n = periodicity (1, 2, 3, ...)
  V = barrier height (kcal/mol)
  δ = phase offset (radians)

Typical values:
  n=3, δ=0   : sp³-sp³ bonds (ethane: staggered favored)
  n=2, δ=π   : sp²-sp² bonds (planar preference)
  n=1        : asymmetric barriers

Gradient derivation:
  dE/dφ = -V*n/2 * sin(nφ - δ)
  
  Then dE/dr = (dE/dφ) * (dφ/dr) using chain rule.
  
  The dφ/dr terms are complex but well-established:
    dφ/dr_i = |b₂|⁻¹ * n₁/|n₁|²
    dφ/dr_l = -|b₂|⁻¹ * n₂/|n₂|²
    dφ/dr_j and dφ/dr_k follow from constraint dφ/dr_i + ... + dφ/dr_l = 0
*/

#include "pot/energy.hpp"
#include "core/geom_ops.hpp"
#include "core/types.hpp"
#include <vector>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace vsepr {

// ============================================================================
// Torsion Energy
// ============================================================================
class TorsionEnergy {
public:
    TorsionEnergy(const std::vector<Torsion>& torsions,
                  const std::vector<TorsionParams>& params)
        : torsions_(torsions), params_(params) {
        if (torsions_.size() != params_.size()) {
            throw std::runtime_error("TorsionEnergy: torsion count != parameter count");
        }
    }

    double evaluate(EnergyContext& ctx) const {
        if (!ctx.coords) {
            throw std::runtime_error("TorsionEnergy: null coords");
        }

        const auto& coords = *ctx.coords;
        double energy = 0.0;

        for (size_t t = 0; t < torsions_.size(); ++t) {
            const Torsion& tor = torsions_[t];
            const TorsionParams& p = params_[t];

            // Get positions
            Vec3 ri = get_pos(coords, tor.i);
            Vec3 rj = get_pos(coords, tor.j);
            Vec3 rk = get_pos(coords, tor.k);
            Vec3 rl = get_pos(coords, tor.l);

            // Bond vectors
            Vec3 b1 = rj - ri;  // i->j
            Vec3 b2 = rk - rj;  // j->k (central bond)
            Vec3 b3 = rl - rk;  // k->l

            // Plane normals
            Vec3 n1 = b1.cross(b2);
            Vec3 n2 = b2.cross(b3);

            double n1_norm2 = n1.norm2();
            double n2_norm2 = n2.norm2();
            double b2_norm = b2.norm();

            // Guard against degenerate (linear) configurations
            constexpr double eps = 1e-12;
            if (n1_norm2 < eps || n2_norm2 < eps || b2_norm < eps) {
                continue;  // Skip this torsion (atoms nearly collinear)
            }

            double n1_norm = std::sqrt(n1_norm2);
            double n2_norm = std::sqrt(n2_norm2);

            // Compute dihedral angle φ
            // Using atan2 for stability: φ = atan2(sin_phi, cos_phi)
            double cos_phi = n1.dot(n2) / (n1_norm * n2_norm);
            cos_phi = std::clamp(cos_phi, -1.0, 1.0);
            
            Vec3 b2_hat = b2 / b2_norm;
            double sin_phi = b2_hat.dot(n1.cross(n2)) / (n1_norm * n2_norm);
            
            double phi = std::atan2(sin_phi, cos_phi);

            // Energy: E = V/2 * [1 + cos(n*phi - delta)]
            double arg = p.n * phi - p.phi0;
            double torsion_energy = 0.5 * p.V * (1.0 + std::cos(arg));
            energy += torsion_energy / p.multiplicity;

            // Gradient
            if (ctx.compute_gradient()) {
                // dE/dphi = -V*n/2 * sin(n*phi - delta)
                double dE_dphi = (-0.5 * p.V * p.n * std::sin(arg)) / p.multiplicity;

                // Standard MD-safe torsion gradient formulation
                // (Allen & Tildesley, many MD codes)
                // See also: Blondel & Karplus (1996)
                
                // Cross products for normal vectors (already have n1, n2)
                Vec3 c1 = n1;  // cross(b1, b2)
                Vec3 c2 = n2;  // cross(b2, b3)
                
                double inv_c1 = 1.0 / n1_norm2;  // 1/|c1|²
                double inv_c2 = 1.0 / n2_norm2;  // 1/|c2|²
                
                // Terminal atom gradients
                // g_i = -b2_len * c1 / |c1|²
                Vec3 g_i = -(b2_norm * inv_c1) * c1;
                
                // g_l = +b2_len * c2 / |c2|²
                Vec3 g_l = (b2_norm * inv_c2) * c2;
                
                // Middle atom gradients from constraint
                double dot_b1_b2 = b1.dot(b2);
                double dot_b3_b2 = b3.dot(b2);
                double b2_norm2 = b2_norm * b2_norm;
                double inv_b2_norm2 = 1.0 / b2_norm2;
                
                // g_j = (b1·b2)/|b2|² * g_i - (b3·b2)/|b2|² * g_l - g_i
                Vec3 g_j = (dot_b1_b2 * inv_b2_norm2) * g_i
                         - (dot_b3_b2 * inv_b2_norm2) * g_l
                         - g_i;
                
                // g_k = (b3·b2)/|b2|² * g_l - (b1·b2)/|b2|² * g_i - g_l
                Vec3 g_k = (dot_b3_b2 * inv_b2_norm2) * g_l
                         - (dot_b1_b2 * inv_b2_norm2) * g_i
                         - g_l;
                
                // Apply chain rule: grad = dE/dφ * g_*
                Vec3 grad_i = dE_dphi * g_i;
                Vec3 grad_j = dE_dphi * g_j;
                Vec3 grad_k = dE_dphi * g_k;
                Vec3 grad_l = dE_dphi * g_l;
                
                accumulate_grad(*ctx.gradient, tor.i, grad_i);
                accumulate_grad(*ctx.gradient, tor.j, grad_j);
                accumulate_grad(*ctx.gradient, tor.k, grad_k);
                accumulate_grad(*ctx.gradient, tor.l, grad_l);
            }
        }

        return energy;
    }

    size_t num_torsions() const { return torsions_.size(); }

private:
    const std::vector<Torsion>& torsions_;
    const std::vector<TorsionParams>& params_;
};

// ============================================================================
// Parameter Assignment
// ============================================================================

// Assign torsion parameters based on bond hybridization (heuristic)
inline TorsionParams assign_torsion_params(
    const Torsion& torsion,
    const std::vector<Atom>& atoms,
    const std::vector<Bond>& bonds)
{
    TorsionParams p;
    
    // Get all four atoms
    const Atom& atom_i = atoms[torsion.i];
    // const Atom& atom_j = atoms[torsion.j];  // Reserved for future use
    // const Atom& atom_k = atoms[torsion.k];  // Reserved for future use
    const Atom& atom_l = atoms[torsion.l];
    
    // Ethane (H-C-C-H) is THE critical test case for torsions!
    // The original "zero out all H-involving" was WRONG - it disabled ethane barriers.
    // Only check terminal atoms (i, l), not central backbone (j, k).
    bool terminal_hydrogen = (atom_i.Z == 1 || atom_l.Z == 1);
    
    if (terminal_hydrogen) {
        // Terminal H torsions (e.g., H-C-C-H in ethane)
        // Needs proper sp³-sp³ barrier - ethane experimental value is 2.9 kcal/mol
        p.n = 3;
        p.V = 2.9;     // kcal/mol (ethane barrier)
        p.phi0 = 0.0;  // Staggered (φ=60°, 180°, 300°) favored
        
        // Calculate multiplicity even for H-involving torsions
        auto count_neighbors_inline = [&](uint32_t idx) {
            int count = 0;
            for (const auto& bond : bonds) {
                if (bond.i == idx || bond.j == idx) count++;
            }
            return count;
        };
        int nj = count_neighbors_inline(torsion.j);
        int nk = count_neighbors_inline(torsion.k);
        p.multiplicity = (nj - 1) * (nk - 1);
        if (p.multiplicity < 1) p.multiplicity = 1;
        return p;
    }
    
    // Count neighbors to estimate hybridization
    // (crude: sp³ if 4 neighbors, sp² if 3, sp if 2)
    auto count_neighbors = [&](uint32_t idx) {
        int count = 0;
        for (const auto& bond : bonds) {
            if (bond.i == idx || bond.j == idx) count++;
        }
        return count;
    };
    
    int neighbors_j = count_neighbors(torsion.j);
    int neighbors_k = count_neighbors(torsion.k);

    // Calculate multiplicity: # substituents on j (excluding k)  # on k (excluding j)
    // For ethane: 3 H on each C  33 = 9 torsions share the barrier
    int substituents_j = neighbors_j - 1;  // Exclude central bond
    int substituents_k = neighbors_k - 1;
    p.multiplicity = substituents_j * substituents_k;
    if (p.multiplicity < 1) p.multiplicity = 1;  // Safety check
    
    // Default: sp³-sp³ backbone (butane-like C-C-C-C without terminal H)
    p.n = 3;
    p.V = 2.9;     // kcal/mol (same as ethane - typical sp³-sp³ barrier)
    p.phi0 = 0.0;  // Staggered (φ=60°, 180°, 300°) favored
    
    // sp²-sp² cases: prefer planarity
    if (neighbors_j == 3 && neighbors_k == 3) {
        p.n = 2;
        p.V = 10.0;    // Stronger barrier for conjugated systems
        p.phi0 = M_PI; // Planar (φ=0°, 180°) favored
    }
    
    // sp cases: linear preference
    if (neighbors_j == 2 || neighbors_k == 2) {
        p.n = 1;
        p.V = 0.5;  // Weak barrier
        p.phi0 = 0.0;
    }
    
    return p;
}

// Generate all proper torsions from bond topology
// A torsion i-j-k-l exists if bonds i-j, j-k, k-l exist
inline std::vector<Torsion> generate_torsions_from_bonds(
    const std::vector<Bond>& bonds,
    size_t num_atoms)
{
    std::vector<Torsion> torsions;
    
    // Build adjacency lists
    std::vector<std::vector<uint32_t>> neighbors(num_atoms);
    for (const auto& bond : bonds) {
        neighbors[bond.i].push_back(bond.j);
        neighbors[bond.j].push_back(bond.i);
    }
    
    // For each bond j-k, find all i-j-k-l torsions
    for (const auto& bond_jk : bonds) {
        uint32_t j = bond_jk.i;
        uint32_t k = bond_jk.j;
        
        // Find all neighbors of j (excluding k) for position i
        for (uint32_t i : neighbors[j]) {
            if (i == k) continue;  // Skip the central bond
            
            // Find all neighbors of k (excluding j) for position l
            for (uint32_t l : neighbors[k]) {
                if (l == j || l == i) continue;  // Avoid duplicates
                
                torsions.push_back(Torsion{i, j, k, l});
            }
        }
    }
    
    return torsions;
}

// Batch parameter assignment
inline std::vector<TorsionParams> assign_torsion_parameters(
    const std::vector<Torsion>& torsions,
    const std::vector<Atom>& atoms,
    const std::vector<Bond>& bonds)
{
    std::vector<TorsionParams> params;
    params.reserve(torsions.size());
    
    for (const auto& torsion : torsions) {
        params.push_back(assign_torsion_params(torsion, atoms, bonds));
    }
    
    return params;
}

} // namespace vsepr
