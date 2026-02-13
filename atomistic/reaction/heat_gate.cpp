#include "heat_gate.hpp"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <map>

namespace atomistic {
namespace reaction {

// ============================================================================
// HeatGateController
// ============================================================================

HeatGateController::HeatGateController(uint16_t heat)
    : config_(heat)
    , params_(default_bio_gate_params())
{
    recompute_weights();
}

void HeatGateController::set_heat(uint16_t h) {
    config_ = HeatConfig(h);
    recompute_weights();
}

double HeatGateController::mode_index() const {
    // Default peptide-family thresholds
    constexpr double X0 = 250.0 / 999.0;
    constexpr double X1 = 650.0 / 999.0;
    return gate(config_.x_normalized, X0, X1);
}

double HeatGateController::enable_weight(BioTemplateId id) const {
    auto idx = static_cast<size_t>(id);
    if (idx < cached_weights_.size()) return cached_weights_[idx];
    return 0.0;
}

bool HeatGateController::is_active(BioTemplateId id, double epsilon) const {
    return enable_weight(id) > epsilon;
}

std::vector<BioTemplateId> HeatGateController::active_bio_templates(double epsilon) const {
    std::vector<BioTemplateId> result;
    for (size_t i = 0; i < cached_weights_.size(); ++i) {
        if (cached_weights_[i] > epsilon) {
            result.push_back(static_cast<BioTemplateId>(i));
        }
    }
    return result;
}

HeatGateController::ScoringResult HeatGateController::score_candidate(
    const CandidateEvent& event, double beta) const
{
    ScoringResult result;

    auto idx = static_cast<size_t>(event.template_id);
    if (idx >= params_.size()) {
        result.score = 0.0;
        result.threshold = 1.0;
        result.accepted = false;
        result.accept_prob = 0.0;
        result.log_line = "BOND_CANDIDATE: INVALID_TEMPLATE";
        return result;
    }

    const auto& p = params_[idx];
    double wk = cached_weights_[idx];

    // S_e = B_k + lambda_k * w_k(h) + G_e - P_e
    // B_k is derived from alpha (base plausibility)
    double Bk = p.alpha * 0.5;  // Base score scales with template importance
    double heat_bias = p.lambda * wk;
    result.score = Bk + heat_bias + event.geometry_score - event.penalty;
    result.threshold = p.tau;

    // Deterministic accept rule
    result.accepted = (result.score >= result.threshold);

    // Probabilistic accept rule: P = sigmoid(beta * (S - tau))
    double z = beta * (result.score - result.threshold);
    result.accept_prob = 1.0 / (1.0 + std::exp(-z));

    // Structured log line
    std::ostringstream oss;
    oss << "BOND_CANDIDATE: " << bio_template_name(event.template_id)
        << " | score=" << std::fixed << std::setprecision(3) << result.score
        << " | heat=" << config_.heat_3
        << " | atoms=(" << event.atoms[0] << "," << event.atoms[1]
        << "," << event.atoms[2] << "," << event.atoms[3] << ")"
        << " | wk=" << std::setprecision(3) << wk
        << " | " << (result.accepted ? "ACCEPT" : "REJECT");
    result.log_line = oss.str();

    return result;
}

void HeatGateController::recompute_weights() {
    cached_weights_.resize(params_.size());
    for (size_t i = 0; i < params_.size(); ++i) {
        cached_weights_[i] = params_[i].enable_weight(config_.x_normalized);
    }
}

// ============================================================================
// Amino Acid Reference Table
// ============================================================================

const std::vector<AminoAcidEntry>& amino_acid_table() {
    static const std::vector<AminoAcidEntry> table = {
        //  name            3-let   1   formula        C   H   N   O   S
        { "Alanine",       "Ala", 'A', "C3H7NO2",     3,  7,  1,  2,  0 },
        { "Arginine",      "Arg", 'R', "C6H14N4O2",   6, 14,  4,  2,  0 },
        { "Asparagine",    "Asn", 'N', "C4H8N2O3",    4,  8,  2,  3,  0 },
        { "Aspartic acid", "Asp", 'D', "C4H7NO4",     4,  7,  1,  4,  0 },
        { "Cysteine",      "Cys", 'C', "C3H7NO2S",    3,  7,  1,  2,  1 },
        { "Glutamic acid", "Glu", 'E', "C5H9NO4",     5,  9,  1,  4,  0 },
        { "Glutamine",     "Gln", 'Q', "C5H10N2O3",   5, 10,  2,  3,  0 },
        { "Glycine",       "Gly", 'G', "C2H5NO2",     2,  5,  1,  2,  0 },
        { "Histidine",     "His", 'H', "C6H9N3O2",    6,  9,  3,  2,  0 },
        { "Isoleucine",    "Ile", 'I', "C6H13NO2",    6, 13,  1,  2,  0 },
        { "Leucine",       "Leu", 'L', "C6H13NO2",    6, 13,  1,  2,  0 },
        { "Lysine",        "Lys", 'K', "C6H14N2O2",   6, 14,  2,  2,  0 },
        { "Methionine",    "Met", 'M', "C5H11NO2S",   5, 11,  1,  2,  1 },
        { "Phenylalanine", "Phe", 'F', "C9H11NO2",    9, 11,  1,  2,  0 },
        { "Proline",       "Pro", 'P', "C5H9NO2",     5,  9,  1,  2,  0 },
        { "Serine",        "Ser", 'S', "C3H7NO3",     3,  7,  1,  3,  0 },
        { "Threonine",     "Thr", 'T', "C4H9NO3",     4,  9,  1,  3,  0 },
        { "Tryptophan",    "Trp", 'W', "C11H12N2O2", 11, 12,  2,  2,  0 },
        { "Tyrosine",      "Tyr", 'Y', "C9H11NO3",    9, 11,  1,  3,  0 },
        { "Valine",        "Val", 'V', "C5H11NO2",    5, 11,  1,  2,  0 },
    };
    return table;
}

// ============================================================================
// Validation Helpers
// ============================================================================

double compute_clash_score(const State& s, double r_min_factor) {
    if (s.N < 2) return 0.0;

    uint64_t n_pairs = 0;
    uint64_t n_clashes = 0;

    for (uint32_t i = 0; i < s.N; ++i) {
        for (uint32_t j = i + 1; j < s.N; ++j) {
            Vec3 dr = s.X[j] - s.X[i];
            double r = norm(dr);

            // Approximate r_min: 0.5 * sum of typical covalent radii
            // Simplified: use r_min_factor * 1.5 Å as universal threshold
            double r_min = r_min_factor * 1.5;

            ++n_pairs;
            if (r < r_min) ++n_clashes;
        }
    }

    return (n_pairs > 0) ? static_cast<double>(n_clashes) / static_cast<double>(n_pairs) : 0.0;
}

namespace {

// Rank-transform a vector for Spearman correlation
std::vector<double> rank_vector(const std::vector<double>& v) {
    size_t n = v.size();
    std::vector<size_t> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(),
              [&](size_t a, size_t b) { return v[a] < v[b]; });

    std::vector<double> ranks(n);
    for (size_t i = 0; i < n; ++i) {
        ranks[idx[i]] = static_cast<double>(i + 1);
    }
    // Handle ties: average ranks
    size_t i = 0;
    while (i < n) {
        size_t j = i;
        while (j < n && v[idx[j]] == v[idx[i]]) ++j;
        if (j > i + 1) {
            double avg = 0.0;
            for (size_t k = i; k < j; ++k) avg += ranks[idx[k]];
            avg /= static_cast<double>(j - i);
            for (size_t k = i; k < j; ++k) ranks[idx[k]] = avg;
        }
        i = j;
    }
    return ranks;
}

double pearson(const std::vector<double>& x, const std::vector<double>& y) {
    size_t n = x.size();
    if (n < 2) return 0.0;
    double mx = 0, my = 0;
    for (size_t i = 0; i < n; ++i) { mx += x[i]; my += y[i]; }
    mx /= n; my /= n;
    double num = 0, dx2 = 0, dy2 = 0;
    for (size_t i = 0; i < n; ++i) {
        double dx = x[i] - mx, dy = y[i] - my;
        num += dx * dy;
        dx2 += dx * dx;
        dy2 += dy * dy;
    }
    double denom = std::sqrt(dx2 * dy2);
    return (denom > 0) ? num / denom : 0.0;
}

double median_of(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    if (n % 2 == 0) return (v[n/2 - 1] + v[n/2]) / 2.0;
    return v[n/2];
}

double iqr_of(std::vector<double> v) {
    if (v.size() < 4) return 0.0;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    double q1 = v[n / 4];
    double q3 = v[3 * n / 4];
    return q3 - q1;
}

} // anonymous namespace

double spearman_correlation(const std::vector<double>& x,
                            const std::vector<double>& y) {
    if (x.size() != y.size() || x.size() < 2) return 0.0;
    auto rx = rank_vector(x);
    auto ry = rank_vector(y);
    return pearson(rx, ry);
}

std::vector<TemperatureBucket> aggregate_by_temperature(
    const std::vector<SingleRunMetrics>& runs)
{
    // Group by temperature
    std::map<double, std::vector<const SingleRunMetrics*>> groups;
    for (const auto& r : runs) {
        groups[r.temperature].push_back(&r);
    }

    std::vector<TemperatureBucket> buckets;
    buckets.reserve(groups.size());

    for (const auto& [temp, group] : groups) {
        TemperatureBucket bucket;
        bucket.temperature = temp;

        std::vector<double> drifts, clashes, geom_viol, msds;
        uint32_t converge_count = 0;
        bool identity_ok = true;
        double event_rate_sum = 0.0;

        for (const auto* r : group) {
            drifts.push_back(r->drift);
            clashes.push_back(r->clash_score);
            geom_viol.push_back(r->geom_violation_rate);
            msds.push_back(r->msd);
            if (r->converged) ++converge_count;
            event_rate_sum += static_cast<double>(r->n_events_accepted);

            // Identity conservation: logged = accepted + rejected
            if (r->n_events_logged != r->n_events_accepted + r->n_events_rejected) {
                identity_ok = false;
            }
        }

        bucket.median_drift = median_of(drifts);
        bucket.drift_iqr = iqr_of(drifts);
        bucket.convergence_rate = static_cast<double>(converge_count)
                                / static_cast<double>(group.size());
        bucket.median_clash = median_of(clashes);
        bucket.median_geom_violation = median_of(geom_viol);
        bucket.mean_event_rate = event_rate_sum / static_cast<double>(group.size());
        bucket.median_msd = median_of(msds);
        bucket.identity_conserved = identity_ok;

        buckets.push_back(bucket);
    }

    // Sort by temperature
    std::sort(buckets.begin(), buckets.end(),
              [](const TemperatureBucket& a, const TemperatureBucket& b) {
                  return a.temperature < b.temperature;
              });

    return buckets;
}

ValidationCampaignResult evaluate_campaign(
    const std::vector<TemperatureBucket>& buckets,
    uint32_t total_runs,
    uint32_t n_temps,
    uint32_t seeds_per_temp)
{
    ValidationCampaignResult result;
    result.total_runs = total_runs;
    result.n_temperatures = n_temps;
    result.seeds_per_temperature = seeds_per_temp;
    result.buckets = buckets;

    // Property A: Energy stability
    result.energy_drift_pass = true;
    result.convergence_pass = true;
    for (const auto& b : buckets) {
        if (b.median_drift > 0.05) result.energy_drift_pass = false;
        if (b.convergence_rate < 0.90) result.convergence_pass = false;
    }

    // Property B: Structural plausibility
    result.clash_pass = true;
    result.geom_violation_pass = true;
    for (const auto& b : buckets) {
        if (b.median_clash >= 1e-3) result.clash_pass = false;
        if (b.median_geom_violation >= 1.0) result.geom_violation_pass = false;
    }

    // Property C: Thermal response consistency
    if (buckets.size() >= 3) {
        std::vector<double> temps, rates, msds;
        for (const auto& b : buckets) {
            temps.push_back(b.temperature);
            rates.push_back(b.mean_event_rate);
            msds.push_back(b.median_msd);
        }
        double rho_rate = spearman_correlation(temps, rates);
        double rho_msd = spearman_correlation(temps, msds);
        result.rate_monotonicity_pass = (rho_rate > 0.8);
        result.msd_monotonicity_pass = (rho_msd > 0.8);
    } else {
        // Not enough data points for meaningful correlation
        result.rate_monotonicity_pass = false;
        result.msd_monotonicity_pass = false;
    }

    // Property D: Chemical identity conservation
    result.identity_conservation_pass = true;
    for (const auto& b : buckets) {
        if (!b.identity_conserved) {
            result.identity_conservation_pass = false;
            break;
        }
    }

    return result;
}

} // namespace reaction
} // namespace atomistic
