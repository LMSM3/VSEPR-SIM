#pragma once
/**
 * aromaticity.hpp  --  Day 84 / WO-84D
 * ============================================================================
 * Huckel 4n+2 aromaticity gate for detected rings.
 *
 * Aromaticity is GATED, not guessed: a ring is only classified aromatic when it
 * satisfies concrete, checkable evidence.  Unsupported ring systems fail
 * VISIBLY (classified NonAromatic / Unsupported), never silently promoted.
 *
 * Evidence used per ring:
 *   - planarity proxy: every ring atom is sp2-like (3 sigma domains or a
 *     conjugation-capable heteroatom lone pair),
 *   - continuous pi system: each ring atom contributes exactly one pi electron
 *     count derived from the Kekule bond-order pattern + heteroatom lone pairs,
 *   - Huckel count: total ring pi electrons == 4n + 2.
 *
 * Results:
 *   benzene        -> Aromatic          (6 pi e-, 4n+2 with n=1)
 *   pyridine       -> Aromatic (hetero) (N contributes 1 pi e- via the ring)
 *   cyclobutadiene -> Antiaromatic      (4 pi e-, 4n)
 *   acetone/ethene -> NonAromatic       (no qualifying ring)
 *
 * This module depends only on ring_detector (topology) and bond_order_provider
 * (Kekule pattern); it does NOT reach into VSEPR or heavy chemistry databases.
 */

#include "atomistic/classify/bond_order_provider.hpp"
#include "atomistic/classify/ring_detector.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace atomistic {

struct State;

namespace classify {

enum class AromaticClass : std::uint8_t {
	NonAromatic = 0,   // no qualifying cyclic pi system
	Aromatic,          // satisfies Huckel 4n+2
	Antiaromatic,      // planar cyclic conjugation with 4n pi electrons
	Unsupported        // ring present but evidence insufficient to decide
};

const char* to_string(AromaticClass c);

// Per-ring aromaticity verdict with the evidence that produced it.
struct RingAromaticity {
	std::vector<std::size_t> atoms;      // ring atom indices (original)
	int           ring_size    = 0;
	int           pi_electrons = 0;      // counted cyclic pi electrons
	bool          all_sp2      = false;  // planarity proxy satisfied
	AromaticClass verdict      = AromaticClass::NonAromatic;
};

struct AromaticityReport {
	std::vector<RingAromaticity> rings;
	int aromatic_ring_count     = 0;
	int antiaromatic_ring_count = 0;
	int unsupported_ring_count  = 0;

	bool any_aromatic() const { return aromatic_ring_count > 0; }
};

// Analyse aromaticity for all detected rings in `state`.
// If `ring_system` / `bond_orders` are supplied they are reused; otherwise they
// are computed internally.  This keeps callers that already have them cheap.
AromaticityReport analyze_aromaticity(const State& state);
AromaticityReport analyze_aromaticity(const State& state,
									  const RingSystem& ring_system,
									  const BondOrderTable& bond_orders);

// Huckel test helper: true when pi_electrons == 4n + 2 for some n >= 0.
bool satisfies_huckel(int pi_electrons);

} // namespace classify
} // namespace atomistic
