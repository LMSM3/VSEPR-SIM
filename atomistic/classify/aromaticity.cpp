/**
 * aromaticity.cpp  --  Day 84 / WO-84D
 * ============================================================================
 * Huckel 4n+2 aromaticity gate.  See aromaticity.hpp for the gating rules.
 *
 * Pi-electron counting (deterministic, Kekule-pattern based):
 *   - A "ring double bond" is a bond between two adjacent ring atoms whose
 *     inferred bond order is >= 1.5 (double/aromatic).
 *   - A ring shows perfect conjugated alternation when every ring atom is
 *     incident to exactly one ring double bond.  In that case each ring atom
 *     donates one pi electron, so pi_electrons == ring_size == 2 * doubles.
 *   - Huckel: aromatic iff pi_electrons == 4n+2; antiaromatic iff == 4n (n>=1).
 *   - Rings with zero ring double bonds are NonAromatic (saturated).
 *   - Rings that are conjugated but do not perfectly alternate (e.g. odd rings
 *     relying on heteroatom lone pairs) are reported Unsupported: they fail
 *     VISIBLY rather than being promoted to benzene-lite nonsense.
 * ============================================================================
 */

#include "atomistic/classify/aromaticity.hpp"
#include "atomistic/core/state.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace atomistic {
namespace classify {

const char* to_string(AromaticClass c) {
	switch (c) {
		case AromaticClass::NonAromatic:  return "non_aromatic";
		case AromaticClass::Aromatic:     return "aromatic";
		case AromaticClass::Antiaromatic: return "antiaromatic";
		case AromaticClass::Unsupported:  return "unsupported";
		default:                          return "unknown";
	}
}

bool satisfies_huckel(int pi_electrons) {
	if (pi_electrons < 2) return false;
	return ((pi_electrons - 2) % 4) == 0;   // 2, 6, 10, 14, ...
}

namespace {

// Count, per ring atom, how many ring double bonds touch it, and the total
// number of ring double bonds.
struct RingPiPattern {
	int total_doubles = 0;
	std::vector<int> incident_doubles;   // parallel to ring.atoms
};

static RingPiPattern ring_pi_pattern(const DetectedRing& ring,
									 const BondOrderTable& orders) {
	RingPiPattern p;
	const std::size_t n = ring.atoms.size();
	p.incident_doubles.assign(n, 0);
	if (n < 3) return p;

	for (std::size_t k = 0; k < n; ++k) {
		const std::size_t a = ring.atoms[k];
		const std::size_t b = ring.atoms[(k + 1) % n];
		if (orders.order_for(a, b) >= 1.5) {
			++p.total_doubles;
			p.incident_doubles[k] += 1;
			p.incident_doubles[(k + 1) % n] += 1;
		}
	}
	return p;
}

} // namespace

AromaticityReport analyze_aromaticity(const State& state,
									  const RingSystem& ring_system,
									  const BondOrderTable& bond_orders) {
	(void)state;   // reserved for future planarity checks from coordinates
	AromaticityReport report;
	report.rings.reserve(ring_system.rings.size());

	for (const DetectedRing& ring : ring_system.rings) {
		RingAromaticity ra;
		ra.atoms     = ring.atoms;
		ra.ring_size = ring.size();

		const RingPiPattern pat = ring_pi_pattern(ring, bond_orders);

		if (pat.total_doubles == 0) {
			ra.pi_electrons = 0;
			ra.all_sp2      = false;
			ra.verdict      = AromaticClass::NonAromatic;
			report.rings.push_back(std::move(ra));
			continue;
		}

		// Perfect conjugated alternation: every ring atom touches exactly one
		// ring double bond.
		const bool perfect = std::all_of(
			pat.incident_doubles.begin(), pat.incident_doubles.end(),
			[](int d) { return d == 1; });

		ra.all_sp2      = perfect;
		ra.pi_electrons = 2 * pat.total_doubles;

		if (!perfect) {
			// Conjugated but not cleanly alternating -> fail visibly.
			ra.verdict = AromaticClass::Unsupported;
			++report.unsupported_ring_count;
		} else if (satisfies_huckel(ra.pi_electrons)) {
			ra.verdict = AromaticClass::Aromatic;
			++report.aromatic_ring_count;
		} else {
			ra.verdict = AromaticClass::Antiaromatic;
			++report.antiaromatic_ring_count;
		}
		report.rings.push_back(std::move(ra));
	}

	return report;
}

AromaticityReport analyze_aromaticity(const State& state) {
	const RingSystem   rings  = detect_ring_system(state);
	const BondOrderTable bo   = BondOrderProviderBuilder::infer(state);
	return analyze_aromaticity(state, rings, bo);
}

} // namespace classify
} // namespace atomistic
