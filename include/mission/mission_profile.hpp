#pragma once
/**
 * mission_profile.hpp
 * ===================
 * Scale Mission: Particles, Clouds, Lattice, and Pipe Gas 3
 *
 * Shared runtime profile and entity identity layer for all four
 * simulation modes (V1 Particles N, V2 Gas Clouds, V3 Lattice, V4 Pipe Gas 3).
 *
 * Architecture:
 *   This header defines the common foundation that all four sim versions share:
 *     1. MissionScale   — runtime budget enum (Instant / 50ms / 5s / 5min)
 *     2. RuntimeProfile — parameter pack mapped from MissionScale per version
 *     3. EntityIdentity — shared identity layer (family, subtype, state vars,
 *                         capability flags, dynamic channels declared)
 *     4. ExternalLayer  — shared mixed state (mobility, force, energy,
 *                         response history, decay-response token)
 *     5. MissionOutput  — unified output format flags
 *
 * Design rules:
 *   - Entity definition is separate from dynamics.
 *   - Species definitions carry capability flags only — no simulation logic.
 *   - Every field is explicit, public, and inspectable.
 *   - MissionScale drives solver detail, entity count, and export fidelity.
 *
 * Integrates with:
 *   include/core/species_family.hpp   — SpeciesFamily / SpeciesSubfamily
 *   include/physics/particle_id.hpp   — ParticleID / species_code namespace
 *   include/identity/provenance_record.hpp — 3-tier provenance
 *   include/io/xyz_format.hpp         — .xyz / .xyza / .xyzc / .xyzf output
 */

#include "core/species_family.hpp"
#include "physics/particle_id.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <array>
#include <optional>

namespace vsepr {
namespace mission {

// ============================================================================
// 1. Runtime budget tiers
//    Tier A: Instant    — constexpr / table / static init
//    Tier B: Short_50ms — CLI inspection, quick screening, UI-refresh-safe
//    Tier C: Medium_5s  — short sweeps, moderate N, one lattice cell
//    Tier D: Long_5min  — broad sweeps, long trajectories, rich export
// ============================================================================

enum class MissionScale : int {
    Instant    = 0,
    Short_50ms = 1,
    Medium_5s  = 2,
    Long_5min  = 3,
};

inline const char* mission_scale_name(MissionScale s) {
    switch (s) {
        case MissionScale::Instant:    return "Instant";
        case MissionScale::Short_50ms: return "Short (50 ms)";
        case MissionScale::Medium_5s:  return "Medium (5 s)";
        case MissionScale::Long_5min:  return "Long (5 min)";
    }
    return "Unknown";
}

inline double mission_scale_budget_s(MissionScale s) {
    switch (s) {
        case MissionScale::Instant:    return 0.0;
        case MissionScale::Short_50ms: return 0.05;
        case MissionScale::Medium_5s:  return 5.0;
        case MissionScale::Long_5min:  return 300.0;
    }
    return 0.0;
}

// ============================================================================
// 2. Runtime profile
//    Maps MissionScale to concrete solver parameters for each sim version.
//    Each sim version has its own profile factory (see sim-specific headers).
// ============================================================================

struct RuntimeProfile {
    MissionScale scale;

    // Entity budget
    std::size_t max_entities;    // particles / clouds / lattice sites / pipe nodes

    // Temporal budget
    std::size_t max_steps;       // integration steps (0 = single-point solve)
    double      dt;              // timestep (seconds or reduced units)

    // Solver fidelity
    bool high_accuracy;          // use higher-order solver / finer kernels
    bool pairwise_interactions;  // enable O(N²) pairwise terms
    bool neighbor_list;          // build neighbor list (needed if pairwise)

    // Output fidelity
    bool export_full_history;    // write trajectory / full sweep table
    bool export_csv;
    bool export_markdown;
    bool export_xyz;             // .xyz / .xyza / .xyzc / .xyzf

    // Approximation level (higher = coarser kernels, more entities possible)
    int approximation_level;     // 0 = full, 1 = reduced, 2 = mean-field

    // Wall budget (seconds, 0.0 = no limit)
    double wall_budget_s;
};

// ============================================================================
// 3. Entity capability flags
//    Each species declares which dynamics channels apply.
//    Scheduler runs only supported channels.
// ============================================================================

enum class Capability : uint64_t {
    Kinematic       = 1ull << 0,   // position/velocity update
    Bonded          = 1ull << 1,   // bonded force terms
    Steric          = 1ull << 2,   // size exclusion / repulsion
    Diffusive       = 1ull << 3,   // diffusion / transport
    Reactive        = 1ull << 4,   // chemical reaction / transformation
    Radiative       = 1ull << 5,   // radiation interaction
    DecaySource     = 1ull << 6,   // emits decay particles
    GrowthEnabled   = 1ull << 7,   // topology may change (growth/branching)
    Thermal         = 1ull << 8,   // thermal response / heat capacity
    Transported     = 1ull << 9,   // bulk flow / pipe transport
    EOS_Active      = 1ull << 10,  // equation of state evaluation
    LatticeConstrained = 1ull << 11, // periodic lattice site constraints
    DefectCarrier   = 1ull << 12,  // can host / emit lattice defects
    CloudMember     = 1ull << 13,  // belongs to statistical cloud ensemble
    PipeFlowing     = 1ull << 14,  // occupies pipe flow state
    MixtureMember   = 1ull << 15,  // participates in species mixture
};

inline Capability operator|(Capability a, Capability b) {
    return static_cast<Capability>(
        static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
}

inline bool has_capability(Capability flags, Capability test) {
    return (static_cast<uint64_t>(flags) & static_cast<uint64_t>(test)) != 0;
}

// ============================================================================
// 4. Entity identity
//    The "what are you?" layer. No simulation logic — only identity, state
//    schema, material tags, dynamic channels, and defaults.
//
//    X_i = {id, kind, r_i, v_i, a_i, m_i, q_i, σ_i, η_i, D_i, T_i, S_i}
// ============================================================================

struct EntityState {
    // Kinematics
    std::array<double, 3> position    {0.0, 0.0, 0.0};  // r_i  (Å or m)
    std::array<double, 3> velocity    {0.0, 0.0, 0.0};  // v_i
    std::array<double, 3> accel       {0.0, 0.0, 0.0};  // a_i

    // Material scalars
    double mass    {1.0};   // m_i  (amu or kg)
    double charge  {0.0};   // q_i  (e)
    double sigma   {1.0};   // σ_i  structural / material descriptor (Å)
    double epsilon {1.0};   // ε_i  LJ well depth or equivalent (kcal/mol)

    // Environment memory
    double eta     {0.0};   // η_i  environment response accumulator

    // Damage / dose
    double dose    {0.0};   // D_i  cumulative radiation dose (eV/atom or Gy)
    double damage  {0.0};   // damage index [0, 1]

    // Thermal state
    double temperature {300.0};  // T_i  (K)

    // Internal species mode (family-specific: conformer, phase, decay state…)
    int    internal_mode {0};    // S_i

    // Species code (ties to ParticleID / element Z / engine-assigned ID)
    int    species_code  {0};
};

struct EntityIdentity {
    // Unique engine-assigned integer ID
    std::size_t id {0};

    // Classification
    SpeciesFamily    family    {SpeciesFamily::GAS};
    SpeciesSubfamily subfamily {SpeciesSubfamily::GAS_POLYATOMIC};
    std::string      name;
    std::string      formula;

    // Capability flags (which dynamics channels apply to this entity)
    Capability capabilities {Capability::Kinematic};

    // Named dynamic channels this entity participates in
    std::vector<std::string> enabled_channels;

    // Current state
    EntityState state;

    // Provenance / species code
    physics::ParticleID particle_id {physics::ParticleID::placeholder};
};

// ============================================================================
// 5. External mixed layer
//    Mobility, force accumulator, energy, response state, decay token.
//    Channels write here; integrator reads from here.
// ============================================================================

struct ExternalLayer {
    std::array<double, 3> force     {0.0, 0.0, 0.0};  // accumulated force (kcal/mol·Å⁻¹)
    double                energy    {0.0};              // potential energy contribution
    double                mobility  {1.0};              // relative mobility factor
    double                drag_coef {0.0};              // γ for Stokes drag

    // Response history (last N values — compact ring buffer via index)
    static constexpr int HISTORY_LEN = 8;
    std::array<double, HISTORY_LEN> energy_history {};
    int history_head {0};

    // Decay-response token (set by decay channel, cleared after integration)
    bool   decay_event_pending   {false};
    int    decay_daughter_code   {0};
    double decay_energy_deposited{0.0};

    void push_energy(double e) {
        energy_history[history_head % HISTORY_LEN] = e;
        ++history_head;
    }

    void zero_force() {
        force = {0.0, 0.0, 0.0};
        energy = 0.0;
    }
};

// ============================================================================
// 6. Output format flags
// ============================================================================

enum class OutputFormat : uint32_t {
    Console  = 1u << 0,
    CSV      = 1u << 1,
    Markdown = 1u << 2,
    XYZ      = 1u << 3,   // static geometry
    XYZA     = 1u << 4,   // extended per-atom state
    XYZC     = 1u << 5,   // checkpoint
    XYZF     = 1u << 6,   // full trajectory
};

inline OutputFormat operator|(OutputFormat a, OutputFormat b) {
    return static_cast<OutputFormat>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool wants_format(OutputFormat flags, OutputFormat test) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(test)) != 0;
}

// ============================================================================
// 7. Mission deliverable record
//    Populated at end of any sim run. Common across all four versions.
// ============================================================================

struct MissionDeliverable {
    std::string  sim_version;       // "V1_Particles", "V2_Clouds", "V3_Lattice", "V4_PipeGas3"
    MissionScale scale;
    std::size_t  entity_count;
    std::size_t  steps_run;
    double       wall_time_s;
    double       energy_total;
    bool         converged;
    std::string  output_dir;

    // Per-version summary fields (optional, version fills what it knows)
    std::optional<double> mobility_mean;
    std::optional<double> collision_rate;
    std::optional<double> pressure_out;
    std::optional<double> temperature_out;
    std::optional<double> delta_P;
    std::optional<double> conformer_entropy;
    std::optional<int>    defect_count;

    std::vector<std::string> output_files;
    std::vector<std::string> warnings;
};

} // namespace mission
} // namespace vsepr
