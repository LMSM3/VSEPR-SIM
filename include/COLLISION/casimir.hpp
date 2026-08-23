#pragma once
/**
 * casimir.hpp  -  SU(3) Color-Force Sign Convention (Casimir factors)
 *
 * Provides the sign and magnitude modifier for the Cornell force between
 * two color-charged particles based on their SU(3) color-charge relationship.
 *
 * Convention:
 *   Same color (qq or ā q̄)             → repulsive (+1/6)
 *   Complementary color-anticolor pair  → attractive (-4/3, singlet)
 *   All other color combinations        → weakly attractive (-1/6, octet)
 *
 * Source: extracted from include/coarse_grain/physics/qcd_transient.hpp
 * v5.1.3~2  |  WO-v513-QCD  |  v5.0.0-main
 */

#include "PARTICLES/qcd_particle.hpp"    // ColorCharge, complementary_color

namespace vsepr {
namespace collision {

// ============================================================================
// Casimir factor for a color-charge pair
// Multiply by |Cornell force| to get the signed radial force component.
// Negative → attractive, positive → repulsive.
// ============================================================================

inline double color_casimir_factor(particles::ColorCharge a, particles::ColorCharge b) {
	if (a == b)                                        return +1.0 / 6.0;
	if (b == particles::complementary_color(a))        return -4.0 / 3.0;
	return -1.0 / 6.0;
}

} // namespace collision
} // namespace vsepr
