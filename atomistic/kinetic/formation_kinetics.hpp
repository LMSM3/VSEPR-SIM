#pragma once
/**
 * formation_kinetics.hpp
 * ======================
 * Universal Kinetic Formation Engine
 *
 * Architectural principle:
 *   Universal kinetic engine + domain-specific state model
 *
 * The engine does NOT separate organics, metals, droplets, crystals,
 * beams, decay, or salt chemistry into distinct kinetic engines.
 * Instead, it provides:
 *
 *   K = { S, E, R, U }
 *
 *   S : state space       (what exists right now)
 *   E : event space       (what CAN happen)
 *   R : rate laws         (how fast each event occurs)
 *   U : update rules      (how the state changes after an event)
 *
 * Domain-specific meaning is injected through DomainAdapter:
 *   - Organics:      connectivity, torsion, steric, polarity
 *   - Metals/crystal: lattice, vacancy, dislocation, recrystallization
 *   - Droplet/beam:   radius, velocity, temperature, oxidation surface
 *   - Decay:          isotope identity, half-life, daughter chain
 *
 * The critical distinction:
 *   "Most stable" ≠ "Most likely to actually form"
 *   Thermodynamic winner ≠ Kinetic winner
 *
 * This engine resolves that distinction by computing BOTH rankings
 * and presenting them as first-class outputs.
 *
 * Compatibility: fits alongside atomistic::reaction::ReactionEngine
 * (which handles topology rewrites and Fukui scoring).  This engine
 * operates one layer above: it decides which transitions are attempted,
 * at what rate, and tracks time evolution.
 *
 * Anti-black-box: every rate, every barrier, every event selection
 * decision is inspectable and deterministic given seed.
 */

#include "../core/state.hpp"
#include "../core/environment.hpp"
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <array>
#include <map>
#include <functional>
#include <memory>
#include <random>
#include <optional>

namespace atomistic {
namespace kinetic {

// ============================================================================
// Forward declarations
// ============================================================================
struct KineticState;
struct TransitionEvent;
struct RateLaw;
struct FormationScore;
class  DomainAdapter;
class  FormationKineticsEngine;

// ============================================================================
// Kinetic state: the universal "what exists right now"
// ============================================================================

/**
 * A KineticState wraps the atomistic State and adds kinetic bookkeeping.
 * Domain-specific meaning is carried in the properties map and the
 * domain tag, NOT in the struct layout.
 */
struct KineticState {
    // Core atomistic state (positions, bonds, elements, charges)
    State atomistic;

    // Time
    double time_s = 0.0;

    // Domain tag: "organic", "crystal", "droplet", "decay", "beam", ...
    std::string domain;

    // Scalar properties (domain-dependent meaning)
    // Keys are human-readable: "temperature_K", "pressure_GPa",
    // "droplet_radius_nm", "oxidation_state", "decay_age_s", etc.
    std::map<std::string, double> properties;

    // History: accumulated event trail (kept compact)
    uint64_t events_applied = 0;

    // Formation identity hash (tracks lineage through transformations)
    uint64_t lineage_hash = 0;
};

// ============================================================================
// Transition events: the universal "what CAN happen"
// ============================================================================

/**
 * TransitionEvent types — universal across all domains.
 * Domain adapters translate these into chemical / physical meaning.
 */
enum class EventType : uint8_t {
    // Chemical topology changes
    BOND_FORMATION,
    BOND_BREAKING,

    // Surface / interface events
    ADSORPTION,
    DESORPTION,

    // Collision / transport
    IMPACT,
    REBOUND,
    COALESCENCE,

    // Phase events
    NUCLEATION,
    PHASE_CHANGE,
    FREEZING,

    // Migration / diffusion
    DIFFUSION_JUMP,
    VACANCY_MIGRATION,

    // Redox
    OXIDATION,
    REDUCTION,

    // Nuclear (rates from hazard / deterministic law, NOT chemical barrier)
    DECAY,
    TRANSMUTATION,

    // Structural rearrangement
    REARRANGEMENT,
    DISLOCATION_MOTION,
    DEFECT_ANNIHILATION,
    RECRYSTALLIZATION,

    // Fragmentation / decomposition
    FRAGMENTATION,
    DECOMPOSITION,

    // Deposition / etching (surface)
    DEPOSITION,
    ETCHING,

    // Evaporation / condensation
    EVAPORATION,
    CONDENSATION,

    // Emission (nuclear / radiative)
    // Note: if the emission particle does not yet exist in the model,
    // the engine records the event and awaits context for how the
    // daughter / emission product must be coded.  No silent invention.
    ALPHA_EMISSION,
    BETA_EMISSION,
    GAMMA_EMISSION,
    HELIUM_ACCUMULATION,

    // Generic placeholder for domain-specific extensions
    DOMAIN_SPECIFIC,
};

/**
 * A single transition candidate: "this event could happen next".
 */
struct TransitionEvent {
    EventType type = EventType::DOMAIN_SPECIFIC;
    std::string label;               // Human-readable: "SN2 at C3", "vacancy hop +x"

    // Affected atom / site indices (context-dependent)
    std::vector<uint32_t> site_indices;

    // Barrier / hazard (units depend on domain)
    double activation_barrier_eV = 0.0;
    double rate_s_inv            = 0.0;  // Arrhenius or hazard rate

    // Thermodynamic drive
    double delta_G_eV            = 0.0;  // Gibbs energy change
    double delta_E_eV            = 0.0;  // Internal energy change (final - initial)

    // Environmental modifiers (pre-computed by DomainAdapter)
    double solvent_factor        = 1.0;  // 1.0 = no solvent effect
    double crowding_factor       = 1.0;  // steric / packing
    double path_accessibility    = 1.0;  // orbital overlap / geometric feasibility

    // Scores (computed by the engine)
    double thermo_score          = 0.0;  // [0,1] thermodynamic favorability
    double kinetic_score         = 0.0;  // [0,1] kinetic accessibility
    double env_score             = 0.0;  // [0,1] environmental steering
    double history_score         = 0.0;  // [0,1] formation-history penalty/bonus

    // Domain-specific metadata (free-form, inspectable)
    std::map<std::string, double> metadata;
};

// ============================================================================
// Rate law: the universal "how fast each event occurs"
// ============================================================================

/**
 * RateLawType determines how k_i is computed for a transition.
 * The engine dispatches on this tag.
 */
enum class RateLawType : uint8_t {
    ARRHENIUS,            // k = A·exp(-Ea / kT)
    HAZARD,               // nuclear decay: λ = ln(2) / t_half
    COLLISION_THEORY,     // k = σ·v_rel·n
    BARRIERLESS,          // k = pre-exponential only (diffusion-limited)
    DETERMINISTIC_STEP,   // event happens at fixed Δt (MD-like)
    WEIGHTED_HYBRID,      // weighted sum of sub-laws
    CUSTOM,               // DomainAdapter provides lambda
};

struct RateLaw {
    RateLawType type = RateLawType::ARRHENIUS;

    // Arrhenius parameters
    double pre_exponential_s_inv = 1.0e13;  // A (typical bond vibration)
    double activation_energy_eV  = 0.0;     // Ea

    // Hazard parameters (nuclear)
    double half_life_s           = 0.0;     // t_1/2 (0 = stable)

    // Collision theory
    double cross_section_A2      = 0.0;     // σ
    double relative_velocity_m_s = 0.0;     // v_rel
    double number_density_m3_inv = 0.0;     // n

    // Custom: domain adapter supplies this
    std::function<double(const KineticState&, const TransitionEvent&)> custom_rate;

    /**
     * Evaluate rate at given temperature.
     * For HAZARD type, temperature is ignored (nuclear).
     */
    double evaluate(double temperature_K,
                    const KineticState& state,
                    const TransitionEvent& event) const;
};

// ============================================================================
// Formation score: the central output — "most stable" vs "most likely"
// ============================================================================

/**
 * FormationScore distinguishes thermodynamic winner from kinetic winner.
 * This is the first-class concept the engine exists to produce.
 *
 * P(product_i) ~ f(E_i, ΔG_i, Ea_i, τ, T, solvent, crowding, path_access)
 */
struct FormationScore {
    std::string candidate_id;

    // ── Thermodynamic branch ──────────────────────────────────────────────
    double energy_final_eV        = 0.0;  // Final static energy (lowest = best)
    double delta_G_eV             = 0.0;  // Gibbs energy of formation
    double thermo_rank            = 0.0;  // Lower = more thermodynamically stable

    // ── Kinetic branch ────────────────────────────────────────────────────
    //
    //  S_kinetic = w1·A_path + w2·R_encounter + w3·S_steric
    //            + w4·T_window + w5·E_barrier
    //
    double A_path                 = 0.0;  // Pathway accessibility       [0,1]
    double R_encounter            = 0.0;  // Reactant encounter rate     [0,1]
    double S_steric               = 0.0;  // Steric permissibility       [0,1]
    double T_window               = 0.0;  // Temperature suitability     [0,1]
    double E_barrier_penalty      = 0.0;  // Activation barrier penalty  [0,1]
    double kinetic_score          = 0.0;  // Weighted sum
    double kinetic_rank           = 0.0;  // Lower = forms faster

    // ── Environmental branch ──────────────────────────────────────────────
    double solvent_score          = 0.0;
    double crowding_score         = 0.0;
    double env_score              = 0.0;

    // ── Data branch (from HGST / meta-scores) ────────────────────────────
    double data_score             = 0.0;  // γ, Q_data, C_compact contribution

    // ── Combined formation likelihood ─────────────────────────────────────
    //
    //  S_form = α·S_thermo + β·S_kinetic + χ·S_env + δ·S_data
    //
    double S_form                 = 0.0;
    double form_rank              = 0.0;  // Lower = most likely observed product

    // ── Interpretation ────────────────────────────────────────────────────
    std::string expected_observation;
    // e.g. "likely after long equilibration" / "likely early product" / "transient intermediate"
};

// ── Kinetic score weights (tunable, exposed for anti-black-box) ────────────
struct KineticWeights {
    double w_path      = 0.20;
    double w_encounter = 0.20;
    double w_steric    = 0.20;
    double w_Twindow   = 0.15;
    double w_barrier   = 0.25;

    // Formation composite weights
    double alpha_thermo = 0.30;   // S_thermo weight
    double beta_kinetic = 0.35;   // S_kinetic weight
    double chi_env      = 0.20;   // S_env weight
    double delta_data   = 0.15;   // S_data weight
};

// ============================================================================
// Candidate ranking table: the engine's summary output
// ============================================================================

/**
 * One row of the thermo-vs-kinetic ranking table.
 *
 *  Candidate | Thermo Rank | Kinetic Rank | Expected Observation
 *  A         | 1           | 3            | likely after long equilibration
 *  B         | 2           | 1            | likely early product
 *  C         | 3           | 2            | transient intermediate
 */
struct CandidateRow {
    std::string id;
    int    thermo_rank  = 0;
    int    kinetic_rank = 0;
    double thermo_score = 0.0;
    double kinetic_score = 0.0;
    double S_form       = 0.0;
    std::string expected_observation;
};

using RankingTable = std::vector<CandidateRow>;

// ============================================================================
// Domain adapter: injects domain-specific meaning
// ============================================================================

/**
 * DomainAdapter is the extension point.
 * Each domain (organic, crystal, droplet, decay, beam) provides one.
 * The kinetic engine calls it; it never subclasses the engine.
 *
 * This is NOT an inheritance hierarchy.  One engine, many adapters.
 */
class DomainAdapter {
public:
    virtual ~DomainAdapter() = default;

    /// Domain tag: "organic", "crystal", "droplet", "decay", "beam", etc.
    virtual std::string domain_tag() const = 0;

    /// Enumerate all possible transitions from current state.
    virtual std::vector<TransitionEvent>
    enumerate_transitions(const KineticState& state) const = 0;

    /// Provide the rate law for a specific event type in this domain.
    virtual RateLaw
    rate_law_for(EventType type, const KineticState& state) const = 0;

    /// Apply an event to the state, returning the updated state.
    /// If the event requires a particle that does not yet exist in the
    /// model (e.g. an alpha particle in a decay chain), the adapter MUST
    /// record the event and leave a placeholder — never silently invent data.
    virtual KineticState
    apply_event(const KineticState& state, const TransitionEvent& event) const = 0;

    /// Compute environmental modifiers for an event.
    virtual void
    compute_environment(const KineticState& state,
                        TransitionEvent& event) const = 0;

    /// Compute history-dependent score (path memory, prior events).
    virtual double
    history_score(const KineticState& state,
                  const TransitionEvent& event) const = 0;
};

// ============================================================================
// Universal Kinetic Formation Engine
// ============================================================================

class FormationKineticsEngine {
public:
    FormationKineticsEngine();
    explicit FormationKineticsEngine(uint64_t seed);

    // ── Configuration ─────────────────────────────────────────────────────
    void set_weights(const KineticWeights& w) { weights_ = w; }
    const KineticWeights& weights() const     { return weights_; }

    void set_temperature(double T_K)          { temperature_K_ = T_K; }
    double temperature() const                { return temperature_K_; }

    void set_max_events(uint64_t n)           { max_events_ = n; }
    void set_max_time(double t_s)             { max_time_s_ = t_s; }

    // ── Domain adapter registration ───────────────────────────────────────
    void set_adapter(std::shared_ptr<DomainAdapter> adapter) {
        adapter_ = std::move(adapter);
    }

    // ── Single-step evolution ─────────────────────────────────────────────

    /**
     * Enumerate transitions, score them, select one, apply it.
     * Returns the transition that was applied (or nullopt if no
     * feasible transition exists).
     */
    std::optional<TransitionEvent>
    step(KineticState& state);

    // ── Multi-step evolution ──────────────────────────────────────────────

    /**
     * Run the kinetic engine until max_events or max_time is reached.
     * Returns the event trail.
     */
    std::vector<TransitionEvent>
    evolve(KineticState& state);

    // ── Formation scoring ─────────────────────────────────────────────────

    /**
     * Score a single candidate product.
     *
     * P(product_i) ~ f(E_i, ΔG_i, Ea_i, τ, T, solvent, crowding, path)
     */
    FormationScore
    score_candidate(const KineticState& candidate,
                    const TransitionEvent& forming_event) const;

    /**
     * Rank multiple candidates and produce the comparison table.
     * Separates thermodynamic winner from kinetic winner.
     */
    RankingTable
    rank_candidates(const std::vector<std::pair<KineticState, TransitionEvent>>& candidates) const;

    // ── Inspectable internals (anti-black-box) ────────────────────────────
    uint64_t events_applied() const { return events_applied_; }
    double   elapsed_time()   const { return elapsed_time_s_; }

private:
    // Score sub-components
    double compute_thermo_score(const TransitionEvent& ev) const;
    double compute_kinetic_score(const TransitionEvent& ev) const;
    double compute_env_score(const TransitionEvent& ev) const;

    // Event selection
    TransitionEvent
    select_event(const std::vector<TransitionEvent>& candidates);

    std::shared_ptr<DomainAdapter> adapter_;
    KineticWeights weights_;
    double temperature_K_ = 298.15;
    uint64_t max_events_  = 10000;
    double   max_time_s_  = 1.0;

    // Bookkeeping
    uint64_t events_applied_ = 0;
    double   elapsed_time_s_ = 0.0;
    std::mt19937_64 rng_;
};

// ============================================================================
// Inline: RateLaw::evaluate
// ============================================================================

inline double RateLaw::evaluate(double temperature_K,
                                const KineticState& state,
                                const TransitionEvent& event) const {
    constexpr double kB_eV = 8.617333262e-5;  // Boltzmann in eV/K

    switch (type) {
    case RateLawType::ARRHENIUS: {
        if (temperature_K <= 0.0) return 0.0;
        double kT = kB_eV * temperature_K;
        return pre_exponential_s_inv * std::exp(-activation_energy_eV / kT);
    }
    case RateLawType::HAZARD: {
        if (half_life_s <= 0.0) return 0.0;  // stable isotope
        return std::log(2.0) / half_life_s;
    }
    case RateLawType::COLLISION_THEORY: {
        return cross_section_A2 * 1.0e-20 * relative_velocity_m_s * number_density_m3_inv;
    }
    case RateLawType::BARRIERLESS: {
        return pre_exponential_s_inv;  // diffusion-limited
    }
    case RateLawType::DETERMINISTIC_STEP: {
        return 1.0;  // always fires at Δt
    }
    case RateLawType::CUSTOM: {
        return custom_rate ? custom_rate(state, event) : 0.0;
    }
    default:
        return 0.0;
    }
}

} // namespace kinetic
} // namespace atomistic
