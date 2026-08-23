#pragma once
/**
 * particle_identity_seeder.hpp
 * ============================
 * WO-66K  |  Hash-Seed Identity Assignment
 *
 * Deterministic birth-time identity seeder for VSIM particles.
 *
 * Every particle receives a unique, reproducible birth hash at spawn.
 * The hash encodes:
 *   - Simulation seed
 *   - Global birth counter (monotonic per run)
 *   - Quantum numbers: charge, mass, spin, lepton family, baryon number
 *   - Birth position (ξ_p = x,y,z,w at spawn moment)
 *   - Type code
 *
 * The resulting `birth_hash` maps to `h_p` in the v5.1.4 bridge paper (§1).
 *
 * Design rules:
 *   - Deterministic across platforms: same seed + same spawn order = same hash.
 *   - Collision-resistant at simulation scale (N ≤ 10^8 particles): FNV-1a 64-bit.
 *   - Position is snapped to integer grid (10^-6 precision) before hashing
 *     so floating-point variance at spawn does not break reproducibility.
 *   - The seeder is NOT a global singleton.  One seeder lives per
 *     simulation run or per formation kernel.  Thread-safety = caller's
 *     responsibility.
 *
 * Usage:
 *   ParticleIdentitySeeder seeder(run_seed);
 *   auto id = seeder.spawn(type_code, charge, mass, spin, lf, bn, x, y, z);
 *   // id.birth_hash is stable across replays with same seed
 *
 * The returned BirthRecord can be handed directly to ParticleIdentity::from_birth().
 *
 * v5.1.4  |  WO-66K  |  v5.0.0-main
 */

#include "particle_identity.hpp"
#include "../vsim/seed256.hpp"

#include <cstdint>
#include <cmath>
#include <string>

namespace vsepr {
namespace identity {

// ============================================================================
// BirthRecord — result of a single spawn call
// ============================================================================

struct BirthRecord {
	// Deterministic birth hash  (h_p in bridge paper §1)
	uint64_t birth_hash   {0};

	// Monotonic birth index within this run (0-based)
	uint64_t birth_index  {0};

	// Birth position (snapped; stored for lifecycle tracing)
	double birth_x {0.0};
	double birth_y {0.0};
	double birth_z {0.0};
	double birth_w {0.0};   // proper-time / S4 extension at birth (usually 0)

	// Mirror of the quantum numbers that were hashed
	int32_t  type_code     {0};
	double   charge        {0.0};
	double   mass          {0.0};
	SpinProxy spin_proxy   {SpinProxy::Half};
	int8_t   lepton_family {0};
	int8_t   baryon_number {0};
	int8_t   isospin_proxy {0};
};

// ============================================================================
// ParticleIdentitySeeder — one per simulation run
// ============================================================================

class ParticleIdentitySeeder {
public:

	// -----------------------------------------------------------------------
	// Construction — Dual-Seed Assignment (Kernel Audit WO-66K-AUDIT)
	//
	//   run_seed    — top-level per-run seed (from [system] seed = N).
	//                 Controls particle-instance identity hash (Pass 1).
	//
	//   world_seed  — simulation-wide 256-bit constant (from [seed] world_seed).
	//                 When nonzero, a second FNV-1a pass mixes the world constant
	//                 into the birth hash (Pass 2). This enables deterministic
	//                 uncertainty resolution for atomic bonding process evolution.
	//                 If world_seed.is_set() == false, single-seed mode only.
	//
	//   start_index — if replaying from a checkpoint, resume counter here.
	// -----------------------------------------------------------------------
	explicit ParticleIdentitySeeder(
		uint64_t run_seed,
		::vsim::Seed256 world_seed = ::vsim::Seed256{},
		uint64_t start_index = 0
	) noexcept
		: run_seed_(run_seed), world_seed_(world_seed), counter_(start_index) {}

	// Current counter (useful for checkpointing)
	uint64_t counter() const noexcept { return counter_; }

	// True when world_seed is set — dual-seed assignment is active
	bool dual_seed_mode() const noexcept { return world_seed_.is_set(); }

	// Read-only access to the current world seed
	const ::vsim::Seed256& world_seed() const noexcept { return world_seed_; }

	// -----------------------------------------------------------------------
	// spawn() — assign a deterministic birth hash for a new particle
	//
	// Parameters map to the I_p identity vector (bridge paper §1):
	//   type_code     : f  (flavor / type)
	//   charge        : Q
	//   mass          : m
	//   spin          : J  (SpinProxy enum)
	//   lepton_family : L_e encoded as family index (1=e, 2=μ, 3=τ)
	//   baryon_number : B
	//   isospin_proxy : T3 proxy (+1=up-type, -1=down-type, 0=none)
	//   x,y,z         : spatial birth position (x_p)
	//   w             : S4 extension at birth (0 for classical MD)
	// -----------------------------------------------------------------------
	BirthRecord spawn(
		int32_t   type_code,
		double    charge,
		double    mass,
		SpinProxy spin         = SpinProxy::Half,
		int8_t    lepton_family = 0,
		int8_t    baryon_number = 0,
		int8_t    isospin_proxy = 0,
		double    x = 0.0,
		double    y = 0.0,
		double    z = 0.0,
		double    w = 0.0
	) noexcept {
		BirthRecord r;
		r.birth_index   = counter_++;
		r.type_code     = type_code;
		r.charge        = charge;
		r.mass          = mass;
		r.spin_proxy    = spin;
		r.lepton_family = lepton_family;
		r.baryon_number = baryon_number;
		r.isospin_proxy = isospin_proxy;
		r.birth_x       = x;
		r.birth_y       = y;
		r.birth_z       = z;
		r.birth_w       = w;
		r.birth_hash    = compute_birth_hash(r);
		return r;
	}

	// -----------------------------------------------------------------------
	// build() — fill a ParticleIdentity from a BirthRecord
	//
	// Sets all quantum-number fields, birth_hash, birth_position, and calls
	// compute_hidden_intensity() + refresh_hash().
	// -----------------------------------------------------------------------
	static ParticleIdentity build(const BirthRecord& r) noexcept {
		ParticleIdentity p;
		p.id            = static_cast<int64_t>(r.birth_hash);
		p.type_code     = r.type_code;
		p.charge        = r.charge;
		p.mass          = r.mass;
		p.spin_proxy    = r.spin_proxy;
		p.lepton_family = r.lepton_family;
		p.baryon_number = r.baryon_number;
		p.isospin_proxy = r.isospin_proxy;
		p.birth_hash    = r.birth_hash;
		p.birth_x       = r.birth_x;
		p.birth_y       = r.birth_y;
		p.birth_z       = r.birth_z;
		// Z2, Z3: identity matrix at birth; caller mutates during evolution
		p.Z2 = identity_2x2();
		p.Z3 = identity_3x3();
		// Scale mask: default (S0+S2+S3); caller overrides for bosons/leptons
		p.scale_mask = 0b00001101;
		p.I_recoverable = 1.0;
		p.compute_hidden_intensity();
		p.refresh_hash();
		return p;
	}

	// -----------------------------------------------------------------------
	// spawn_and_build() — one-shot convenience
	// -----------------------------------------------------------------------
	ParticleIdentity spawn_and_build(
		int32_t   type_code,
		double    charge,
		double    mass,
		SpinProxy spin          = SpinProxy::Half,
		int8_t    lepton_family = 0,
		int8_t    baryon_number = 0,
		int8_t    isospin_proxy = 0,
		double    x = 0.0,
		double    y = 0.0,
		double    z = 0.0,
		double    w = 0.0
	) noexcept {
		return build(spawn(type_code, charge, mass, spin,
						   lepton_family, baryon_number, isospin_proxy,
						   x, y, z, w));
	}

	// -----------------------------------------------------------------------
	// anti_partner_hash()
	//
	// Given the birth hash of a particle, compute the expected birth hash
	// of its canonical anti-partner.  This allows C_id(i,j) to check
	// compatibility without storing an explicit "anti_of" pointer.
	//
	// Anti-partner is defined by:
	//   charge → -charge
	//   lepton_family → same (lepton number sign flips outside this hash)
	//   baryon_number → -baryon_number
	//   all other fields same
	//
	// Used by AnnihilationEvent::check_identity_gate().
	// -----------------------------------------------------------------------
	static uint64_t anti_partner_type_hash(
		int32_t   type_code,
		double    charge,
		double    mass,
		SpinProxy spin,
		int8_t    lepton_family,
		int8_t    baryon_number,
		int8_t    isospin_proxy
	) noexcept {
		// Flip signs that define anti-ness; do NOT include position or run seed
		// (the gate checks type compatibility, not instance identity)
		constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
		constexpr uint64_t FNV_PRIME  = 1099511628211ULL;
		uint64_t h = FNV_OFFSET;
		auto mix = [&](const void* d, std::size_t n) noexcept {
			const auto* p = reinterpret_cast<const uint8_t*>(d);
			for (std::size_t i = 0; i < n; ++i) { h ^= p[i]; h *= FNV_PRIME; }
		};
		// Anti-partner: negate charge and baryon_number
		double   anti_charge  = -charge;
		int8_t   anti_baryon  = static_cast<int8_t>(-baryon_number);
		int8_t   anti_isospin = static_cast<int8_t>(-isospin_proxy);
		mix(&type_code,     sizeof(type_code));
		mix(&anti_charge,   sizeof(anti_charge));
		mix(&mass,          sizeof(mass));
		mix(&spin,          sizeof(spin));
		mix(&lepton_family, sizeof(lepton_family));
		mix(&anti_baryon,   sizeof(anti_baryon));
		mix(&anti_isospin,  sizeof(anti_isospin));
		return h;
	}

	// Overload taking a ParticleIdentity directly
	static uint64_t anti_partner_type_hash(const ParticleIdentity& p) noexcept {
		return anti_partner_type_hash(p.type_code, p.charge, p.mass,
									  p.spin_proxy, p.lepton_family,
									  p.baryon_number, p.isospin_proxy);
	}

	// -----------------------------------------------------------------------
	// are_anti_partners()
	//
	// Returns true if A and B form a valid annihilation pair.
	// Checks type-level compatibility only — does not check distance/energy.
	// -----------------------------------------------------------------------
	static bool are_anti_partners(const ParticleIdentity& a,
								  const ParticleIdentity& b) noexcept {
		// Charges must be opposite and equal in magnitude
		constexpr double tol = 1e-9;
		if (std::abs(std::abs(a.charge) - std::abs(b.charge)) > tol) return false;
		if (std::abs(a.charge + b.charge) > tol * std::max(std::abs(a.charge), 1.0))
			return false;
		// Masses must match (same species)
		if (std::abs(a.mass - b.mass) > tol * std::max(a.mass, 1.0)) return false;
		// Spin must match
		if (a.spin_proxy != b.spin_proxy) return false;
		// Baryon numbers must be opposite
		if (a.baryon_number + b.baryon_number != 0) return false;
		// Lepton families must match (both carry same family, opposite L)
		if (a.lepton_family != b.lepton_family) return false;
		// Type codes must match (same fundamental type)
		if (a.type_code != b.type_code) return false;
		return true;
	}

private:

	uint64_t              run_seed_;
	::vsim::Seed256 world_seed_;  // zero = single-seed mode
	uint64_t              counter_;

	// -----------------------------------------------------------------------
	// compute_birth_hash()
	//
	// FNV-1a 64-bit over: run_seed, birth_index, type_code, charge (quantized),
	// mass (quantized), spin, lepton_family, baryon_number, isospin_proxy,
	// birth position (snapped to 1e-6 grid as int64).
	//
	// Position is snapped to prevent float variance from breaking replay.
	// -----------------------------------------------------------------------
	uint64_t compute_birth_hash(const BirthRecord& r) const noexcept {
		constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
		constexpr uint64_t FNV_PRIME  = 1099511628211ULL;

		// ----------------------------------------------------------------
		// Pass 1 — particle-instance context (run_seed + birth state)
		// Always executed; controls per-particle deterministic identity.
		// ----------------------------------------------------------------
		uint64_t h = FNV_OFFSET;
		auto mix = [&](const void* d, std::size_t n) noexcept {
			const auto* p = reinterpret_cast<const uint8_t*>(d);
			for (std::size_t i = 0; i < n; ++i) { h ^= p[i]; h *= FNV_PRIME; }
		};

		// Run context
		mix(&run_seed_,       sizeof(run_seed_));
		mix(&r.birth_index,   sizeof(r.birth_index));

		// Quantum numbers (stable; match bridge paper §1 h_p definition)
		mix(&r.type_code,     sizeof(r.type_code));
		mix(&r.charge,        sizeof(r.charge));
		mix(&r.mass,          sizeof(r.mass));
		mix(&r.spin_proxy,    sizeof(r.spin_proxy));
		mix(&r.lepton_family, sizeof(r.lepton_family));
		mix(&r.baryon_number, sizeof(r.baryon_number));
		mix(&r.isospin_proxy, sizeof(r.isospin_proxy));

		// Birth position — snapped to 1e-6 grid as int64 to avoid float jitter
		auto snap = [](double v) -> int64_t {
			return static_cast<int64_t>(std::round(v * 1e6));
		};
		int64_t sx = snap(r.birth_x);
		int64_t sy = snap(r.birth_y);
		int64_t sz = snap(r.birth_z);
		int64_t sw = snap(r.birth_w);
		mix(&sx, sizeof(sx));
		mix(&sy, sizeof(sy));
		mix(&sz, sizeof(sz));
		mix(&sw, sizeof(sw));

		// ----------------------------------------------------------------
		// Pass 2 — world context (world_seed)
		// Executed only when world_seed.is_set() == true (dual-seed mode).
		// Mixes the full 256-bit world constant into the running hash.
		// This encodes the simulation's environmental identity into every
		// particle — enabling deterministic bonding uncertainty resolution
		// in process evolution equations.
		// ----------------------------------------------------------------
		if (world_seed_.is_set()) {
			::vsim::fnv1a_mix_seed256(h, world_seed_);
		}

		return h;
	}
};

} // namespace identity
} // namespace vsepr
