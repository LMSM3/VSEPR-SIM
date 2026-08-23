#pragma once
/**
 * qcd_transient.hpp  -  QCD Extreme-Environment Transient Layer
 *
 * v5.1.3~2  |  WO-v513-QCD  |  v5.0.0-main
 *
 * Models dynamic quarks and gluons on time-dilated (slowed) scales alongside
 * the existing bead/hadronic kernel using the T_A|B dual-timescale
 * architecture established in WO-63A.
 *
 *   T_B  — slow bead / hadronic structural step  (Δt_B, existing kernel)
 *   T_A  — fast QCD substep  (Δt_A = Δt_B / K,  default K = 200)
 *
 * Physics model (scaled analog — NOT a full lattice QCD solver):
 *   - SU(3) color charge: R / G / B  for quarks; Rbar / Gbar / Bbar for antiquarks
 *   - Eight gluon color channels (color-anticolor pairs, Gell-Mann basis)
 *   - Cornell potential:  V(r) = -κ/r + σr  (Coulombic + linear string)
 *   - Confinement gate: when string length > r_conf, hadronization event fires
 *   - Time dilation factor τ_scale controls how slowly the QCD layer runs
 *     relative to clock time (default: QCD 1 fm/c → τ_scale × 1 fs in VSIM time)
 *
 * Negative type codes (extends WO-63A transient code table):
 *   -7   up-quark          -8  down-quark         -9  strange-quark
 *   -10  charm-quark       -11 bottom-quark        -12 top-quark
 *   -13  gluon             -14 antiquark (generic, color determined by field)
 *
 * Design rules:
 *   - Header-only; no GL/Qt/OS dependency
 *   - Deterministic: identical seed → identical color assignment
 *   - T_A substep loop is a free function; caller owns scheduler
 *   - Cornell kernel is inlined for substep hot-path
 *
 * Reference: WO-VSEPR_SIM-63A-ARCHIVE §particle-roles, §T_A|B architecture
 */

#include "atomistic/core/state.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace vsepr {
namespace qcd {

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
	Neutral    = 6,    // gluons and mesons before color assignment
	White      = 7,    // color-singlet / confined hadron
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
// Gluon Color Channel (8 Gell-Mann channels, stored as pair index 0–7)
// ============================================================================

struct GluonChannel {
	uint8_t index{0};        // 0–7 (Gell-Mann basis)
	ColorCharge color_src{}; // source color charge
	ColorCharge color_dst{}; // destination color charge (anti of source for diagonal)

	static constexpr std::array<std::pair<ColorCharge,ColorCharge>, 8> CHANNELS = {{
		{ColorCharge::Red,      ColorCharge::AntiRed   },  // g1  rr̄
		{ColorCharge::Red,      ColorCharge::AntiGreen },  // g2  rḡ
		{ColorCharge::Green,    ColorCharge::AntiRed   },  // g3  gr̄
		{ColorCharge::Green,    ColorCharge::AntiGreen },  // g4  gḡ  (diagonal)
		{ColorCharge::Red,      ColorCharge::AntiBlue  },  // g5  rb̄
		{ColorCharge::Blue,     ColorCharge::AntiRed   },  // g6  br̄
		{ColorCharge::Green,    ColorCharge::AntiBlue  },  // g7  gb̄
		{ColorCharge::Blue,     ColorCharge::AntiBlue  },  // g8  bb̄  (diagonal)
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
// Quark type codes  (negative; extends WO-63A table)
// ============================================================================

enum class QuarkFlavor : int32_t {
	Up       = -7,
	Down     = -8,
	Strange  = -9,
	Charm    = -10,
	Bottom   = -11,
	Top      = -12,
	Gluon    = -13,
	AntiQuark= -14,
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

/// Fractional electric charge for a quark flavor (in units of e).
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

/// Rest-mass analog in dimensionless VSIM mass units (scaled from MeV/c²).
/// Top quark ~173 GeV → assigned highest mass; up ~2.2 MeV → near zero.
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
// QCD Particle State
// ============================================================================

struct QCDParticle {
	uint32_t      id{};
	QuarkFlavor   flavor{QuarkFlavor::Up};
	ColorCharge   color{ColorCharge::Red};
	GluonChannel  gluon_channel{};    // only meaningful for Gluon flavor

	atomistic::Vec3 position{};
	atomistic::Vec3 velocity{};
	atomistic::Vec3 force{};          // accumulated Cornell + color force

	double mass{};          // VSIM mass unit (set from quark_rest_mass)
	double energy{};        // kinetic + string potential energy
	double string_length{}; // current confinement string length (Å analog)

	bool   active{true};
	bool   confined{false}; // true when part of a color-singlet cluster

	// Binding partner for confinement strings (index into plasma particle list)
	int    string_partner{-1};

	void reset_force() { force = {0.0, 0.0, 0.0}; }
};

// ============================================================================
// Cornell Potential (confinement kernel)
//
//   V(r)  = -κ/r  +  σ·r        (r > r_min)
//   F(r)  =  dV/dr = κ/r²  +  σ  (repulsive short, confining long)
//
// Parameters are dimensionless analogs; caller sets κ and σ relative
// to the VSIM length/energy units and τ_scale.
// ============================================================================

struct CornellParams {
	double kappa{0.52};          // Coulombic coefficient (αs analog, ~0.3–0.6)
	double sigma{0.18};          // string tension  (GeV²/c analog, rescaled)
	double r_min{0.05};          // short-distance cutoff (Å analog)
	double r_conf{2.0};          // confinement radius threshold (hadronization gate)
	double tau_scale{1.0e-3};    // time-dilation: 1 fm/c → tau_scale fs in VSIM time
};

/// Returns the magnitude of the radial Cornell force between two QCD particles.
/// Positive = repulsive at short range (Coulombic) + confining at long range (string).
inline double cornell_force_magnitude(double r, const CornellParams& p) {
	double r_eff = (r < p.r_min) ? p.r_min : r;
	return p.kappa / (r_eff * r_eff) + p.sigma;
}

/// Full Cornell potential energy.
inline double cornell_potential(double r, const CornellParams& p) {
	double r_eff = (r < p.r_min) ? p.r_min : r;
	return -p.kappa / r_eff + p.sigma * r_eff;
}

// ============================================================================
// Color-Force Sign Convention
//
// Quark-quark same color:   repulsive (like charges in color space)
// Quark-antiquark complement: attractive (Casimir factor -4/3)
// Other pairs:               reduced-strength (partial color matching, factor -1/3 or +1/6)
//
// Simplified Casimir factor table (SU(3) singlet / octet projection):
// ============================================================================

inline double color_casimir_factor(ColorCharge a, ColorCharge b) {
	if (a == b)                                return +1.0/6.0;   // repulsive (same)
	if (b == complementary_color(a))           return -4.0/3.0;   // attractive (singlet)
	return -1.0/6.0;                                               // partial (octet)
}

// ============================================================================
// T_A|B Dual-Timescale Scheduler
//
//   T_B  — outer bead/hadronic step  (Δt_B = caller's bead dt, typically 1 fs)
//   T_A  — inner QCD substep         (Δt_A = Δt_B / K, default K = 200)
//
// One call to qcd_substep_pass() runs one T_A substep for all QCD particles.
// The caller loops K times per T_B step.
// ============================================================================

struct TABScheduler {
	double  dt_B{1.0};          // bead step (fs)
	int     K{200};             // substeps per bead step
	double  dt_A() const { return dt_B / static_cast<double>(K); }
};

// ============================================================================
// QuarkGluonPlasma  —  container for the QCD sidecar
// ============================================================================

struct QuarkGluonPlasma {
	std::vector<QCDParticle>  particles;
	CornellParams             cornell;
	TABScheduler              scheduler;
	bool                      enabled{true};

	// Counters
	int   hadronization_events{0};
	int   gluon_emission_events{0};
	int   step_count{0};

	int  num_active() const {
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
// Force accumulation pass
//
// Computes Cornell + color-force for all active QCD particle pairs.
// O(N²) — acceptable for small QCD plasma (N < 200 typical).
// ============================================================================

inline void qcd_accumulate_forces(QuarkGluonPlasma& plasma) {
	const auto& cp = plasma.cornell;

	for (auto& p : plasma.particles) p.reset_force();

	const int N = static_cast<int>(plasma.particles.size());
	for (int i = 0; i < N; ++i) {
		auto& pi = plasma.particles[i];
		if (!pi.active) continue;

		for (int j = i + 1; j < N; ++j) {
			auto& pj = plasma.particles[j];
			if (!pj.active) continue;

			atomistic::Vec3 dr = {
				pj.position.x - pi.position.x,
				pj.position.y - pi.position.y,
				pj.position.z - pi.position.z
			};
			double r2 = dr.x*dr.x + dr.y*dr.y + dr.z*dr.z;
			double r  = std::sqrt(r2 + 1e-20);

			// Update string length for confined pairs
			if (pi.string_partner == j || pj.string_partner == i) {
				pi.string_length = r;
				pj.string_length = r;
			}

			double F_mag = cornell_force_magnitude(r, cp);
			double casimir = color_casimir_factor(pi.color, pj.color);
			// Negative casimir → attractive, positive → repulsive
			double F_net = casimir * F_mag / r;

			pi.force.x += F_net * dr.x;
			pi.force.y += F_net * dr.y;
			pi.force.z += F_net * dr.z;
			pj.force.x -= F_net * dr.x;
			pj.force.y -= F_net * dr.y;
			pj.force.z -= F_net * dr.z;
		}
	}
}

// ============================================================================
// T_A substep integrator  (velocity-Verlet, one substep)
// ============================================================================

inline void qcd_substep_pass(QuarkGluonPlasma& plasma) {
	const double dt = plasma.scheduler.dt_A();
	const double r_conf = plasma.cornell.r_conf;

	qcd_accumulate_forces(plasma);

	for (auto& p : plasma.particles) {
		if (!p.active) continue;

		double inv_m = (p.mass > 1e-15) ? 1.0 / p.mass : 0.0;

		// Velocity-Verlet half-kick + full-step position
		p.velocity.x += 0.5 * p.force.x * inv_m * dt;
		p.velocity.y += 0.5 * p.force.y * inv_m * dt;
		p.velocity.z += 0.5 * p.force.z * inv_m * dt;

		p.position.x += p.velocity.x * dt;
		p.position.y += p.velocity.y * dt;
		p.position.z += p.velocity.z * dt;

		// Second half-kick (force will be re-accumulated next call; approximate)
		p.velocity.x += 0.5 * p.force.x * inv_m * dt;
		p.velocity.y += 0.5 * p.force.y * inv_m * dt;
		p.velocity.z += 0.5 * p.force.z * inv_m * dt;

		// Kinetic energy update
		double v2 = p.velocity.x*p.velocity.x
				  + p.velocity.y*p.velocity.y
				  + p.velocity.z*p.velocity.z;
		p.energy = 0.5 * p.mass * v2;

		// Hadronization gate: if string exceeds confinement radius, mark event
		if (p.string_partner >= 0
			&& p.string_length > r_conf
			&& p.string_partner < static_cast<int>(plasma.particles.size())) {
			// Soft hadronization: deactivate both; record event
			plasma.particles[static_cast<size_t>(p.string_partner)].active = false;
			p.active = false;
			++plasma.hadronization_events;
		}
	}

	++plasma.step_count;
}

// ============================================================================
// T_B full-step: run K T_A substeps
// ============================================================================

inline void qcd_run_tb_step(QuarkGluonPlasma& plasma) {
	if (!plasma.enabled || plasma.particles.empty()) return;
	const int K = std::max(1, plasma.scheduler.K);
	for (int k = 0; k < K; ++k) {
		qcd_substep_pass(plasma);
	}
}

// ============================================================================
// Population builder  —  seed a small quark-gluon plasma
// ============================================================================

struct QCDPopulationConfig {
	int    n_quarks{6};        // number of quarks to seed
	int    n_gluons{4};        // number of gluons
	double box_radius{3.0};    // Å-analog; particles placed inside a sphere of this radius
	double v_thermal{0.1};     // thermal velocity scale (Å/fs analog)
	uint32_t seed{0x513'0001u};
};

inline void qcd_seed_plasma(QuarkGluonPlasma& plasma, const QCDPopulationConfig& cfg) {
	plasma.clear();

	uint32_t s = cfg.seed;
	auto lcg = [&s]() -> double {
		s ^= s << 13; s ^= s >> 17; s ^= s << 5;
		return (s & 0x7FFFFFFFu) / static_cast<double>(0x7FFFFFFFu);
	};
	auto lcg_signed = [&]() -> double { return 2.0 * lcg() - 1.0; };

	// Quark flavors cycle through up/down/strange/charm/bottom/top
	const QuarkFlavor flavors[] = {
		QuarkFlavor::Up, QuarkFlavor::Down, QuarkFlavor::Strange,
		QuarkFlavor::Charm, QuarkFlavor::Bottom, QuarkFlavor::Top
	};
	const ColorCharge colors[] = {
		ColorCharge::Red, ColorCharge::Green, ColorCharge::Blue
	};

	uint32_t next_id = 0u;

	// Seed quarks
	for (int i = 0; i < cfg.n_quarks; ++i) {
		QCDParticle p;
		p.id     = next_id++;
		p.flavor = flavors[i % 6];
		p.color  = colors[i % 3];
		p.mass   = quark_rest_mass(p.flavor);
		p.position = {
			cfg.box_radius * lcg_signed(),
			cfg.box_radius * lcg_signed(),
			cfg.box_radius * lcg_signed()
		};
		p.velocity = {
			cfg.v_thermal * lcg_signed(),
			cfg.v_thermal * lcg_signed(),
			cfg.v_thermal * lcg_signed()
		};
		// Pair adjacent quarks with complementary colors as string partners
		if (i % 2 == 1 && i >= 1) {
			int partner = static_cast<int>(plasma.particles.size()) - 1;
			p.string_partner = partner;
			plasma.particles[static_cast<size_t>(partner)].string_partner =
				static_cast<int>(plasma.particles.size());
			// Give partner the complementary color
			plasma.particles[static_cast<size_t>(partner)].color =
				complementary_color(p.color);
		}
		p.active = true;
		plasma.particles.push_back(p);
	}

	// Seed gluons
	for (int i = 0; i < cfg.n_gluons; ++i) {
		QCDParticle g;
		g.id      = next_id++;
		g.flavor  = QuarkFlavor::Gluon;
		g.color   = ColorCharge::Neutral;
		g.gluon_channel = GluonChannel::from_index(static_cast<uint8_t>(i % 8));
		g.mass    = 0.0;
		g.position = {
			cfg.box_radius * lcg_signed() * 0.5,
			cfg.box_radius * lcg_signed() * 0.5,
			cfg.box_radius * lcg_signed() * 0.5
		};
		g.velocity = {
			cfg.v_thermal * lcg_signed() * 2.0,   // gluons faster (massless)
			cfg.v_thermal * lcg_signed() * 2.0,
			cfg.v_thermal * lcg_signed() * 2.0
		};
		g.active = true;
		plasma.particles.push_back(g);
	}
}

} // namespace qcd
} // namespace vsepr
