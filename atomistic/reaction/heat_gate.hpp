#pragma once
/**
 * heat_gate.hpp
 * -------------
 * Heat-gated reaction control system (SS8b).
 *
 * A single 3-digit integer h ∈ {0,1,...,999} deterministically controls
 * which reaction template families are active, how strongly they are
 * biased, and what scoring threshold a candidate event must exceed.
 *
 * Core equations (see docs/section8b_heat_gated_reaction_control.tex):
 *   x  = h / 999                                        (heat normalisation)
 *   g  = clamp((x - x0) / (x1 - x0), 0, 1)             (smooth gate)
 *   wk = alpha_k * g(x)                                 (template enable weight)
 *   Se = Bk + lambda_k * wk(h) + Ge - Pe                (candidate score)
 *   accept(e) <=> Se >= tau_k                            (deterministic rule)
 *   T(h) = T_base ∪ {k ∈ T_bio : wk(h) > epsilon}      (active template set)
 */

#include "../core/state.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <functional>

namespace atomistic {
namespace reaction {

// ============================================================================
// Heat Configuration
// ============================================================================

struct HeatConfig {
    uint16_t heat_3;        // Raw 3-digit value [0..999]
    double   x_normalized;  // h / 999.0 ∈ [0, 1]

    explicit HeatConfig(uint16_t h = 0)
        : heat_3(std::min<uint16_t>(h, 999))
        , x_normalized(heat_3 / 999.0) {}
};

// ============================================================================
// Smooth Gate Function
// ============================================================================

/**
 * Piecewise-linear ramp: g(x) ∈ [0, 1].
 *   x <= x0  → 0
 *   x >= x1  → 1
 *   else     → (x - x0) / (x1 - x0)
 */
inline double gate(double x, double x0, double x1) {
    if (x <= x0) return 0.0;
    if (x >= x1) return 1.0;
    return (x - x0) / (x1 - x0);
}

// ============================================================================
// Template Family Classification
// ============================================================================

/**
 * Which family a reaction template belongs to.
 * BASE templates are always active; BIO templates are heat-gated.
 */
enum class TemplateFamily : uint8_t {
    BASE,   // Radical recombination, acid-base, SN2, ion pairing
    BIO,    // Peptide, amide, ester, thioester, disulfide, imide/urea
};

// ============================================================================
// Bio-Template Identifiers
// ============================================================================

enum class BioTemplateId : uint8_t {
    PEPTIDE_BOND,
    GENERAL_AMIDE,
    ESTER,
    THIOESTER,
    DISULFIDE,
    IMIDE_UREA,
    COUNT
};

inline const char* bio_template_name(BioTemplateId id) {
    switch (id) {
        case BioTemplateId::PEPTIDE_BOND:   return "PEPTIDE_BOND";
        case BioTemplateId::GENERAL_AMIDE:  return "GENERAL_AMIDE";
        case BioTemplateId::ESTER:          return "ESTER";
        case BioTemplateId::THIOESTER:      return "THIOESTER";
        case BioTemplateId::DISULFIDE:      return "DISULFIDE";
        case BioTemplateId::IMIDE_UREA:     return "IMIDE_UREA";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// Per-Template Gate Parameters
// ============================================================================

struct BioTemplateGateParams {
    BioTemplateId id;
    double alpha;       // Base strength ∈ [0, 1]
    double x0;          // Gate lower threshold
    double x1;          // Gate upper threshold
    double lambda;      // Coupling constant for score bias
    double tau;         // Acceptance threshold

    double enable_weight(double x_norm) const {
        return alpha * gate(x_norm, x0, x1);
    }
};

/**
 * Default gate parameters per bio-template.
 * x0 = 250/999, x1 = 650/999 for the peptide/amide family.
 */
inline std::vector<BioTemplateGateParams> default_bio_gate_params() {
    constexpr double X0 = 250.0 / 999.0;
    constexpr double X1 = 650.0 / 999.0;
    return {
        { BioTemplateId::PEPTIDE_BOND,   1.0, X0, X1, 0.30, 0.50 },
        { BioTemplateId::GENERAL_AMIDE,  0.9, X0, X1, 0.28, 0.50 },
        { BioTemplateId::ESTER,          0.8, X0, X1, 0.25, 0.50 },
        { BioTemplateId::THIOESTER,      0.7, X0, X1, 0.22, 0.55 },
        { BioTemplateId::DISULFIDE,      0.6, X0, X1, 0.20, 0.55 },
        { BioTemplateId::IMIDE_UREA,     0.5, X0, X1, 0.18, 0.60 },
    };
}

// ============================================================================
// Temperature → Heat Parameter Conversion
// ============================================================================

/**
 * Map thermodynamic temperature T (Kelvin) to heat parameter h ∈ [0, 999].
 * 
 * Physical interpretation:
 *   - Low T (< 250 K): General organic chemistry only (h < 250)
 *   - Mid T (250-650 K): Transitional regime (bio templates ramp up)
 *   - High T (> 650 K): Full biochemical scaffolding (h ≥ 650)
 * 
 * This is a **deterministic configuration mapping**, not a physical law.
 * The heat parameter controls template selection, not MD thermostat temperature.
 * 
 * Default mapping: linear with saturation
 *   h = clamp(T * slope, 0, 999)
 *   slope ≈ 1.5 gives h=250 at T=167K, h=650 at T=433K, h=999 at T=666K
 */
inline uint16_t temperature_to_heat(double T_kelvin, double slope = 1.5) {
    if (T_kelvin < 0.0) return 0;
    double h_raw = T_kelvin * slope;
    if (h_raw > 999.0) return 999;
    return static_cast<uint16_t>(h_raw);
}

/**
 * Inverse mapping: heat parameter → approximate temperature.
 * Used for reporting/logging, not for physical calculations.
 */
inline double heat_to_temperature(uint16_t h, double slope = 1.5) {
    if (slope <= 0.0) return 0.0;
    return static_cast<double>(h) / slope;
}

// ============================================================================
// Heat-Gated Template Controller
// ============================================================================

class HeatGateController {
public:
    explicit HeatGateController(uint16_t heat = 0);

    // Reconfigure heat without rebuilding
    void set_heat(uint16_t h);
    void set_heat_from_temperature(double T_kelvin, double slope = 1.5);
    const HeatConfig& config() const { return config_; }

    // Mode index m(h) = g(h/999) using default peptide-family thresholds
    double mode_index() const;

    // Query enable weight for a given bio-template
    double enable_weight(BioTemplateId id) const;

    // Is a bio-template active (wk > epsilon)?
    bool is_active(BioTemplateId id, double epsilon = 1e-3) const;

    // Return the full set of active bio-template IDs
    std::vector<BioTemplateId> active_bio_templates(double epsilon = 1e-3) const;

    // ========================================================================
    // Candidate Bond Scoring (Eq. 4 from SS8b)
    // ========================================================================

    struct CandidateEvent {
        BioTemplateId template_id;
        double geometry_score;   // G_e ∈ [0, 1]
        double penalty;          // P_e ≥ 0
        std::array<uint32_t, 4> atoms;  // Participating atom indices
    };

    struct ScoringResult {
        double score;           // S_e
        double threshold;       // tau_k
        bool   accepted;        // S_e >= tau_k
        double accept_prob;     // sigmoid(beta * (S_e - tau_k))
        std::string log_line;   // Structured log entry
    };

    /**
     * Score a candidate bond-forming event.
     *   S_e = B_k + lambda_k * w_k(h) + G_e - P_e
     */
    ScoringResult score_candidate(const CandidateEvent& event,
                                  double beta = 10.0) const;

    // Access gate parameters
    const std::vector<BioTemplateGateParams>& gate_params() const { return params_; }

private:
    HeatConfig config_;
    std::vector<BioTemplateGateParams> params_;
    std::vector<double> cached_weights_;  // Pre-computed enable weights

    void recompute_weights();
};

// ============================================================================
// Amino Acid Reference (20 proteinogenic amino acids)
// ============================================================================

struct AminoAcidEntry {
    const char* name;
    const char* three_letter;
    char        one_letter;
    const char* formula;
    uint8_t     C, H, N, O, S;  // Atom counts
};

/**
 * Return the canonical table of 20 proteinogenic amino acids.
 * Sorted by one-letter code for binary search.
 */
const std::vector<AminoAcidEntry>& amino_acid_table();

// ============================================================================
// Substructure Detection Motifs
// ============================================================================

enum class MotifType : uint8_t {
    AMINE,           // -NH2 or -NH-
    CARBOXYL,        // -C(=O)OH
    THIOL,           // -SH
    THIOETHER,       // -S-
    EXTRA_CARBOXYL,  // Second -COOH
    EXTRA_AMINE,     // Second -NH2
    GUANIDINIUM,     // -C(=NH)(NH2)2
    IMIDAZOLE,       // Five-membered N-containing ring
    AROMATIC_RING,   // Six-membered aromatic
    PHENOLIC_OH,     // -OH on aromatic ring
    INDOLE,          // Bicyclic indole
};

struct MotifHit {
    MotifType type;
    uint32_t  anchor_atom;   // Central atom of the motif
    double    confidence;    // 0.0–1.0
};

struct AminoAcidDetection {
    std::string name;
    std::string formula;
    double confidence;
    std::vector<MotifHit> motif_hits;
    std::string log_line;
};

// ============================================================================
// Validation Campaign Metrics (500-sim temperature sweep)
// ============================================================================

struct SingleRunMetrics {
    uint32_t seed;
    double   temperature;
    uint16_t heat_3;
    uint32_t steps;

    double E_start;
    double E_end;
    double drift;           // |E_end - E_start| / (|E_start| + epsilon)
    bool   converged;

    double clash_score;     // Fraction of overlapping pairs
    double geom_violation_rate; // Violations per 1000 steps

    uint32_t n_events_logged;
    uint32_t n_events_accepted;
    uint32_t n_events_rejected;

    double msd;             // Mean squared displacement
};

struct TemperatureBucket {
    double temperature;
    double median_drift;
    double drift_iqr;        // Interquartile range
    double convergence_rate;
    double median_clash;
    double median_geom_violation;
    double mean_event_rate;
    double median_msd;
    bool   identity_conserved;  // 100% consistency check
};

struct ValidationCampaignResult {
    uint32_t total_runs;
    uint32_t n_temperatures;
    uint32_t seeds_per_temperature;

    std::vector<TemperatureBucket> buckets;

    // Global pass/fail
    bool energy_drift_pass;       // median drift <= 0.05 for all T
    bool convergence_pass;        // rate >= 0.90 for all T
    bool clash_pass;              // median < 1e-3 for all T
    bool geom_violation_pass;     // median < 1.0 per 1000 steps
    bool rate_monotonicity_pass;  // Spearman rho(T, k) > 0.8
    bool msd_monotonicity_pass;   // Spearman rho(T, MSD) > 0.8
    bool identity_conservation_pass; // 100% for all T

    bool all_passed() const {
        return energy_drift_pass && convergence_pass && clash_pass &&
               geom_violation_pass && rate_monotonicity_pass &&
               msd_monotonicity_pass && identity_conservation_pass;
    }
};

// ============================================================================
// Validation Helpers
// ============================================================================

/**
 * Compute energy drift metric.
 *   ΔE = |E_end - E_start| / (|E_start| + epsilon)
 */
inline double compute_energy_drift(double E_start, double E_end,
                                   double epsilon = 1e-10) {
    return std::abs(E_end - E_start) / (std::abs(E_start) + epsilon);
}

/**
 * Compute clash score: fraction of atom pairs closer than r_min.
 */
double compute_clash_score(const State& s, double r_min_factor = 0.5);

/**
 * Aggregate per-run metrics into per-temperature buckets.
 */
std::vector<TemperatureBucket> aggregate_by_temperature(
    const std::vector<SingleRunMetrics>& runs);

/**
 * Compute Spearman rank correlation between two vectors.
 */
double spearman_correlation(const std::vector<double>& x,
                            const std::vector<double>& y);

/**
 * Evaluate the full validation campaign from aggregated buckets.
 */
ValidationCampaignResult evaluate_campaign(
    const std::vector<TemperatureBucket>& buckets,
    uint32_t total_runs,
    uint32_t n_temps,
    uint32_t seeds_per_temp);

} // namespace reaction
} // namespace atomistic
