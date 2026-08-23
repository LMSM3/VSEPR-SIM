#pragma once
/**
 * trail.hpp  -  Particle Trail Geometry Helpers
 *
 * Provides trail point storage and the geometry/colour helpers used by
 * both the transient and QCD overlay factories.
 *
 *   TrailPoint      — one historical position+speed sample
 *   trail_radius()  — quadratic taper from tail to head
 *   velocity_gradient_colour() — blue -> cyan -> yellow heat ramp
 *   trail_segment_colour()     — age-blended velocity × base colour
 *
 * Source: extracted from include/coarse_grain/vis/transient_renderer.hpp
 * v5.1.3~2  |  WO-63A  |  v5.0.0-main
 */

#include "PARTICLES/rgb.hpp"
#include <cmath>

namespace vsepr {
namespace particles {

// ============================================================================
// TrailPoint  -  one historical position sample in a velocity trail
// ============================================================================

struct TrailPoint {
	float x{}, y{}, z{};       // world-space position (Å)
	float speed_norm{0.f};     // speed / reference_max at capture time
};

// ============================================================================
// Velocity-gradient colour heat ramp
//   0 (rest)  -> deep blue
//   0.5       -> cyan-green
//   1 (fast)  -> hot yellow-white
// ============================================================================

inline RGB velocity_gradient_colour(float speed_norm) {
	speed_norm = (speed_norm < 0.f) ? 0.f : (speed_norm > 1.f) ? 1.f : speed_norm;
	float r, g, b;
	if (speed_norm < 0.5f) {
		float t = speed_norm * 2.f;
		r = 0.10f + t * (0.05f - 0.10f);
		g = 0.18f + t * (0.95f - 0.18f);
		b = 0.98f + t * (0.55f - 0.98f);
	} else {
		float t = (speed_norm - 0.5f) * 2.f;
		r = 0.05f + t * (1.00f - 0.05f);
		g = 0.95f;
		b = 0.55f + t * (0.05f - 0.55f);
	}
	return {r, g, b};
}

// ============================================================================
// Trail radius: quadratic taper from very thin at tail to base_radius at head.
// segment_index: 0 = oldest, trail_length-1 = newest
// ============================================================================

inline float trail_radius(int segment_index, int trail_length, float base_radius) {
	if (trail_length <= 0) return 0.f;
	float t = static_cast<float>(segment_index) / static_cast<float>(trail_length);
	return base_radius * (0.04f + 0.62f * t * t);
}

// ============================================================================
// Trail segment colour:
//   age_t = 0 (oldest) -> velocity-gradient colour
//   age_t = 1 (newest) -> particle base colour
// ============================================================================

inline RGB trail_segment_colour(const RGB& base, float speed_norm, float age_t) {
	RGB vel = velocity_gradient_colour(speed_norm);
	float a = age_t * age_t;
	return {
		vel.r + a * (base.r - vel.r),
		vel.g + a * (base.g - vel.g),
		vel.b + a * (base.b - vel.b)
	};
}

} // namespace particles
} // namespace vsepr
