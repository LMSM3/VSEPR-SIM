#pragma once
/**
 * cornell.hpp  -  Cornell Potential (Coulombic + linear string confinement)
 *
 * V(r) = -kappa/r  +  sigma*r      (r > r_min)
 * F(r) = dV/dr = kappa/r^2 + sigma (repulsive short + confining long)
 *
 * This is the force kernel for QCD-like confinement at the VSIM scale.
 * Parameters are dimensionless analogs calibrated to VSIM length/energy units
 * and tau_scale.
 *
 * Source: extracted from include/coarse_grain/physics/qcd_transient.hpp
 * v5.1.3~2  |  WO-v513-QCD  |  v5.0.0-main
 */

#include <cmath>

namespace vsepr {
namespace collision {

// ============================================================================
// Cornell potential parameters
// ============================================================================

struct CornellParams {
	double kappa{0.52};      // Coulombic coefficient (alpha_s analog, ~0.3–0.6)
	double sigma{0.18};      // String tension (GeV²/c analog, rescaled to VSIM)
	double r_min{0.05};      // Short-distance cutoff (Å analog)
	double r_conf{2.0};      // Confinement radius threshold — hadronization gate
	double tau_scale{1.0e-3}; // Time-dilation: 1 fm/c -> tau_scale fs in VSIM time
};

// ============================================================================
// Cornell force magnitude  |F(r)| = kappa/r_eff^2 + sigma
// Positive = net radial force magnitude (sign determined by Casimir factor).
// ============================================================================

inline double cornell_force_magnitude(double r, const CornellParams& p) {
	double r_eff = (r < p.r_min) ? p.r_min : r;
	return p.kappa / (r_eff * r_eff) + p.sigma;
}

// ============================================================================
// Cornell potential energy  V(r) = -kappa/r_eff + sigma*r_eff
// ============================================================================

inline double cornell_potential(double r, const CornellParams& p) {
	double r_eff = (r < p.r_min) ? p.r_min : r;
	return -p.kappa / r_eff + p.sigma * r_eff;
}

} // namespace collision
} // namespace vsepr
