#pragma once
/**
 * domain_extract.hpp
 *
 * Electrohydrodynamic Simulation — Stage 2: Domain Extraction
 *
 * Extracts the computational domains Ω_f (fluid) and Ω_s (solid) from
 * the CAD assembly description.  Performs Boolean subtraction of embedded
 * solids (helix, supports) from the fluid envelope to produce a clean
 * watertight fluid region with tagged interface surfaces.
 *
 * Domain decomposition:
 *   Ω = Ω_f ∪ Ω_s
 *   ∂Ω_f ∩ ∂Ω_s = electrode/insulator interface boundaries
 */

#include "sim/ehd/ehd_types.hpp"
#include "sim/ehd/cad/step_export.hpp"
#include <string>
#include <vector>
#include <algorithm>

namespace vsepr {
namespace ehd {
namespace domain {

// ============================================================================
// Domain Region
// ============================================================================

enum class DomainType {
    FLUID,
    SOLID_CONDUCTOR,
    SOLID_DIELECTRIC,
    BOUNDARY_INLET,
    BOUNDARY_OUTLET,
    BOUNDARY_WALL
};

struct DomainRegion {
    std::string name;
    DomainType  type;
    double      z_min = 0.0, z_max = 0.0;
    double      r_inner = 0.0, r_outer = 0.0;
    double      volume_m3 = 0.0;   // computed after extraction
};

// ============================================================================
// Domain Assembly
// ============================================================================

struct DomainAssembly {
    std::vector<DomainRegion> regions;

    void add(const DomainRegion& r) { regions.push_back(r); }

    const DomainRegion* find(const std::string& name) const {
        for (const auto& r : regions) {
            if (r.name == name) return &r;
        }
        return nullptr;
    }

    std::vector<const DomainRegion*> by_type(DomainType t) const {
        std::vector<const DomainRegion*> result;
        for (const auto& r : regions) {
            if (r.type == t) result.push_back(&r);
        }
        return result;
    }

    double total_fluid_volume() const {
        double V = 0.0;
        for (const auto& r : regions) {
            if (r.type == DomainType::FLUID) V += r.volume_m3;
        }
        return V;
    }
};

// ============================================================================
// Domain Extraction from STEP Manifest
// ============================================================================

/**
 * Extract computational domains from a STEP manifest.
 *
 * For each named body in the manifest, classify it as fluid, solid, or boundary
 * and compute an approximate cylindrical volume.
 *
 * In a real implementation, this would perform CSG Boolean subtraction of
 * electrode/support volumes from the fluid envelope.  Here we compute the
 * net fluid volume analytically for cylindrical geometries.
 */
inline DomainAssembly extract_domains(const cad::StepManifest& manifest,
                                       const EHDParameters& params)
{
    DomainAssembly assembly;

    // Track volume to subtract from fluid
    double subtract_vol = 0.0;

    for (const auto& body : manifest.bodies) {
        DomainRegion reg;
        reg.name    = body.name;
        reg.z_min   = body.z_min;
        reg.z_max   = body.z_max;
        reg.r_inner = body.r_inner;
        reg.r_outer = body.r_outer;

        if (body.role == "fluid") {
            reg.type = DomainType::FLUID;
            double L = body.z_max - body.z_min;
            reg.volume_m3 = constants::PI * body.r_outer * body.r_outer * L;
        }
        else if (body.role == "conductor" || body.role == "conductor_gnd") {
            reg.type = DomainType::SOLID_CONDUCTOR;
            if (body.geometry_type == "helix_sweep") {
                // Approximate helix volume: π·r_wire² · (total wire length)
                double r_w = params.wire_diameter_m * 0.5;
                double helix_r = params.helix_radius();
                double wire_len = params.num_turns
                    * std::sqrt(std::pow(2.0 * constants::PI * helix_r, 2.0)
                              + std::pow(params.helix_pitch_m, 2.0));
                reg.volume_m3 = constants::PI * r_w * r_w * wire_len;
                subtract_vol += reg.volume_m3;
            } else {
                double L = body.z_max - body.z_min;
                reg.volume_m3 = constants::PI * body.r_outer * body.r_outer * L;
            }
        }
        else if (body.role == "dielectric") {
            reg.type = DomainType::SOLID_DIELECTRIC;
            double L = body.z_max - body.z_min;
            reg.volume_m3 = constants::PI
                * (body.r_outer * body.r_outer - body.r_inner * body.r_inner) * L;
        }
        else if (body.name == "inlet") {
            reg.type = DomainType::BOUNDARY_INLET;
        }
        else if (body.name == "outlet") {
            reg.type = DomainType::BOUNDARY_OUTLET;
        }
        else {
            reg.type = DomainType::BOUNDARY_WALL;
        }

        assembly.add(reg);
    }

    // Adjust fluid volume by subtracting embedded solids
    for (auto& r : assembly.regions) {
        if (r.type == DomainType::FLUID) {
            r.volume_m3 = std::max(0.0, r.volume_m3 - subtract_vol);
        }
    }

    return assembly;
}

} // namespace domain
} // namespace ehd
} // namespace vsepr
