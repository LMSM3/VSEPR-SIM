#pragma once
/**
 * providers.hpp  --  Chemistry metadata provider interfaces for the classify module
 *
 * WO-83F: BondOrderProvider
 * WO-83G: LonePairProvider
 * WO-83H: FormalChargeProvider
 * WO-83J: RingProvider
 *
 * All interfaces use a lightweight callback/functor pattern.
 * If a provider is absent (nullptr / not set), classifiers fall back to
 * geometry-only or element-based heuristics.
 *
 * These are stubs.  Implementations live outside the classify module.
 * The classify module MUST NOT depend on any heavy chemistry database.
 *
 * Design rule: every provider returns an optional<T> or a sentinel
 * (e.g. -1 for unknown int, 0.0 for unknown double) so callers can
 * distinguish "provided zero" from "not available".
 */

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace atomistic {
namespace classify {

// ============================================================================
// WO-83F: BondOrderProvider
//
// Returns the bond order between atom i and atom j, or nullopt if unknown.
// Convention: 1.0 = single, 2.0 = double, 3.0 = triple, 1.5 = aromatic.
// ============================================================================

struct BondOrderProvider {
	// Functional form: (atom_i, atom_j) -> bond_order or nullopt
	using Fn = std::function<std::optional<double>(std::size_t, std::size_t)>;

	Fn fn;

	// Convenience: query bond order, returns nullopt if provider absent
	std::optional<double> get(std::size_t i, std::size_t j) const {
		if (!fn) return std::nullopt;
		return fn(i, j);
	}

	// Returns true if this provider has been set
	bool available() const { return static_cast<bool>(fn); }

	// Null provider — always returns nullopt
	static BondOrderProvider null() { return {}; }
};

// ============================================================================
// WO-83G: LonePairProvider
//
// Returns the lone-pair domain count for atom i, or nullopt if unknown.
// This supplements element-based inference in VSEPROptions.
// ============================================================================

struct LonePairProvider {
	using Fn = std::function<std::optional<int>(std::size_t)>;

	Fn fn;

	std::optional<int> get(std::size_t i) const {
		if (!fn) return std::nullopt;
		return fn(i);
	}

	bool available() const { return static_cast<bool>(fn); }

	static LonePairProvider null() { return {}; }
};

// ============================================================================
// WO-83H: FormalChargeProvider
//
// Returns the formal charge (in units of e) for atom i, or nullopt.
// Absent charge is treated as neutral by classifiers, not as guessed zero.
// ============================================================================

struct FormalChargeProvider {
	using Fn = std::function<std::optional<int>(std::size_t)>;

	Fn fn;

	std::optional<int> get(std::size_t i) const {
		if (!fn) return std::nullopt;
		return fn(i);
	}

	bool available() const { return static_cast<bool>(fn); }

	static FormalChargeProvider null() { return {}; }
};

// ============================================================================
// WO-83J: RingProvider
//
// Returns ring membership data for atoms.
// ring_sizes_for(i)  -> sorted list of ring sizes atom i participates in
// ring_count()       -> total distinct ring count (SSSR), or nullopt
//
// Ring detection is NOT owned by VSEPR or OrganicCandidate.
// This interface is the only allowed ring data inlet into classify.
// ============================================================================

struct RingProvider {
	using RingSizesFn  = std::function<std::vector<int>(std::size_t)>;
	using RingCountFn  = std::function<std::optional<int>()>;

	RingSizesFn  ring_sizes_for_atom;  // per-atom ring membership
	RingCountFn  total_ring_count;     // total SSSR ring count

	std::vector<int> ring_sizes(std::size_t i) const {
		if (!ring_sizes_for_atom) return {};
		return ring_sizes_for_atom(i);
	}

	std::optional<int> total_rings() const {
		if (!total_ring_count) return std::nullopt;
		return total_ring_count();
	}

	// True if atom i is in any ring
	bool in_ring(std::size_t i) const {
		return !ring_sizes(i).empty();
	}

	// True if atom i is in a strained ring (size <= 4)
	bool in_strained_ring(std::size_t i) const {
		for (int s : ring_sizes(i)) {
			if (s <= 4) return true;
		}
		return false;
	}

	bool available() const {
		return static_cast<bool>(ring_sizes_for_atom);
	}

	static RingProvider null() { return {}; }
};

// ============================================================================
// ProviderSet  --  convenience aggregate
//
// Pass a ProviderSet to classifiers that accept multiple provider types.
// All providers default to null (not available).
// ============================================================================

struct ProviderSet {
	BondOrderProvider    bond_order;
	LonePairProvider     lone_pair;
	FormalChargeProvider formal_charge;
	RingProvider         ring;

	// Null provider set — all absent, all fallbacks active
	static ProviderSet null() { return {}; }
};

} // namespace classify
} // namespace atomistic
