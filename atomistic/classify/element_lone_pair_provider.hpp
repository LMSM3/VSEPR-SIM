#pragma once
/**
 * element_lone_pair_provider.hpp  --  Day 84 / WO-84B
 * ============================================================================
 * Real element-based lone-pair inference for VSEPR geometry.
 *
 * The VSEPR classifier already carries a small built-in element heuristic
 * (infer_lone_pairs_from_element in vsepr.cpp), but it deliberately returns
 * nullopt for the heavy noble gases (Xe, Kr, Rn) and the transition metals,
 * so those centres collapse onto geometry-only fallback.  This provider fills
 * that gap and is intended to be attached to VSEPROptions::providers.lone_pair
 * so it OVERRIDES the built-in heuristic wherever it has an answer.
 *
 * Coverage:
 *   - Main-group:  H, B, C, N, O, S, P, F, Cl, Br, I
 *   - Noble gas:   He, Ne, Ar, Kr, Xe, Rn  (octet expansion aware)
 *   - Transition metals: conservative fallback (0 stereochemically-active LP)
 *
 * Geometry targets this unlocks (via molecular_shape_from_AXE):
 *   H2O -> bent          NH3 -> trigonal pyramidal   CH4 -> tetrahedral
 *   CO2 -> linear        XeF2 -> linear (AX2E3)       XeF4 -> square planar
 *   XeF6 -> expanded / heavy-noble-gas warning        KrF2 -> linear
 *
 * The provider is bound to a specific State (it needs per-atom bonded-domain
 * counts).  bind() returns a LonePairProvider whose functor closes over the
 * precomputed per-atom table, so it plugs directly into ProviderSet.
 */

#include "atomistic/classify/providers.hpp"

#include <cstddef>
#include <optional>

namespace atomistic {

struct State;

namespace classify {

class ElementLonePairProvider {
public:
	// Neighbor cutoff used when a State has no explicit bond list.  Matches
	// VSEPROptions::neighbor_cutoff_angstrom so provider and classifier agree.
	static constexpr double kDefaultCutoffAngstrom = 3.5;

	ElementLonePairProvider() = default;
	explicit ElementLonePairProvider(double neighbor_cutoff_angstrom)
		: cutoff_(neighbor_cutoff_angstrom) {}

	// Pure element chemistry: lone-pair domain count for a centre of atomic
	// number Z with the given number of bonded domains (sigma neighbours).
	// Returns nullopt for elements this provider does not cover, so callers
	// can defer to other fallbacks deterministically.
	static std::optional<int> lone_pairs_for(int z, std::size_t bonded_domains);

	// True when Z is one of the noble gases (He, Ne, Ar, Kr, Xe, Rn).
	static bool is_noble_gas(int z);

	// True when the (Z, bonded_domains) combination is a heavy noble gas case
	// whose electron domain count exceeds 6 (e.g. XeF6 -> AX6E1, 7 domains).
	// These need the expanded-coordination / warning path, not plain octahedral.
	static bool is_heavy_noble_gas_warning(int z, std::size_t bonded_domains);

	// Bind to a State: precompute per-atom lone-pair counts and return a
	// LonePairProvider ready to drop into ProviderSet::lone_pair.
	LonePairProvider bind(const State& state) const;

private:
	double cutoff_ = kDefaultCutoffAngstrom;
};

} // namespace classify
} // namespace atomistic
