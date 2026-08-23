#pragma once
/**
 * tab_scheduler.hpp  -  T_A|B Dual-Timescale Scheduler
 *
 * Implements the T_A|B architecture established in WO-63A and used by the
 * QCD extreme-environment layer:
 *
 *   T_B  — slow outer bead/hadronic step     (dt_B, caller-driven)
 *   T_A  — fast inner QCD/physics substep    (dt_A = dt_B / K)
 *
 * One call to qcd_substep_pass() runs one T_A substep for all particles.
 * The caller loops K times per T_B step (see qcd_force.hpp).
 *
 * Default: K = 200, dt_B = 1.0 fs  →  dt_A = 0.005 fs per substep
 *
 * Source: extracted from include/coarse_grain/physics/qcd_transient.hpp
 * v5.1.3~2  |  WO-v513-QCD  |  v5.0.0-main
 */

namespace vsepr {
namespace collision {

struct TABScheduler {
	double dt_B{1.0};    // Outer bead step (fs)
	int    K{200};        // Number of T_A substeps per T_B step

	double dt_A() const {
		return dt_B / static_cast<double>(K);
	}
};

} // namespace collision
} // namespace vsepr
