#pragma once
/**
 * transient_particle.hpp  -  Transient Particle Types and Overlay Factory
 *
 * Defines the WO-63A transient particle system:
 *   - Negative type codes (-1..-6) for electron/ion/neutron/gamma/alpha/reserved
 *   - TransientParticle state struct (physics side)
 *   - 14-entry deterministic colour palette
 *   - Per-type colour lookup with ID scramble
 *   - Sphere radius by type
 *   - TransientOverlay render descriptor (MAX_TRAIL = 32)
 *   - TransientKernelData input struct
 *   - make_overlay() factory
 *
 * Source: extracted from include/cli/system_state.hpp (type codes + state)
 *         and include/coarse_grain/vis/transient_renderer.hpp (vis helpers)
 * v5.1.3~2  |  WO-63A  |  v5.0.0-main
 */

#include "PARTICLES/rgb.hpp"
#include "PARTICLES/trail.hpp"
#include "atomistic/core/state.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace vsepr {
namespace particles {

// ============================================================================
// Transient type codes (negative)
// ============================================================================

enum class TransientTypeCode : int32_t {
	ElectronLike = -1,
	IonLike      = -2,
	NeutronLike  = -3,
	GammaLike    = -4,
	AlphaLike    = -5,
	ReservedBase = -6
};

inline bool is_transient_type_code(int32_t code) { return code < 0 && code >= -6; }
inline bool is_bead_type_code(int32_t code)       { return code > 0; }

inline bool transient_is_neutral(int32_t code) {
	return code == static_cast<int32_t>(TransientTypeCode::NeutronLike)
		|| code == static_cast<int32_t>(TransientTypeCode::GammaLike);
}

// ============================================================================
// TransientParticle  -  physics-side state
// ============================================================================

struct TransientParticle {
	uint32_t         id{};
	int32_t          type_code{static_cast<int32_t>(TransientTypeCode::ElectronLike)};
	atomistic::Vec3  position{};
	atomistic::Vec3  velocity{};
	double           mass{0.0};
	double           charge{0.0};
	double           energy{0.0};
	bool             active{true};
};

// ============================================================================
// Colour palette (14 visually distinct hues)
// ============================================================================

inline constexpr std::array<RGB, 14> TRANSIENT_PALETTE = {{
	{0.98f, 0.28f, 0.28f},  //  0  hot red         (electron-like default)
	{0.18f, 0.70f, 0.98f},  //  1  sky blue         (neutron-like default)
	{0.32f, 0.95f, 0.35f},  //  2  lime green
	{0.98f, 0.75f, 0.15f},  //  3  amber
	{0.85f, 0.25f, 0.98f},  //  4  violet
	{0.15f, 0.92f, 0.80f},  //  5  teal
	{0.98f, 0.48f, 0.12f},  //  6  orange
	{0.90f, 0.90f, 0.18f},  //  7  yellow
	{0.40f, 0.20f, 0.98f},  //  8  indigo
	{0.98f, 0.55f, 0.75f},  //  9  pink
	{0.10f, 0.78f, 0.40f},  // 10  emerald
	{0.98f, 0.18f, 0.58f},  // 11  magenta
	{0.55f, 0.82f, 0.98f},  // 12  powder blue
	{0.95f, 0.62f, 0.40f},  // 13  peach
}};

inline int palette_index_for_type(int32_t type_code, uint32_t id) {
	int base = 0;
	switch (type_code) {
		case -1: base = 0;  break;
		case -2: base = 6;  break;
		case -3: base = 1;  break;
		case -4: base = 7;  break;
		case -5: base = 4;  break;
		default: base = 12; break;
	}
	uint32_t jitter = ((id * 2654435761u) >> 28) % 3u;
	return (base + static_cast<int>(jitter)) % static_cast<int>(TRANSIENT_PALETTE.size());
}

inline RGB transient_colour(int32_t type_code, uint32_t id) {
	return TRANSIENT_PALETTE[palette_index_for_type(type_code, id)];
}

inline float transient_sphere_radius(int32_t type_code) {
	switch (type_code) {
		case -1: return 0.18f;
		case -2: return 0.30f;
		case -3: return 0.28f;
		case -4: return 0.14f;
		case -5: return 0.38f;
		default: return 0.22f;
	}
}

// ============================================================================
// TransientOverlay  -  per-particle render descriptor
// ============================================================================

struct TransientOverlay {
	uint32_t  id{};
	int32_t   type_code{-1};

	float x{}, y{}, z{};
	float radius{0.25f};

	RGB   colour{};
	float speed_norm{0.f};

	static constexpr int MAX_TRAIL = 32;
	std::vector<TrailPoint> trail;

	bool active{true};
};

// ============================================================================
// Factory
// ============================================================================

struct TransientKernelData {
	uint32_t id{};
	int32_t  type_code{-1};
	float    px{}, py{}, pz{};
	float    vx{}, vy{}, vz{};
	bool     active{true};
};

inline TransientOverlay make_overlay(const TransientKernelData& kd,
									 float reference_max_speed,
									 const TransientOverlay* prev = nullptr)
{
	TransientOverlay ov;
	ov.id        = kd.id;
	ov.type_code = kd.type_code;
	ov.x = kd.px; ov.y = kd.py; ov.z = kd.pz;
	ov.radius    = transient_sphere_radius(kd.type_code);
	ov.colour    = transient_colour(kd.type_code, kd.id);
	ov.active    = kd.active;

	float spd = std::sqrt(kd.vx*kd.vx + kd.vy*kd.vy + kd.vz*kd.vz);
	float rms = (reference_max_speed > 1e-9f) ? reference_max_speed : 1.0f;
	ov.speed_norm = std::min(1.f, spd / rms);

	if (prev && !prev->trail.empty()) {
		ov.trail = prev->trail;
		if (static_cast<int>(ov.trail.size()) >= TransientOverlay::MAX_TRAIL)
			ov.trail.erase(ov.trail.begin());
	} else if (prev) {
		TrailPoint tp0;
		tp0.x = prev->x; tp0.y = prev->y; tp0.z = prev->z;
		tp0.speed_norm = prev->speed_norm;
		ov.trail.push_back(tp0);
	}

	TrailPoint tp;
	tp.x = kd.px; tp.y = kd.py; tp.z = kd.pz;
	tp.speed_norm = ov.speed_norm;
	ov.trail.push_back(tp);

	return ov;
}

} // namespace particles
} // namespace vsepr
