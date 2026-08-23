#pragma once
/**
 * particle_identity.hpp  -  ParticleIdentity: recoverable identity-information state
 *
 * Represents a particle's full identity bundle for the SM-channel interaction layer:
 *   - Physical quantum numbers: charge, mass, spin_proxy
 *   - Color/anticolor codes (SU(3) analogue)
 *   - Lepton family and baryon number proxies
 *   - Z2  : 2×2 state matrix (runtime, updated each interaction step)
 *   - Z3  : 3×3 fundamental identity matrix (generation/flavor space, slower to change)
 *   - I_recoverable : scalar recoverable identity-information quantity
 *   - identity_hash : FNV-1a 64-bit hash over quantized quantum-number fields
 *                     (excludes counters and continuous trajectory data)
 *
 * Design rules:
 *   - Plain aggregate — no virtual dispatch, trivially-copyable payload.
 *   - Z2 and Z3 are the particle's "field configuration".
 *     The evaluate_pair() function reads them; the integrator writes them.
 *   - I_recoverable tracks the information available to reconstruct this
 *     particle's identity after an interaction.  Loss => state_loss; gain =>
 *     positive_information_event.
 *
 * Relationship to existing PARTICLES/ types:
 *   ParticleIdentity is NOT a replacement for QCDParticle or TransientParticle.
 *   It is an SM-channel annotation layer.  The recommended pattern is to
 *   maintain a parallel std::vector<ParticleIdentity> aligned by index to the
 *   existing particle containers, or to embed ParticleIdentity as a member.
 *
 * IKK grounding (IKK Doc I §2, Doc II §B–D):
 *   - scale_mask   : bitmask of IKK scale levels active for this particle
 *                    bit 0 = S0 (existence), bit 2 = S2 (color/gluon),
 *                    bit 3 = S3 (EM/spatial), bit 4 = S4 (weak/time-ordered)
 *   - r_fuzz       : fuzz proxy — spread of identity anchor at S3 spatial scale
 *                    (dimensionless analog of r_f / ell_S3 from IKK Doc I §2.3)
 *   - H_charge     : hidden charge intensity  H = c_j^T W c_j  (IKK Doc II D.3)
 *                    For a quark triplet: H_charge = (2/3)^2 + (1/3)^2 + (1/3)^2 = 2/3
 *                    Net-zero charge (neutron-like) can still have H_charge > 0.
 *   - H_color      : hidden color intensity (analogous, over color components)
 *   - H_spin       : hidden spin intensity (over spin component vector)
 *
 * v5.2.0  |  WO-SM-IDENTITY  |  v5.0.0-main
 */

#include "IDENTITY/identity_matrix.hpp"
#include "PARTICLES/qcd_particle.hpp"   // ColorCharge, QuarkFlavor

#include <cstdint>
#include <cstring>

namespace vsepr {
namespace identity {

// ============================================================================
// Spin proxy enumeration  (coarse — not a full spinor)
// ============================================================================

enum class SpinProxy : int8_t {
	Half    = 0,   // fermion-like (electron, quark)
	Integer = 1,   // boson-like  (photon, W, Z, Higgs analogue)
	Zero    = 2,   // scalar
};

// ============================================================================
// ParticleIdentity
// ============================================================================

struct ParticleIdentity {
	// --- core identifiers ---------------------------------------------------
	int64_t  id          {0};
	int32_t  type_code   {0};   // matches TransientTypeCode / QuarkFlavor int value

	// --- quantum numbers -----------------------------------------------------
	double   charge      {0.0};  // electric charge in units of e
	double   mass        {0.0};  // rest mass (VSIM units — same as QCDParticle)
	SpinProxy spin_proxy {SpinProxy::Half};

	// --- color sector -------------------------------------------------------
	particles::ColorCharge color      {particles::ColorCharge::Neutral};
	particles::ColorCharge anti_color {particles::ColorCharge::Neutral};

	// --- generation / flavor bookkeeping ------------------------------------
	int8_t   lepton_family    {0};   //  0=none, 1=electron, 2=muon, 3=tau
	int8_t   baryon_number    {0};   // +1=baryon, -1=antibaryon, 0=none
	int8_t   isospin_proxy    {0};   // +1=up-type, -1=down-type, 0=none

	// --- Z-matrices (field configuration) -----------------------------------
	Matrix2x2 Z2;   // 2×2 runtime state matrix
	Matrix3x3 Z3;   // 3×3 fundamental identity matrix (generation space)

	// --- recoverable identity-information -----------------------------------
	double   I_recoverable {1.0};   // 1.0 = fully intact, 0.0 = fully lost

	// --- IKK scale layer (IKK Doc I §2, Doc II §B) ---------------------------
	// Bitmask: bit n = scale S_n active for this particle
	//   S0=existence(1), S1=vector(2), S2=color/gluon(4),
	//   S3=EM/spatial(8), S4=weak/time(16), S5=curvature(32), SD=dark(64)
	uint8_t  scale_mask    {0b00001101};  // default: S0+S2+S3 (quark-colored bead)

	// Fuzz proxy r_fuzz: dimensionless spread at S3 — caller updates each step
	// Corresponds to Phi_S = r_f / ell_S from IKK Doc I §2.3
	double   r_fuzz        {0.0};

	// Hidden intensities H_j = c_j^T W c_j  (IKK Doc II §D.3)
	// Diagonal W = I assumed (identity weight matrix) for each channel.
	// For charge: H_charge = sum_k q_k^2 over constituent sub-charges
	// For composite particles built from n quarks, populate before evaluate_pair().
	double   H_charge      {0.0};   // hidden charge intensity
	double   H_color       {0.0};   // hidden color intensity
	double   H_spin        {0.0};   // hidden spin intensity

	// --- persistent hash (over quantized quantum numbers only) --------------
	uint64_t identity_hash {0};

	// --- birth-time fields (WO-66K) ------------------------------------------
	// birth_hash: h_p = H(id, f, g, Q, B, L, J, m, τ, ξ_p) at spawn moment.
	// Set once by ParticleIdentitySeeder::build(); never updated after birth.
	// Distinct from identity_hash which refreshes on quantum-number mutations.
	uint64_t birth_hash    {0};

	// Snapped birth position (ξ_p at t=0, stored for lifecycle tracing)
	double   birth_x       {0.0};
	double   birth_y       {0.0};
	double   birth_z       {0.0};

	// Refresh the FNV-1a 64-bit hash from the stable quantum-number fields.
	// Call after construction and after any quantum-number mutation event.
	void refresh_hash() noexcept {
		// FNV-1a 64-bit
		constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
		constexpr uint64_t FNV_PRIME  = 1099511628211ULL;
		uint64_t h = FNV_OFFSET;

		auto mix_bytes = [&](const void* data, std::size_t n) noexcept {
			const auto* p = reinterpret_cast<const uint8_t*>(data);
			for (std::size_t i = 0; i < n; ++i) {
				h ^= static_cast<uint64_t>(p[i]);
				h *= FNV_PRIME;
			}
		};

		mix_bytes(&id,            sizeof(id));
		mix_bytes(&type_code,     sizeof(type_code));
		mix_bytes(&charge,        sizeof(charge));
		mix_bytes(&mass,          sizeof(mass));
		mix_bytes(&spin_proxy,    sizeof(spin_proxy));
		mix_bytes(&color,         sizeof(color));
		mix_bytes(&anti_color,    sizeof(anti_color));
		mix_bytes(&lepton_family, sizeof(lepton_family));
		mix_bytes(&baryon_number, sizeof(baryon_number));
		mix_bytes(&isospin_proxy, sizeof(isospin_proxy));

		identity_hash = h;
	}

	// -------------------------------------------------------------------------
	// compute_hidden_intensity()
	//
	// Populates H_charge, H_color, H_spin from the particle's own quantum numbers.
	//
	// For elementary particles (electron, positron, full quark) the sub-charge
	// vector has one entry — the particle itself.  For composite particles
	// (e.g. a proton built from u+u+d) the caller should sum constituent squares
	// directly.  This convenience version handles the single-constituent case and
	// the quark-triplet case derived from the particle's baryon_number bookkeeping.
	//
	// Formula:  H_j = c_j^T W c_j,  W = diag(1,1,...,1)  (IKK Doc II §D.3)
	// -------------------------------------------------------------------------
	void compute_hidden_intensity() noexcept {
		// --- H_charge ---
		// Single-particle: H = q^2
		// Baryon (quark triplet proxy via isospin): up charge = +2/3, two downs = -1/3
		if (baryon_number != 0) {
			// Proxy composition: isospin +1 => (u,u,d), isospin -1 => (u,d,d)
			double qu = (isospin_proxy >= 0) ? (+2.0/3.0) : (+2.0/3.0);
			double qd = -1.0/3.0;
			int n_up   = (isospin_proxy >= 0) ? 2 : 1;
			int n_down = 3 - n_up;
			H_charge = n_up   * (qu * qu) + n_down * (qd * qd);
		} else {
			H_charge = charge * charge;
		}

		// --- H_color ---
		// Color-charged: H_color = 1 per active color component.
		// Neutral / white: H_color = 0.
		bool is_colored = (color != particles::ColorCharge::Neutral &&
						   color != particles::ColorCharge::White);
		H_color = is_colored ? 1.0 : 0.0;

		// --- H_spin ---
		// Half-integer fermions: spin component = 1/2 => H = (1/2)^2 = 0.25
		// Integer bosons: H = 1^2 = 1.0
		// Scalar (Zero): H = 0
		switch (spin_proxy) {
			case SpinProxy::Half:    H_spin = 0.25; break;
			case SpinProxy::Integer: H_spin = 1.00; break;
			case SpinProxy::Zero:    H_spin = 0.00; break;
		}
	}

	// Returns true when this particle has net-zero observable charge but
	// nonzero hidden charge intensity (neutron-like; IKK Doc II §D.4).
	bool hidden_active_charge(double tol = 1e-9) const noexcept {
		return (std::abs(charge) < tol) && (H_charge > tol);
	}

	// -------------------------------------------------------------------------
	// from_birth()  (WO-66K)
	//
	// Factory: fill this identity from a pre-computed BirthRecord.
	// Declared here; implementation calls ParticleIdentitySeeder::build()
	// by delegating to a free function defined in particle_identity_seeder.hpp.
	// This header does NOT include the seeder to avoid circular dependency —
	// callers that need the full factory should include particle_identity_seeder.hpp
	// and call ParticleIdentitySeeder::build(record) directly.
	//
	// This thin overload supports cases where a BirthRecord-like struct is
	// already available and only basic fields need copying.
	// -------------------------------------------------------------------------
	void apply_birth_record(uint64_t bh, double bx, double by, double bz) noexcept {
		birth_hash = bh;
		birth_x    = bx;
		birth_y    = by;
		birth_z    = bz;
	}
};

// ============================================================================
// Convenience constructors for common particle archetypes
// ============================================================================

// Electron-like: charge -1, spin 1/2, lepton family 1, neutral color
inline ParticleIdentity make_electron_like(int64_t id, double mass = 0.000511) {
	ParticleIdentity p;
	p.id           = id;
	p.type_code    = -1;
	p.charge       = -1.0;
	p.mass         = mass;
	p.spin_proxy   = SpinProxy::Half;
	p.lepton_family = 1;
	p.scale_mask   = 0b00011001;  // S0(1)+S3(8)+S4(16) — EM+weak active
	p.Z2           = identity_2x2();
	p.Z3           = identity_3x3();
	p.I_recoverable = 1.0;
	p.compute_hidden_intensity();
	p.refresh_hash();
	return p;
}

// Positron-like: charge +1, spin 1/2, lepton family 1, neutral color
inline ParticleIdentity make_positron_like(int64_t id, double mass = 0.000511) {
	ParticleIdentity p;
	p.id           = id;
	p.type_code    = -1;
	p.charge       = +1.0;
	p.mass         = mass;
	p.spin_proxy   = SpinProxy::Half;
	p.lepton_family = 1;
	p.Z2           = identity_2x2();
	// flip Z2 off-diagonal to signal antiparticle opposition
	p.Z2.m[0][1]   =  1.0;
	p.Z2.m[1][0]   = -1.0;
	p.Z3           = identity_3x3();
	p.scale_mask   = 0b00011001;  // S0+S3+S4
	p.I_recoverable = 1.0;
	p.compute_hidden_intensity();
	p.refresh_hash();
	return p;
}

// Quark-like: color-charged, fractional charge, baryon 1/3 proxy as +1
inline ParticleIdentity make_quark_like(int64_t id,
										particles::QuarkFlavor flavor,
										particles::ColorCharge color) {
	ParticleIdentity p;
	p.id           = id;
	p.type_code    = static_cast<int32_t>(flavor);
	p.charge       = particles::quark_electric_charge(flavor);
	p.mass         = particles::quark_rest_mass(flavor);
	p.spin_proxy   = SpinProxy::Half;
	p.color        = color;
	p.baryon_number = +1;
	p.isospin_proxy = (p.charge > 0.0) ? +1 : -1;
	p.Z2           = identity_2x2();
	p.Z3           = identity_3x3();
	p.scale_mask   = 0b00001101;  // S0(1)+S2(4)+S3(8) — color+EM active
	p.I_recoverable = 1.0;
	p.compute_hidden_intensity();
	p.refresh_hash();
	return p;
}

} // namespace identity
} // namespace vsepr
