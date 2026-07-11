#pragma once
/**
 * ehd_types.hpp
 *
 * Electrohydrodynamic Simulation — Fundamental Types and Constants
 *
 * Defines the core data structures shared across all five EHD stages:
 *   - Physical constants (Faraday, vacuum permittivity, Boltzmann)
 *   - Vector3 utility for continuum field quantities
 *   - IonicSpecies descriptor (valence, diffusivity, mobility)
 *   - FluidProperties (density, viscosity, permittivity)
 *   - EHDParameters (full run-card parameterization)
 *
 * All units are SI unless otherwise noted.
 */

#include <array>
#include <cmath>
#include <string>
#include <vector>
#include "core/math_vec3.hpp"

namespace vsepr {
namespace ehd {

// ============================================================================
// Physical Constants (SI)
// ============================================================================

namespace constants {
    inline constexpr double FARADAY        = 96485.33212;       // C/mol
    inline constexpr double BOLTZMANN      = 1.380649e-23;      // J/K
    inline constexpr double AVOGADRO       = 6.02214076e23;     // 1/mol
    inline constexpr double EPSILON_0      = 8.8541878128e-12;  // F/m
    inline constexpr double ELEMENTARY_E   = 1.602176634e-19;   // C
    inline constexpr double GAS_CONSTANT   = 8.314462618;       // J/(mol·K)
    inline constexpr double PI             = 3.14159265358979323846;
} // namespace constants

// ============================================================================
// Pump Topology — The four principal EHD device configurations
// ============================================================================

enum class PumpTopology {
    HELICAL_WIRE,       // Helical wire electrode wound inside cylindrical tube
    PLANAR_CHANNEL,     // (a) Planar electrode pair driving flow through channel
    NEEDLE_RING,        // (b) Needle-ring electrode generating ion injection
    DISK_STACK,         // (c) Stacked perforated disk electrodes (cathode/anode)
    PRISM_SLIT          // (d) Triangular prism slit-electrode cross-flow
};

// ============================================================================
// EHD Pumping Mechanism
// ============================================================================

enum class PumpMechanism {
    COULOMB,            // Ion-drag / Coulomb force:  f = ρ_e · E
    DIELECTROPHORETIC,  // DEP force: f = (P·∇)E  ≈  ε₀·χ·∇(|E|²)/2
    ELECTROOSMOTIC      // EOF slip: u_slip = -(ε·ζ/μ)·E_tangential
};

// ============================================================================
// Reactive Multiphase — enable combustion coupling
// ============================================================================

struct CombustionParameters {
    bool   enabled            = false;
    int    particle_count     = 100;       // N_p injected
    double particle_diameter  = 10.0e-6;   // d₀ (m) — initial diameter
    double injection_velocity = 0.5;       // m/s
    double ambient_temperature = 300.0;    // T_∞ (K)
    double O2_mass_fraction   = 0.233;     // Y_O2 (air default)
    double ehd_enhancement_alpha = 0.1;    // α_E (electric field enhancement)
};

// ============================================================================
// Basic 3D Vector — Day #56: alias to vsepr::Vec3, no local struct.
// ============================================================================

using Vec3 = vsepr::Vec3;

// ============================================================================
// Ionic Species Descriptor
// ============================================================================

struct IonicSpecies {
    std::string name;                // e.g. "Na+", "Cl-", "X-"
    int         valence     = 0;     // z_i (signed)
    double      diffusivity = 0.0;   // D_i (m²/s)
    double      mobility    = 0.0;   // μ_i (m²/(V·s))  — if 0, computed from Nernst-Einstein
    double      init_conc   = 0.0;   // c_i,0 (mol/m³)

    /** Nernst-Einstein relation: μ = |z|·F·D / (R·T) */
    double effective_mobility(double temperature_K) const {
        if (mobility > 0.0) return mobility;
        return std::abs(valence) * constants::FARADAY * diffusivity
             / (constants::GAS_CONSTANT * temperature_K);
    }
};

// ============================================================================
// Fluid Properties
// ============================================================================

struct FluidProperties {
    std::string name         = "water";
    double density           = 998.2;       // kg/m³
    double viscosity         = 1.002e-3;    // Pa·s
    double relative_permittivity = 78.5;    // ε_r (dimensionless)
    double temperature_K     = 298.15;      // K

    double permittivity() const {
        return relative_permittivity * constants::EPSILON_0;
    }
};

// ============================================================================
// Full EHD Run-Card Parameters
// ============================================================================

struct EHDParameters {
    // --- Device Topology and Mechanism ---
    PumpTopology  topology  = PumpTopology::HELICAL_WIRE;
    PumpMechanism mechanism = PumpMechanism::COULOMB;

    // --- Geometry (helical wire / cylindrical tube — original) ---
    double tube_radius_m     = 4.0e-3;    // R_tube
    double tube_length_m     = 60.0e-3;   // L
    double helix_pitch_m     = 6.0e-3;    // p_helix
    double wire_diameter_m   = 0.8e-3;    // d_wire
    int    num_turns         = 9;          // n_turns
    double wall_clearance_m  = 0.2e-3;    // g (electrode gap to wall)
    double inlet_length_m    = 5.0e-3;    // development region
    double outlet_length_m   = 5.0e-3;    // exit region

    // --- Geometry (planar channel — config a) ---
    double channel_height_m  = 2.0e-3;    // H (gap between planar electrodes)
    double channel_width_m   = 10.0e-3;   // W (electrode span, transverse)
    double channel_length_m  = 40.0e-3;   // L_ch (electrode length, streamwise)

    // --- Geometry (needle-ring — config b) ---
    double needle_tip_radius_m = 50.0e-6; // r_tip (needle tip curvature)
    double ring_inner_radius_m = 2.0e-3;  // R_ring (ring electrode inner radius)
    double needle_ring_gap_m   = 5.0e-3;  // d_gap (needle tip to ring plane)

    // --- Geometry (disk stack — config c) ---
    double disk_radius_m       = 8.0e-3;  // R_disk (disk outer radius)
    double disk_thickness_m    = 0.5e-3;  // t_disk
    int    disk_count          = 6;        // number of disks (alternating polarity)
    double disk_spacing_m      = 3.0e-3;  // axial gap between disks
    int    perforations_per_disk = 12;     // through-holes per disk
    double perforation_radius_m = 1.0e-3; // r_perf

    // --- Geometry (prism slit — config d) ---
    double slit_width_m        = 1.5e-3;  // w_slit (gap between prism electrodes)
    double slit_depth_m        = 5.0e-3;  // d_slit (flow-through depth)
    double prism_base_m        = 3.0e-3;  // b_prism (triangular base width)
    double prism_height_m      = 4.0e-3;  // h_prism (triangular height)
    int    prism_count         = 4;        // number of prism electrodes in array

    // --- Dielectrophoretic / Electroosmotic properties ---
    double zeta_potential       = -50.0e-3;   // ζ (V) — wall zeta potential for EOF
    double clausius_mossotti    = 0.5;        // Re[K(ω)] — CM factor for DEP

    // --- Reactive Multiphase / Combustion ---
    CombustionParameters combustion;

    // --- Electrical ---
    double voltage_pos       = 50.0;      // V+ (V)
    double voltage_neg       = 0.0;       // V- (V)

    // --- Flow ---
    double inlet_velocity    = 0.01;      // m/s  (or use volumetric_flow_rate)
    double outlet_pressure   = 0.0;       // Pa (gauge)
    double volumetric_flow_rate = 0.0;    // m³/s (if >0, overrides inlet_velocity)

    // --- Fluid ---
    FluidProperties fluid;

    // --- Species ---
    std::vector<IonicSpecies> species;

    // --- Derived ---
    double helix_radius() const {
        return tube_radius_m - wall_clearance_m - wire_diameter_m * 0.5;
    }

    double hydraulic_diameter() const {
        switch (topology) {
            case PumpTopology::PLANAR_CHANNEL:
                // Rectangular channel: D_h = 2HW/(H+W)
                return 2.0 * channel_height_m * channel_width_m
                     / (channel_height_m + channel_width_m);
            case PumpTopology::NEEDLE_RING:
                return 2.0 * ring_inner_radius_m;
            case PumpTopology::DISK_STACK:
                return 2.0 * disk_radius_m;
            case PumpTopology::PRISM_SLIT:
                return 2.0 * slit_width_m;
            default: // HELICAL_WIRE
                return 2.0 * tube_radius_m;
        }
    }

    double effective_channel_length() const {
        switch (topology) {
            case PumpTopology::PLANAR_CHANNEL:
                return channel_length_m;
            case PumpTopology::NEEDLE_RING:
                return needle_ring_gap_m;
            case PumpTopology::DISK_STACK:
                return disk_count * disk_spacing_m;
            case PumpTopology::PRISM_SLIT:
                return slit_depth_m * prism_count;
            default:
                return tube_length_m + inlet_length_m + outlet_length_m;
        }
    }

    double effective_cross_section_area() const {
        switch (topology) {
            case PumpTopology::PLANAR_CHANNEL:
                return channel_height_m * channel_width_m;
            case PumpTopology::NEEDLE_RING:
                return constants::PI * ring_inner_radius_m * ring_inner_radius_m;
            case PumpTopology::DISK_STACK:
                return perforations_per_disk * constants::PI
                     * perforation_radius_m * perforation_radius_m;
            case PumpTopology::PRISM_SLIT:
                return slit_width_m * prism_height_m * prism_count;
            default:
                return constants::PI * tube_radius_m * tube_radius_m;
        }
    }

    double reynolds_number() const {
        double U = effective_inlet_velocity();
        return fluid.density * U * hydraulic_diameter() / fluid.viscosity;
    }

    double effective_inlet_velocity() const {
        if (volumetric_flow_rate > 0.0) {
            return volumetric_flow_rate / effective_cross_section_area();
        }
        return inlet_velocity;
    }
};

} // namespace ehd
} // namespace vsepr
