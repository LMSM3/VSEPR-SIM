#pragma once
/**
 * annihilation_event.hpp
 * ======================
 * WO-66J  |  Annihilation Event Channel
 *
 * Defines the AnnihilationEvent record and the three-gate trigger function
 * for particle–antiparticle annihilation.
 *
 * Separation principle (bridge paper §13.4):
 *   Annihilation is a first-class event channel, distinct from:
 *     - ChemistryBondEvent  (bond formation / breaking)
 *     - ChemistryNonBondEvent  (non-bonded pair forces)
 *     - DecayEvent          (single-particle decay)
 *     - ParticleParticleEvent  (classical collision / interaction)
 *
 * Gate logic (bridge paper §13.2):
 *   annihilate(i,j) = 1   if  r_ij < r_c  AND  C_id(i,j)=1  AND  E_rel > E_c
 *                   = 0   otherwise
 *
 * Live print stream:
 *   [ANN] step=184 t=0.184 pair=e_001+pos_001 xc=(0.01,0.00,0.00)
 *         Ein=...  Eout=...  products=gamma_001,gamma_002  residualE=...
 *
 * For v5.1.4: M_ij^X = 0 — subatomic sampling disabled.
 *   The event record and gate logic are defined here so the annihilation
 *   infrastructure exists before any higher-energy channel is enabled.
 *
 * v5.1.4  |  WO-66J  |  v5.0.0-main
 */

#include "../../identity/particle_identity.hpp"
#include "../../identity/particle_identity_seeder.hpp"

#include <cstdint>
#include <cmath>
#include <string>
#include <vector>

namespace vsim::bridge65 {

// ============================================================================
// AnnihilationGateResult — diagnostic record from check_annihilation_gate()
// ============================================================================

struct AnnihilationGateResult {
	bool passed_identity_gate {false};
	bool passed_distance_gate {false};
	bool passed_energy_gate   {false};

	// Computed quantities
	double r_ij    {0.0};   // |x_i - x_j|
	double E_rel   {0.0};   // ½ μ |v_i - v_j|²
	double mu      {0.0};   // reduced mass

	// Overall trigger
	bool triggered() const noexcept {
		return passed_identity_gate && passed_distance_gate && passed_energy_gate;
	}

	// [PP] line for the particle-particle stream (bridge paper §13.5)
	std::string pp_line(std::uint64_t step, double t,
						const std::string& id_a, const std::string& id_b) const {
		char buf[256];
		std::snprintf(buf, sizeof(buf),
			"[PP] step=%llu t=%.3f idA=%s idB=%s r=%.4f Erel=%.4f "
			"gate=%s%s%s action=%s",
			static_cast<unsigned long long>(step), t,
			id_a.c_str(), id_b.c_str(),
			r_ij, E_rel,
			passed_identity_gate ? "id"   : "",
			passed_distance_gate ? "+dist": "",
			passed_energy_gate   ? "+energy" : "",
			triggered() ? "ANNIHILATE" : "NONE");
		return std::string(buf);
	}
};

// ============================================================================
// AnnihilationProduct — single product emitted from an annihilation event
// ============================================================================

struct AnnihilationProduct {
	std::string label;      // e.g. "gamma_001", "pion_proxy_001"
	double px {0.0};        // momentum components
	double py {0.0};
	double pz {0.0};
	double energy {0.0};    // product energy
	int    charge {0};
};

// ============================================================================
// AnnihilationEvent — full record for one annihilation pair
//
// E_ann = [ id_i, id_j, t, x_c, E_in, E_out, P_products, ΔS, ΔI, flags ]
// (bridge paper §13.1)
// ============================================================================

struct AnnihilationEvent {
	// ---- simulation context ------------------------------------------------
	std::uint64_t step   {0};
	double        time_s {0.0};

	// ---- pair identity -----------------------------------------------------
	int64_t  id_i   {-1};
	int64_t  id_j   {-1};
	uint64_t hash_i {0};    // birth_hash of particle i
	uint64_t hash_j {0};    // birth_hash of particle j

	std::string label_i;    // human-readable label, e.g. "e_001"
	std::string label_j;    // e.g. "pos_001"

	// ---- kinematics --------------------------------------------------------
	// Collision center:  x_c = (m_i x_i + m_j x_j) / (m_i + m_j)
	double xc_x {0.0};
	double xc_y {0.0};
	double xc_z {0.0};

	// Incoming energy:   E_in = ½ m_i |v_i|² + ½ m_j |v_j|² + U_ij
	double E_in  {0.0};   // total incoming energy (kinetic + potential)
	double E_out {0.0};   // total outgoing energy in products

	// Residuals  (bridge paper §13.1 and §14 ANN-05)
	double residual_E {0.0};  // |E_out - E_expected| / E_expected
	double residual_p {0.0};  // |p_out - p_in|

	// ---- products ----------------------------------------------------------
	std::vector<AnnihilationProduct> products;

	// ---- event flags -------------------------------------------------------
	bool conserve_energy   {true};
	bool conserve_momentum {true};

	// ΔS, ΔI — entropy and information change proxies
	double delta_S {0.0};   // entropy change (log-based proxy)
	double delta_I {0.0};   // identity information change

	// ---- gate diagnostic (optional, set by check_annihilation_gate) --------
	AnnihilationGateResult gate;

	// ---- [ANN] live print line (bridge paper §13.5) ------------------------
	std::string ann_line() const {
		char buf[512];
		std::string prod_list;
		for (std::size_t k = 0; k < products.size(); ++k) {
			if (k) prod_list += ",";
			prod_list += products[k].label;
		}
		std::snprintf(buf, sizeof(buf),
			"[ANN] step=%llu t=%.3f pair=%s+%s "
			"xc=(%.4f,%.4f,%.4f) "
			"Ein=%.4f Eout=%.4f residualE=%.2e products=%s",
			static_cast<unsigned long long>(step), time_s,
			label_i.c_str(), label_j.c_str(),
			xc_x, xc_y, xc_z,
			E_in, E_out, residual_E,
			prod_list.empty() ? "none" : prod_list.c_str());
		return std::string(buf);
	}
};

// ============================================================================
// check_annihilation_gate()
//
// Evaluate the three-gate annihilation trigger for a candidate pair (i,j).
//
// Parameters:
//   pi, pj         — identity records (for identity gate C_id)
//   xi,yi,zi       — position of i
//   xj,yj,zj       — position of j
//   vxi,vyi,vzi    — velocity of i
//   vxj,vyj,vzj    — velocity of j
//   r_capture      — capture radius r_c  (gate 1)
//   energy_threshold — minimum E_rel    (gate 2)
//
// Returns AnnihilationGateResult with all three gate flags set.
// ============================================================================

inline AnnihilationGateResult check_annihilation_gate(
	const vsepr::identity::ParticleIdentity& pi,
	const vsepr::identity::ParticleIdentity& pj,
	double xi, double yi, double zi,
	double xj, double yj, double zj,
	double vxi, double vyi, double vzi,
	double vxj, double vyj, double vzj,
	double r_capture,
	double energy_threshold
) noexcept {
	AnnihilationGateResult g;

	// Gate 1: identity compatibility C_id(i,j)
	g.passed_identity_gate =
		vsepr::identity::ParticleIdentitySeeder::are_anti_partners(pi, pj);

	// Gate 2: distance r_ij < r_c
	double dx = xi - xj, dy = yi - yj, dz = zi - zj;
	g.r_ij = std::sqrt(dx*dx + dy*dy + dz*dz);
	g.passed_distance_gate = (g.r_ij < r_capture);

	// Gate 3: relative kinetic energy E_rel > E_c
	double mi = pi.mass, mj = pj.mass;
	double denom = mi + mj;
	g.mu = (denom > 0.0) ? (mi * mj / denom) : 0.0;
	double dvx = vxi - vxj, dvy = vyi - vyj, dvz = vzi - vzj;
	double v2  = dvx*dvx + dvy*dvy + dvz*dvz;
	g.E_rel = 0.5 * g.mu * v2;
	g.passed_energy_gate = (g.E_rel > energy_threshold);

	return g;
}

// ============================================================================
// build_annihilation_event()
//
// Construct an AnnihilationEvent from a triggered gate result plus particle
// kinematics.  Computes collision center, E_in, and populates the record
// ready for product emission and logging.
//
// Products must be appended by the caller after emission model runs.
// ============================================================================

inline AnnihilationEvent build_annihilation_event(
	std::uint64_t step, double time_s,
	const vsepr::identity::ParticleIdentity& pi,
	const vsepr::identity::ParticleIdentity& pj,
	const std::string& label_i,
	const std::string& label_j,
	double xi, double yi, double zi,
	double xj, double yj, double zj,
	double vxi, double vyi, double vzi,
	double vxj, double vyj, double vzj,
	double U_ij,
	const AnnihilationGateResult& gate
) noexcept {
	AnnihilationEvent ev;
	ev.step   = step;
	ev.time_s = time_s;
	ev.id_i   = pi.id;
	ev.id_j   = pj.id;
	ev.hash_i = pi.birth_hash;
	ev.hash_j = pj.birth_hash;
	ev.label_i = label_i;
	ev.label_j = label_j;
	ev.gate   = gate;

	// Collision center  x_c = (m_i x_i + m_j x_j) / (m_i + m_j)
	double mi = pi.mass, mj = pj.mass;
	double total_m = mi + mj;
	if (total_m > 0.0) {
		ev.xc_x = (mi * xi + mj * xj) / total_m;
		ev.xc_y = (mi * yi + mj * yj) / total_m;
		ev.xc_z = (mi * zi + mj * zj) / total_m;
	}

	// E_in = ½ m_i |v_i|² + ½ m_j |v_j|² + U_ij
	ev.E_in = 0.5 * mi * (vxi*vxi + vyi*vyi + vzi*vzi)
			+ 0.5 * mj * (vxj*vxj + vyj*vyj + vzj*vzj)
			+ U_ij;

	return ev;
}

} // namespace vsim::bridge65
