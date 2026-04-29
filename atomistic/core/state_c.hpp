#pragma once
/**
 * state_c.hpp — Dev Day ~42 Revised Notation: State Machine with
 *               Physics-Informed Bookkeeping
 * ================================================================
 *
 * This header implements the revised notation system that replaces
 * the overloaded symbol I (which collides with current, moment of
 * inertia, identity matrix, intensity, etc.) with unambiguous
 * containers:
 *
 *   S_0 = { m_0, E_0, x_0, v_0, C, E, K, Sigma }     (initial)
 *   S_f = { m_f, E_f, Phi, S, Pi, Omega, Q }          (outcome)
 *   S_f = T(S_0, t, Lambda)                             (transformation)
 *
 * The entire point:
 *   You are not solving reality. You are mapping states through
 *   controlled abstraction.
 *
 * Mass-energy sanity constraint (enforced even at C-level):
 *   E_0 + E_in - E_out ≈ E_f + E_loss
 *   m_0 + m_in - m_out ≈ m_f
 *
 * What this actually is:
 *   - A state machine
 *   - With physics-informed bookkeeping
 *   - Feeding a multi-scale transformation engine
 *   - With traceable data lineage
 *
 * Anti-black-box: every field is inspectable, every mapping decision
 * is explicit, every intermediate result is deterministic.
 *
 * Terminology: atomistic throughout (no meso).
 */

#include "atomistic/core/state.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace atomistic {

// ============================================================================
// Composition (C) — what the system is made of and at what resolution
// ============================================================================

/**
 * Composition describes the representation level of the system.
 *
 * This is NOT a physics quantity — it is a bookkeeping tag that tells
 * the engine which force evaluators, integrators, and analysis tools
 * are valid for the current state.
 */
enum class Composition : uint8_t {
    Atomistic = 0,   // Full atomic resolution (every atom explicit)
    Bead      = 1,   // Coarse-grained bead representation
    Hybrid    = 2,   // Mixed atomistic + bead (adaptive resolution)
    Macro     = 3    // Continuum / macro-scale field representation
};

inline const char* composition_name(Composition c) {
    switch (c) {
        case Composition::Atomistic: return "atomistic";
        case Composition::Bead:      return "bead";
        case Composition::Hybrid:    return "hybrid";
        case Composition::Macro:     return "macro";
        default:                     return "unknown";
    }
}

// ============================================================================
// Environment (E) — external conditions acting on the system
// ============================================================================

/**
 * Environment encodes the thermodynamic and field conditions.
 *
 * These are boundary conditions / external influences, NOT system
 * properties. The system responds to them; it does not generate them.
 *
 * Convention: zero / false = absent / off. No hidden defaults.
 */
struct Environment {
    double temperature{0.0};     // T (K)
    double pressure{0.0};        // P (atm)

    // External fields
    double E_field[3]{};         // Electric field (V/Å)
    double B_field[3]{};         // Magnetic field (T)

    // Radiation / photon coupling
    double radiation_flux{0.0};  // Incident radiation flux (W/m²)
    double radiation_eV{0.0};    // Photon energy (eV), 0 = broadband/off

    // Flow / shear (for non-equilibrium)
    double shear_rate{0.0};      // Shear rate (1/ps)
    double flow_velocity[3]{};   // Bulk flow velocity (Å/ps)

    bool has_fields() const {
        return (E_field[0] != 0 || E_field[1] != 0 || E_field[2] != 0 ||
                B_field[0] != 0 || B_field[1] != 0 || B_field[2] != 0);
    }

    bool has_flow() const {
        return (shear_rate != 0 ||
                flow_velocity[0] != 0 || flow_velocity[1] != 0 || flow_velocity[2] != 0);
    }
};

// ============================================================================
// Constraints (K) — rules the system must obey
// ============================================================================

/**
 * Constraints encode geometry restrictions, bonding rules, and
 * allowed state transitions.
 *
 * These are hard rules, not soft potentials. If a constraint is
 * violated, the engine rejects the move / flags the state.
 */
struct Constraints {
    // Geometry constraints
    bool fix_positions{false};           // Freeze all positions
    std::vector<uint32_t> frozen_atoms;  // Indices of frozen atoms (subset)

    // Bonding constraints
    bool allow_bond_breaking{false};     // Can bonds be removed?
    bool allow_bond_forming{false};      // Can new bonds be created?
    uint32_t max_coordination{0};        // Max bonds per atom (0 = unconstrained)

    // Transition rules
    bool allow_composition_change{false}; // Can the system change resolution level?
    bool allow_particle_creation{false};  // Can new particles appear (e.g. dissociation)?
    bool allow_particle_destruction{false}; // Can particles be removed?

    // Symmetry
    std::string symmetry_group;          // Point/space group constraint ("" = none)

    bool is_fully_constrained() const {
        return fix_positions;
    }

    bool is_reactive() const {
        return allow_bond_breaking || allow_bond_forming;
    }
};

// ============================================================================
// SourceTag (Sigma) — provenance / data lineage
// ============================================================================

/**
 * SourceTag carries the full provenance chain for a state.
 *
 * Where did this state come from? What generated it? What was the
 * input? What version of the engine produced it?
 *
 * This is the "traceable data lineage" component. If you cannot
 * answer "where did this number come from?" then your simulation
 * is not reproducible.
 */
struct SourceTag {
    std::string origin;              // "file:water.xyz", "builder:formula", "phase_10A:Cl"
    std::string engine_version;      // "VSEPR-SIM 2.6.0"
    std::string hash;                // SHA-512 of the generating pathway (if applicable)
    uint64_t    creation_step{0};    // Step at which this state was created
    uint64_t    parent_id{0};        // ID of the parent state (0 = root)
    std::string notes;               // Human-readable annotation
};

// ============================================================================
// ModelLevel (Lambda) — abstraction fidelity
// ============================================================================

/**
 * ModelLevel encodes the fidelity of the transformation operator T.
 *
 * This is the Λ parameter in S_f = T(S_0, t, Λ).
 *
 * At C-level (the current project scope), we are not resolving
 * quantum mechanics or full electronic structure. We are mapping
 * states through controlled classical approximation.
 *
 * The level tells the engine HOW MUCH physics to include in T.
 */
enum class ModelLevel : uint8_t {
    C_Empirical   = 0,  // Classical empirical (harmonic bonds, LJ, Coulomb)
    C_Reactive    = 1,  // Classical with bond-order potentials (ReaxFF-like)
    C_Polarizable = 2,  // Classical with induced dipoles / SCF polarization
    CG_Reduced    = 3,  // Coarse-grained reduced model
    CG_Enriched   = 4,  // Coarse-grained enriched descriptor model
    Hybrid_AdRes  = 5   // Adaptive resolution (atomistic core + CG shell)
};

inline const char* model_level_name(ModelLevel lv) {
    switch (lv) {
        case ModelLevel::C_Empirical:   return "C-Empirical";
        case ModelLevel::C_Reactive:    return "C-Reactive";
        case ModelLevel::C_Polarizable: return "C-Polarizable";
        case ModelLevel::CG_Reduced:    return "CG-Reduced";
        case ModelLevel::CG_Enriched:   return "CG-Enriched";
        case ModelLevel::Hybrid_AdRes:  return "Hybrid-AdRes";
        default:                        return "unknown";
    }
}

// ============================================================================
// StateC (S_0) — Initial State Container
// ============================================================================

/**
 * StateC — the Dev Day ~42 initial state container.
 *
 *   S_0 = { m_0, E_0, x_0, v_0, C, E, K, Sigma }
 *
 * "Clean, neutral, non-annoying."
 *
 * This replaces the overloaded symbol I which collides with:
 *   - electric current
 *   - moment of inertia
 *   - identity matrix
 *   - intensity
 *   - every other field in physics
 *
 * The struct can be constructed from an existing atomistic::State
 * (zero-copy view) or populated independently.
 */
struct StateC {
    double mass{0.0};              // m_0: total system mass (amu)
    double energy{0.0};            // E_0: total system energy (kcal/mol)
    Vec3   position{};             // x_0: center of mass position (Å)
    Vec3   velocity{};             // v_0: center of mass velocity (Å/ps)

    Composition   comp{Composition::Atomistic};  // C: representation level
    Environment   env{};                          // E: external conditions
    Constraints   constraints{};                  // K: rules / restrictions
    SourceTag     provenance{};                   // Σ: data lineage

    // --- Link to full atomistic state (optional, non-owning) ---
    // When the StateC wraps a full State, this points to the detailed data.
    // When StateC is a summary (e.g. for reporting), this is nullptr.
    const State* detail{nullptr};

    // --- Factory: build from atomistic::State ---
    static StateC from_state(const State& s, const SourceTag& prov = {}) {
        StateC sc;

        // Total mass
        sc.mass = 0.0;
        for (uint32_t i = 0; i < s.N; ++i)
            sc.mass += s.M[i];

        // Total energy
        sc.energy = s.E.total();

        // Center of mass position and velocity
        if (s.N > 0 && sc.mass > 0.0) {
            Vec3 com{}, cov{};
            for (uint32_t i = 0; i < s.N; ++i) {
                com.x += s.M[i] * s.X[i].x;
                com.y += s.M[i] * s.X[i].y;
                com.z += s.M[i] * s.X[i].z;
                cov.x += s.M[i] * s.V[i].x;
                cov.y += s.M[i] * s.V[i].y;
                cov.z += s.M[i] * s.V[i].z;
            }
            sc.position = com * (1.0 / sc.mass);
            sc.velocity = cov * (1.0 / sc.mass);
        }

        sc.comp = Composition::Atomistic;
        sc.provenance = prov;
        sc.detail = &s;

        return sc;
    }
};

// ============================================================================
// Structural State (Phi) — geometry / topology class of the outcome
// ============================================================================

/**
 * StructuralState describes the geometry/topology of the final state.
 *
 * After transformation, the system lands in a structural class.
 * This is the Φ in the outcome state S_f.
 */
struct StructuralState {
    std::string geometry_class;   // "linear", "trigonal_planar", "tetrahedral", etc.
    std::string topology;         // "chain", "ring", "branched", "lattice", "amorphous"
    double      symmetry_score{}; // 0 = no symmetry, 1 = perfect
    uint32_t    coordination{};   // Average coordination number
};

// ============================================================================
// StabilityMetric (S in S_f) — how stable is the outcome
// ============================================================================

/**
 * StabilityMetric quantifies the thermodynamic and kinetic stability
 * of the outcome state.
 */
struct StabilityMetric {
    double energy_per_particle{};  // E/N (kcal/mol/particle)
    double rms_force{};            // RMS force magnitude (kcal/(mol·Å))
    double max_force{};            // Maximum force component
    bool   is_local_minimum{false}; // Hessian eigenvalues > 0?
    bool   is_converged{false};     // Below force tolerance?
};

// ============================================================================
// PerformanceProperties (Pi) — transport / material properties
// ============================================================================

/**
 * Transport and material properties computed from the outcome state.
 *
 * These are the quantities a researcher actually wants from the
 * simulation. Everything else is scaffolding to get here.
 */
struct PerformanceProperties {
    double diffusion_coeff{};     // D (Å²/ps)
    double viscosity{};           // η (cP)
    double thermal_conductivity{}; // κ (W/(m·K))
    double bulk_modulus{};        // K (GPa)
    double density{};             // ρ (g/cm³)
    double melting_point{};       // T_m (K), 0 = not computed
};

// ============================================================================
// EventLog (Omega) — what happened during transformation
// ============================================================================

/**
 * EventRecord captures a single discrete event during transformation.
 */
struct EventRecord {
    uint64_t    step{};           // When it happened
    std::string type;             // "bond_break", "bond_form", "phase_transition", "failure"
    std::string description;      // Human-readable detail
    double      energy_delta{};   // Energy change associated with event
};

/**
 * EventLog is the complete Ω for an outcome.
 */
struct EventLog {
    std::vector<EventRecord> events;

    void record(uint64_t step, const std::string& type,
                const std::string& desc, double dE = 0.0) {
        events.push_back({step, type, desc, dE});
    }

    int count_by_type(const std::string& type) const {
        int n = 0;
        for (const auto& e : events)
            if (e.type == type) ++n;
        return n;
    }

    double total_energy_delta() const {
        double sum = 0.0;
        for (const auto& e : events)
            sum += e.energy_delta;
        return sum;
    }
};

// ============================================================================
// QualityFlags (Q) — confidence / quality of the result
// ============================================================================

/**
 * QualityFlags indicate how much you should trust the result.
 *
 * If energy_conserved is false, your "approximation layer" quietly
 * became fiction.
 */
struct QualityFlags {
    bool   energy_conserved{true};   // E_0 + E_in - E_out ≈ E_f + E_loss?
    bool   mass_conserved{true};     // m_0 + m_in - m_out ≈ m_f?
    bool   forces_converged{true};   // Below tolerance?
    bool   symmetry_preserved{true}; // Symmetry constraints respected?
    double energy_drift{0.0};        // Absolute drift in total energy
    double mass_drift{0.0};          // Absolute drift in total mass
    int    warning_count{0};         // Number of non-fatal warnings

    bool is_trustworthy() const {
        return energy_conserved && mass_conserved && forces_converged;
    }
};

// ============================================================================
// OutcomeState (S_f) — the final state after transformation
// ============================================================================

/**
 * OutcomeState — Dev Day ~42 outcome container.
 *
 *   S_f = { m_f, E_f, Phi, S, Pi, Omega, Q }
 *
 * This is what T(S_0, t, Λ) returns.
 */
struct OutcomeState {
    double mass{0.0};                     // m_f: final total mass
    double energy{0.0};                   // E_f: final total energy

    StructuralState      structure{};     // Φ: geometry/topology class
    StabilityMetric      stability{};     // S: stability metric
    PerformanceProperties properties{};   // Π: transport properties
    EventLog             events{};        // Ω: event log
    QualityFlags         quality{};       // Q: confidence flags

    // --- Link to full atomistic state (optional) ---
    const State* detail{nullptr};

    // --- Provenance chain: which S_0 produced this ---
    SourceTag origin{};
};

// ============================================================================
// Mass-Energy Sanity Constraint
// ============================================================================

/**
 * EnergyBalance — tracks the mass-energy sanity constraint.
 *
 * Even at C-level, enforce:
 *   E_0 + E_in - E_out ≈ E_f + E_loss
 *   m_0 + m_in - m_out ≈ m_f
 *
 * If this drifts, your "approximation layer" quietly becomes fiction.
 */
struct EnergyBalance {
    double E_0{};           // Initial energy
    double E_in{};          // Energy injected (thermostat, fields, etc.)
    double E_out{};         // Energy removed (friction, thermostat, etc.)
    double E_f{};           // Final energy
    double E_loss{};        // Dissipative losses (explicit, tracked)

    double m_0{};           // Initial mass
    double m_in{};          // Mass added (particle creation)
    double m_out{};         // Mass removed (particle destruction)
    double m_f{};           // Final mass

    double energy_drift() const {
        return std::abs((E_0 + E_in - E_out) - (E_f + E_loss));
    }

    double mass_drift() const {
        return std::abs((m_0 + m_in - m_out) - m_f);
    }

    bool energy_sane(double tol = 1e-6) const {
        return energy_drift() < tol;
    }

    bool mass_sane(double tol = 1e-10) const {
        return mass_drift() < tol;
    }

    QualityFlags check(double e_tol = 1e-6, double m_tol = 1e-10) const {
        QualityFlags q;
        q.energy_conserved = energy_sane(e_tol);
        q.mass_conserved   = mass_sane(m_tol);
        q.energy_drift     = energy_drift();
        q.mass_drift       = mass_drift();
        return q;
    }
};

// ============================================================================
// Transformation Operator: S_f = T(S_0, t, Lambda)
// ============================================================================

/**
 * TransformConfig — parameters for the transformation operator T.
 *
 *   S_f = T(S_0, t, Λ)
 *
 * Where:
 *   T  = transformation operator (your engine)
 *   t  = time / iteration domain
 *   Λ  = model fidelity level
 */
struct TransformConfig {
    double     dt{0.001};            // Time step (ps)
    uint64_t   n_steps{1000};        // Number of steps
    ModelLevel level{ModelLevel::C_Empirical}; // Λ: fidelity

    // Convergence criteria (for optimization)
    double tol_rms_force{1e-3};      // RMS force tolerance
    double tol_max_force{1e-3};      // Max force tolerance
    double tol_energy{1e-8};         // Energy change tolerance

    // Conservation tolerance (sanity checks)
    double energy_conservation_tol{1e-4};  // E drift alarm threshold
    double mass_conservation_tol{1e-10};   // m drift alarm threshold

    // Control
    bool   track_events{true};       // Record Ω events?
    bool   compute_properties{false}; // Compute Π at end?
    int    snapshot_interval{100};    // Save frames every N steps
};

/**
 * evolve — The transformation operator.
 *
 *   OutcomeState evolve(const StateC& s0, double dt, ModelLevel level)
 *
 * This is the whole point: you are mapping states through controlled
 * abstraction. Not solving reality. Not pretending to be mystical.
 *
 * The function:
 *   1. Validates S_0 (mass > 0, energy finite, composition set)
 *   2. Initializes the energy balance ledger
 *   3. Runs the transformation loop for n_steps at fidelity Λ
 *   4. Checks mass-energy sanity at each step
 *   5. Classifies the structural outcome Φ
 *   6. Computes stability metric S
 *   7. Optionally computes transport properties Π
 *   8. Assembles event log Ω
 *   9. Sets quality flags Q
 *  10. Returns S_f
 *
 * If the energy balance drifts beyond tolerance, Q.energy_conserved
 * is set to false and Q.warning_count is incremented. The simulation
 * does NOT silently continue with broken bookkeeping.
 */
inline OutcomeState evolve(const StateC& s0,
                           const TransformConfig& config)
{
    OutcomeState sf;

    // --- Validate S_0 ---
    if (s0.mass <= 0.0) {
        sf.quality.energy_conserved = false;
        sf.quality.warning_count = 1;
        sf.events.record(0, "failure", "S_0 has non-positive mass", 0.0);
        return sf;
    }
    if (!std::isfinite(s0.energy)) {
        sf.quality.energy_conserved = false;
        sf.quality.warning_count = 1;
        sf.events.record(0, "failure", "S_0 has non-finite energy", 0.0);
        return sf;
    }

    // --- Initialize energy balance ledger ---
    EnergyBalance balance;
    balance.E_0 = s0.energy;
    balance.m_0 = s0.mass;
    balance.m_f = s0.mass;  // No particle creation/destruction by default

    // --- If we have a full State, run the transformation loop ---
    if (s0.detail && sane(*s0.detail)) {
        // Working copy of energy for tracking
        double running_energy = s0.energy;

        for (uint64_t step = 0; step < config.n_steps; ++step) {

            // [PLACEHOLDER] In the real engine, this is where the
            // integrator (FIRE, velocity Verlet, Langevin, etc.)
            // advances the state by dt at fidelity level Λ.
            //
            // The force evaluation, thermostat coupling, PBC wrapping,
            // and constraint enforcement all happen here.
            //
            // For now, we track the bookkeeping framework without
            // coupling to a specific integrator.

            // --- Mass-energy sanity check (every step) ---
            balance.E_f = running_energy;
            if (!balance.energy_sane(config.energy_conservation_tol)) {
                if (config.track_events) {
                    sf.events.record(step, "warning",
                        "energy drift exceeded tolerance: " +
                        std::to_string(balance.energy_drift()),
                        balance.energy_drift());
                }
                sf.quality.energy_conserved = false;
                sf.quality.warning_count++;
            }
        }

        // --- Final bookkeeping ---
        balance.E_f = running_energy;
        balance.m_f = s0.mass;  // Static particle count for now
    }
    else {
        // No detail state — summary-level evolution only
        balance.E_f = s0.energy;
        balance.m_f = s0.mass;
    }

    // --- Populate outcome ---
    sf.mass   = balance.m_f;
    sf.energy = balance.E_f;

    // Structural state (placeholder — real classification from geometry analysis)
    sf.structure.geometry_class = "unclassified";
    sf.structure.topology       = "unclassified";

    // Stability
    sf.stability.energy_per_particle = (s0.detail && s0.detail->N > 0)
        ? balance.E_f / s0.detail->N : 0.0;
    sf.stability.is_converged = sf.quality.energy_conserved;

    // Quality flags from balance check
    QualityFlags balance_q = balance.check(
        config.energy_conservation_tol,
        config.mass_conservation_tol);
    sf.quality.energy_conserved = balance_q.energy_conserved;
    sf.quality.mass_conserved   = balance_q.mass_conserved;
    sf.quality.energy_drift     = balance_q.energy_drift;
    sf.quality.mass_drift       = balance_q.mass_drift;

    // Provenance chain
    sf.origin = s0.provenance;

    return sf;
}

// ============================================================================
// Convenience: one-line evolve with explicit (dt, level) signature
// ============================================================================

/**
 * evolve — simplified signature matching the implementation hint.
 *
 *   StateC evolve(const StateC& s0, double dt, ModelLevel level);
 *
 * Constructs a default TransformConfig and delegates.
 * Returns a StateC (not OutcomeState) for composability.
 */
inline StateC evolve(const StateC& s0, double dt, ModelLevel level)
{
    TransformConfig config;
    config.dt    = dt;
    config.level = level;
    config.n_steps = 1;  // Single step for the simplified API

    OutcomeState sf = evolve(s0, config);

    // Wrap outcome back into StateC for chaining: T(T(T(S_0)))
    StateC result;
    result.mass        = sf.mass;
    result.energy      = sf.energy;
    result.position    = s0.position;  // Updated by integrator in real impl
    result.velocity    = s0.velocity;  // Updated by integrator in real impl
    result.comp        = s0.comp;
    result.env         = s0.env;
    result.constraints = s0.constraints;

    // Chain provenance
    result.provenance        = s0.provenance;
    result.provenance.notes += " -> evolve(dt=" + std::to_string(dt)
                            +  ", Λ=" + model_level_name(level) + ")";
    result.provenance.creation_step++;

    return result;
}

} // namespace atomistic
