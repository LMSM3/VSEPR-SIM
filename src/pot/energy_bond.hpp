#pragma once
/*
energy_bond.hpp
---------------
Harmonic bond stretching energy term.

Energy function:
  E = 0.5 * k * (r - r0)²

where:
  r  = distance between bonded atoms
  r0 = equilibrium bond length
  k  = force constant

Gradient (force):
  For bond i-j with displacement vector r_ij = r_j - r_i
  Unit direction: r_hat = r_ij / |r_ij|
  
  Force on atom i: F_i = -k * (r - r0) * r_hat
  Force on atom j: F_j = +k * (r - r0) * r_hat
  
  (gradient = -force, so signs flip)
*/

#include "core/types.hpp"
#include "core/geom_ops.hpp"
#include "pot/covalent_radii.hpp"
#include "pot/energy.hpp"
#include <vector>
#include <cmath>
#include <stdexcept>

namespace vsepr {

class BondEnergy {
public:
    // Constructor: takes bond topology and parameters
    BondEnergy(const std::vector<Bond>& bonds, 
               const std::vector<BondParams>& params)
        : bonds_(bonds), params_(params) {
        // Validate alignment
        if (bonds_.size() != params_.size()) {
            throw std::runtime_error("BondEnergy: bond count != parameter count");
        }
    }

    // Evaluate energy (and optionally gradient)
    double evaluate(EnergyContext& ctx) const {
        if (!ctx.coords) {
            throw std::runtime_error("BondEnergy: null coords");
        }

        const auto& coords = *ctx.coords;
        double energy = 0.0;

        for (size_t b = 0; b < bonds_.size(); ++b) {
            const Bond& bond = bonds_[b];
            const BondParams& p = params_[b];

            // Get atom positions
            Vec3 ri = get_pos(coords, bond.i);
            Vec3 rj = get_pos(coords, bond.j);
            
            // Displacement vector: r_ij = r_j - r_i
            Vec3 rij = rj - ri;
            double r = rij.norm();

            // Handle degenerate case (atoms on top of each other)
            constexpr double eps = 1e-10;
            if (r < eps) {
                // Skip or apply large penalty (for now, skip)
                continue;
            }

            // Compute energy
            double delta = r - p.r0;
            energy += 0.5 * p.k * delta * delta;

            // Compute gradient if requested
            if (ctx.compute_gradient()) {
                // Unit direction vector: r_hat points from i to j
                Vec3 r_hat = rij / r;
                
                // Energy gradient: dE/dx = k * (r - r0) * (dx/d|x|)
                // For atom i: dE/dr_i = -k * delta * r_hat (points toward j when stretched)
                // For atom j: dE/dr_j = +k * delta * r_hat (points toward i when stretched)
                double grad_mag = p.k * delta;
                
                Vec3 grad_i = -grad_mag * r_hat;   // points from j toward i
                Vec3 grad_j = grad_mag * r_hat;    // points from i toward j
                
                accumulate_grad(*ctx.gradient, bond.i, grad_i);
                accumulate_grad(*ctx.gradient, bond.j, grad_j);
            }
        }

        return energy;
    }

    size_t num_bonds() const { return bonds_.size(); }

private:
    const std::vector<Bond>& bonds_;
    const std::vector<BondParams>& params_;
};

// ============================================================================
// Parameter Assignment Helpers
// ============================================================================

// Assign bond parameters using covalent radii and default force constants
inline std::vector<BondParams> assign_bond_parameters(
    const std::vector<Bond>& bonds,
    const std::vector<Atom>& atoms,
    double default_k = 300.0)  // kcal/mol/Å² (typical for C-C)
{
    std::vector<BondParams> params;
    params.reserve(bonds.size());

    for (const auto& bond : bonds) {
        const Atom& ai = atoms[bond.i];
        const Atom& aj = atoms[bond.j];

        BondParams p;
        
        // Equilibrium length from covalent radii
        double r_i = get_covalent_radius(ai.Z);
        double r_j = get_covalent_radius(aj.Z);
        p.r0 = (r_i + r_j) * bond_order_scale(bond.order);

        // Force constant (could be refined based on bond order, elements)
        // For now, scale by bond order: stronger bonds = stiffer
        p.k = default_k * bond.order;

        params.push_back(p);
    }

    return params;
}

} // namespace vsepr
