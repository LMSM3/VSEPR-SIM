#pragma once
/**
 * clean_gate.hpp
 * ==============
 * Lightweight Adaptive Verification Gate
 *
 * CleanGate samples prerequisite and performance modules across tiers
 * T1-T50, computes interpretable risk variables (weirdness_score,
 * temperature_score, convergence_confidence), and escalates to verbose
 * or deep verification only when structural irregularity, thermal
 * severity, or low convergence confidence justify it.
 *
 * Design rationale:
 *   Not every case deserves 250 checks.  CleanGate replaces the
 *   religious-ritual approach with stratified semi-random sampling
 *   and risk-driven escalation.
 *
 * Sampling strategy (semi-random, stratified):
 *   - Always run 2-3 fixed core modules  (identity, bounds, geometry)
 *   - Choose 2 from T1-T10   (low tier)
 *   - Choose 2 from T11-T25  (mid tier)
 *   - Choose 2 from T26-T50  (high tier)
 *   - Add 1 targeted module for detected case type
 *
 * Risk variables:
 *   Weirdness W   = w1*C + w2*G + w3*E + w4*B + w5*R
 *   Temperature Ts = a1*(T/Tref) + a2*(|dP/dT|/S) + a3*Phi_phase + a4*Phi_noise
 *
 * Escalation:
 *   low  risk  -> continue normally
 *   moderate   -> enable verbose tracking
 *   high       -> launch deeper gate modules or mark provisional
 *
 * Emergence classification:
 *   0 = no meaningful emergence
 *   1 = emergence visible, weakly verified
 *   2 = emergence visible, strongly verified
 *
 * Anti-black-box: every score, every sampled module name, every
 * escalation decision is explicitly recorded in CleanGateReport.
 */

#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>
#include <sstream>
#include <iomanip>

namespace atomistic {
namespace validation {

// ============================================================================
// Gate tier classification
// ============================================================================

enum class GateTier : uint8_t {
    CORE   = 0,   // Always run — identity, bounds, geometry
    LOW    = 1,   // T1-T10:  formatting, bounds, species coverage
    MID    = 2,   // T11-T25: geometry plausibility, energy sign, bonding
    HIGH   = 3,   // T26-T50: partial relaxation, local stability, thermal
    TARGET = 4    // Case-type targeted module
};

inline const char* tier_name(GateTier t) {
    switch (t) {
        case GateTier::CORE:   return "CORE";
        case GateTier::LOW:    return "LOW";
        case GateTier::MID:    return "MID";
        case GateTier::HIGH:   return "HIGH";
        case GateTier::TARGET: return "TARGET";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// Gate module descriptor
// ============================================================================

struct GateModule {
    uint32_t    id;
    GateTier    tier;
    const char* name;
    const char* description;
    bool        is_fatal;      // true = failure blocks the pipeline

    // Execution result (filled after run)
    bool   executed  = false;
    bool   passed    = true;
    double score     = 1.0;    // [0,1] quality/confidence
    std::string detail;
};

// ============================================================================
// Weirdness score components
// ============================================================================

struct WeirdnessComponents {
    double C_class_uncertain   = 0.0;  // Hard to classify            [0,1]
    double G_geom_irregular    = 0.0;  // Geometry looks abnormal     [0,1]
    double E_energy_conflict   = 0.0;  // Cheap models disagree       [0,1]
    double B_bond_ambiguity    = 0.0;  // Bonding assignment unstable [0,1]
    double R_reference_distance= 0.0;  // Far from known families     [0,1]
};

struct WeirdnessWeights {
    double w1 = 0.20;  // classification uncertainty
    double w2 = 0.25;  // geometric irregularity
    double w3 = 0.20;  // energy conflict
    double w4 = 0.15;  // bond ambiguity
    double w5 = 0.20;  // reference distance

    double sum() const { return w1 + w2 + w3 + w4 + w5; }
};

/**
 * Compute weirdness score W in [0,1].
 *
 * W = w1*C + w2*G + w3*E + w4*B + w5*R
 *
 * Classification bands:
 *   0.00-0.25: ordinary
 *   0.25-0.50: unusual
 *   0.50-0.75: highly irregular
 *   0.75-1.00: exotic or poorly classifiable
 */
inline double compute_weirdness(const WeirdnessComponents& c,
                                const WeirdnessWeights& w = {}) {
    double raw = w.w1 * c.C_class_uncertain
               + w.w2 * c.G_geom_irregular
               + w.w3 * c.E_energy_conflict
               + w.w4 * c.B_bond_ambiguity
               + w.w5 * c.R_reference_distance;
    return std::clamp(raw / w.sum(), 0.0, 1.0);
}

inline const char* weirdness_band(double W) {
    if (W < 0.25) return "ordinary";
    if (W < 0.50) return "unusual";
    if (W < 0.75) return "highly irregular";
    return "exotic or poorly classifiable";
}

// ============================================================================
// Temperature score components
// ============================================================================

struct TemperatureComponents {
    double T_actual       = 300.0;   // System temperature (K)
    double T_ref          = 300.0;   // Reference temperature for material (K)
    double dP_dT_abs      = 0.0;     // |dP/dT| thermal response gradient
    double S_expected     = 1.0;     // Expected sensitivity scale (avoid /0)
    double phi_phase_risk = 0.0;     // Proximity to phase transition [0,1]
    double phi_thermal_noise = 0.0;  // Instability under perturbation [0,1]
};

struct TemperatureWeights {
    double a1 = 0.30;  // normalized temperature severity
    double a2 = 0.25;  // thermal response gradient
    double a3 = 0.25;  // phase risk
    double a4 = 0.20;  // thermal noise

    double sum() const { return a1 + a2 + a3 + a4; }
};

/**
 * Compute temperature score Ts in [0,1].
 *
 * Ts = a1*(T/Tref) + a2*(|dP/dT|/S_expected) + a3*phi_phase + a4*phi_noise
 *
 * This is NOT just temperature — it is thermal severity relative
 * to the material/system.
 *
 * Classification bands:
 *   0.00-0.30: thermally mild
 *   0.30-0.60: notable thermal influence
 *   0.60-0.80: high thermal stress
 *   0.80-1.00: likely phase-risk or decomposition-risk regime
 */
inline double compute_temperature_score(const TemperatureComponents& c,
                                        const TemperatureWeights& w = {}) {
    double S_safe = std::max(c.S_expected, 1e-12);
    double T_ratio = (c.T_ref > 0.0) ? (c.T_actual / c.T_ref) : 1.0;

    // Clamp individual terms to [0,1] before combining
    double t1 = std::clamp(T_ratio, 0.0, 1.0);
    double t2 = std::clamp(c.dP_dT_abs / S_safe, 0.0, 1.0);
    double t3 = std::clamp(c.phi_phase_risk, 0.0, 1.0);
    double t4 = std::clamp(c.phi_thermal_noise, 0.0, 1.0);

    double raw = w.a1 * t1 + w.a2 * t2 + w.a3 * t3 + w.a4 * t4;
    return std::clamp(raw / w.sum(), 0.0, 1.0);
}

inline const char* temperature_band(double Ts) {
    if (Ts < 0.30) return "thermally mild";
    if (Ts < 0.60) return "notable thermal influence";
    if (Ts < 0.80) return "high thermal stress";
    return "likely phase-risk or decomposition-risk regime";
}

// ============================================================================
// Convergence confidence
// ============================================================================

/**
 * Classification bands:
 *   > 0.80: strong
 *   0.55-0.80: acceptable
 *   0.35-0.55: marginal
 *   < 0.35: suspicious
 */
inline const char* convergence_band(double cc) {
    if (cc > 0.80) return "strong";
    if (cc > 0.55) return "acceptable";
    if (cc > 0.35) return "marginal";
    return "suspicious";
}

// ============================================================================
// Emergence classification
// ============================================================================

enum class EmergenceState : uint8_t {
    NONE             = 0,  // No meaningful emergence
    VISIBLE_WEAK     = 1,  // Emergence visible but weakly verified
    VISIBLE_STRONG   = 2   // Emergence visible and strongly verified
};

inline const char* emergence_name(EmergenceState e) {
    switch (e) {
        case EmergenceState::NONE:           return "none";
        case EmergenceState::VISIBLE_WEAK:   return "visible, weakly verified";
        case EmergenceState::VISIBLE_STRONG: return "visible, strongly verified";
        default: return "unknown";
    }
}

// ============================================================================
// Case classification helpers
// ============================================================================

enum class CaseType : uint8_t {
    ORDINARY           = 0,
    PROTEIN            = 1,
    ORGANOMETALLIC     = 2,
    EXOTIC_LATTICE     = 3,
    THERMAL_STRESS     = 4,
    COMPLEX_ORGANIC    = 5,
    LARGE_SYSTEM       = 6
};

inline const char* case_type_name(CaseType ct) {
    switch (ct) {
        case CaseType::ORDINARY:        return "ordinary";
        case CaseType::PROTEIN:         return "protein";
        case CaseType::ORGANOMETALLIC:  return "organometallic";
        case CaseType::EXOTIC_LATTICE:  return "exotic lattice";
        case CaseType::THERMAL_STRESS:  return "thermal stress";
        case CaseType::COMPLEX_ORGANIC: return "complex organic";
        case CaseType::LARGE_SYSTEM:    return "large system";
        default: return "unknown";
    }
}

// ============================================================================
// Case data input — lightweight view for CleanGate
// ============================================================================

struct CaseData {
    // Identity
    uint64_t    case_id       = 0;
    std::string name;
    CaseType    case_type     = CaseType::ORDINARY;

    // Atomic system summary
    uint32_t num_atoms        = 0;
    uint32_t num_bonds        = 0;
    uint32_t num_species      = 0;     // Distinct element types
    uint32_t num_components   = 0;     // Material components

    // Thermal context
    double temperature_K      = 300.0;
    double melting_point_K    = 0.0;   // 0 = unknown
    double boiling_point_K    = 0.0;   // 0 = unknown

    // Energy context (from cheap evaluators)
    double energy_primary     = 0.0;   // Primary model energy
    double energy_secondary   = 0.0;   // Secondary/cross-check energy
    bool   has_secondary      = false;

    // Geometry summary
    double max_bond_deviation = 0.0;   // Max deviation from expected lengths
    double symmetry_score     = 1.0;   // [0,1] 1=perfectly symmetric
    double compactness        = 1.0;   // Rg / expected Rg

    // Classification signals
    double rarity_score       = 0.0;   // [0,1] how rare/exotic
    double instability_index  = 0.0;   // [0,1] instability
    bool   has_unknown_species= false;
    bool   mixed_bonding      = false; // Partly metallic + partly covalent
    bool   rare_coordination  = false; // Unusual coordination environments

    // Convergence from prior minimization
    double convergence_fmax   = 0.0;   // Max force component at end
    double convergence_rms    = 0.0;   // RMS force at end
    int    convergence_steps  = 0;     // Steps taken
    bool   converged          = false;
};

// ============================================================================
// CleanGate report — the output
// ============================================================================

struct CleanGateReport {
    // Gate decisions
    bool pass_basic         = false;
    bool escalate_verbose   = false;
    bool require_deep_gate  = false;
    bool provisional_only   = false;

    // Computed risk variables
    double weirdness_score         = 0.0;
    double temperature_score       = 0.0;
    double convergence_confidence  = 0.0;
    double prereq_coverage         = 0.0;  // Fraction of sampled modules passed

    // Decomposed components (for traceability)
    WeirdnessComponents   weirdness_components;
    TemperatureComponents temperature_components;

    // Emergence
    EmergenceState emergence = EmergenceState::NONE;

    // Audit trail
    std::vector<std::string> modules_sampled;
    std::vector<std::string> warnings;
    std::vector<std::string> notes;

    // ----------------------------------------------------------------
    // Internal report (dense, numeric, machine-readable)
    // ----------------------------------------------------------------
    std::string internal_report() const {
        std::ostringstream o;
        o << std::fixed << std::setprecision(2);
        o << "clean_gate.pass_basic              = " << (pass_basic ? "true" : "false") << "\n";
        o << "clean_gate.weirdness_score         = " << weirdness_score << "\n";
        o << "clean_gate.temperature_score       = " << temperature_score << "\n";
        o << "clean_gate.convergence_confidence  = " << convergence_confidence << "\n";
        o << "clean_gate.prereq_coverage         = " << prereq_coverage << "\n";
        o << "clean_gate.escalate_verbose        = " << (escalate_verbose ? "true" : "false") << "\n";
        o << "clean_gate.require_deep_gate       = " << (require_deep_gate ? "true" : "false") << "\n";
        o << "clean_gate.provisional_only        = " << (provisional_only ? "true" : "false") << "\n";
        o << "clean_gate.emergence               = " << emergence_name(emergence) << "\n";
        o << "clean_gate.modules_sampled         = " << modules_sampled.size() << "\n";
        o << "clean_gate.warnings                = " << warnings.size() << "\n";
        return o.str();
    }

    // ----------------------------------------------------------------
    // External report (human-readable summary)
    // ----------------------------------------------------------------
    std::string external_report() const {
        std::ostringstream o;
        o << "CleanGate Summary:\n";
        o << "  - Basic prerequisites: " << (pass_basic ? "PASSED" : "FAILED") << "\n";
        o << "  - Material/system irregularity: " << weirdness_band(weirdness_score)
          << " (" << std::fixed << std::setprecision(2) << weirdness_score << ")\n";
        o << "  - Thermal severity: " << temperature_band(temperature_score)
          << " (" << temperature_score << ")\n";
        o << "  - Convergence confidence: " << convergence_band(convergence_confidence)
          << " (" << convergence_confidence << ")\n";
        o << "  - Emergence: " << emergence_name(emergence) << "\n";

        if (escalate_verbose)
            o << "  - [ESCALATED] Verbose tracking enabled.\n";
        if (require_deep_gate)
            o << "  - [ESCALATED] Deep verification recommended.\n";
        if (provisional_only)
            o << "  - [PROVISIONAL] Result not promoted as stable.\n";

        if (!warnings.empty()) {
            o << "  Warnings:\n";
            for (const auto& w : warnings)
                o << "    ! " << w << "\n";
        }
        if (!notes.empty()) {
            o << "  Notes:\n";
            for (const auto& n : notes)
                o << "    - " << n << "\n";
        }
        return o.str();
    }
};

// ============================================================================
// Built-in gate module registry
// ============================================================================

/**
 * The full registry of gate modules.  Each has a tier, a name,
 * and a flag for whether failure is fatal.
 *
 * Core modules (always run):
 *   0: identity_check
 *   1: bounds_check
 *   2: geometry_sanity
 *
 * Low tier (T1-T10):
 *   3: formatting_valid
 *   4: species_coverage
 *   5: charge_balance
 *   6: stoichiometry_sane
 *   7: coordinate_range
 *   8: mass_positive
 *   9: index_integrity
 *  10: element_recognized
 *
 * Mid tier (T11-T25):
 *  11: geometry_plausibility
 *  12: energy_sign_sanity
 *  13: bonding_sanity
 *  14: angle_distribution
 *  15: torsion_range
 *  16: ring_closure
 *  17: valence_check
 *  18: coordination_number
 *  19: neighbor_count
 *  20: overlap_check
 *  21: bond_order_consistency
 *  22: dipole_magnitude
 *  23: symmetry_plausible
 *  24: planarity_check
 *  25: chirality_consistent
 *
 * High tier (T26-T50):
 *  26: partial_relaxation
 *  27: local_stability
 *  28: thermal_response_tendency
 *  29: force_magnitude
 *  30: hessian_eigenvalue_sign
 *  31: normal_mode_frequency
 *  32: virial_consistency
 *  33: pressure_estimate
 *  34: enthalpy_sign
 *  35: free_energy_rank
 *  36: phase_risk_indicator
 *  37: decomposition_risk
 *  38: surface_energy
 *  39: elastic_response
 *  40: phonon_stability
 *  41: diffusion_barrier
 *  42: vacancy_formation
 *  43: interface_energy
 *  44: grain_boundary
 *  45: defect_formation
 *  46: thermal_expansion
 *  47: bulk_modulus
 *  48: shear_modulus
 *  49: creep_tendency
 *  50: fatigue_indicator
 */

inline std::vector<GateModule> build_gate_registry() {
    std::vector<GateModule> reg;
    reg.reserve(51);

    auto add = [&](uint32_t id, GateTier t, const char* nm,
                   const char* desc, bool fatal) {
        reg.push_back({id, t, nm, desc, fatal});
    };

    // Core (always run)
    add( 0, GateTier::CORE, "identity_check",    "Verify atom identities and species",     true);
    add( 1, GateTier::CORE, "bounds_check",       "Position bounds and NaN detection",      true);
    add( 2, GateTier::CORE, "geometry_sanity",    "Basic distance/angle sanity",            true);

    // Low tier (T1-T10)
    add( 3, GateTier::LOW, "formatting_valid",    "Data format and field integrity",         false);
    add( 4, GateTier::LOW, "species_coverage",    "All species have parameters",             false);
    add( 5, GateTier::LOW, "charge_balance",      "Net charge consistent",                   false);
    add( 6, GateTier::LOW, "stoichiometry_sane",  "Atom counts match formula",               false);
    add( 7, GateTier::LOW, "coordinate_range",    "Coordinates within expected box",          false);
    add( 8, GateTier::LOW, "mass_positive",       "All masses > 0",                           true);
    add( 9, GateTier::LOW, "index_integrity",     "Bond indices within atom count",           true);
    add(10, GateTier::LOW, "element_recognized",  "All Z values in element database",         false);

    // Mid tier (T11-T25)
    add(11, GateTier::MID, "geometry_plausibility","Bond/angle distributions normal",          false);
    add(12, GateTier::MID, "energy_sign_sanity",   "Energy sign matches expectations",         false);
    add(13, GateTier::MID, "bonding_sanity",       "Bond pattern consistent with chemistry",   false);
    add(14, GateTier::MID, "angle_distribution",   "Bond angles within element expectations",  false);
    add(15, GateTier::MID, "torsion_range",        "Torsion angles within allowed range",      false);
    add(16, GateTier::MID, "ring_closure",         "Detected rings are properly closed",       false);
    add(17, GateTier::MID, "valence_check",        "Valence within element limits",            false);
    add(18, GateTier::MID, "coordination_number",  "Coordination number normal for species",   false);
    add(19, GateTier::MID, "neighbor_count",       "Neighbor count consistent with topology",  false);
    add(20, GateTier::MID, "overlap_check",        "No steric overlaps beyond threshold",      false);
    add(21, GateTier::MID, "bond_order_consistency","Bond orders sum to expected valence",      false);
    add(22, GateTier::MID, "dipole_magnitude",     "Dipole within plausible range",            false);
    add(23, GateTier::MID, "symmetry_plausible",   "Detected symmetry internally consistent",  false);
    add(24, GateTier::MID, "planarity_check",      "Expected planar groups are planar",        false);
    add(25, GateTier::MID, "chirality_consistent", "Chiral centers match expected config",     false);

    // High tier (T26-T50)
    add(26, GateTier::HIGH, "partial_relaxation",       "Short minimization converges",             false);
    add(27, GateTier::HIGH, "local_stability",          "Positive curvature at minimum",            false);
    add(28, GateTier::HIGH, "thermal_response_tendency","Temperature perturbation stable",          false);
    add(29, GateTier::HIGH, "force_magnitude",          "Residual forces below threshold",          false);
    add(30, GateTier::HIGH, "hessian_eigenvalue_sign",  "No imaginary frequencies at minimum",      false);
    add(31, GateTier::HIGH, "normal_mode_frequency",    "Frequencies in physical range",            false);
    add(32, GateTier::HIGH, "virial_consistency",       "Virial consistent with pressure estimate", false);
    add(33, GateTier::HIGH, "pressure_estimate",        "Pressure within expected bounds",          false);
    add(34, GateTier::HIGH, "enthalpy_sign",            "Enthalpy sign matches phase expectation",  false);
    add(35, GateTier::HIGH, "free_energy_rank",         "Free energy ranking stable across models", false);
    add(36, GateTier::HIGH, "phase_risk_indicator",     "Phase boundary proximity flagged",         false);
    add(37, GateTier::HIGH, "decomposition_risk",       "Decomposition energy negative?",           false);
    add(38, GateTier::HIGH, "surface_energy",           "Surface energy positive and reasonable",   false);
    add(39, GateTier::HIGH, "elastic_response",         "Elastic constants positive-definite",      false);
    add(40, GateTier::HIGH, "phonon_stability",         "No unstable phonon branches detected",     false);
    add(41, GateTier::HIGH, "diffusion_barrier",        "Barrier height physical",                  false);
    add(42, GateTier::HIGH, "vacancy_formation",        "Vacancy energy positive",                  false);
    add(43, GateTier::HIGH, "interface_energy",         "Interface energy reasonable",               false);
    add(44, GateTier::HIGH, "grain_boundary",           "Grain boundary energy in range",           false);
    add(45, GateTier::HIGH, "defect_formation",         "Defect formation energy positive",         false);
    add(46, GateTier::HIGH, "thermal_expansion",        "Thermal expansion coefficient physical",   false);
    add(47, GateTier::HIGH, "bulk_modulus",             "Bulk modulus positive and in range",       false);
    add(48, GateTier::HIGH, "shear_modulus",            "Shear modulus positive",                   false);
    add(49, GateTier::HIGH, "creep_tendency",           "Creep activation energy reasonable",       false);
    add(50, GateTier::HIGH, "fatigue_indicator",        "Fatigue life estimate positive",           false);

    return reg;
}

// ============================================================================
// Semi-random stratified sampling
// ============================================================================

/**
 * Select modules using stratified semi-random sampling.
 *
 * Strategy:
 *   - Always include all CORE modules (identity, bounds, geometry)
 *   - Randomly choose 2 from LOW  tier
 *   - Randomly choose 2 from MID  tier
 *   - Randomly choose 2 from HIGH tier
 *   - Add 1 targeted module for detected case type
 *
 * Gives repeatable coverage with enough variety to catch brittle failures.
 */
struct SamplingConfig {
    uint32_t low_count    = 2;
    uint32_t mid_count    = 2;
    uint32_t high_count   = 2;
    uint32_t target_count = 1;
};

inline std::vector<GateModule> select_modules(
    const std::vector<GateModule>& registry,
    CaseType case_type,
    uint64_t seed,
    const SamplingConfig& cfg = {})
{
    std::mt19937 rng(seed);
    std::vector<GateModule> selected;

    // Partition by tier
    std::vector<size_t> core_idx, low_idx, mid_idx, high_idx;
    for (size_t i = 0; i < registry.size(); ++i) {
        switch (registry[i].tier) {
            case GateTier::CORE:   core_idx.push_back(i); break;
            case GateTier::LOW:    low_idx.push_back(i);  break;
            case GateTier::MID:    mid_idx.push_back(i);  break;
            case GateTier::HIGH:   high_idx.push_back(i); break;
            default: break;
        }
    }

    // Always include all core modules
    for (auto i : core_idx)
        selected.push_back(registry[i]);

    // Stratified random selection helper
    auto pick = [&](const std::vector<size_t>& pool, uint32_t count) {
        if (pool.empty()) return;
        std::vector<size_t> shuffled = pool;
        std::shuffle(shuffled.begin(), shuffled.end(), rng);
        uint32_t n = std::min(count, static_cast<uint32_t>(shuffled.size()));
        for (uint32_t k = 0; k < n; ++k)
            selected.push_back(registry[shuffled[k]]);
    };

    pick(low_idx,  cfg.low_count);
    pick(mid_idx,  cfg.mid_count);
    pick(high_idx, cfg.high_count);

    // Targeted module for case type
    auto add_targeted = [&](uint32_t mod_id, const char* reason) {
        for (const auto& m : registry) {
            if (m.id == mod_id) {
                GateModule targeted = m;
                targeted.tier = GateTier::TARGET;
                targeted.detail = reason;
                selected.push_back(targeted);
                return;
            }
        }
    };

    switch (case_type) {
        case CaseType::PROTEIN:
            add_targeted(28, "protein: thermal response critical"); break;
        case CaseType::ORGANOMETALLIC:
            add_targeted(18, "organometallic: coordination number critical"); break;
        case CaseType::EXOTIC_LATTICE:
            add_targeted(40, "exotic lattice: phonon stability critical"); break;
        case CaseType::THERMAL_STRESS:
            add_targeted(46, "thermal stress: expansion coefficient critical"); break;
        case CaseType::COMPLEX_ORGANIC:
            add_targeted(15, "complex organic: torsion range critical"); break;
        case CaseType::LARGE_SYSTEM:
            add_targeted(29, "large system: force magnitude critical"); break;
        case CaseType::ORDINARY:
            add_targeted(17, "ordinary: valence check targeted"); break;
    }

    return selected;
}

// ============================================================================
// Score computation from CaseData
// ============================================================================

/**
 * Build weirdness components from case data.
 * Each component is normalized to [0,1].
 */
inline WeirdnessComponents assess_weirdness(const CaseData& c) {
    WeirdnessComponents wc;

    // C: classification uncertainty
    // Unknown species, mixed bonding, or high rarity all increase uncertainty
    double c_val = 0.0;
    if (c.has_unknown_species) c_val += 0.4;
    if (c.mixed_bonding)       c_val += 0.3;
    c_val += 0.3 * c.rarity_score;
    wc.C_class_uncertain = std::clamp(c_val, 0.0, 1.0);

    // G: geometric irregularity
    // Large bond deviations, low symmetry, abnormal compactness
    double g_val = 0.0;
    g_val += 0.4 * std::clamp(c.max_bond_deviation / 0.5, 0.0, 1.0);
    g_val += 0.3 * (1.0 - c.symmetry_score);
    double compact_dev = std::abs(c.compactness - 1.0);
    g_val += 0.3 * std::clamp(compact_dev / 0.5, 0.0, 1.0);
    wc.G_geom_irregular = std::clamp(g_val, 0.0, 1.0);

    // E: energy conflict between cheap models
    if (c.has_secondary) {
        double E_avg = 0.5 * (std::abs(c.energy_primary) + std::abs(c.energy_secondary));
        double E_diff = std::abs(c.energy_primary - c.energy_secondary);
        double frac = (E_avg > 1e-12) ? (E_diff / E_avg) : 0.0;
        wc.E_energy_conflict = std::clamp(frac, 0.0, 1.0);
    }

    // B: bond ambiguity
    double b_val = 0.0;
    if (c.mixed_bonding)       b_val += 0.5;
    if (c.rare_coordination)   b_val += 0.5;
    wc.B_bond_ambiguity = std::clamp(b_val, 0.0, 1.0);

    // R: reference distance (how exotic is this system?)
    wc.R_reference_distance = std::clamp(c.rarity_score + 0.2 * c.instability_index, 0.0, 1.0);

    return wc;
}

/**
 * Build temperature components from case data.
 */
inline TemperatureComponents assess_temperature(const CaseData& c) {
    TemperatureComponents tc;
    tc.T_actual = c.temperature_K;

    // Use melting point as reference if known, otherwise default 300 K
    tc.T_ref = (c.melting_point_K > 0.0) ? c.melting_point_K : 300.0;

    // Phase risk: how close is T to melting or boiling?
    if (c.melting_point_K > 0.0) {
        double frac_melt = c.temperature_K / c.melting_point_K;
        if (frac_melt > 0.8 && frac_melt < 1.2)
            tc.phi_phase_risk = 0.8;  // Near melting
        else if (frac_melt >= 1.2 && c.boiling_point_K > 0.0) {
            double frac_boil = c.temperature_K / c.boiling_point_K;
            if (frac_boil > 0.8)
                tc.phi_phase_risk = 0.9;  // Near boiling
        }
    }

    // Thermal noise from instability index
    tc.phi_thermal_noise = std::clamp(c.instability_index, 0.0, 1.0);

    // Gradient and expected sensitivity — zeroed until external data provided
    tc.dP_dT_abs  = 0.0;
    tc.S_expected  = 1.0;

    return tc;
}

/**
 * Estimate convergence confidence from case data.
 * Returns [0,1] where 1 = fully converged.
 */
inline double estimate_convergence_confidence(const CaseData& c) {
    if (!c.converged) {
        // Didn't converge — confidence depends on how close
        double fmax_score = std::clamp(1.0 - c.convergence_fmax / 0.1, 0.0, 0.5);
        return fmax_score;
    }

    // Converged — confidence from residual force quality
    double fmax_qual = std::clamp(1.0 - c.convergence_fmax / 0.01, 0.0, 1.0);
    double rms_qual  = std::clamp(1.0 - c.convergence_rms  / 0.005, 0.0, 1.0);

    // Penalize excessive step counts (hints at difficult landscape)
    double step_penalty = 0.0;
    if (c.convergence_steps > 5000) step_penalty = 0.1;
    if (c.convergence_steps > 10000) step_penalty = 0.2;

    return std::clamp(0.5 * fmax_qual + 0.4 * rms_qual - step_penalty + 0.1, 0.0, 1.0);
}

/**
 * Classify emergence state from weirdness, convergence, and energy signals.
 */
inline EmergenceState classify_emergence(double weirdness,
                                         double convergence_confidence,
                                         const CaseData& c) {
    // Emergence requires some structural novelty
    if (weirdness < 0.30)
        return EmergenceState::NONE;

    // If interesting but poorly converged, it's weakly verified at best
    if (convergence_confidence < 0.55)
        return EmergenceState::VISIBLE_WEAK;

    // Interesting and well converged = strongly verified emergence
    if (convergence_confidence >= 0.55 && weirdness >= 0.30)
        return EmergenceState::VISIBLE_STRONG;

    return EmergenceState::NONE;
}

// ============================================================================
// Case type detection helpers
// ============================================================================

inline bool is_exotic_material(const CaseData& c) {
    return c.rarity_score > 0.6 || c.has_unknown_species;
}

inline bool is_complex_organic(const CaseData& c) {
    return c.case_type == CaseType::COMPLEX_ORGANIC ||
           (c.num_atoms > 50 && c.num_species <= 5);  // Many atoms, few elements
}

inline bool is_large_protein_case(const CaseData& c) {
    return c.case_type == CaseType::PROTEIN || c.num_atoms > 500;
}

// ============================================================================
// Main gate runner
// ============================================================================

/**
 * Execute the sampled prerequisite modules against CaseData.
 *
 * This is a stub execution — each module's actual logic is evaluated
 * based on the summary signals in CaseData.  Real per-module evaluators
 * can be registered as callbacks in production use.
 *
 * Returns the fraction of modules that passed (prereq_coverage).
 */
struct PrereqResult {
    double coverage_ratio = 0.0;
    bool   no_fatal_errors = true;
    std::vector<std::string> failed_modules;
};

inline PrereqResult run_sampled_prereqs(
    const CaseData& c,
    std::vector<GateModule>& modules)
{
    PrereqResult pr;
    uint32_t pass_count = 0;

    for (auto& m : modules) {
        m.executed = true;
        m.passed   = true;
        m.score    = 1.0;

        // Core gate: identity
        if (m.id == 0) {
            if (c.num_atoms == 0 || c.has_unknown_species) {
                m.passed = false;
                m.score  = 0.0;
                m.detail = "Empty system or unknown species";
            }
        }
        // Core gate: bounds
        else if (m.id == 1) {
            // Proxy: if convergence_fmax is NaN-like, fail
            if (std::isnan(c.convergence_fmax) || std::isinf(c.convergence_fmax)) {
                m.passed = false;
                m.score  = 0.0;
                m.detail = "NaN/Inf detected in convergence data";
            }
        }
        // Core gate: geometry sanity
        else if (m.id == 2) {
            if (c.max_bond_deviation > 0.8) {
                m.passed = false;
                m.score  = std::clamp(1.0 - c.max_bond_deviation, 0.0, 1.0);
                m.detail = "Extreme bond deviation: " + std::to_string(c.max_bond_deviation);
            }
        }
        // Energy sign sanity (mid tier)
        else if (m.id == 12) {
            if (c.has_secondary) {
                bool sign_match = (c.energy_primary >= 0.0) == (c.energy_secondary >= 0.0);
                if (!sign_match) {
                    m.passed = false;
                    m.score  = 0.3;
                    m.detail = "Energy sign disagrees between models";
                }
            }
        }
        // Partial relaxation (high tier)
        else if (m.id == 26) {
            if (!c.converged && c.convergence_fmax > 0.05) {
                m.passed = false;
                m.score  = std::clamp(1.0 - c.convergence_fmax / 0.1, 0.0, 1.0);
                m.detail = "Partial relaxation did not converge: fmax=" +
                           std::to_string(c.convergence_fmax);
            }
        }
        // Thermal response tendency (high tier)
        else if (m.id == 28) {
            if (c.melting_point_K > 0.0 &&
                c.temperature_K > 0.9 * c.melting_point_K) {
                m.passed = false;
                m.score  = 0.4;
                m.detail = "T near melting point — thermal instability likely";
            }
        }
        // Default: pass with quality from symmetry/compactness
        else {
            m.score = std::clamp(0.5 * c.symmetry_score + 0.5 * (1.0 - c.instability_index),
                                 0.0, 1.0);
            m.passed = (m.score > 0.2);
            if (!m.passed)
                m.detail = "Quality score below threshold: " + std::to_string(m.score);
        }

        if (m.passed)
            pass_count++;
        else {
            pr.failed_modules.push_back(m.name);
            if (m.is_fatal)
                pr.no_fatal_errors = false;
        }
    }

    pr.coverage_ratio = modules.empty() ? 0.0
        : static_cast<double>(pass_count) / static_cast<double>(modules.size());
    return pr;
}

// ============================================================================
// run_clean_gate — the main entry point
// ============================================================================

/**
 * Execute the CleanGate adaptive verification pipeline.
 *
 * Flow:
 *   1. Select semi-random modules (stratified by tier)
 *   2. Run sampled prerequisite modules
 *   3. Compute weirdness score
 *   4. Compute temperature score
 *   5. Estimate convergence confidence
 *   6. Classify emergence
 *   7. Escalation decisions
 *
 * All results are recorded in CleanGateReport for full traceability.
 */
inline CleanGateReport run_clean_gate(const CaseData& c,
                                      uint64_t seed = 0,
                                      const SamplingConfig& scfg = {},
                                      const WeirdnessWeights& ww = {},
                                      const TemperatureWeights& tw = {})
{
    CleanGateReport r;

    // 1. Build registry and select modules
    auto registry = build_gate_registry();
    auto sampled  = select_modules(registry, c.case_type, seed, scfg);

    for (const auto& m : sampled)
        r.modules_sampled.push_back(m.name);

    // 2. Run sampled prerequisite modules
    auto prereq = run_sampled_prereqs(c, sampled);
    r.prereq_coverage = prereq.coverage_ratio;

    // 3. Compute weirdness score
    r.weirdness_components = assess_weirdness(c);
    r.weirdness_score      = compute_weirdness(r.weirdness_components, ww);

    // 4. Compute temperature score
    r.temperature_components = assess_temperature(c);
    r.temperature_score      = compute_temperature_score(r.temperature_components, tw);

    // 5. Estimate convergence confidence
    r.convergence_confidence = estimate_convergence_confidence(c);

    // 6. Classify emergence
    r.emergence = classify_emergence(r.weirdness_score, r.convergence_confidence, c);

    // 7. Basic gate: pass if no fatal prereq failures
    r.pass_basic = prereq.no_fatal_errors;

    if (!r.pass_basic) {
        r.warnings.push_back("Fatal prerequisite failure in sampled gate.");
        r.provisional_only = true;
        for (const auto& fm : prereq.failed_modules)
            r.warnings.push_back("  Failed: " + fm);
        return r;
    }

    // 8. Escalation: verbose
    if (r.weirdness_score > 0.70 || r.temperature_score > 0.75) {
        r.escalate_verbose = true;
        r.notes.push_back("Escalated due to high irregularity or thermal severity.");
    }

    // 9. Escalation: deep gate
    if (r.convergence_confidence < 0.45 ||
        is_exotic_material(c) ||
        is_complex_organic(c) ||
        is_large_protein_case(c))
    {
        r.require_deep_gate = true;
        r.notes.push_back("Deep gate required for trustable verification.");
    }

    // 10. Provisional marking
    if (r.require_deep_gate && r.convergence_confidence < 0.30) {
        r.provisional_only = true;
        r.warnings.push_back(
            "Result may exhibit visible emergence without robust convergence proof.");
    }

    // Record emergence note if detected
    if (r.emergence != EmergenceState::NONE) {
        r.notes.push_back(std::string("Emergence: ") + emergence_name(r.emergence));
    }

    return r;
}

} // namespace validation
} // namespace atomistic
