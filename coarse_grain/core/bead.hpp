#pragma once
/**
 * bead.hpp — Coarse-Grained Bead
 *
 * A bead is the fundamental unit in the coarse-grained representation.
 * Each bead maps to a group of atomistic particles and carries aggregate
 * physical properties (mass, charge, position, type).
 *
 * Design:  Plain-old-data with named accessors.
 * Philosophy: Anti-black-box — every field is inspectable, every mapping
 *             is traceable back to the parent atom group.
 */

#include "atomistic/core/state.hpp"
#include "coarse_grain/core/surface_descriptor.hpp"
#include "coarse_grain/core/multi_channel_descriptor.hpp"
#include "coarse_grain/core/unified_descriptor.hpp"
#include "coarse_grain/core/orientation.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace coarse_grain {

/**
 * BeadType — classification tag for a bead.
 *
 * Examples:
 *   "BB"   — backbone bead (polymer)
 *   "SC1"  — side-chain bead 1
 *   "W"    — water bead (4:1 mapping)
 *   "ION"  — ionic bead
 */
struct BeadType {
    std::string name;          // Human-readable label
    uint32_t    id{};          // Numeric id (unique within a mapping scheme)
    double      sigma{};       // Effective LJ sigma (Å)
    double      epsilon{};     // Effective LJ epsilon (kcal/mol)
};

/**
 * Bead — one coarse-grained site.
 *
 * Stores both the CG-level state AND the provenance (parent atom indices).
 * This is the anti-black-box contract: you can always trace a bead back
 * to the atoms that produced it.
 */
struct Bead {
    // --- CG-level state ---
    atomistic::Vec3 position{};     // Center of bead (Å)
    atomistic::Vec3 velocity{};     // Bead velocity (Å/fs)
    double          mass{};         // Sum of parent atom masses (amu)
    double          charge{};       // Sum of parent atom charges (e)
    uint32_t        type_id{};      // Index into BeadType table

    // --- Provenance (mapping traceability) ---
    std::vector<uint32_t> parent_atom_indices;   // Indices into atomistic::State
    uint32_t              mapping_rule_id{};      // Which MappingRule produced this bead

    // --- Diagnostics ---
    atomistic::Vec3 com_position{};     // Center-of-mass position (reference)
    atomistic::Vec3 cog_position{};     // Center-of-geometry position (reference)
    double          mapping_residual{}; // |COM - COG| distance (Å), mapping quality metric

    // --- Anisotropic Surface Data (optional) ---
    std::optional<SurfaceDescriptor> surface;   // Populated by SurfaceMapper
    bool has_surface_data() const { return surface.has_value(); }

    // --- Multi-Channel Descriptor (optional, enriched representation) ---
    std::optional<MultiChannelDescriptor> multi_channel;  // Populated by MultiChannelMapper
    bool has_multi_channel_data() const { return multi_channel.has_value(); }

    // --- Unified Descriptor (adaptive resolution, supersedes tier-based selection) ---
    std::optional<UnifiedDescriptor> unified;  // Single formalism, adaptive truncation
    bool has_unified_data() const { return unified.has_value(); }

    // --- Orientation (for orientation-coupled interactions) ---
    BeadOrientation orientation{};              // Primary axis from inertia frame
    bool            has_orientation{false};     // True if orientation was computed

    // Convenience accessors for surface properties
    double asphericity() const {
        return surface ? surface->asphericity() : 0.0;
    }
    double anisotropy_ratio() const {
        return surface ? surface->anisotropy_ratio() : 0.0;
    }
};

/**
 * Projection mode for computing bead center from parent atoms.
 */
enum class ProjectionMode {
    CENTER_OF_MASS,      // Weighted by atomic masses: r = Σ(m_i * r_i) / Σ(m_i)
    CENTER_OF_GEOMETRY   // Unweighted average:        r = Σ(r_i) / N
};

} // namespace coarse_grain
