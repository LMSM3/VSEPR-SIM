#pragma once
/**
 * bond_order_provider.hpp  --  Day 84 / WO-84D
 * ============================================================================
 * Deterministic bond-order inference for organic classification.
 *
 * The atomistic::State graph carries connectivity (Edge = {i,j}) but no bond
 * order.  This builder infers per-bond orders from element valence deficits and
 * returns a BondOrderProvider (providers.hpp) that OrganicClassifier and the
 * aromaticity gate can query.
 *
 * Method (valence-filling / greedy pi assignment):
 *   1. sigma_degree(atom)   = number of bonds (including H).
 *   2. deficit(atom)        = typical_valence(Z) - sigma_degree, clamped >= 0.
 *   3. Greedily pair adjacent atoms that both still have deficit, promoting the
 *      bond order (single -> double -> triple).  Deterministic edge ordering
 *      makes the Kekule assignment reproducible (benzene -> 3 alternating C=C).
 *
 * Convention (matches providers.hpp): 1.0 single, 2.0 double, 3.0 triple.
 * Aromatic 1.5 is NOT emitted here; aromaticity is decided by aromaticity.hpp
 * from the Kekule pattern, so this provider stays a pure sigma/pi accountant.
 */

#include "atomistic/classify/providers.hpp"

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

namespace atomistic {

struct State;

namespace classify {

// Per-bond order table plus the query provider, produced together so callers
// that need the raw pattern (e.g. aromaticity) do not re-run inference.
struct BondOrderTable {
	// Canonical (min,max) original-atom-index bond -> inferred order.
	std::map<std::pair<std::size_t, std::size_t>, double> orders;

	double order_for(std::size_t i, std::size_t j) const {
		const auto key = std::make_pair(std::min(i, j), std::max(i, j));
		auto it = orders.find(key);
		return (it == orders.end()) ? 1.0 : it->second;
	}

	bool has_double_or_higher() const {
		for (const auto& [k, v] : orders) { (void)k; if (v >= 1.5) return true; }
		return false;
	}
};

class BondOrderProviderBuilder {
public:
	BondOrderProviderBuilder() = default;

	// Infer bond orders for `state` and return a BondOrderProvider.
	BondOrderProvider build(const State& state) const;

	// Same inference, but also hand back the full table.
	BondOrderProvider build(const State& state, BondOrderTable& out_table) const;

	// Expose the raw inference for callers that only need the table.
	static BondOrderTable infer(const State& state);

	// Typical (neutral) valence used for deficit accounting.  Returns 0 for
	// elements without a simple main-group valence (e.g. transition metals),
	// which keeps their bonds at single order (conservative).
	static int typical_valence(int z);
};

} // namespace classify
} // namespace atomistic
