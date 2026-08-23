#pragma once
/**
 * qcd_particle.hpp  -  QCD Particle Types and State
 *
 * Defines all particle-type content from the QCD layer:
 *   - SU(3) ColorCharge enum and predicates
 *   - GluonChannel (8 Gell-Mann channels)
 *   - QuarkFlavor enum (-7..-14)
 *   - QCDParticle state struct
 *   - QuarkGluonPlasma container
 *   - QCDPopulationConfig seeder params
 *   - Physical property tables (charge, mass)
 *
 * NOTE: Force accumulation and integration live in COLLISION/qcd_force.hpp.
 *       This header is type/state only.
 *
 * Source: extracted from include/coarse_grain/physics/qcd_transient.hpp
 * v5.1.3~2  |  WO-v513-QCD  |  v5.0.0-main
 */

#include "COLLISION/cornell.hpp"      // CornellParams (plasma owns one)
#include "COLLISION/tab_scheduler.hpp" // TABScheduler (plasma owns one)
#include "atomistic/core/state.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace vsepr {
namespace particles {

// ============================================================================
// SU(3) Color Charge
// ============================================================================

enum class ColorCharge : uint8_t {
	Red        = 0,
	Green      = 1,
	Blue       = 2,
	AntiRed    = 3,
	AntiGreen  = 4,
	AntiBlue   = 5,
	Neutral    = 6,
	White      = 7,
};

inline bool is_quark_color(ColorCharge c) {
	return c == ColorCharge::Red || c == ColorCharge::Green || c == ColorCharge::Blue;
}
inline bool is_antiquark_color(ColorCharge c) {
	return c == ColorCharge::AntiRed || c == ColorCharge::AntiGreen || c == ColorCharge::AntiBlue;
}
inline ColorCharge complementary_color(ColorCharge c) {
	switch (c) {
		case ColorCharge::Red:      return ColorCharge::AntiRed;
		case ColorCharge::Green:    return ColorCharge::AntiGreen;
		case ColorCharge::Blue:     return ColorCharge::AntiBlue;
		case ColorCharge::AntiRed:  return ColorCharge::Red;
		case ColorCharge::AntiGreen:return ColorCharge::Green;
		case ColorCharge::AntiBlue: return ColorCharge::Blue;
		default:                    return ColorCharge::White;
	}
}

// ============================================================================
// Gluon Channel (8 Gell-Mann channels)
// ============================================================================

struct GluonChannel {
	uint8_t     index{0};
	ColorCharge color_src{};
	ColorCharge color_dst{};

	static constexpr std::array<std::pair<ColorCharge,ColorCharge>, 8> CHANNELS = {{
		{ColorCharge::Red,   ColorCharge::AntiRed  },
		{ColorCharge::Red,   ColorCharge::AntiGreen},
		{ColorCharge::Green, ColorCharge::AntiRed  },
		{ColorCharge::Green, ColorCharge::AntiGreen},
		{ColorCharge::Red,   ColorCharge::AntiBlue },
		{ColorCharge::Blue,  ColorCharge::AntiRed  },
		{ColorCharge::Green, ColorCharge::AntiBlue },
		{ColorCharge::Blue,  ColorCharge::AntiBlue },
	}};

	static GluonChannel from_index(uint8_t idx) {
		GluonChannel g;
		g.index     = idx % 8u;
		g.color_src = CHANNELS[g.index].first;
		g.color_dst = CHANNELS[g.index].second;
		return g;
	}
};

// ============================================================================
// Quark Flavor
// ============================================================================

enum class QuarkFlavor : int32_t {
	Up        = -7,
	Down      = -8,
	Strange   = -9,
	Charm     = -10,
	Bottom    = -11,
	Top       = -12,
	Gluon     = -13,
	AntiQuark = -14,
};

inline const char* quark_flavor_name(QuarkFlavor f) {
	switch (f) {
		case QuarkFlavor::Up:       return "up";
		case QuarkFlavor::Down:     return "down";
		case QuarkFlavor::Strange:  return "strange";
		case QuarkFlavor::Charm:    return "charm";
		case QuarkFlavor::Bottom:   return "bottom";
		case QuarkFlavor::Top:      return "top";
		case QuarkFlavor::Gluon:    return "gluon";
		case QuarkFlavor::AntiQuark:return "antiquark";
		default:                    return "unknown";
	}
}

inline bool is_qcd_type_code(int32_t code) {
	return code <= -7 && code >= -14;
}

inline double quark_electric_charge(QuarkFlavor f) {
	switch (f) {
		case QuarkFlavor::Up:     return +2.0/3.0;
		case QuarkFlavor::Down:   return -1.0/3.0;
		case QuarkFlavor::Strange:return -1.0/3.0;
		case QuarkFlavor::Charm:  return +2.0/3.0;
		case QuarkFlavor::Bottom: return -1.0/3.0;
		case QuarkFlavor::Top:    return +2.0/3.0;
		case QuarkFlavor::Gluon:  return  0.0;
		default:                  return  0.0;
	}
}

inline double quark_rest_mass(QuarkFlavor f) {
	switch (f) {
		case QuarkFlavor::Up:     return 0.0022;
		case QuarkFlavor::Down:   return 0.0047;
		case QuarkFlavor::Strange:return 0.096;
		case QuarkFlavor::Charm:  return 1.28;
		case QuarkFlavor::Bottom: return 4.18;
		case QuarkFlavor::Top:    return 173.1;
		case QuarkFlavor::Gluon:  return 0.0;
		default:                  return 0.1;
	}
}

// ============================================================================
// QCDParticle state
// ============================================================================

struct QCDParticle {
	uint32_t      id{};
	QuarkFlavor   flavor{QuarkFlavor::Up};
	ColorCharge   color{ColorCharge::Red};
	GluonChannel  gluon_channel{};

	atomistic::Vec3 position{};
	atomistic::Vec3 velocity{};
	atomistic::Vec3 force{};

	double mass{};
	double energy{};
	double string_length{};

	bool   active{true};
	bool   confined{false};
	int    string_partner{-1};

	void reset_force() { force = {0.0, 0.0, 0.0}; }
};

// ============================================================================
// QuarkGluonPlasma container
// ============================================================================

struct QuarkGluonPlasma {
	std::vector<QCDParticle>    particles;
	collision::CornellParams    cornell;
	collision::TABScheduler     scheduler;
	bool                        enabled{true};

	int  hadronization_events{0};
	int  gluon_emission_events{0};
	int  step_count{0};

	int num_active() const {
		int n = 0;
		for (const auto& p : particles) if (p.active) ++n;
		return n;
	}

	void clear() {
		particles.clear();
		hadronization_events = 0;
		gluon_emission_events = 0;
		step_count = 0;
	}
};

// ============================================================================
// Population config
// ============================================================================

struct QCDPopulationConfig {
	int      n_quarks{6};
	int      n_gluons{4};
	double   box_radius{3.0};
	double   v_thermal{0.1};
	uint32_t seed{0x513'0001u};
};

} // namespace particles
} // namespace vsepr
