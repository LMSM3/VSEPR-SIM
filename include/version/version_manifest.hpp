#pragma once
/**
 * version_manifest.hpp — VSEPR-SIM Version Lineage & Scale Registry
 * =================================================================
 *
 * Authoritative version map tracing kernel evolution from v0.1 through
 * the current 4.0 legacy-beta release. Every version is a checkpoint
 * in the kernel's capability surface.
 *
 * Lineage:
 *   Kernel v0.1   Initial commit → LJ dimer, basic State
 *   Kernel v0.2   Bonded model, FIRE minimizer, PBC
 *   Kernel v0.3   VSEPR seeds, crystal presets, deep verification
 *   User   2.2.0  First tagged release, formula pipeline
 *   User   2.3.1  GUI + force field architecture
 *   User   2.5.0  13-section LaTeX methodology
 *   User   2.5.2  Heat-gated reactions, alpha model
 *   User   2.7.1  Deep verification milestone (194 checks)
 *   User   2.7.2  Desktop system, 3D visualization expansion
 *   User   2.9.2  1013 tests, modular testing, CG layer
 *   User   3.0.1  Code audit checkpoint (frozen reference)
 *   Engine 4.0.1  Current CMake version
 *   Branch 4.0-LB Legacy-beta: multi-scale property search
 *
 * Scale Registry:
 *   Scale 1  Atomistic         (Å, fs, kcal/mol)     — kernel v0.1+
 *   Scale 2  Coarse-grained    (nm, ps, kJ/mol)      — kernel v0.3+
 *   Scale 3  Grain/continuum   (µm, ns, Pa)           — 4.0-LB
 *   Scale 4  Component         (mm, µs, MPa)           — 4.0-LB target
 *   Scale 5  Macroscopic       (m, s, engineering)      — 4.0-LB target
 */

#include <cstdint>
#include <string>
#include <array>

namespace vsepr {
namespace version {

// ============================================================================
// Semantic Version
// ============================================================================

struct SemanticVersion {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
    const char* label;       // "release", "beta", "legacy-beta", "rc1"

    constexpr bool operator>(const SemanticVersion& o) const {
        if (major != o.major) return major > o.major;
        if (minor != o.minor) return minor > o.minor;
        return patch > o.patch;
    }

    constexpr bool operator==(const SemanticVersion& o) const {
        return major == o.major && minor == o.minor && patch == o.patch;
    }
};

// ============================================================================
// Current Version
// ============================================================================

inline constexpr SemanticVersion CURRENT_VERSION = {4, 0, 0, "legacy-beta"};

// ============================================================================
// Kernel Lineage Checkpoint
// ============================================================================

enum class KernelEra {
    Genesis,        // v0.1 — initial commit, LJ dimer, basic State
    Foundation,     // v0.2 — bonded model, FIRE, PBC, topology
    Formation,      // v0.3 — VSEPR seeds, crystal presets, verification
    Methodology,    // 2.5  — 13-section LaTeX formalization
    Verification,   // 2.7  — deep verification (194 checks, 7 phases)
    Integration,    // 2.9  — 1013 tests, CG layer, modular build
    Audit,          // 3.0  — code audit freeze, terminology purge
    MultiScale,     // 4.0  — multi-scale property search (current)
};

struct KernelCheckpoint {
    const char*     tag;            // git tag or version string
    KernelEra       era;
    const char*     commit_prefix;  // first 7 chars of commit hash (or "")
    const char*     description;
    uint32_t        test_count;     // approximate tests passing at this point
};

inline constexpr KernelCheckpoint KERNEL_LINEAGE[] = {
    {"v0.1",    KernelEra::Genesis,
     "3ca9159", "Initial commit — LJ dimer, basic State container",               0},

    {"v0.2",    KernelEra::Genesis,
     "db4922f", "Complete source structure, minimal implementations",              0},

    {"v0.2.3",  KernelEra::Foundation,
     "689a9f5", "Bonded model, FIRE minimizer, build fixes",                      5},

    {"v2.2.0",  KernelEra::Foundation,
     "",        "First tagged release, formula pipeline",                         12},

    {"v2.3.1",  KernelEra::Foundation,
     "",        "GUI and force field architecture",                               20},

    {"v2.5.0",  KernelEra::Methodology,
     "6e07981", "13-section LaTeX methodology (186 pages)",                       28},

    {"v2.5.2",  KernelEra::Methodology,
     "397f004", "Heat-gated reactions, alpha model, amino acid table",            56},

    {"v2.6.3",  KernelEra::Methodology,
     "8faa5fe", "Empirical lookup table Z=1-118, alpha booklet 37pp",             80},

    {"v2.7.1",  KernelEra::Verification,
     "6443450", "Deep verification milestone — 194 checks, 7 phases",           194},

    {"v2.7.2",  KernelEra::Verification,
     "",        "Desktop system, 3D visualization, trajectory viewer",           194},

    {"v2.9.2",  KernelEra::Integration,
     "70f41f2", "1013 tests, modular testing, CG layer, animated demos",        1013},

    {"v3.0.1",  KernelEra::Audit,
     "",        "Code audit checkpoint — terminology purge, frozen reference",   1013},

    {"v4.0.1",  KernelEra::MultiScale,
     "",        "CMake version — multi-scale architecture, property search",     1013},

    {"v4.0-LB", KernelEra::MultiScale,
     "",        "Legacy-beta: multi-scale property search, 3-5 scale bridge",   1013},
};

inline constexpr size_t LINEAGE_COUNT = sizeof(KERNEL_LINEAGE) / sizeof(KernelCheckpoint);

// ============================================================================
// Scale Registry — the 3-5 scale hierarchy
// ============================================================================

enum class SimulationScale : uint8_t {
    Atomistic       = 1,    // Å, fs, kcal/mol   — LJ, bonded, Coulomb
    CoarseGrained   = 2,    // nm, ps, kJ/mol    — beads, environment-responsive
    Grain           = 3,    // µm, ns, Pa        — grain boundaries, defects
    Component       = 4,    // mm, µs, MPa       — FEA-coupled, part-level
    Macroscopic     = 5,    // m, s, engineering  — system-level, digital twin
};

struct ScaleDescriptor {
    SimulationScale scale;
    const char*     name;
    const char*     length_unit;
    const char*     time_unit;
    const char*     energy_unit;
    const char*     status;         // "active", "partial", "planned"
    const char*     bridge_up;      // scale transition upward
    const char*     bridge_down;    // scale transition downward
    KernelEra       introduced;     // when this scale became available
};

inline constexpr ScaleDescriptor SCALE_REGISTRY[] = {
    {SimulationScale::Atomistic,      "Atomistic",
     "Å",  "fs",  "kcal/mol",
     "active",
     "Fragment → Bead mapping (Morgan + inertia frame)",
     "—",
     KernelEra::Genesis},

    {SimulationScale::CoarseGrained,  "Coarse-Grained",
     "nm", "ps",  "kJ/mol",
     "active",
     "Bead ensemble → continuum properties (Boltzmann averaging)",
     "Bead → fragment back-mapping (restrained MD)",
     KernelEra::Formation},

    {SimulationScale::Grain,          "Grain / Microstructure",
     "µm", "ns",  "Pa",
     "partial",
     "Grain ensemble → effective medium (Voigt-Reuss-Hill)",
     "Grain → CG back-resolution (defect microstate injection)",
     KernelEra::MultiScale},

    {SimulationScale::Component,      "Component / Part",
     "mm", "µs",  "MPa",
     "planned",
     "FEA mesh → system assembly (contact + BC)",
     "Hotspot → grain-level refinement (adaptive zoom)",
     KernelEra::MultiScale},

    {SimulationScale::Macroscopic,    "Macroscopic / System",
     "m",  "s",   "engineering units",
     "planned",
     "— (top level: digital twin output)",
     "System → component decomposition (load extraction)",
     KernelEra::MultiScale},
};

inline constexpr size_t SCALE_COUNT = sizeof(SCALE_REGISTRY) / sizeof(ScaleDescriptor);

// ============================================================================
// Property Search Domain — what the multi-scale search can find
// ============================================================================

enum class PropertyDomain : uint8_t {
    Structural,     // bond lengths, angles, coordination, crystal structure
    Mechanical,     // E, G, K, ν, σ_yield, hardness
    Thermal,        // k, Cp, α, Debye T, melting point
    Electronic,     // band gap, conductivity, dielectric, polarizability
    Transport,      // diffusivity, viscosity, ionic conductivity
    Stability,      // formation energy, decomposition T, radiation tolerance
    Nuclear,        // neutron cross-section proxy, displacement energy, fission gas
};

struct PropertyTarget {
    PropertyDomain  domain;
    const char*     name;
    const char*     unit;
    SimulationScale source_scale;  // lowest scale that can compute this
    const char*     scraper;       // function or module that extracts it
};

inline constexpr PropertyTarget SEARCH_TARGETS[] = {
    // Structural (Scale 1)
    {PropertyDomain::Structural, "Bond length",              "Å",     SimulationScale::Atomistic,     "geom_scraper"},
    {PropertyDomain::Structural, "Bond angle",               "deg",   SimulationScale::Atomistic,     "geom_scraper"},
    {PropertyDomain::Structural, "Coordination number",      "—",     SimulationScale::Atomistic,     "coordination_shell"},
    {PropertyDomain::Structural, "RDF peak positions",       "Å",     SimulationScale::Atomistic,     "rdf_accumulator"},

    // Mechanical (Scale 2-3)
    {PropertyDomain::Mechanical, "Young's modulus",          "GPa",   SimulationScale::CoarseGrained, "elastic_tensor"},
    {PropertyDomain::Mechanical, "Shear modulus",            "GPa",   SimulationScale::CoarseGrained, "elastic_tensor"},
    {PropertyDomain::Mechanical, "Bulk modulus",             "GPa",   SimulationScale::CoarseGrained, "elastic_tensor"},
    {PropertyDomain::Mechanical, "Poisson ratio",            "—",     SimulationScale::CoarseGrained, "elastic_tensor"},

    // Thermal (Scale 1-3)
    {PropertyDomain::Thermal,    "Heat capacity (Cp)",       "J/mol·K", SimulationScale::Atomistic,   "thermo_scraper"},
    {PropertyDomain::Thermal,    "Thermal conductivity",     "W/m·K",   SimulationScale::Grain,       "green_kubo"},
    {PropertyDomain::Thermal,    "Thermal expansion",        "1/K",     SimulationScale::CoarseGrained,"volume_fluctuation"},
    {PropertyDomain::Thermal,    "Debye temperature",        "K",       SimulationScale::Atomistic,    "phonon_dos"},

    // Electronic (Scale 1)
    {PropertyDomain::Electronic, "Polarizability",           "Å³",    SimulationScale::Atomistic,     "alpha_model"},
    {PropertyDomain::Electronic, "Dipole moment",            "Debye", SimulationScale::Atomistic,     "charge_scraper"},

    // Stability (Scale 1-2)
    {PropertyDomain::Stability,  "Formation energy",         "eV/atom", SimulationScale::Atomistic,   "energy_scraper"},
    {PropertyDomain::Stability,  "Thermal stability",        "K",       SimulationScale::Atomistic,   "thermal_runner"},
    {PropertyDomain::Stability,  "HGST viability",           "—",       SimulationScale::Atomistic,   "hgst_scorer"},

    // Nuclear (Scale 1-3) — the reason this matters
    {PropertyDomain::Nuclear,    "Displacement energy (Ed)", "eV",    SimulationScale::Atomistic,     "cascade_sim"},
    {PropertyDomain::Nuclear,    "Frenkel pair formation",   "eV",    SimulationScale::Atomistic,     "defect_energy"},
    {PropertyDomain::Nuclear,    "Radiation tolerance",      "dpa",   SimulationScale::Grain,         "damage_accumulator"},
    {PropertyDomain::Nuclear,    "Fission gas diffusion",    "m²/s",  SimulationScale::Grain,         "fission_gas_model"},
};

inline constexpr size_t SEARCH_TARGET_COUNT = sizeof(SEARCH_TARGETS) / sizeof(PropertyTarget);

} // namespace version
} // namespace vsepr
