#pragma once
/**
 * qcd_colour.hpp  -  QCD Particle Colour-Charge to RGB Mapping and Overlays
 *
 * Provides:
 *   - SU(3) ColorCharge -> vivid RGB
 *   - Gluon channel rainbow trail interpolation with midpoint flash
 *   - Confinement string tension colour ramp (slack=blue, breaking=white)
 *   - QCDOverlay render descriptor (MAX_TRAIL = 48)
 *   - QCDKernelData input struct
 *   - make_qcd_overlay() factory with trail propagation and speed blend
 *   - quark_trail_colour(), gluon_trail_colour_aged() helpers
 *
 * Source: extracted from include/coarse_grain/vis/qcd_colour_charge.hpp
 * v5.1.3~2  |  WO-v513-QCD  |  v5.0.0-main
 */

#include "PARTICLES/rgb.hpp"
#include "PARTICLES/trail.hpp"
#include "PARTICLES/qcd_particle.hpp"

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace vsepr {
namespace particles {

// ============================================================================
// SU(3) ColorCharge -> vivid RGB
// ============================================================================

inline RGB color_charge_to_rgb(ColorCharge c) {
	switch (c) {
		case ColorCharge::Red:       return {1.00f, 0.12f, 0.12f};
		case ColorCharge::Green:     return {0.10f, 0.95f, 0.18f};
		case ColorCharge::Blue:      return {0.12f, 0.28f, 1.00f};
		case ColorCharge::AntiRed:   return {0.00f, 0.90f, 0.90f};
		case ColorCharge::AntiGreen: return {0.90f, 0.10f, 0.90f};
		case ColorCharge::AntiBlue:  return {0.95f, 0.90f, 0.00f};
		case ColorCharge::Neutral:   return {1.00f, 1.00f, 1.00f};
		case ColorCharge::White:     return {0.95f, 0.95f, 0.95f};
		default:                     return {0.70f, 0.70f, 0.70f};
	}
}

// ============================================================================
// Gluon channel colours
// ============================================================================

inline std::pair<RGB,RGB> gluon_channel_colours(const GluonChannel& ch) {
	return { color_charge_to_rgb(ch.color_src), color_charge_to_rgb(ch.color_dst) };
}

/// Interpolated rainbow colour along a gluon trail.
/// t=0 -> color_src, t=1 -> color_dst, with a vivid midpoint flash.
inline RGB gluon_trail_colour(const GluonChannel& ch, float t) {
	auto [cs, cd] = gluon_channel_colours(ch);
	float flash = std::exp(-20.0f * (t - 0.5f) * (t - 0.5f));
	float r = cs.r + t*(cd.r-cs.r) + flash*(1.0f-cs.r-t*(cd.r-cs.r))*0.6f;
	float g = cs.g + t*(cd.g-cs.g) + flash*(1.0f-cs.g-t*(cd.g-cs.g))*0.6f;
	float b = cs.b + t*(cd.b-cs.b) + flash*(1.0f-cs.b-t*(cd.b-cs.b))*0.4f;
	if (r > 1.f) r = 1.f;
	if (g > 1.f) g = 1.f;
	if (b > 1.f) b = 1.f;
	return {r, g, b};
}

// ============================================================================
// Confinement string tension colour ramp
//   slack -> blue, taut -> orange/red, breaking -> white
// ============================================================================

inline RGB confinement_string_colour(double string_length, double r_conf) {
	float t = (r_conf > 1e-9) ? static_cast<float>(string_length / r_conf) : 0.0f;
	if (t > 1.f) t = 1.f;
	float r, g, b;
	if (t < 0.5f) {
		float u = t * 2.f;
		r = 0.15f + u*(0.95f-0.15f);
		g = 0.30f + u*(0.45f-0.30f);
		b = 0.95f + u*(0.10f-0.95f);
	} else {
		float u = (t - 0.5f) * 2.f;
		r = 0.95f + u*(1.0f-0.95f);
		g = 0.45f + u*(1.0f-0.45f);
		b = 0.10f + u*(1.0f-0.10f);
	}
	return {r, g, b};
}

// ============================================================================
// QCDOverlay  -  per-particle render descriptor
// ============================================================================

struct QCDOverlay {
	uint32_t     id{};
	QuarkFlavor  flavor{QuarkFlavor::Up};
	ColorCharge  color_charge{ColorCharge::Red};
	GluonChannel gluon_channel{};

	float x{}, y{}, z{};
	float radius{};

	RGB   sphere_colour{};
	float speed_norm{0.f};
	float string_length_norm{0.f};
	int   string_partner_overlay{-1};

	bool  active{true};

	static constexpr int MAX_TRAIL = 48;
	std::vector<TrailPoint> trail;
};

inline float qcd_sphere_radius(QuarkFlavor f) {
	switch (f) {
		case QuarkFlavor::Up:
		case QuarkFlavor::Down:    return 0.14f;
		case QuarkFlavor::Strange: return 0.18f;
		case QuarkFlavor::Charm:   return 0.24f;
		case QuarkFlavor::Bottom:  return 0.30f;
		case QuarkFlavor::Top:     return 0.40f;
		case QuarkFlavor::Gluon:   return 0.10f;
		default:                   return 0.16f;
	}
}

// ============================================================================
// QCDOverlay factory
// ============================================================================

struct QCDKernelData {
	uint32_t          id{};
	QuarkFlavor       flavor{QuarkFlavor::Up};
	ColorCharge       color{ColorCharge::Red};
	GluonChannel      gluon_channel{};
	float             px{}, py{}, pz{};
	float             vx{}, vy{}, vz{};
	double            string_length{0.0};
	double            r_conf{2.0};
	bool              active{true};
	int               string_partner_idx{-1};
};

inline QCDOverlay make_qcd_overlay(const QCDKernelData& kd,
								   float v_ref,
								   const QCDOverlay* prev = nullptr)
{
	QCDOverlay ov;
	ov.id             = kd.id;
	ov.flavor         = kd.flavor;
	ov.color_charge   = kd.color;
	ov.gluon_channel  = kd.gluon_channel;
	ov.x = kd.px; ov.y = kd.py; ov.z = kd.pz;
	ov.radius         = qcd_sphere_radius(kd.flavor);
	ov.active         = kd.active;
	ov.string_partner_overlay = kd.string_partner_idx;

	float spd = std::sqrt(kd.vx*kd.vx + kd.vy*kd.vy + kd.vz*kd.vz);
	float vr  = (v_ref > 1e-9f) ? v_ref : 1.0f;
	ov.speed_norm = spd / vr;
	if (ov.speed_norm > 1.f) ov.speed_norm = 1.f;

	ov.string_length_norm = (kd.r_conf > 1e-9)
		? static_cast<float>(kd.string_length / kd.r_conf) : 0.f;
	if (ov.string_length_norm > 1.f) ov.string_length_norm = 1.f;

	RGB base = color_charge_to_rgb(kd.color);
	RGB vel  = velocity_gradient_colour(ov.speed_norm);
	float blend = ov.speed_norm * 0.45f;
	ov.sphere_colour = {
		base.r + blend*(vel.r-base.r),
		base.g + blend*(vel.g-base.g),
		base.b + blend*(vel.b-base.b)
	};

	if (prev && !prev->trail.empty()) {
		ov.trail = prev->trail;
		if (static_cast<int>(ov.trail.size()) >= QCDOverlay::MAX_TRAIL)
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

// ============================================================================
// Trail helpers
// ============================================================================

inline RGB quark_trail_colour(const RGB& charge_colour, float speed_norm, float age_t) {
	return trail_segment_colour(charge_colour, speed_norm, age_t);
}

inline RGB gluon_trail_colour_aged(const GluonChannel& ch, float age_t) {
	return gluon_trail_colour(ch, age_t);
}

} // namespace particles
} // namespace vsepr
