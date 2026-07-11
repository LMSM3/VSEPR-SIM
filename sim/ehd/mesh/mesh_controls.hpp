#pragma once
/**
 * mesh_controls.hpp
 *
 * Electrohydrodynamic Simulation — Stage 4: Mesh
 *
 * Mesh sizing and refinement control parameters.
 * Emits mesh hints that can be consumed by external meshing tools (gmsh,
 * ANSYS Meshing, OpenFOAM snappyHexMesh) or by the internal grid generator.
 *
 * Key refinement drivers:
 *   - |∇φ| is large  →  electrode vicinity
 *   - |∇c| is large  →  species boundary layers
 *   - |∇u| is large  →  wall shear layers, helix gaps
 */

#include "sim/ehd/ehd_types.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <cmath>

namespace vsepr {
namespace ehd {
namespace mesh {

// ============================================================================
// Mesh Sizing Specification
// ============================================================================

struct MeshSizing {
    double base_size       = 0.5e-3;   // global base element size (m)
    double electrode_size  = 0.05e-3;  // near-electrode element size (m)
    double wall_size       = 0.1e-3;   // near-wall element size (m)
    double throat_size     = 0.08e-3;  // narrow-gap element size (m)
    double inlet_size      = 0.2e-3;   // inlet/outlet face size (m)

    // Growth rate from fine to coarse
    double growth_rate     = 1.2;

    // Curvature refinement angle threshold (degrees)
    double curvature_angle = 18.0;

    // Proximity refinement: min cells across narrow gaps
    int    proximity_cells = 3;
};

// ============================================================================
// Inflation Layer Specification (boundary layer mesh)
// ============================================================================

struct InflationSpec {
    std::string surface_name;      // which surface to inflate from
    int    num_layers     = 5;     // number of prism layers
    double first_height   = 0.01e-3;  // first cell height (m)
    double growth_rate    = 1.3;      // layer growth rate
    double max_thickness  = 0.5e-3;   // max total inflation thickness (m)
};

// ============================================================================
// Local Refinement Zone
// ============================================================================

struct RefinementZone {
    std::string name;
    double z_min = 0.0, z_max = 0.0;
    double r_min = 0.0, r_max = 0.0;
    double target_size = 0.0;        // local element size (m)
    std::string reason;              // why this zone exists
};

// ============================================================================
// Complete Mesh Control Specification
// ============================================================================

struct MeshControlSpec {
    MeshSizing              sizing;
    std::vector<InflationSpec>   inflation;
    std::vector<RefinementZone>  refinements;

    void add_inflation(const InflationSpec& inf) { inflation.push_back(inf); }
    void add_refinement(const RefinementZone& rz) { refinements.push_back(rz); }
};

/**
 * Build default mesh controls for a helical EHD reactor.
 */
inline MeshControlSpec build_default_mesh_controls(const EHDParameters& p) {
    MeshControlSpec spec;

    // Global sizing
    spec.sizing.base_size      = p.tube_radius_m * 0.12;
    spec.sizing.electrode_size = p.wire_diameter_m * 0.06;
    spec.sizing.wall_size      = p.tube_radius_m * 0.025;

    // Inflation on inner wall
    spec.add_inflation({
        "wall_inner", 5,
        p.tube_radius_m * 0.003,  // first cell ~ 0.3% of radius
        1.3, p.tube_radius_m * 0.12
    });

    // Inflation on electrode surfaces
    spec.add_inflation({
        "electrode_pos_surface", 4,
        p.wire_diameter_m * 0.02,
        1.4, p.wire_diameter_m * 0.5
    });

    // Refinement zone around each helix turn
    double helix_r = p.helix_radius();
    for (int t = 0; t < p.num_turns; ++t) {
        double z_center = p.inlet_length_m + (t + 0.5) * p.helix_pitch_m;
        RefinementZone rz;
        rz.name = "helix_turn_" + std::to_string(t);
        rz.z_min = z_center - p.helix_pitch_m * 0.6;
        rz.z_max = z_center + p.helix_pitch_m * 0.6;
        rz.r_min = helix_r - p.wire_diameter_m;
        rz.r_max = helix_r + p.wire_diameter_m;
        rz.target_size = spec.sizing.electrode_size;
        rz.reason = "High |∇φ| and |∇u| near helix wire";
        spec.add_refinement(rz);
    }

    // Refinement at inlet/outlet transition
    spec.add_refinement({
        "inlet_transition",
        0.0, p.inlet_length_m * 0.3,
        0.0, p.tube_radius_m,
        spec.sizing.wall_size,
        "Flow development region"
    });

    return spec;
}

/**
 * Write mesh control specification to a text file consumable by meshing tools.
 */
inline bool write_mesh_spec(const MeshControlSpec& spec, const std::string& filepath) {
    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << std::fixed << std::setprecision(6);
    out << "# ============================================================================\n";
    out << "# EHD Mesh Control Specification\n";
    out << "# ============================================================================\n\n";

    out << "[global_sizing]\n";
    out << "  base_size       = " << spec.sizing.base_size       << "\n";
    out << "  electrode_size  = " << spec.sizing.electrode_size  << "\n";
    out << "  wall_size       = " << spec.sizing.wall_size       << "\n";
    out << "  throat_size     = " << spec.sizing.throat_size     << "\n";
    out << "  growth_rate     = " << spec.sizing.growth_rate     << "\n";
    out << "  curvature_angle = " << spec.sizing.curvature_angle << "\n";
    out << "  proximity_cells = " << spec.sizing.proximity_cells << "\n\n";

    for (size_t i = 0; i < spec.inflation.size(); ++i) {
        const auto& inf = spec.inflation[i];
        out << "[inflation." << i << "]\n";
        out << "  surface       = " << inf.surface_name  << "\n";
        out << "  num_layers    = " << inf.num_layers     << "\n";
        out << "  first_height  = " << inf.first_height   << "\n";
        out << "  growth_rate   = " << inf.growth_rate    << "\n";
        out << "  max_thickness = " << inf.max_thickness  << "\n\n";
    }

    for (size_t i = 0; i < spec.refinements.size(); ++i) {
        const auto& rz = spec.refinements[i];
        out << "[refinement." << i << "]\n";
        out << "  name        = " << rz.name        << "\n";
        out << "  z_min       = " << rz.z_min       << "\n";
        out << "  z_max       = " << rz.z_max       << "\n";
        out << "  r_min       = " << rz.r_min       << "\n";
        out << "  r_max       = " << rz.r_max       << "\n";
        out << "  target_size = " << rz.target_size  << "\n";
        out << "  reason      = " << rz.reason       << "\n\n";
    }

    return true;
}

} // namespace mesh
} // namespace ehd
} // namespace vsepr
