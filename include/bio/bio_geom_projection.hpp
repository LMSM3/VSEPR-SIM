#pragma once
/**
 * bio_geom_projection.hpp  —  WO-75E: Optional Geometry Projection
 * ==================================================================
 *
 * Provides the explicit, opt-in projection transform from a semantic
 * BioObject (bio/nongeom) to a geometry representation (geom/mesh).
 *
 * This transform is NEVER called automatically. A simulation or analysis
 * pass that does not require spatial extent never triggers it.
 *
 * Identity notation for the transform:
 *   [leaf]_{bio/nongeom}  --project-->  [leaf surface]_{geom/mesh}
 *
 * The GeomMesh struct is a minimal placeholder. Full mesh generation
 * (LOD, surface parametrisation, voxelisation) is out of scope for WO-75E
 * and will be wired in a later work order.
 *
 * VSEPR-SIM  |  WO-75E  |  v5.13.5
 */

#include "bio/bio_object.hpp"

#include <string>
#include <vector>

namespace vsepr {
namespace bio {

// ============================================================================
// GeomMesh — minimal geometry representation (placeholder)
// ============================================================================

struct GeomMesh {
	std::string  source_name;     // name of the BioObject this was projected from
	std::string  domain;          // always "geom/mesh"
	bool         is_placeholder;  // true until full mesh generation is implemented

	// Stub vertex / face storage — empty until a real projector is wired
	std::vector<std::array<float, 3>> vertices;
	std::vector<std::array<uint32_t, 3>> faces;

	std::string summary() const {
		std::string s = "[";
		s += source_name;
		s += " surface]_{geom/mesh}";
		if (is_placeholder) s += "  [stub — geometry not yet generated]";
		return s;
	}
};

// ============================================================================
// project() — explicit opt-in projection
//
// Converts a resolved BioObject into a GeomMesh.
// The BioObject must have been resolve()d before calling project().
//
// Currently returns a placeholder mesh. The projection tag and source
// provenance are always populated so callers can verify the transform
// was applied correctly even before full mesh generation is available.
// ============================================================================

inline GeomMesh project(const BioObject& obj) {
	GeomMesh mesh;
	mesh.source_name     = std::string(obj.name());
	mesh.domain          = "geom/mesh";
	mesh.is_placeholder  = true;
	// vertices and faces remain empty — stub for WO-75E
	// Full parametric surface generation is deferred to a later WO
	return mesh;
}

} // namespace bio
} // namespace vsepr
