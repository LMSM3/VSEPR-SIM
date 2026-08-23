/**
 * bridge_objects.hpp  -  FEABridgeObject and DEMBridgeObject typed structs
 *
 * WO-66N  |  Constructor Objects (bridge field schema freeze)
 * WO-67N  |  DEM Bridge Object     (consumer)
 * WO-67O  |  FEA Bridge Object     (consumer)
 * v5.1.4  |  v5.0.0-main
 *
 * THIS SCHEMA IS FROZEN.
 * WO-67N (DEM bridge) and WO-67O (FEA bridge) depend on the exact field names
 * and types defined here.  Do not rename or remove fields without updating
 * both bridge WO documents and their acceptance checklists.
 *
 * Diagnostic codes:
 *   DEM bridge:  VSIM-E070 … VSIM-W075  (see WO-VSIM-67N-DEM-Bridge.md)
 *   FEA bridge:  VSIM-E080 … VSIM-W085  (see WO-VSIM-67O-FEA-Bridge.md)
 */

#pragma once

#include "object_path.hpp"
#include "non_molecular_objects.hpp"

#include <string>
#include <vector>

namespace vsim {

// ============================================================================
// DEMBridgeObject  —  frozen schema for WO-67N
// ============================================================================

struct DEMBridgeObject {
	ObjectPath  path;               // declared path, e.g. "dem.pipe_packing"

	// [frozen] Input references
	ObjectPath  source_surface;     // "from"     — must resolve to a SurfaceObject
	ObjectPath  geometry;           // "geometry" — must resolve to a GeometryObject
	ObjectPath  inlet;              // "inlet"    — must resolve to a SourceObject
	ObjectPath  outlet;             // "outlet"   — must resolve to a SinkObject
	ObjectPath  ambient;            // "carrier"  — must resolve to an AmbientObject

	// [frozen] Field subscription list
	std::vector<std::string> fields;  // subset of: "pressure","shear","flow_rate","cavitation"

	// [frozen] Packing model
	std::string packing_model   = "hard_sphere";     // "hard_sphere" | "soft_sphere"
	std::string contact_model   = "hertz_mindlin";   // "hertz_mindlin" | "linear_spring"

	// [frozen] Particle parameters
	double friction             = 0.30;
	double restitution          = 0.20;
	double particle_density_kg_m3 = 2500.0;  // per-species override: see per_species_density
	std::vector<std::pair<std::string,double>> per_species_density;  // species → kg/m³

	// [frozen] Export
	bool   export_manifest      = true;
	bool   export_table         = true;
	std::string export_format   = "dem_manifest";

	// Source location
	int source_line = 0;

	// Validation helpers
	bool has_source()   const noexcept { return source_surface.is_set(); }
	bool has_geometry() const noexcept { return geometry.is_set(); }
};

// ============================================================================
// FEABridgeObject  —  frozen schema for WO-67O
// ============================================================================

struct FEABridgeObject {
	ObjectPath  path;               // declared path, e.g. "fea.pipe_wall"

	// [frozen] Input references
	ObjectPath  source_surface;     // "from"   — must resolve to a SurfaceObject
	ObjectPath  target_geometry;    // "target" — must resolve to a GeometryObject

	// [frozen] Material tag (cross-ref to geometry.material or explicit override)
	std::string material            = "unknown";

	// [frozen] Field subscription list
	std::vector<std::string> fields;  // subset of: "pressure","shear","temperature","cavitation"

	// [frozen] Mapping mode
	std::string mapping_mode        = "surface_to_mesh";  // only mode in v5.1.4

	// [frozen] Stress criterion
	std::string criterion           = "von_mises";

	// [frozen] Yield / fatigue parameters
	double yield_strength_Pa        = 0.0;   // required when criterion="von_mises"; 0=absent→VSIM-W085
	bool   fatigue_enabled          = false;

	// Cavitation damage amplification constant (k_c in WO-67O §4.7)
	double cavitation_k             = 0.15;

	// [frozen] Export flags
	bool   export_json              = true;
	bool   export_tsv               = true;
	bool   export_vtk               = false; // gated — format spec must be committed first

	// Source location
	int source_line = 0;

	// Validation helpers
	bool has_source()          const noexcept { return source_surface.is_set(); }
	bool has_target()          const noexcept { return target_geometry.is_set(); }
	bool has_yield_strength()  const noexcept { return yield_strength_Pa > 0.0; }
};

// ============================================================================
// BridgeObjectStore  —  typed collections on VsimDocument
// ============================================================================

struct BridgeObjectStore {
	std::vector<DEMBridgeObject> dem_bridges;
	std::vector<FEABridgeObject> fea_bridges;

	bool empty() const noexcept {
		return dem_bridges.empty() && fea_bridges.empty();
	}
};

} // namespace vsim
