#pragma once
/**
 * qcd_force.hpp  -  QCD Force Accumulation, Integrator, and Plasma Engine
 *
 * Provides the full T_A substep loop for a QuarkGluonPlasma:
 *   qcd_accumulate_forces()  — O(N²) Cornell + Casimir pair force pass
 *   qcd_substep_pass()       — velocity-Verlet T_A step + hadronization gate
 *   qcd_run_tb_step()        — K × qcd_substep_pass (one full T_B step)
 *   qcd_seed_plasma()        — deterministic plasma population builder
 *
 * NOTE: qcd_substep_pass() uses a single-pass velocity-Verlet approximation.
 * The second half-kick uses the pre-step force estimate. This is acceptable
 * for the scaled-analog QCD layer but should not be confused with a fully
 * symplectic integrator.
 *
 * NOTE: hadronization deactivation currently marks both partners in the
 * forward integration pass. A deferred-deactivation list (planned) will
 * eliminate the ordering hazard.
 *
 * Source: extracted from include/coarse_grain/physics/qcd_transient.hpp
 * v5.1.3~2  |  WO-v513-QCD  |  v5.0.0-main
 */

#include "PARTICLES/qcd_particle.hpp"
#include "COLLISION/cornell.hpp"
#include "COLLISION/casimir.hpp"
#include "COLLISION/tab_scheduler.hpp"
#include "atomistic/core/state.hpp"
#include "IDENTITY/identity.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vsepr {
namespace collision {

// ============================================================================
// Force accumulation pass
// Computes Cornell + color-force for all active QCD particle pairs. O(N²).
// ============================================================================

inline void qcd_accumulate_forces(particles::QuarkGluonPlasma& plasma) {
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

			if (pi.string_partner == j || pj.string_partner == i) {
				pi.string_length = r;
				pj.string_length = r;
			}

			double F_mag    = cornell_force_magnitude(r, cp);
			double casimir  = color_casimir_factor(pi.color, pj.color);
			double F_net    = casimir * F_mag / r;

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

inline void qcd_substep_pass(particles::QuarkGluonPlasma& plasma) {
	const double dt      = plasma.scheduler.dt_A();
	const double r_conf  = plasma.cornell.r_conf;

	qcd_accumulate_forces(plasma);

	for (auto& p : plasma.particles) {
		if (!p.active) continue;

		double inv_m = (p.mass > 1e-15) ? 1.0 / p.mass : 0.0;

		// Half-kick + position step
		p.velocity.x += 0.5 * p.force.x * inv_m * dt;
		p.velocity.y += 0.5 * p.force.y * inv_m * dt;
		p.velocity.z += 0.5 * p.force.z * inv_m * dt;

		p.position.x += p.velocity.x * dt;
		p.position.y += p.velocity.y * dt;
		p.position.z += p.velocity.z * dt;

		// Second half-kick (NOTE: single-pass approximation — uses pre-step force)
		p.velocity.x += 0.5 * p.force.x * inv_m * dt;
		p.velocity.y += 0.5 * p.force.y * inv_m * dt;
		p.velocity.z += 0.5 * p.force.z * inv_m * dt;

		double v2 = p.velocity.x*p.velocity.x
				  + p.velocity.y*p.velocity.y
				  + p.velocity.z*p.velocity.z;
		p.energy = 0.5 * p.mass * v2;

		// Hadronization gate
		if (p.string_partner >= 0
			&& p.string_length > r_conf
			&& p.string_partner < static_cast<int>(plasma.particles.size())) {
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

inline void qcd_run_tb_step(particles::QuarkGluonPlasma& plasma) {
	if (!plasma.enabled || plasma.particles.empty()) return;
	const int K = std::max(1, plasma.scheduler.K);
	for (int k = 0; k < K; ++k) {
		qcd_substep_pass(plasma);
	}
}

// ============================================================================
// Plasma seeder
// ============================================================================

inline void qcd_seed_plasma(particles::QuarkGluonPlasma& plasma,
							 const particles::QCDPopulationConfig& cfg)
{
	plasma.clear();

	uint32_t s = cfg.seed;
	auto lcg = [&s]() -> double {
		s ^= s << 13; s ^= s >> 17; s ^= s << 5;
		return (s & 0x7FFFFFFFu) / static_cast<double>(0x7FFFFFFFu);
	};
	auto lcg_signed = [&]() -> double { return 2.0 * lcg() - 1.0; };

	using namespace particles;
	const QuarkFlavor flavors[] = {
		QuarkFlavor::Up, QuarkFlavor::Down, QuarkFlavor::Strange,
		QuarkFlavor::Charm, QuarkFlavor::Bottom, QuarkFlavor::Top
	};
	const ColorCharge colors[] = {
		ColorCharge::Red, ColorCharge::Green, ColorCharge::Blue
	};

	uint32_t next_id = 0u;

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
		if (i % 2 == 1 && i >= 1) {
			int partner = static_cast<int>(plasma.particles.size()) - 1;
			p.string_partner = partner;
			plasma.particles[static_cast<size_t>(partner)].string_partner =
				static_cast<int>(plasma.particles.size());
			plasma.particles[static_cast<size_t>(partner)].color =
				complementary_color(p.color);
		}
		p.active = true;
		plasma.particles.push_back(p);
	}

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
			cfg.v_thermal * lcg_signed() * 2.0,
			cfg.v_thermal * lcg_signed() * 2.0,
			cfg.v_thermal * lcg_signed() * 2.0
		};
		g.active = true;
		plasma.particles.push_back(g);
	}
}

// ============================================================================
// SM-channel gated force accumulation pass
//
// Wraps qcd_accumulate_forces() with a per-pair SM channel evaluation.
// Cornell force fires only when the color channel score (m_g) is active.
//
// identity_layer  — parallel array of ParticleIdentity, indexed by QCDParticle
//                   position in plasma.particles.  Must be the same size.
// policy          — ChannelPolicy controlling thresholds and weights.
// candidates_out  — optional output: filled with one InteractionCandidate per
//                   active pair where at least one SM gate triggered.
//                   Pass nullptr to skip candidate recording.
//
// NOTE: This overload does NOT replace qcd_accumulate_forces().
//       Call qcd_accumulate_forces() directly for pure Cornell operation.
// ============================================================================

inline void qcd_accumulate_forces_sm(
	particles::QuarkGluonPlasma&                      plasma,
	std::vector<identity::ParticleIdentity>&           identity_layer,
	const identity::ChannelPolicy&                     policy,
	std::vector<identity::InteractionCandidate>*       candidates_out)
{
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

			// String length tracking (unchanged)
			if (pi.string_partner == j || pj.string_partner == i) {
				pi.string_length = r;
				pj.string_length = r;
			}

			// SM channel evaluation
			double E_rel = pi.energy + pj.energy;
			identity::InteractionCandidate cand;
			bool have_identity = (i < static_cast<int>(identity_layer.size()) &&
								  j < static_cast<int>(identity_layer.size()));
			if (have_identity) {
				cand = identity::evaluate_pair(
					identity_layer[static_cast<size_t>(i)],
					identity_layer[static_cast<size_t>(j)],
					policy, r, E_rel, 0.0);
			}

			// Cornell force fires only when color channel is active
			bool fire_cornell = !have_identity ||
								 cand.channels.color_channel_active(policy.m_g_threshold);
			if (fire_cornell) {
				double F_mag   = cornell_force_magnitude(r, cp);
				double casimir = color_casimir_factor(pi.color, pj.color);
				double F_net   = casimir * F_mag / r;

				pi.force.x += F_net * dr.x;
				pi.force.y += F_net * dr.y;
				pi.force.z += F_net * dr.z;
				pj.force.x -= F_net * dr.x;
				pj.force.y -= F_net * dr.y;
				pj.force.z -= F_net * dr.z;
			}

			// Record candidate if requested and a gate triggered
			if (candidates_out && have_identity &&
				(cand.annihilation_candidate || cand.positive_information_event ||
				 cand.color_exchange_active  || cand.weak_identity_transition)) {
				candidates_out->push_back(cand);
			}
		}
	}
}

} // namespace collision
} // namespace vsepr
} // namespace vsepr