#pragma once
/**
 * electrode_layout.hpp
 *
 * Electrohydrodynamic Simulation — Stage 1: CAD / Geometry
 *
 * Defines electrode placement and polarity within the tube.
 * Supports coaxial, helical, and opposing-pair configurations.
 *
 * Each electrode is a named body with polarity and geometric description
 * so that downstream boundary-condition assignment is unambiguous.
 */

#include "sim/ehd/ehd_types.hpp"
#include <string>
#include <vector>

namespace vsepr {
namespace ehd {
namespace cad {

// ============================================================================
// Electrode Configuration Types
// ============================================================================

enum class ElectrodeType {
    HELICAL,       // wire wound inside tube
    COAXIAL,       // central rod + outer shell
    PLANAR_PAIR    // opposing flat plates (2D study analog)
};

enum class Polarity {
    POSITIVE,
    NEGATIVE,
    GROUNDED
};

// ============================================================================
// Single Electrode Descriptor
// ============================================================================

struct ElectrodeDescriptor {
    std::string    name;              // e.g. "electrode_pos", "electrode_neg"
    ElectrodeType  type      = ElectrodeType::HELICAL;
    Polarity       polarity  = Polarity::POSITIVE;
    double         voltage   = 0.0;   // applied voltage (V)

    // Geometric parameters (interpretation depends on type)
    double radius       = 0.0;   // centerline radius for HELICAL, rod radius for COAXIAL
    double wire_diam    = 0.0;   // wire diameter (HELICAL) or thickness
    double pitch        = 0.0;   // helix pitch (HELICAL only)
    int    turns        = 0;     // helix turns (HELICAL only)
    double length       = 0.0;   // axial extent
    double z_start      = 0.0;   // axial start position
};

// ============================================================================
// Electrode Layout (collection of all electrodes in a device)
// ============================================================================

struct ElectrodeLayout {
    std::vector<ElectrodeDescriptor> electrodes;

    void add(const ElectrodeDescriptor& e) {
        electrodes.push_back(e);
    }

    const ElectrodeDescriptor* find_by_name(const std::string& name) const {
        for (const auto& e : electrodes) {
            if (e.name == name) return &e;
        }
        return nullptr;
    }

    std::vector<const ElectrodeDescriptor*> by_polarity(Polarity p) const {
        std::vector<const ElectrodeDescriptor*> result;
        for (const auto& e : electrodes) {
            if (e.polarity == p) result.push_back(&e);
        }
        return result;
    }
};

/**
 * Build a default helical dual-electrode layout from EHDParameters.
 * Creates one positive helix and one negative (grounded) outer shell.
 */
inline ElectrodeLayout build_default_helical_layout(const EHDParameters& p) {
    ElectrodeLayout layout;

    ElectrodeDescriptor pos;
    pos.name      = "electrode_pos";
    pos.type      = ElectrodeType::HELICAL;
    pos.polarity  = Polarity::POSITIVE;
    pos.voltage   = p.voltage_pos;
    pos.radius    = p.helix_radius();
    pos.wire_diam = p.wire_diameter_m;
    pos.pitch     = p.helix_pitch_m;
    pos.turns     = p.num_turns;
    pos.length    = p.tube_length_m;
    pos.z_start   = p.inlet_length_m;
    layout.add(pos);

    ElectrodeDescriptor neg;
    neg.name      = "electrode_neg";
    neg.type      = ElectrodeType::COAXIAL;
    neg.polarity  = Polarity::GROUNDED;
    neg.voltage   = p.voltage_neg;
    neg.radius    = p.tube_radius_m;
    neg.length    = p.tube_length_m;
    neg.z_start   = p.inlet_length_m;
    layout.add(neg);

    return layout;
}

/**
 * Build a planar electrode pair layout (config a).
 * Bottom electrode = positive, top electrode = grounded.
 */
inline ElectrodeLayout build_planar_layout(const EHDParameters& p) {
    ElectrodeLayout layout;

    ElectrodeDescriptor bot;
    bot.name     = "electrode_bottom";
    bot.type     = ElectrodeType::PLANAR_PAIR;
    bot.polarity = Polarity::POSITIVE;
    bot.voltage  = p.voltage_pos;
    bot.radius   = 0.0;
    bot.length   = p.channel_length_m;
    bot.z_start  = 0.0;
    layout.add(bot);

    ElectrodeDescriptor top;
    top.name     = "electrode_top";
    top.type     = ElectrodeType::PLANAR_PAIR;
    top.polarity = Polarity::GROUNDED;
    top.voltage  = p.voltage_neg;
    top.radius   = p.channel_height_m;
    top.length   = p.channel_length_m;
    top.z_start  = 0.0;
    layout.add(top);

    return layout;
}

/**
 * Build a needle-ring electrode layout (config b).
 * Needle = positive (high voltage), ring = grounded.
 */
inline ElectrodeLayout build_needle_ring_layout(const EHDParameters& p) {
    ElectrodeLayout layout;

    ElectrodeDescriptor needle;
    needle.name     = "electrode_needle";
    needle.type     = ElectrodeType::COAXIAL;  // axisymmetric approximation
    needle.polarity = Polarity::POSITIVE;
    needle.voltage  = p.voltage_pos;
    needle.radius   = p.needle_tip_radius_m;
    needle.length   = p.needle_ring_gap_m;
    needle.z_start  = 0.0;
    layout.add(needle);

    ElectrodeDescriptor ring;
    ring.name     = "electrode_ring";
    ring.type     = ElectrodeType::COAXIAL;
    ring.polarity = Polarity::GROUNDED;
    ring.voltage  = p.voltage_neg;
    ring.radius   = p.ring_inner_radius_m;
    ring.length   = 0.5e-3;  // ring thickness
    ring.z_start  = p.needle_ring_gap_m;
    layout.add(ring);

    return layout;
}

/**
 * Build a disk-stack electrode layout (config c).
 * Alternating polarity disks.
 */
inline ElectrodeLayout build_disk_stack_layout(const EHDParameters& p) {
    ElectrodeLayout layout;

    for (int i = 0; i < p.disk_count; ++i) {
        ElectrodeDescriptor disk;
        disk.name     = "disk_" + std::to_string(i);
        disk.type     = ElectrodeType::PLANAR_PAIR;
        disk.polarity = (i % 2 == 0) ? Polarity::POSITIVE : Polarity::NEGATIVE;
        disk.voltage  = (i % 2 == 0) ? p.voltage_pos : p.voltage_neg;
        disk.radius   = p.disk_radius_m;
        disk.length   = p.disk_thickness_m;
        disk.z_start  = i * (p.disk_thickness_m + p.disk_spacing_m);
        layout.add(disk);
    }

    return layout;
}

/**
 * Build a prism-slit electrode layout (config d).
 * Alternating polarity prisms.
 */
inline ElectrodeLayout build_prism_slit_layout(const EHDParameters& p) {
    ElectrodeLayout layout;

    double pitch = p.prism_base_m + p.slit_width_m;

    for (int i = 0; i < p.prism_count; ++i) {
        ElectrodeDescriptor prism;
        prism.name     = "prism_" + std::to_string(i);
        prism.type     = ElectrodeType::PLANAR_PAIR;
        prism.polarity = (i % 2 == 0) ? Polarity::POSITIVE : Polarity::NEGATIVE;
        prism.voltage  = (i % 2 == 0) ? p.voltage_pos : p.voltage_neg;
        prism.radius   = p.prism_height_m;
        prism.wire_diam = p.prism_base_m;
        prism.length   = p.slit_depth_m;
        prism.z_start  = i * pitch;
        layout.add(prism);
    }

    return layout;
}

/**
 * Factory: build the appropriate electrode layout based on topology.
 */
inline ElectrodeLayout build_layout_for_topology(const EHDParameters& p) {
    switch (p.topology) {
        case PumpTopology::PLANAR_CHANNEL:  return build_planar_layout(p);
        case PumpTopology::NEEDLE_RING:     return build_needle_ring_layout(p);
        case PumpTopology::DISK_STACK:      return build_disk_stack_layout(p);
        case PumpTopology::PRISM_SLIT:      return build_prism_slit_layout(p);
        default:                            return build_default_helical_layout(p);
    }
}

} // namespace cad
} // namespace ehd
} // namespace vsepr
