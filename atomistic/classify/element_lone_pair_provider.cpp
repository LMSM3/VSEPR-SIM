/**
 * element_lone_pair_provider.cpp  --  Day 84 / WO-84B
 * ============================================================================
 * Implementation of the element-based lone-pair provider.  See the header for
 * design rationale and geometry targets.
 *
 * The core chemistry is deliberately simple and deterministic:
 *   - Main-group elements use the same valence heuristics as the built-in
 *     VSEPR element table, so behaviour is consistent when both are active.
 *   - Heavy noble gases (Kr, Xe, Rn) use valence-electron counting with octet
 *     expansion:  lone_pairs = (8 - bonded_domains) / 2.
 *   - Transition metals return a conservative 0 stereochemically-active lone
 *     pairs, letting bonded-domain geometry (e.g. octahedral) dominate.
 * ============================================================================
 */

#include "atomistic/classify/element_lone_pair_provider.hpp"
#include "atomistic/core/state.hpp"

#include <algorithm>
#include <vector>

namespace atomistic {
namespace classify {

namespace {

// Transition-metal / lanthanide / actinide test (conservative fallback group).
static bool is_transition_metal(int z) {
	return (z >= 21 && z <= 30) ||   // Sc - Zn
		   (z >= 39 && z <= 48) ||   // Y  - Cd
		   (z >= 57 && z <= 80) ||   // La - Hg (incl. lanthanides)
		   (z >= 89 && z <= 112);    // Ac - Cn (incl. actinides)
}

// Noble-gas valence electron count (He = 2, all heavier = 8).
static int noble_gas_valence(int z) {
	return (z == 2) ? 2 : 8;
}

// Build per-atom bonded-domain counts.  Mirrors vsepr.cpp: use explicit bonds
// when present, otherwise a distance cutoff.  Only the DEGREE is needed here.
static std::vector<std::size_t> bonded_domain_counts(const State& state, double cutoff) {
	std::vector<std::size_t> degree(state.N, 0);
	if (state.N == 0) return degree;

	if (!state.B.empty()) {
		for (const Edge& e : state.B) {
			if (e.i >= state.N || e.j >= state.N || e.i == e.j) continue;
			++degree[e.i];
			++degree[e.j];
		}
		return degree;
	}

	if (state.X.size() < state.N) return degree;
	const double cutoff_sq = cutoff * cutoff;
	for (std::size_t i = 0; i < state.N; ++i) {
		for (std::size_t j = i + 1; j < state.N; ++j) {
			Vec3 dr = state.X[j] - state.X[i];
			if (state.box.enabled) {
				dr = state.box.delta(state.X[i], state.X[j]);
			}
			if (dot(dr, dr) <= cutoff_sq) {
				++degree[i];
				++degree[j];
			}
		}
	}
	return degree;
}

} // namespace

bool ElementLonePairProvider::is_noble_gas(int z) {
	return z == 2 || z == 10 || z == 18 || z == 36 || z == 54 || z == 86;
}

bool ElementLonePairProvider::is_heavy_noble_gas_warning(int z, std::size_t bonded_domains) {
	// Heavy noble gases whose total electron domain count exceeds 6.
	const bool heavy = (z == 36 || z == 54 || z == 86);
	if (!heavy) return false;
	const int valence = noble_gas_valence(z);
	int remaining = valence - static_cast<int>(bonded_domains);
	if (remaining < 0) remaining = 0;
	const std::size_t lone_pairs = static_cast<std::size_t>(remaining / 2);
	return (bonded_domains + lone_pairs) > 6;  // e.g. XeF6 -> AX6E1 = 7 domains
}

std::optional<int>
ElementLonePairProvider::lone_pairs_for(int z, std::size_t bonded_domains) {
	switch (z) {
		case 1:                 // H
			return 0;
		case 5:                 // B
		case 6:                 // C
			return 0;
		case 7:                 // N
			if (bonded_domains <= 3) return 1;
			if (bonded_domains == 4) return 0;
			return std::nullopt;
		case 8:                 // O
			if (bonded_domains <= 2) return 2;
			if (bonded_domains == 3) return 1;
			if (bonded_domains == 4) return 0;
			return std::nullopt;
		case 9:                 // F
		case 17:                // Cl
		case 35:                // Br
		case 53:                // I
			if (bonded_domains <= 1) return 3;
			if (bonded_domains == 2) return 2;
			if (bonded_domains == 3 || bonded_domains == 4) return 1;
			return 0;
		case 15:                // P
			if (bonded_domains >= 5) return 0;
			if (bonded_domains == 3 || bonded_domains == 4) return 1;
			if (bonded_domains == 2) return 2;
			return std::nullopt;
		case 16:                // S
			if (bonded_domains >= 6) return 0;
			if (bonded_domains == 4 || bonded_domains == 5) return 1;
			if (bonded_domains == 2 || bonded_domains == 3) return 2;
			return std::nullopt;
		default:
			break;
	}

	// Noble gases (WO-84B special handling for Xe, Kr, Rn).
	if (is_noble_gas(z)) {
		const int valence = noble_gas_valence(z);
		if (bonded_domains == 0) {
			// Isolated atom: shape is Atom regardless of lone pairs, but
			// report the full valence shell for completeness.
			return valence / 2;
		}
		const bool heavy = (z == 36 || z == 54 || z == 86);
		if (!heavy) {
			// He/Ne/Ar cannot expand the octet; a bonded light noble gas is
			// outside the supported model -> conservative 0.
			return 0;
		}
		int remaining = valence - static_cast<int>(bonded_domains);
		if (remaining < 0) remaining = 0;
		return remaining / 2;   // XeF2->3, XeF4->2, XeF6->1
	}

	// Transition metals / lanthanides / actinides: conservative fallback with
	// no stereochemically-active lone pairs, so bonded geometry dominates.
	if (is_transition_metal(z)) {
		return 0;
	}

	// Unsupported element: defer to other fallbacks.
	return std::nullopt;
}

LonePairProvider ElementLonePairProvider::bind(const State& state) const {
	// Precompute lone-pair counts once so the returned functor is O(1) per atom
	// and does not capture the State by reference beyond this call's contract.
	const auto degree = bonded_domain_counts(state, cutoff_);
	std::vector<std::optional<int>> table(state.N);
	for (std::size_t i = 0; i < state.N; ++i) {
		const int z = (i < state.type.size())
			? static_cast<int>(state.type[i])
			: 0;
		table[i] = lone_pairs_for(z, degree[i]);
	}

	LonePairProvider provider;
	provider.fn = [table = std::move(table)](std::size_t i) -> std::optional<int> {
		if (i >= table.size()) return std::nullopt;
		return table[i];
	};
	return provider;
}

} // namespace classify
} // namespace atomistic
