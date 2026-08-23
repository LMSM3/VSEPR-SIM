#pragma once
/**
 * bio_object.hpp  —  WO-75E: Biological Object Identity
 * =======================================================
 *
 * Defines the semantic identity layer for plant-scale biological objects.
 *
 * A BioObject is a non-geometry entity: it carries domain, class tag, and
 * a resolved composition vector but NO position, mesh, or spatial fields.
 * Geometry is an opt-in downstream projection (see bio_geom_projection.hpp).
 *
 * Identity notation:
 *   [leaf]_{plant/object}^{nongeom}
 *   [stem]_{plant/object}^{nongeom}
 *   [plant]_{plant/object}^{nongeom}
 *
 * Resolution chain:
 *   BioObject{leaf}  →  plant_composition(Leaf)  →  [OrganicClass]_{organic}
 *
 * VSEPR-SIM  |  WO-75E  |  v5.13.5
 */

#include "bio/organic_class.hpp"
#include "bio/plant_composition.hpp"

#include <string>
#include <string_view>

namespace vsepr {
namespace bio {

// ============================================================================
// BioObject — semantic identity record (nongeom)
// ============================================================================

struct BioObject {
	// --- Identity ---
	BioObjectKind    kind        { BioObjectKind::Unknown };
	std::string      domain      { "bio/nongeom" };   // always bio/nongeom at identity layer
	std::string      class_tag   { "plant/object" };
	bool             nongeom     { true };            // must always be true; geometry is projection only

	// --- Resolved composition (populated by resolve()) ---
	PlantComposition composition { plant_composition(BioObjectKind::Unknown) };
	bool             resolved    { false };

	// --- Factory ---
	static BioObject make(BioObjectKind k) {
		BioObject obj;
		obj.kind        = k;
		obj.domain      = "bio/nongeom";
		obj.class_tag   = "plant/object";
		obj.nongeom     = true;
		obj.resolved    = false;
		return obj;
	}

	static BioObject from_name(std::string_view name) {
		return make(bio_object_kind_from_name(name));
	}

	// --- Composition resolution ---
	// Populates the composition vector from the plant_composition() table.
	// Must be called before accessing composition data.
	BioObject& resolve() {
		composition = plant_composition(kind);
		resolved    = true;
		return *this;
	}

	// --- Accessors ---
	std::string_view name() const {
		return bio_object_kind_name(kind);
	}

	double weight_of(OrganicClass cls) const {
		return composition.weight_of(cls);
	}

	// Summary string for diagnostics (no geometry content)
	std::string summary() const {
		std::string s;
		s += "[";
		s += std::string(name());
		s += "]_{";
		s += domain;
		s += "}^{nongeom}";
		if (resolved) {
			s += "  composition: ";
			for (const auto& e : composition.components) {
				if (e.cls == OrganicClass::Unknown) continue;
				auto d = organic_class_descriptor(e.cls);
				s += std::string(d.name);
				s += "(";
				// format weight to 3dp without <iomanip>
				int pct = static_cast<int>(e.weight * 1000.0 + 0.5);
				s += std::to_string(pct / 10);
				s += ".";
				s += std::to_string(pct % 10);
				s += "%)  ";
			}
		}
		return s;
	}
};

} // namespace bio
} // namespace vsepr
