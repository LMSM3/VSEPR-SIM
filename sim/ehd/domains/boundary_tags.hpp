#pragma once
/**
 * boundary_tags.hpp
 *
 * Electrohydrodynamic Simulation — Stage 2: Domain Extraction
 *
 * Boundary condition template assembly.  Maps named surface regions to
 * physics-specific boundary conditions for flow, electric, and transport solves.
 */

#include "sim/ehd/domains/named_regions.hpp"
#include "sim/ehd/ehd_types.hpp"
#include <string>
#include <vector>

namespace vsepr {
namespace ehd {
namespace domain {

// ============================================================================
// BC Type Enumerations
// ============================================================================

enum class FlowBCType {
    VELOCITY_INLET,
    PRESSURE_OUTLET,
    WALL_NOSLIP,
    SYMMETRY
};

enum class ElectricBCType {
    DIRICHLET_VOLTAGE,
    NEUMANN_INSULATING,   // n·(ε∇φ) = 0
    PERIODIC
};

enum class TransportBCType {
    FIXED_CONCENTRATION,
    ZERO_FLUX,
    CONVECTIVE_OUTFLOW
};

// ============================================================================
// BC Entry
// ============================================================================

struct BoundaryCondition {
    std::string surface_name;

    FlowBCType      flow_bc      = FlowBCType::WALL_NOSLIP;
    double           flow_value   = 0.0;   // velocity or pressure

    ElectricBCType   elec_bc      = ElectricBCType::NEUMANN_INSULATING;
    double           elec_value   = 0.0;   // voltage

    TransportBCType  transport_bc = TransportBCType::ZERO_FLUX;
    double           transport_value = 0.0; // concentration (mol/m³)
};

// ============================================================================
// BC Template
// ============================================================================

struct BCTemplate {
    std::vector<BoundaryCondition> conditions;

    void add(const BoundaryCondition& bc) { conditions.push_back(bc); }

    const BoundaryCondition* find(const std::string& surface) const {
        for (const auto& bc : conditions) {
            if (bc.surface_name == surface) return &bc;
        }
        return nullptr;
    }
};

/**
 * Build default BC template from EHDParameters and named regions.
 */
inline BCTemplate build_default_bcs(const EHDParameters& p,
                                     const NamedRegionRegistry& /*regions*/)
{
    BCTemplate tmpl;

    // Inlet: velocity + insulating + fixed species concentration
    {
        BoundaryCondition bc;
        bc.surface_name   = "inlet";
        bc.flow_bc        = FlowBCType::VELOCITY_INLET;
        bc.flow_value     = p.effective_inlet_velocity();
        bc.elec_bc        = ElectricBCType::NEUMANN_INSULATING;
        bc.transport_bc   = TransportBCType::FIXED_CONCENTRATION;
        bc.transport_value = (p.species.empty()) ? 0.0 : p.species[0].init_conc;
        tmpl.add(bc);
    }

    // Outlet: pressure + insulating + convective outflow
    {
        BoundaryCondition bc;
        bc.surface_name   = "outlet";
        bc.flow_bc        = FlowBCType::PRESSURE_OUTLET;
        bc.flow_value     = p.outlet_pressure;
        bc.elec_bc        = ElectricBCType::NEUMANN_INSULATING;
        bc.transport_bc   = TransportBCType::CONVECTIVE_OUTFLOW;
        tmpl.add(bc);
    }

    // Inner wall: no-slip + insulating + zero flux
    {
        BoundaryCondition bc;
        bc.surface_name   = "wall_inner";
        bc.flow_bc        = FlowBCType::WALL_NOSLIP;
        bc.elec_bc        = ElectricBCType::NEUMANN_INSULATING;
        bc.transport_bc   = TransportBCType::ZERO_FLUX;
        tmpl.add(bc);
    }

    // Electrode positive: no-slip + voltage + zero flux
    {
        BoundaryCondition bc;
        bc.surface_name   = "electrode_pos_surface";
        bc.flow_bc        = FlowBCType::WALL_NOSLIP;
        bc.elec_bc        = ElectricBCType::DIRICHLET_VOLTAGE;
        bc.elec_value     = p.voltage_pos;
        bc.transport_bc   = TransportBCType::ZERO_FLUX;
        tmpl.add(bc);
    }

    // Electrode negative / ground: no-slip + voltage + zero flux
    {
        BoundaryCondition bc;
        bc.surface_name   = "electrode_neg_surface";
        bc.flow_bc        = FlowBCType::WALL_NOSLIP;
        bc.elec_bc        = ElectricBCType::DIRICHLET_VOLTAGE;
        bc.elec_value     = p.voltage_neg;
        bc.transport_bc   = TransportBCType::ZERO_FLUX;
        tmpl.add(bc);
    }

    return tmpl;
}

} // namespace domain
} // namespace ehd
} // namespace vsepr
