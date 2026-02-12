#pragma once
/*
energy_angle.hpp
----------------
Harmonic angle bending energy term (cosine formulation).

Energy function (in cosine space - smoother near linear):
  E = 0.5 * k * (cos(θ) - cos(θ₀))²

where:
  θ  = angle between bonds i-j and k-j (j is vertex)
  θ₀ = equilibrium angle
  k  = force constant (kcal/mol)

Advantages of cosine formulation:
- No acos() evaluation (faster, more stable)
- Smooth derivatives near linear (θ → 180°)
- Numerically stable

Gradient derivation:
  cos(θ) = (r_ji · r_jk) / (|r_ji| * |r_jk|)
  
  Let c = cos(θ), then:
  dc/dr_i =  (1/(ab)) * (r_jk - (r_ji · r_jk / a²) * r_ji)
  dc/dr_k =  (1/(ab)) * (r_ji - (r_ji · r_jk / b²) * r_jk)
  dc/dr_j = -(dc/dr_i + dc/dr_k)
  
  where a = |r_ji|, b = |r_jk|
*/

#include "pot/energy.hpp"
#include "vsepr_geometry.hpp"
#include "core/geom_ops.hpp"
#include "core/types.hpp"
#include "core/chemistry.hpp"
#include <vector>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace vsepr {

class AngleEnergy {
public:
    AngleEnergy(const std::vector<Angle>& angles,
                const std::vector<AngleParams>& params)
        : angles_(angles), params_(params) {
        if (angles_.size() != params_.size()) {
            throw std::runtime_error("AngleEnergy: angle count != parameter count");
        }
    }

    double evaluate(EnergyContext& ctx) const {
        if (!ctx.coords) {
            throw std::runtime_error("AngleEnergy: null coords");
        }

        const auto& coords = *ctx.coords;
        double energy = 0.0;

        for (size_t ang_idx = 0; ang_idx < angles_.size(); ++ang_idx) {
            const Angle& angle = angles_[ang_idx];
            const AngleParams& p = params_[ang_idx];

            // Get atom positions
            Vec3 ri = get_pos(coords, angle.i);
            Vec3 rj = get_pos(coords, angle.j);  // vertex
            Vec3 rk = get_pos(coords, angle.k);

            // Bond vectors: j -> i and j -> k
            Vec3 u = ri - rj;
            Vec3 v = rk - rj;

            double a2 = u.norm2();
            double b2 = v.norm2();
            double a = std::sqrt(a2);
            double b = std::sqrt(b2);

            // Guard against degenerate angles
            constexpr double eps = 1e-12;
            if (a < eps || b < eps) continue;

            double inv_ab = 1.0 / (a * b);
            double udotv = u.dot(v);

            // cos(theta)
            double c = udotv * inv_ab;
            c = std::clamp(c, -1.0, 1.0);

            // cos(theta0)
            double cos0 = std::cos(p.theta0);

            // Energy: E = 0.5 * k * (c - cos0)²
            double dc = c - cos0;
            energy += 0.5 * p.k * dc * dc;

            // Gradient
            if (ctx.compute_gradient()) {
                double g = p.k * dc;  // dE/dc

                // dc/du = (1/(ab)) * (v - (u·v/a²) * u)
                Vec3 dc_du = (v - u * (udotv / a2)) * inv_ab;
                
                // dc/dv = (1/(ab)) * (u - (u·v/b²) * v)
                Vec3 dc_dv = (u - v * (udotv / b2)) * inv_ab;

                // Chain rule: dE/dr = (dE/dc) * (dc/dr)
                Vec3 dE_du = dc_du * g;
                Vec3 dE_dv = dc_dv * g;

                // Distribute gradients
                accumulate_grad(*ctx.gradient, angle.i, dE_du);
                accumulate_grad(*ctx.gradient, angle.k, dE_dv);
                accumulate_grad(*ctx.gradient, angle.j, -(dE_du + dE_dv));
            }
        }

        return energy;
    }

    size_t num_angles() const { return angles_.size(); }

private:
    const std::vector<Angle>& angles_;
    const std::vector<AngleParams>& params_;
};

// ============================================================================
// VSEPR-based Angle Parameter Assignment
// ============================================================================

// Get ideal angle for VSEPR geometry based on steric number and lone pairs
inline double vsepr_ideal_angle(int steric_number, int lone_pairs) {
    // AXnEm notation: n = bonded atoms, m = lone pairs
    // steric_number = n + m
    
    if (steric_number == 2) {
        return M_PI;  // 180° (linear)
    }
    else if (steric_number == 3) {
        if (lone_pairs == 0) {
            return 120.0 * M_PI / 180.0;  // 120° (trigonal planar)
        } else {
            return 120.0 * M_PI / 180.0;  // AX2E: bent ~120° (ideal trigonal)
        }
    }
    else if (steric_number == 4) {
        if (lone_pairs == 0) {
            return 109.5 * M_PI / 180.0;  // 109.5° (tetrahedral)
        }
        else if (lone_pairs == 1) {
            // AX3E (NH3): compressed from tetrahedral
            // Ideal: 109.5°, actual ~107° (LP-BP repulsion)
            // For now, use 107° - will refine with nonbonded terms
            return 107.0 * M_PI / 180.0;
        }
        else if (lone_pairs == 2) {
            // AX2E2 (H2O): compressed from tetrahedral
            // Ideal: 109.5°, actual ~104.5° (LP-LP > LP-BP repulsion)
            return 104.5 * M_PI / 180.0;
        }
    }
    else if (steric_number == 5) {
        return 90.0 * M_PI / 180.0;  // Trigonal bipyramidal (equatorial)
    }
    else if (steric_number == 6) {
        return 90.0 * M_PI / 180.0;  // Octahedral
    }
    
    // Default: tetrahedral
    return 109.5 * M_PI / 180.0;
}

// Estimate force constant based on central atom
inline double angle_force_constant(uint8_t Z_central) {
    // kcal/mol/rad² (rough estimates)
    // Lighter atoms generally stiffer angles
    
    if (Z_central == 1) return 30.0;   // H (rarely central)
    if (Z_central <= 2) return 50.0;   // He (rare)
    if (Z_central <= 10) {
        // Period 2: C, N, O, F
        if (Z_central == 6) return 70.0;  // C (sp3)
        if (Z_central == 7) return 80.0;  // N
        if (Z_central == 8) return 100.0; // O (stiffer)
        return 60.0;
    }
    if (Z_central <= 18) return 50.0;  // Period 3: Si, P, S, Cl
    
    return 40.0;  // Heavier elements: softer angles
}

// Assign angle parameters using VSEPR heuristics
inline std::vector<AngleParams> assign_angle_parameters(
    const std::vector<Angle>& angles,
    const std::vector<Atom>& atoms,
    const std::vector<Bond>& bonds,
    const std::vector<double>& coords)
{
    std::vector<AngleParams> params;
    params.reserve(angles.size());

    // Count neighbors per atom for VSEPR analysis
    std::vector<int> neighbor_count(atoms.size(), 0);
    for (const auto& bond : bonds) {
        neighbor_count[bond.i]++;
        neighbor_count[bond.j]++;
    }

    for (const auto& angle : angles) {
        const Atom& central = atoms[angle.j];  // j is vertex
        
        AngleParams p;
        
        // VSEPR: steric number = bonded neighbors + lone pairs
        int bonded = neighbor_count[angle.j];
        
        // Estimate lone pairs from valence electrons
        // Valence = group number (simplified)
        int valence = 0;
        if (central.Z == 1) valence = 1;       // H
        else if (central.Z == 2) valence = 0;  // He
        else if (central.Z <= 10) {            // Period 2 (Li-Ne)
            valence = central.Z - 2;           // B=3, C=4, N=5, O=6, F=7
        }
        else if (central.Z <= 18) {            // Period 3 (Na-Ar)
            valence = central.Z - 10;          // Al=3, Si=4, P=5, S=6, Cl=7
        }
        else if (central.Z <= 36) {            // Period 4
            valence = (central.Z - 18) <= 8 ? (central.Z - 18) : 2;  // Transition metals ~2
        }
        else {
            valence = 4;  // Default
        }
        
        // Lone pairs = (valence - bonded) / 2
        // Each bond uses 1 electron, remaining electrons form lone pairs
        // Assuming single bonds for this estimate
        int electrons_in_bonds = bonded;
        int remaining_electrons = std::max(0, valence - electrons_in_bonds);
        int lone_pairs = remaining_electrons / 2;  // 2 electrons per lone pair
        
        // Clamp to reasonable range
        lone_pairs = std::clamp(lone_pairs, 0, 3);
        
        // Detect VSEPR geometry
        VSEPRGeometry geom = detect_vsepr_geometry(bonded, lone_pairs);
        
        // Get ideal angle from geometry-specific analysis
        // This properly handles AX5 (90°, 120°, 180°), AX6 (90°, 180°), etc.
        p.theta0 = get_vsepr_ideal_angle(geom, coords, angle.i, angle.j, angle.k);
        
        // Get force constant
        p.k = angle_force_constant(central.Z);
        
        params.push_back(p);
    }

    return params;
}

} // namespace vsepr
