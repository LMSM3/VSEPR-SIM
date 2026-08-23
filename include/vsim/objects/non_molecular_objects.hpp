/**
 * non_molecular_objects.hpp  -  Non-molecular typed object structs
 *
 * WO-66Q  |  Non-Molecular Objects + XBIT Documentation
 * v5.1.4  |  v5.0.0-main
 *
 * Defines the typed structs for all non-molecular constructor objects:
 *   GeometryObject  — pipe, box, sphere, or generic geometry
 *   SurfaceObject   — control surface (wall, interface)
 *   SourceObject    — particle/flow inlet
 *   SinkObject      — particle/flow outlet
 *   AmbientObject   — ambient population / environment state
 *
 * These are the objects referenced by bridge constructors (WO-67N/67O) via
 * ObjectPath.  The field schema here is frozen as the contract for those WOs.
 *
 * Diagnostic codes assigned to this module: VSIM-E090 … VSIM-W099
 */

#pragma once

#include "object_path.hpp"

#include <string>
#include <vector>

namespace vsim {

// ============================================================================
// GeometryObject  —  physical geometry descriptor
// ============================================================================

struct GeometryObject {
	ObjectPath  path;

	std::string geometry_type   = "pipe";   // "pipe" | "box" | "sphere" | "generic"
	std::string name;

	// Pipe parameters
	double      radius_m        = 0.0;      // inner radius [m]
	double      length_m        = 0.0;      // length [m]
	double      wall_thickness_m = 0.001;   // wall thickness [m]
	double      bend_angle_deg  = 0.0;      // 0 = straight

	// Box parameters
	double      lx_m            = 0.0;
	double      ly_m            = 0.0;
	double      lz_m            = 0.0;

	// Sphere parameters
	double      sphere_radius_m = 0.0;

	// Material tag (cross-referenced by FEA bridge for E, ν, α_T, σ_Y, σ_u)
	std::string material        = "unknown";

	// Generic geometry: path to external mesh file
	std::string mesh_file;

	// Source line for diagnostics
	int source_line = 0;
};

// ============================================================================
// SurfaceObject  —  control surface (wall, interface)
//
// Carries the field schema consumed by DEMBridge and FEABridge.
// Fields marked [frozen] must not be renamed without updating WO-67N/67O.
// ============================================================================

struct SurfaceObject {
	ObjectPath  path;
	std::string surface_type    = "wall";   // "wall" | "interface" | "inlet_plane"
	std::string name;

	// Geometry reference — the surface lives on this geometry
	ObjectPath  geometry;

	// [frozen] Pressure field
	bool        has_pressure    = true;
	double      pressure_ref_Pa = 101325.0;

	// [frozen] Shear field (3-component)
	bool        has_shear       = true;

	// [frozen] Flow velocity
	bool        has_velocity    = true;

	// [frozen] Temperature
	bool        has_temperature = false;
	double      temperature_ref_K = 293.15;

	// [frozen] Cavitation / void fraction
	bool        has_cavitation  = false;

	// [frozen] Particle crossing flux
	bool        has_flux        = false;

	// Sampling / resolution
	int         region_grid_n   = 16;      // number of surface regions along axis
	std::string binning_mode    = "axis_aligned";  // "axis_aligned" | "adaptive"

	int source_line = 0;
};

// ============================================================================
// SourceObject  —  particle / flow inlet
// ============================================================================

struct SourceObject {
	ObjectPath  path;
	std::string name;

	std::string source_type     = "inlet";  // "inlet" | "injection" | "reservoir"
	ObjectPath  geometry;                   // geometry this inlet is attached to

	// [frozen] particle flux record
	double      flux_rate       = 0.0;      // particles/s
	double      particle_radius_m = 0.001;
	double      particle_density_kg_m3 = 2500.0;

	std::string particle_species = "default";

	// Velocity boundary condition
	double      inlet_velocity_m_s = 0.0;
	std::string velocity_profile = "uniform";  // "uniform" | "parabolic"

	int source_line = 0;
};

// ============================================================================
// SinkObject  —  particle / flow outlet
// ============================================================================

struct SinkObject {
	ObjectPath  path;
	std::string name;

	std::string sink_type       = "outlet"; // "outlet" | "absorber"
	ObjectPath  geometry;

	// [frozen] outlet flux record
	double      outlet_pressure_Pa = 101325.0;
	bool        record_flux     = true;

	int source_line = 0;
};

// ============================================================================
// AmbientObject  —  ambient population / environment state carrier
// ============================================================================

struct AmbientObject {
	ObjectPath  path;
	std::string name;

	double      temperature_K   = 293.15;
	double      pressure_Pa     = 101325.0;
	double      density_kg_m3   = 1000.0;
	double      viscosity_Pa_s  = 0.001;

	std::string fluid_species   = "water";

	// Gravity vector [m/s²]
	double      gx = 0.0;
	double      gy = -9.81;
	double      gz = 0.0;

	int source_line = 0;
};

// ============================================================================
// NonMolecularObjectStore  —  typed collections on VsimDocument
// ============================================================================

struct NonMolecularObjectStore {
	std::vector<GeometryObject>  geometries;
	std::vector<SurfaceObject>   surfaces;
	std::vector<SourceObject>    sources;
	std::vector<SinkObject>      sinks;
	std::vector<AmbientObject>   ambients;

	bool empty() const noexcept {
		return geometries.empty() && surfaces.empty() &&
			   sources.empty()   && sinks.empty()    && ambients.empty();
	}

	// Lookup by ObjectPath — return nullptr if not found
	const SurfaceObject*  find_surface (const ObjectPath& p) const {
		for (const auto& s : surfaces)  if (s.path == p) return &s;
		return nullptr;
	}
	const GeometryObject* find_geometry(const ObjectPath& p) const {
		for (const auto& g : geometries) if (g.path == p) return &g;
		return nullptr;
	}
	const SourceObject*   find_source  (const ObjectPath& p) const {
		for (const auto& s : sources)   if (s.path == p) return &s;
		return nullptr;
	}
	const SinkObject*     find_sink    (const ObjectPath& p) const {
		for (const auto& s : sinks)     if (s.path == p) return &s;
		return nullptr;
	}
	const AmbientObject*  find_ambient (const ObjectPath& p) const {
		for (const auto& a : ambients)  if (a.path == p) return &a;
		return nullptr;
	}
};

} // namespace vsim
