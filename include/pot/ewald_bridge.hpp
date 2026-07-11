#pragma once
/**
 * ewald_bridge.hpp — State → EwaldSum adapter
 * =============================================
 *
 * Bridges atomistic::State (Vec3 positions, BoxOrtho box) to the flat-array
 * API of EwaldSum (vector<double> coords, BoxOrtho box).
 *
 * Why this exists:
 *   EwaldSum takes flat double arrays for maximum performance and portability.
 *   State carries vector<Vec3> positions.
 *   This adapter is the only place that translates between the two — no other
 *   code should duplicate the flattening logic.
 *
 * Usage:
 *
 *   #include "pot/ewald_bridge.hpp"
 *
 *   EwaldParams params;
 *   params.alpha = 0.3; params.rcut_real = 8.0; params.kmax = 6;
 *
 *   // Energy only
 *   double E = vsepr::pot::ewald_energy(state, params);
 *
 *   // Energy + forces accumulated into state.F
 *   double E = vsepr::pot::ewald_eval(state, params);
 *
 * Preconditions:
 *   - state.box.enabled must be true
 *   - state.Q.size() == state.N
 *   - state.X.size() == state.N
 *   - state.F.size() == state.N  (for ewald_eval; zeroing is caller's responsibility
 *                                  if a clean force pass is wanted)
 *
 * Day #57A  |  WO-56C  |  beta-8 gate
 */

#include "box/pbc.hpp"
#include "pot/ewald_sum.hpp"
#include "atomistic/core/state.hpp"

#include <stdexcept>
#include <vector>

namespace vsepr::pot {

// ---------------------------------------------------------------------------
// flatten_positions — Vec3 array → flat double array
// ---------------------------------------------------------------------------

inline std::vector<double> flatten_positions(const atomistic::State& s)
{
	std::vector<double> coords;
	coords.reserve(s.N * 3);
	for (const auto& r : s.X) {
		coords.push_back(r.x);
		coords.push_back(r.y);
		coords.push_back(r.z);
	}
	return coords;
}

// ---------------------------------------------------------------------------
// unflatten_forces — accumulate flat force array back into State::F
// ---------------------------------------------------------------------------

inline void unflatten_forces(const std::vector<double>& flat,
							  atomistic::State& s)
{
	for (size_t i = 0; i < s.N; ++i) {
		s.F[i].x += flat[3*i + 0];
		s.F[i].y += flat[3*i + 1];
		s.F[i].z += flat[3*i + 2];
	}
}

// ---------------------------------------------------------------------------
// ewald_energy — Coulomb energy only, no force update
// ---------------------------------------------------------------------------

inline double ewald_energy(const atomistic::State& s,
							const EwaldParams& params)
{
	if (!s.box.enabled)
		throw std::runtime_error("ewald_energy: state.box must be enabled");
	if (s.Q.size() != s.N)
		throw std::runtime_error("ewald_energy: Q.size() != N");

	auto coords = flatten_positions(s);
	std::vector<double> forces(s.N * 3, 0.0);   // discarded

	EwaldSum ewald(params);
	return ewald.evaluate(coords, s.Q, s.box, forces);
}

// ---------------------------------------------------------------------------
// ewald_eval — Coulomb energy + forces accumulated into state.F
//
// Forces are ACCUMULATED (+=), not replaced.
// Zero state.F before calling if a clean Ewald-only pass is needed.
// ---------------------------------------------------------------------------

inline double ewald_eval(atomistic::State& s,
						  const EwaldParams& params)
{
	if (!s.box.enabled)
		throw std::runtime_error("ewald_eval: state.box must be enabled");
	if (s.Q.size() != s.N)
		throw std::runtime_error("ewald_eval: Q.size() != N");
	if (s.F.size() != s.N)
		throw std::runtime_error("ewald_eval: F.size() != N");

	auto coords = flatten_positions(s);
	std::vector<double> flat_forces(s.N * 3, 0.0);

	EwaldSum ewald(params);
	double E = ewald.evaluate(coords, s.Q, s.box, flat_forces);

	unflatten_forces(flat_forces, s);
	return E;
}

} // namespace vsepr::pot
