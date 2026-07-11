#pragma once
/**
 * bead_transport.hpp — Transport / Acceleration Shell for V5 Demo
 * ═══════════════════════════════════════════════════════════════
 *
 * Implements the reduced motion testbed:
 *
 *   dv/dt = A(u - v) + B*G + C*H
 *
 * where:
 *   A(u - v)  = drag / relaxation shell  (velocity coupling)
 *   B*G       = body-force shell          (gravity / external field)
 *   C*H       = gradient-driving shell    (environment gradient forces)
 *
 * This is the TRANSPORT layer, not a replacement for the environment
 * model.  Environment responsiveness enters through how parameters
 * or kernels depend on (eta, rho, C, P2), which is handled by the
 * existing coarse_grain::EnvironmentState + environment_coupling.hpp.
 *
 * Position-velocity integration: velocity Verlet.
 *
 * Update order per step (ERB §, mandatory):
 *   1. Integrate poses (R, Q)
 *   2. Compute fast observables (rho, C, P2)
 *   3. Update slow state (eta)
 *   4. Evaluate modulated kernels K_k(eta)
 *   5. Compute forces and torques
 *   (loop back to 1)
 *
 * VSEPR-SIM V5.0  |  2026-04-16
 */

#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/models/environment_coupling.hpp"
#include "coarse_grain/models/lj_bead_helper.hpp"
#include "tests/scene_builders.hpp"
#include "atomistic/core/state.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace v5 {

// ============================================================================
// Transport Coefficients
// ============================================================================

/**
 * TransportParams — the three shell coefficients + ambient field definitions.
 *
 * A: drag coefficient (inverse relaxation time, fs^-1)
 * B: body-force coupling strength
 * C: gradient-driving coupling strength
 */
struct TransportParams {
	double A{0.01};     // drag / relaxation rate (fs^-1)
	double B{1.0};      // body-force multiplier
	double C_grad{0.5}; // gradient-driving multiplier (C is taken by coord number)
};

// ============================================================================
// Per-Bead Dynamic State
// ============================================================================

/**
 * BeadDynState — full per-bead state for the v5 transport testbed.
 *
 * Owns position, velocity, acceleration, plus the environment state.
 * Orientation is carried from the SceneBead but not dynamically evolved
 * in this demo (rigid orientation assumption for Phase A/B).
 */
struct BeadDynState {
	// Pose
	atomistic::Vec3 position{};
	atomistic::Vec3 velocity{};
	atomistic::Vec3 acceleration{};

	// Orientation (fixed for now; torque evolution is future work)
	atomistic::Vec3 n_hat{0, 0, 1};
	bool has_orientation{true};

	// Environment state (X_B)
	coarse_grain::EnvironmentState env{};

	// Mass (reduced units; default 1.0)
	double mass{1.0};

	// Acceleration decomposition (for diagnostics)
	atomistic::Vec3 a_drag{};
	atomistic::Vec3 a_body{};
	atomistic::Vec3 a_grad{};
	atomistic::Vec3 a_total{};

	// Kernel modulation report (for diagnostics)
	coarse_grain::ModulationReport mod_report{};
};

// ============================================================================
// Ambient Field Functions
// ============================================================================

/**
 * Ambient velocity field u(R).
 *
 * Models a laminar shear flow with a transverse gradient:
 *
 *   u_x(R) = U0 + dU/dy * y + dU/dz * z
 *   u_y(R) = 0
 *   u_z(R) = 0
 *
 * Parameters (reduced units, fs^-1 * Å):
 *   U0   = 0.5   — base flow speed along +x
 *   shear_y = 0.10  — velocity gradient in the y-direction (du_x/dy)
 *   shear_z = 0.04  — secondary gradient in the z-direction (du_x/dz)
 *
 * This creates a 2D shear profile: beads displaced in y or z experience
 * a proportionally higher or lower drag velocity, driving lateral ordering.
 */
inline atomistic::Vec3 ambient_velocity(const atomistic::Vec3& R) {
	constexpr double U0      = 0.5;
	constexpr double shear_y = 0.10;
	constexpr double shear_z = 0.04;
	return {U0 + shear_y * R.y + shear_z * R.z, 0.0, 0.0};
}

/**
 * Body-force field G(R).
 *
 * Models a spatially modulated sedimentation field:
 *
 *   G_z(R) = -g0 * (1 + kappa * |R_xy|)
 *   G_x = G_y = 0
 *
 * Parameters:
 *   g0    = 0.010  — base gravitational acceleration (Å fs^-2)
 *   kappa = 0.008  — radial enhancement: beads farther from the z-axis
 *                    experience stronger downward pull, mimicking a
 *                    centrifugation or sedimentation gradient
 *
 * |R_xy| = sqrt(x^2 + y^2)
 */
inline atomistic::Vec3 body_force_field(const atomistic::Vec3& R) {
	constexpr double g0    = 0.010;
	constexpr double kappa = 0.008;
	double r_xy = std::sqrt(R.x * R.x + R.y * R.y);
	double gz = -g0 * (1.0 + kappa * r_xy);
	return {0.0, 0.0, gz};
}

/**
 * Gradient-driving field H(R).
 *
 * Models a composite confinement potential with:
 *   (a) Soft radial centering: pulls beads toward the origin
 *       H_radial = -lambda / (r + r0)^2 * R_hat
 *   (b) Axial anisotropy: mild additional restoring force along z only,
 *       representing an asymmetric confining potential (e.g. a capillary
 *       or channel with stronger lateral than axial confinement)
 *       H_axial = -mu * z * z_hat
 *
 * Parameters:
 *   lambda = 0.012  — radial centering strength (Å^2 fs^-2)
 *   r0     = 2.0    — regularisation length (Å)
 *   mu     = 0.002  — axial harmonic stiffness (fs^-2)
 *
 * Total: H = H_radial + H_axial
 */
inline atomistic::Vec3 gradient_field(const atomistic::Vec3& R) {
	constexpr double lambda = 0.012;
	constexpr double r0     = 2.0;
	constexpr double mu     = 0.002;

	double r = atomistic::norm(R);
	atomistic::Vec3 H_radial{};
	if (r > 1e-10) {
		double scale = -lambda / ((r + r0) * (r + r0) * r);
		H_radial = R * scale;
	}
	// Axial harmonic restoring force along z
	atomistic::Vec3 H_axial{0.0, 0.0, -mu * R.z};

	return H_radial + H_axial;
}

// ============================================================================
// Transport Acceleration
// ============================================================================

/**
 * Result of transport acceleration decomposition.
 */
struct TransportResult {
	atomistic::Vec3 a_drag{};
	atomistic::Vec3 a_body{};
	atomistic::Vec3 a_grad{};
	atomistic::Vec3 a_total{};
};

/**
 * Compute the transport acceleration for a single bead:
 *
 *   a = A * (u(R) - v) + B * G(R) + C * H(R)
 *
 * This is the transport shell only.  Environment-dependent kernel
 * forces enter separately through the interaction engine.
 * Returns decomposed components for diagnostics.
 */
inline TransportResult compute_transport_acceleration(
	const BeadDynState& bead,
	const TransportParams& tp)
{
	auto u = ambient_velocity(bead.position);
	auto G = body_force_field(bead.position);
	auto H = gradient_field(bead.position);

	TransportResult r;
	r.a_drag  = (u - bead.velocity) * tp.A;
	r.a_body  = G * tp.B;
	r.a_grad  = H * tp.C_grad;
	r.a_total = r.a_drag + r.a_body + r.a_grad;
	return r;
}

// ============================================================================
// Kernel-Modulated Pairwise Force  (delegates to lj_bead_helper)
// ============================================================================

/**
 * Thin wrapper around coarse_grain::compute_lj_bead_force.
 * Returns the force vector on bead i due to bead j with
 * environment-modulated steric / dispersion channels.
 */
inline atomistic::Vec3 compute_pairwise_force(
	const BeadDynState& bi,
	const BeadDynState& bj,
	const coarse_grain::EnvironmentParams& env_params,
	const coarse_grain::LJBeadParams& lj = {})
{
	auto res = coarse_grain::compute_lj_bead_force(
		bi.position, bj.position,
		bi.env.eta, bj.env.eta,
		env_params, lj);
	return res.force;
}

// ============================================================================
// Step Logger
// ============================================================================

/**
 * StepLog — CSV logger for per-step diagnostics.
 *
 * Columns: step, bead_id, x, y, z, vx, vy, vz,
 *          rho, C, P2, eta, target_f, g_steric, g_elec, g_disp,
 *          Fx_drag, Fy_drag, Fz_drag, Fx_body, Fy_body, Fz_body
 */
class StepLog {
	std::ofstream out_;
public:
	explicit StepLog(const std::string& path) : out_(path) {
		if (out_.is_open()) {
			out_ << "step,bead_id,x,y,z,vx,vy,vz,"
				 << "rho,C,P2,rho_hat,P2_hat,eta,target_f,"
				 << "g_steric,g_elec,g_disp,"
				 << "ax_drag,ay_drag,az_drag,"
				 << "ax_body,ay_body,az_body,"
				 << "ax_grad,ay_grad,az_grad\n";
		}
	}

	void log(int step, int bead_id, const BeadDynState& b) {
		if (!out_.is_open()) return;
		char buf[512];
		std::snprintf(buf, sizeof(buf),
			"%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,"
			"%.6f,%.4f,%.6f,%.6f,%.6f,%.6f,%.6f,"
			"%.6f,%.6f,%.6f,"
			"%.8f,%.8f,%.8f,"
			"%.8f,%.8f,%.8f,"
			"%.8f,%.8f,%.8f\n",
			step, bead_id,
			b.position.x, b.position.y, b.position.z,
			b.velocity.x, b.velocity.y, b.velocity.z,
			b.env.rho, b.env.C, b.env.P2,
			b.env.rho_hat, b.env.P2_hat,
			b.env.eta, b.env.target_f,
			b.mod_report.g_steric, b.mod_report.g_electrostatic,
			b.mod_report.g_dispersion,
			b.a_drag.x, b.a_drag.y, b.a_drag.z,
			b.a_body.x, b.a_body.y, b.a_body.z,
			b.a_grad.x, b.a_grad.y, b.a_grad.z);
		out_ << buf;
	}

	void flush() { if (out_.is_open()) out_.flush(); }
};

// ============================================================================
// Invariant Checker
// ============================================================================

/**
 * Check all ERB invariants for a set of beads. Returns number of violations.
 */
inline int check_invariants(
	const std::vector<BeadDynState>& beads,
	const coarse_grain::EnvironmentParams& params,
	int step,
	bool verbose = false)
{
	int violations = 0;
	for (int i = 0; i < static_cast<int>(beads.size()); ++i) {
		const auto& b = beads[i];

		// eta in [0, 1]
		if (b.env.eta < 0.0 || b.env.eta > 1.0) {
			if (verbose)
				std::printf("  [INVARIANT] step %d bead %d: eta=%.6f out of [0,1]\n",
							step, i, b.env.eta);
			++violations;
		}

		// Kernel sign invariant: |1 + gamma_k * eta_bar| > 0
		// Check against self (worst case eta_bar = eta)
		double eta_bar = b.env.eta;
		if (1.0 + params.gamma_steric * eta_bar <= 0.0) {
			if (verbose)
				std::printf("  [INVARIANT] step %d bead %d: steric kernel sign violation\n",
							step, i);
			++violations;
		}
		if (1.0 + params.gamma_elec * eta_bar <= 0.0) {
			if (verbose)
				std::printf("  [INVARIANT] step %d bead %d: elec kernel sign violation\n",
							step, i);
			++violations;
		}
		if (1.0 + params.gamma_disp * eta_bar <= 0.0) {
			if (verbose)
				std::printf("  [INVARIANT] step %d bead %d: disp kernel sign violation\n",
							step, i);
			++violations;
		}
	}
	return violations;
}

// ============================================================================
// Full Step — Document-Compatible Update Order
// ============================================================================

/**
 * Execute one full dynamics step for the entire bead ensemble.
 *
 * Update order (ERB §, mandatory, invariant chain):
 *   1. Integrate poses (velocity Verlet half-kick + drift)
 *   2. Compute fast observables (rho, C, P2) from current positions
 *   3. Update slow state (eta) from target function
 *   4. Evaluate modulated kernels / effective coefficients
 *   5. Assemble acceleration (transport + pairwise)
 *   6. Velocity Verlet half-kick (complete velocity update)
 *   7. Log diagnostics
 *
 * @param beads          Mutable bead array (updated in-place)
 * @param env_params     Environment parameters (alpha, beta, tau, gamma_k)
 * @param tp             Transport shell parameters (A, B, C)
 * @param dt             Timestep (fs)
 * @param step           Current step number (for logging)
 * @param logger         Optional CSV logger (nullptr to skip)
 */
inline void full_step(
	std::vector<BeadDynState>& beads,
	const coarse_grain::EnvironmentParams& env_params,
	const TransportParams& tp,
	double dt,
	int step,
	StepLog* logger)
{
	int n = static_cast<int>(beads.size());

	// ── Step 1: Velocity Verlet half-kick + drift ──────────────────────
	for (auto& b : beads) {
		// Half-kick: v += 0.5 * dt * a
		b.velocity = b.velocity + b.acceleration * (0.5 * dt);
		// Drift: R += dt * v
		b.position = b.position + b.velocity * dt;
	}

	// ── Step 2: Compute fast observables ───────────────────────────────
	// Build neighbour lists from current positions
	// (using scene_builders pattern: all pairs, cutoff in kernel)
	for (int i = 0; i < n; ++i) {
		std::vector<coarse_grain::NeighbourInfo> nbs;
		nbs.reserve(n - 1);
		for (int j = 0; j < n; ++j) {
			if (j == i) continue;
			coarse_grain::NeighbourInfo nb;
			nb.distance = atomistic::norm(beads[j].position - beads[i].position);
			nb.n_hat = beads[j].n_hat;
			nb.has_orientation = beads[j].has_orientation;
			nbs.push_back(nb);
		}

		// Step 2 + 3: fast observables AND eta update in one call
		// (update_environment_state does both in the correct order internally)
		double prev_eta = beads[i].env.eta;
		beads[i].env = coarse_grain::update_environment_state(
			prev_eta, beads[i].n_hat, beads[i].has_orientation,
			nbs, env_params, dt);
	}

	// ── Step 4: Evaluate modulated kernels (diagnostic) ───────────────
	for (int i = 0; i < n; ++i) {
		// Use self-eta for the modulation report (worst-case diagnostic)
		beads[i].mod_report = coarse_grain::compute_modulation_report(
			beads[i].env.eta, beads[i].env.eta, env_params);
	}

	// ── Step 5: Assemble acceleration ─────────────────────────────────
	for (int i = 0; i < n; ++i) {
		// Transport shell (returns decomposed acceleration)
		auto tr = compute_transport_acceleration(beads[i], tp);
		beads[i].a_drag = tr.a_drag;
		beads[i].a_body = tr.a_body;
		beads[i].a_grad = tr.a_grad;

		// Pairwise forces (kernel-modulated) → convert to acceleration via mass
		atomistic::Vec3 F_pair{};
		for (int j = 0; j < n; ++j) {
			if (j == i) continue;
			auto fij = compute_pairwise_force(beads[i], beads[j], env_params);
			F_pair = F_pair + fij;
		}
		atomistic::Vec3 a_pair = F_pair * (1.0 / beads[i].mass);

		beads[i].a_total = tr.a_total + a_pair;
		beads[i].acceleration = beads[i].a_total;
	}

	// ── Step 6: Velocity Verlet second half-kick ──────────────────────
	for (auto& b : beads) {
		b.velocity = b.velocity + b.acceleration * (0.5 * dt);
	}

	// ── Step 7: Log diagnostics ───────────────────────────────────────
	if (logger) {
		for (int i = 0; i < n; ++i) {
			logger->log(step, i, beads[i]);
		}
	}
}

} // namespace v5
