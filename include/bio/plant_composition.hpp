#pragma once
/**
 * plant_composition.hpp  —  WO-75C: Plant Composition Vector
 * ============================================================
 *
 * Maps a biological object kind (leaf, stem, plant) to a weighted vector
 * of OrganicClass components.
 *
 * The composition vector C⃗_plant is the bridge between the semantic
 * bio/nongeom identity layer and the organic class layer:
 *
 *   [plant object]_{bio/nongeom}  →  C⃗_plant  →  [OrganicClass]_{organic}
 *
 * Default mass-fraction weights are approximate literature values for
 * dry plant cell wall composition:
 *
 *   Leaf / Stem:  cellulose ~40%, lignin ~25%, pectin ~10%
 *                 (remainder: hemicellulose, protein — not yet modelled)
 *   Generic Plant: cellulose ~35%, lignin ~20%, pectin ~8%
 *
 * Weights are normalised over the modelled classes only; callers that need
 * full dry-matter balance should extend the OrganicClass enum.
 *
 * VSEPR-SIM  |  WO-75C  |  v5.13.5
 */

#include "bio/organic_class.hpp"

#include <array>
#include <string_view>

namespace vsepr {
namespace bio {

// ============================================================================
// BioObjectKind — semantic identity of a plant-scale object
// ============================================================================

enum class BioObjectKind : uint8_t {
	Unknown = 0,
	Leaf    = 1,
	Stem    = 2,
	Plant   = 3,   // whole-plant aggregate
};

inline BioObjectKind bio_object_kind_from_name(std::string_view name) {
	if (name == "leaf")  return BioObjectKind::Leaf;
	if (name == "stem")  return BioObjectKind::Stem;
	if (name == "plant") return BioObjectKind::Plant;
	return BioObjectKind::Unknown;
}

inline std::string_view bio_object_kind_name(BioObjectKind k) {
	switch (k) {
		case BioObjectKind::Leaf:  return "leaf";
		case BioObjectKind::Stem:  return "stem";
		case BioObjectKind::Plant: return "plant";
		default:                   return "unknown";
	}
}

// ============================================================================
// CompositionEntry — one (class, weight) pair
// ============================================================================

struct CompositionEntry {
	OrganicClass cls;
	double       weight;   // normalised mass fraction over modelled classes
};

// ============================================================================
// PlantComposition — composition vector C⃗_plant for one BioObjectKind
// ============================================================================

struct PlantComposition {
	BioObjectKind                  object_kind;
	std::array<CompositionEntry, 3> components;  // Cellulose, Lignin, Pectin

	// Convenience: look up weight for a specific class
	double weight_of(OrganicClass cls) const {
		for (const auto& e : components)
			if (e.cls == cls) return e.weight;
		return 0.0;
	}
};

// ============================================================================
// plant_composition() — default literature-derived compositions
// ============================================================================

inline constexpr PlantComposition plant_composition(BioObjectKind kind) {
	switch (kind) {
		case BioObjectKind::Leaf:
			// Dry cell wall: cellulose ~40%, lignin ~25%, pectin ~10%
			// Normalised over three modelled classes → sum = 0.75 → scaled to 1.0
			return { BioObjectKind::Leaf, {{
				{ OrganicClass::Cellulose, 0.533 },
				{ OrganicClass::Lignin,    0.333 },
				{ OrganicClass::Pectin,    0.133 },
			}}};

		case BioObjectKind::Stem:
			// Higher lignin fraction in vascular tissue
			// cellulose ~38%, lignin ~32%, pectin ~8% → normalised
			return { BioObjectKind::Stem, {{
				{ OrganicClass::Cellulose, 0.487 },
				{ OrganicClass::Lignin,    0.410 },
				{ OrganicClass::Pectin,    0.103 },
			}}};

		case BioObjectKind::Plant:
			// Whole-plant average
			// cellulose ~35%, lignin ~20%, pectin ~8% → normalised
			return { BioObjectKind::Plant, {{
				{ OrganicClass::Cellulose, 0.556 },
				{ OrganicClass::Lignin,    0.317 },
				{ OrganicClass::Pectin,    0.127 },
			}}};

		default:
			return { BioObjectKind::Unknown, {{
				{ OrganicClass::Unknown, 0.0 },
				{ OrganicClass::Unknown, 0.0 },
				{ OrganicClass::Unknown, 0.0 },
			}}};
	}
}

} // namespace bio
} // namespace vsepr
