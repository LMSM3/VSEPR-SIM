#pragma once
/**
 * layer_stack.hpp — Five-Layer Vertical Integration Stack
 *
 * VSEPR-SIM materials discovery engine.
 * Full fuel cycle simulation.  Cradle to product.
 *
 * The stack:
 *
 *   L1  Paper Identity     Formula, class, name.  Static.  Never mutates.
 *   L2  Atomistic          Z, A, Q, ε.  Physics and energy state.
 *   L3  Molecular 3D       Geometry, bonding, coordination, local environment.
 *   L4  Atomistic          Coarse-grained beads, phase behaviour, bulk interactions.
 *   L5  Macro              CAD-scale.  Geometry, volumes, surfaces, material bodies.
 *
 * The particle identity from L2 propagates all the way up.  The iron in a pipe
 * at L5 is still Fe with its Z, A, Q underneath.  The macro geometry is the
 * accumulated consequence of everything below it.
 *
 * Vertical integration contract (propagation invariant):
 *
 *   For any particle i and any layer k ∈ {1..5}:
 *     L_k(i).Z == L2(i).Z   (nuclear identity is immutable)
 *     L_k(i).A == L2(i).A   (mass identity is immutable)
 *
 *   Charge Q_i and energy state ε_i may change across layers as the
 *   chemical environment evolves (Fe³⁺ in acid leach ≠ Fe in ore body),
 *   but Z and A never change below a nuclear event.
 *
 * Two hard problems acknowledged here by design:
 *
 *   1. Compute cost: Millions of L1 candidates; only survivors reach L2–L5.
 *      Addressed by: LayerGate (screening contract), lazy evaluation per layer.
 *
 *   2. L2 ↔ L4 phase transition boundary: where atomic behaviour becomes
 *      bulk behaviour.  Addressed by: layer_boundary.hpp — explicit interface
 *      with no cheating or silent approximation.
 *
 * Anti-black-box: every layer transition is a named, typed, inspectable object.
 * Deterministic: same stack inputs → same stack outputs.
 *
 * Reference: docs/section_layer_stack.tex
 */

#include "atomistic/core/state.hpp"
#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/bead_system.hpp"
#include "coarse_grain/core/bit32_identity.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace layer_stack {

// ============================================================================
// L1 — Paper Identity
// ============================================================================

/**
 * L1_PaperIdentity — static species record.
 *
 * This is what is known from a formula, a name, and a CAS number BEFORE
 * any physics is computed.  It never mutates.  A candidate that fails at
 * L1 is never instantiated at L2 or above.
 *
 * Fields:
 *   formula     — Hill-order chemical formula string ("Fe2O3", "UO2")
 *   iupac_name  — Full IUPAC systematic name (may be empty for elements)
 *   common_name — Common / trade name ("hematite", "urania")
 *   cas_number  — CAS registry number string (empty if unknown)
 *   class_tag   — Coarse classification: "element", "oxide", "halide",
 *                 "organometallic", "alloy", "mineral", etc.
 *   Z_set       — Set of unique atomic numbers in the formula.
 *                 Single element: {26}.  Compound: {26, 8}.
 *   stoich      — Stoichiometric coefficients paired with Z_set.
 *
 * A species is L1-valid if formula is non-empty and Z_set is non-empty.
 */
struct L1_PaperIdentity {
    std::string              formula;
    std::string              iupac_name;
    std::string              common_name;
    std::string              cas_number;
    std::string              class_tag;

    std::vector<uint8_t>     Z_set;       // Unique atomic numbers
    std::vector<uint32_t>    stoich;      // Stoichiometry per Z (same order)

    bool is_valid() const {
        return !formula.empty()
            && !Z_set.empty()
            && Z_set.size() == stoich.size();
    }

    bool is_single_element() const {
        return Z_set.size() == 1;
    }
};

// ============================================================================
// L2 — Atomistic Layer
// ============================================================================

/**
 * L2_AtomisticState — physics and energy state of a species.
 *
 * Carries the full §0 Identity–State vector plus the LJ energy parameters.
 * Q_i and ε_i are environment-dependent: they carry the values current
 * for this simulation context (e.g. Fe³⁺ in acid leach has different Q
 * than Fe in ore body — same Z, different Q).
 *
 * L2 maps 1:1 to a particle in atomistic::State.
 */
struct L2_AtomisticState {
    // §0 Identity Vector (immutable nuclear identity + mutable environment state)
    uint8_t  Z{};       // Atomic number — immutable
    uint8_t  A{};       // Mass number   — immutable (0 = natural average)
    double   Q{};       // Effective charge participation (environment-dependent)
    double   epsilon{}; // LJ well depth ε (kcal/mol) — energy state
    double   sigma{};   // LJ radius σ (Å)

    // Structural role and stability (from §0 Σ_i, Λ_i)
    coarse_grain::StructuralRole  structural_role{coarse_grain::StructuralRole::Mixed};
    coarse_grain::StabilityClass  stability_class{coarse_grain::StabilityClass::AmbientStable};

    // 32-bit packed identity word (derived, for hot-path lookup)
    coarse_grain::Identity32      identity_word{};

    // Thermodynamic state
    double   temperature{298.15};  // K — local temperature proxy
    double   pressure{1.0};        // bar — local pressure proxy
    coarse_grain::chemistry::Phase phase{coarse_grain::chemistry::Phase::SOLID};

    // Provenance: which L1 species produced this particle
    std::string source_formula;    // L1 formula this came from
    uint32_t    particle_index{};  // Index in the parent atomistic::State

    // Pack the identity word from current fields
    void pack_identity() {
        identity_word = coarse_grain::pack_identity(
            Z, A, Q, structural_role, stability_class,
            coarse_grain::ProvenanceTag::MappedAtomistic);
    }

    bool is_valid() const { return Z > 0 && Z <= 118; }
};

// ============================================================================
// L3 — Molecular 3D Layer
// ============================================================================

/**
 * L3_MolecularGeometry — 3D structural description of a molecule or fragment.
 *
 * Carries geometry, bonding topology, coordination environment, and
 * local geometry classification.  Built from atomistic::State output.
 */
struct L3_BondRecord {
    uint32_t i{}, j{};
    uint8_t  order{1};       // 1=single, 2=double, 3=triple, 4=aromatic
    double   length{};       // Å
    double   energy{};       // kcal/mol (bond dissociation proxy)
};

struct L3_AngleRecord {
    uint32_t i{}, j{}, k{};  // j is central atom
    double   angle_deg{};
    double   ideal_deg{};
    double   deviation{};     // |angle - ideal| (°)
};

struct L3_CoordinationRecord {
    uint32_t  center_atom{};
    uint8_t   Z_center{};
    uint32_t  coord_number{};
    double    mean_bond_length{}; // Å
    double    bond_length_std{};  // Å — spread of bond lengths (distortion indicator)
    std::string geometry_class;  // "tetrahedral", "octahedral", "trigonal-planar", etc.
};

struct L3_MolecularGeometry {
    std::string formula;          // Back-reference to L1
    uint32_t    N_atoms{};

    std::vector<atomistic::Vec3>   positions;       // Angstrom
    std::vector<uint8_t>           atomic_numbers;
    std::vector<L3_BondRecord>     bonds;
    std::vector<L3_AngleRecord>    angles;
    std::vector<L3_CoordinationRecord> coordination;

    // Geometry quality metrics
    double rms_force{};           // kcal/(mol·Å) — convergence quality
    double total_energy{};        // kcal/mol
    bool   converged{false};

    // Local environment summary
    double mean_coordination{};
    double anisotropy_index{};    // How non-spherical the local environment is

    bool is_valid() const { return N_atoms > 0 && converged; }
};

// ============================================================================
// L4 — Atomistic / CG Layer (Beads, Phase Behaviour, Bulk Interactions)
// ============================================================================

/**
 * L4_AtomisticBeadState — coarse-grained bead representation.
 *
 * Anisotropic surface mapping lives here.  Phase behaviour and bulk
 * interaction channels live here.  This is the L2↔L4 boundary output.
 *
 * The bead carries the L2 identity of its dominant constituent atom
 * (the atom with the highest contribution to the group's physics).
 */
struct L4_PhaseRegion {
    coarse_grain::chemistry::Phase phase{coarse_grain::chemistry::Phase::SOLID};
    double volume_fraction{};      // 0–1 fraction of this phase in the bead group
    double order_parameter{};      // P₂ orientational order
    double local_density{};        // ρ_B (Gaussian-weighted)
};

struct L4_AtomisticBeadState {
    coarse_grain::Bead          bead;        // Full CG bead (carries Σ, Λ, channels)
    L4_PhaseRegion              phase_info;
    double                      eta{};       // Slow environmental state η
    double                      rho{};       // Local density

    // L2 identity of the dominant atom in this bead group
    L2_AtomisticState           dominant_atom;

    // Anisotropic surface: which channel dominates the bead's interaction
    double g_steric{1.0};
    double g_electrostatic{1.0};
    double g_dispersion{1.0};

    bool is_valid() const { return dominant_atom.is_valid(); }
};

// ============================================================================
// L5 — Macro / CAD Layer
// ============================================================================

/**
 * MacroBodyType — what kind of engineering body this is.
 */
enum class MacroBodyType : uint8_t {
    Pipe,           // Cylindrical flow conduit
    Vessel,         // Pressure vessel or reactor
    Valve,          // Flow control body (seat, disc, stem)
    HeatExchanger,  // Thermal transfer surface
    Membrane,       // Selective barrier (porous/dense)
    Electrode,      // Electrochemical interface
    Structural,     // Load-bearing generic body
    Custom          // User-defined
};

inline const char* macro_body_type_name(MacroBodyType t) {
    switch (t) {
        case MacroBodyType::Pipe:           return "Pipe";
        case MacroBodyType::Vessel:         return "Vessel";
        case MacroBodyType::Valve:          return "Valve";
        case MacroBodyType::HeatExchanger:  return "HeatExchanger";
        case MacroBodyType::Membrane:       return "Membrane";
        case MacroBodyType::Electrode:      return "Electrode";
        case MacroBodyType::Structural:     return "Structural";
        case MacroBodyType::Custom:         return "Custom";
        default:                            return "Unknown";
    }
}

/**
 * L5_MacroGeometry — CAD-scale material body.
 *
 * The macro body is the accumulated consequence of L1–L4.  It carries:
 *   - Physical dimensions (SI units: metres)
 *   - Material identity — back-reference to the L1 formula of the bulk material
 *   - Surface area and volume
 *   - Operating conditions (T, P, flow)
 *   - Degradation / corrosion state (driven by L2–L4 chemistry at the interface)
 *
 * The iron in a pipe is still Fe (Z=26) underneath.  The macro geometry
 * is just the engineering expression of that accumulated atomic identity.
 */
struct L5_SurfaceCondition {
    double corrosion_depth_m{};     // metres of wall loss
    double oxide_layer_m{};         // metres of passivation layer thickness
    double roughness_Ra_m{};        // surface roughness Ra (m)
    std::string dominant_species;   // L1 formula of the surface layer ("Fe2O3", "CuO")
};

struct L5_OperatingConditions {
    double temperature_K{298.15};
    double pressure_bar{1.0};
    double flow_velocity_ms{0.0};   // m/s (0 = static)
    std::string fluid_formula;      // L1 formula of the fluid ("H2SO4_aq", "NaCl_aq")
};

struct L5_MacroGeometry {
    std::string         body_id;          // Unique identifier
    MacroBodyType       body_type{MacroBodyType::Custom};
    std::string         material_formula; // L1 formula of bulk material ("Fe", "Ti6Al4V")

    // Dimensions (metres)
    double              length_m{};
    double              outer_diameter_m{};
    double              wall_thickness_m{};
    double              volume_m3{};
    double              surface_area_m2{};

    // State
    L5_SurfaceCondition    surface;
    L5_OperatingConditions conditions;

    // Link to the L4 bead field that populates this body's material properties
    uint32_t            bead_system_id{};  // Which BeadSystem drives this body's physics

    // Derived integrity metric (0=failed, 1=pristine)
    double              structural_integrity{1.0};

    bool is_valid() const {
        return !material_formula.empty() && volume_m3 > 0.0 && surface_area_m2 > 0.0;
    }
};

// ============================================================================
// LayerParticle — the vertically-integrated particle record
// ============================================================================

/**
 * LayerParticle — one particle tracked across all five layers.
 *
 * Propagation invariant: Z and A never change.
 * Q, ε, phase, and structural role evolve with chemical environment.
 *
 * Optional layers: L3, L4, L5 are populated only for particles that have
 * survived screening and warranted full treatment.
 */
struct LayerParticle {
    L1_PaperIdentity               l1;
    L2_AtomisticState              l2;
    std::optional<L3_MolecularGeometry>    l3;
    std::optional<L4_AtomisticBeadState>   l4;
    std::optional<L5_MacroGeometry>        l5;

    // Screening state
    bool passed_l1_screen{false};
    bool passed_l2_screen{false};
    bool passed_l3_screen{false};
    bool passed_l4_screen{false};

    // Check propagation invariant
    bool invariant_holds() const {
        if (!l2.is_valid()) return true;         // L2 not populated, skip check
        if (l3.has_value()) {
            for (auto Z_atom : l3->atomic_numbers)
                if (Z_atom != l2.Z) return false; // heterogeneous fragment — OK, skip
        }
        if (l4.has_value()) {
            if (l4->dominant_atom.Z != l2.Z) return false;
            if (l4->dominant_atom.A != l2.A) return false;
        }
        return true;
    }
};

// ============================================================================
// LayerGate — screening contract
// ============================================================================

/**
 * LayerGateResult — outcome of one layer's screening pass.
 *
 * Each layer gate makes a binary accept/reject decision plus a score.
 * Accepted particles propagate to the next layer.
 * Rejected particles are recorded with a reason and discarded.
 */
struct LayerGateResult {
    bool   accepted{false};
    double score{};           // Higher = better candidate
    std::string reject_reason;

    explicit operator bool() const { return accepted; }
};

/**
 * L1Gate — screen by formula validity and class.
 *
 * Rejects candidates with:
 *   - Empty or unparseable formula
 *   - Z values outside 1–118
 *   - class_tag in the exclusion list
 */
inline LayerGateResult l1_gate(
    const L1_PaperIdentity& id,
    const std::vector<std::string>& excluded_classes = {})
{
    if (!id.is_valid())
        return {false, 0.0, "invalid formula or empty Z_set"};

    for (const auto& Z : id.Z_set)
        if (Z == 0 || Z > 118)
            return {false, 0.0, "Z out of range"};

    for (const auto& ex : excluded_classes)
        if (id.class_tag == ex)
            return {false, 0.0, "excluded class: " + ex};

    // Score: prefer single-element > binary > ternary (for screening speed)
    double score = 1.0 / static_cast<double>(id.Z_set.size());
    return {true, score, ""};
}

/**
 * L2Gate — screen by energy state.
 *
 * Rejects candidates with:
 *   - epsilon outside physically reasonable range
 *   - Q beyond ionic saturation limit
 *   - StabilityClass below minimum
 */
inline LayerGateResult l2_gate(
    const L2_AtomisticState&  st,
    double                    epsilon_max   = 5.0,    // kcal/mol
    double                    Q_abs_max     = 8.0,    // e
    coarse_grain::StabilityClass min_stability =
        coarse_grain::StabilityClass::Transient)
{
    if (!st.is_valid())
        return {false, 0.0, "invalid Z"};
    if (st.epsilon < 0.0 || st.epsilon > epsilon_max)
        return {false, 0.0, "epsilon out of range"};
    if (std::abs(st.Q) > Q_abs_max)
        return {false, 0.0, "charge Q exceeds saturation"};
    if (static_cast<uint8_t>(st.stability_class) <
        static_cast<uint8_t>(min_stability))
        return {false, 0.0, "stability class below minimum"};

    // Score: favour lower energy (tighter binding) and higher stability
    double e_score  = 1.0 - st.epsilon / (epsilon_max + 1e-9);
    double s_score  = static_cast<double>(st.stability_class) / 3.0;
    return {true, 0.5 * e_score + 0.5 * s_score, ""};
}

} // namespace layer_stack
