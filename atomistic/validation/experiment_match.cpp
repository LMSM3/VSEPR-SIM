#include "atomistic/validation/experiment_match.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <cassert>

namespace atomistic {
namespace validation {

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

// Compute per-atom coordination numbers within cutoff
std::vector<int> coordination_numbers(const State& s, double cutoff) {
    const double rc2 = cutoff * cutoff;
    std::vector<int> cn(s.N, 0);

    for (uint32_t i = 0; i < s.N; ++i) {
        for (uint32_t j = i + 1; j < s.N; ++j) {
            Vec3 dr = s.box.delta(s.X[i], s.X[j]);
            double d2 = dot(dr, dr);
            if (d2 < rc2) {
                ++cn[i];
                ++cn[j];
            }
        }
    }
    return cn;
}

// Per-atom potential energy via half-pair virial decomposition:
//   e_i ≈ -0.5 * Σ_j F_ij · r_ij  (virial estimate)
// Fallback: distribute total PE equally when pair decomposition not available.
std::vector<double> per_atom_energy(const State& s, double cutoff) {
    const uint32_t N = s.N;
    std::vector<double> ea(N, 0.0);

    // If forces are available, use virial decomposition
    bool has_forces = (s.F.size() == N);
    if (!has_forces) {
        // Uniform fallback
        double e_each = s.E.total() / static_cast<double>(N);
        std::fill(ea.begin(), ea.end(), e_each);
        return ea;
    }

    // Pair virial decomposition: e_i = 0.5 * Σ_j (f_ij · r_ij) / 2
    // Approximate by projecting total force onto displacement to each neighbour
    const double rc2 = cutoff * cutoff;
    for (uint32_t i = 0; i < N; ++i) {
        double sum = 0.0;
        int count = 0;
        for (uint32_t j = 0; j < N; ++j) {
            if (i == j) continue;
            Vec3 dr = s.box.delta(s.X[i], s.X[j]);
            double d2 = dot(dr, dr);
            if (d2 < rc2 && d2 > 1e-12) {
                double r = std::sqrt(d2);
                // Radial force projection
                double f_r = (s.F[i].x * dr.x + s.F[i].y * dr.y + s.F[i].z * dr.z) / r;
                sum += f_r * r;
                ++count;
            }
        }
        // virial contribution: e_i ≈ -0.5 * sum / count (avg pair)
        ea[i] = (count > 0) ? -0.5 * sum : 0.0;
    }

    // Rescale so that Σe_i = total PE (conserve total energy)
    double raw_total = std::accumulate(ea.begin(), ea.end(), 0.0);
    double target = s.E.total();
    if (std::abs(raw_total) > 1e-30) {
        double scale = target / raw_total;
        for (auto& e : ea) e *= scale;
    } else {
        double e_each = target / static_cast<double>(N);
        std::fill(ea.begin(), ea.end(), e_each);
    }

    return ea;
}

// Simple spherical surface area estimate: fit sphere to positions, A = 4πR²
double estimate_surface_area(const State& s) {
    if (s.N == 0) return 0.0;

    // Centroid
    Vec3 c{0, 0, 0};
    for (uint32_t i = 0; i < s.N; ++i) {
        c.x += s.X[i].x;
        c.y += s.X[i].y;
        c.z += s.X[i].z;
    }
    c.x /= s.N; c.y /= s.N; c.z /= s.N;

    // RMS distance from centroid
    double sum_r2 = 0.0;
    double max_r  = 0.0;
    for (uint32_t i = 0; i < s.N; ++i) {
        Vec3 dr = s.X[i] - c;
        double r2 = dot(dr, dr);
        sum_r2 += r2;
        double r = std::sqrt(r2);
        if (r > max_r) max_r = r;
    }

    // Use max radius as effective nanoparticle radius
    double R = max_r;
    return 4.0 * M_PI * R * R;
}

// Score helper: Gaussian penalty, 1 = perfect, decays with deviation
double gaussian_score(double sim, double exp_val, double tolerance) {
    if (tolerance <= 0.0) return 1.0;
    double delta = sim - exp_val;
    double z = delta / tolerance;
    return std::exp(-0.5 * z * z);
}

// Score helper: fractional deviation score
double fractional_score(double sim, double exp_val, double tol_frac) {
    if (std::abs(exp_val) < 1e-30) return (std::abs(sim) < 1e-30) ? 1.0 : 0.0;
    double frac = std::abs((sim - exp_val) / exp_val);
    double z = frac / tol_frac;
    return std::exp(-0.5 * z * z);
}

} // anonymous namespace

// ============================================================================
// classify_sites
// ============================================================================

std::vector<SiteClass> classify_sites(
    const State& s,
    double cutoff,
    int bulk_CN_thresh)
{
    auto cn = coordination_numbers(s, cutoff);
    std::vector<SiteClass> classes(s.N, SiteClass::Bulk);

    // Thresholds derived from bulk CN:
    //   bulk:    CN >= bulk_CN_thresh
    //   surface: CN in [bulk_CN_thresh - 3, bulk_CN_thresh)
    //   edge:    CN in [bulk_CN_thresh - 6, bulk_CN_thresh - 3)
    //   vertex:  CN < bulk_CN_thresh - 6
    int surf_min = bulk_CN_thresh - 3;
    int edge_min = bulk_CN_thresh - 6;

    for (uint32_t i = 0; i < s.N; ++i) {
        if (cn[i] >= bulk_CN_thresh) {
            classes[i] = SiteClass::Bulk;
        } else if (cn[i] >= surf_min) {
            classes[i] = SiteClass::Surface;
        } else if (cn[i] >= edge_min) {
            classes[i] = SiteClass::Edge;
        } else {
            classes[i] = SiteClass::Vertex;
        }
    }

    return classes;
}

// ============================================================================
// compute_geometric_energy
// ============================================================================

GeometricEnergyDecomposition compute_geometric_energy(
    const State& s,
    const std::vector<SiteClass>& classes,
    double cutoff)
{
    assert(classes.size() == s.N);

    GeometricEnergyDecomposition geo{};

    auto ea = per_atom_energy(s, cutoff);

    double sum_bulk = 0, sum_surf = 0, sum_edge = 0, sum_vert = 0;

    for (uint32_t i = 0; i < s.N; ++i) {
        switch (classes[i]) {
            case SiteClass::Bulk:
                ++geo.n_bulk;    sum_bulk += ea[i]; break;
            case SiteClass::Surface:
                ++geo.n_surface; sum_surf += ea[i]; break;
            case SiteClass::Edge:
                ++geo.n_edge;    sum_edge += ea[i]; break;
            case SiteClass::Vertex:
                ++geo.n_vertex;  sum_vert += ea[i]; break;
        }
    }

    geo.E_bulk    = (geo.n_bulk    > 0) ? sum_bulk / geo.n_bulk    : 0.0;
    geo.E_surface = (geo.n_surface > 0) ? sum_surf / geo.n_surface : 0.0;
    geo.E_edge    = (geo.n_edge    > 0) ? sum_edge / geo.n_edge    : 0.0;
    geo.E_vertex  = (geo.n_vertex  > 0) ? sum_vert / geo.n_vertex  : 0.0;

    // Cohesive energy per atom (total PE / N, typically negative)
    geo.E_cohesive_per_atom = s.E.total() / static_cast<double>(s.N);

    // Surface area estimate
    geo.surface_area_est = estimate_surface_area(s);

    // Surface energy per area: excess energy of surface atoms relative to bulk
    if (geo.surface_area_est > 1e-10 && geo.n_surface > 0 && geo.n_bulk > 0) {
        double excess = sum_surf - static_cast<double>(geo.n_surface) * geo.E_bulk;
        geo.surface_energy_per_area = excess / geo.surface_area_est;
    }

    return geo;
}

// ============================================================================
// compute_structural_fingerprint
// ============================================================================

StructuralFingerprint compute_structural_fingerprint(
    const State& s,
    double cutoff,
    double rdf_dr,
    double rdf_max)
{
    StructuralFingerprint fp{};

    // --- CN histogram ---
    auto cn = coordination_numbers(s, cutoff);
    double cn_sum = 0;
    for (uint32_t i = 0; i < s.N; ++i) {
        fp.cn_histogram[cn[i]]++;
        cn_sum += cn[i];
    }
    fp.mean_CN = (s.N > 0) ? cn_sum / s.N : 0.0;

    // --- RDF ---
    const int n_bins = static_cast<int>(rdf_max / rdf_dr);
    std::vector<double> hist(n_bins, 0.0);

    for (uint32_t i = 0; i < s.N; ++i) {
        for (uint32_t j = i + 1; j < s.N; ++j) {
            Vec3 dr = s.box.delta(s.X[i], s.X[j]);
            double d = norm(dr);
            int bin = static_cast<int>(d / rdf_dr);
            if (bin >= 0 && bin < n_bins) {
                hist[bin] += 2.0; // count both i-j and j-i
            }
        }
    }

    // Normalize: g(r) = hist / (N * 4π r² dr * ρ)
    // For finite nanoparticle, use number density ρ = N / V_sphere
    double R_max = 0.0;
    {
        Vec3 c{0, 0, 0};
        for (uint32_t i = 0; i < s.N; ++i) {
            c.x += s.X[i].x; c.y += s.X[i].y; c.z += s.X[i].z;
        }
        c.x /= s.N; c.y /= s.N; c.z /= s.N;
        for (uint32_t i = 0; i < s.N; ++i) {
            Vec3 dr = s.X[i] - c;
            double r = norm(dr);
            if (r > R_max) R_max = r;
        }
    }
    double V_est = (4.0 / 3.0) * M_PI * R_max * R_max * R_max;
    double rho = (V_est > 1e-10) ? static_cast<double>(s.N) / V_est : 1.0;

    std::vector<double> g_r(n_bins, 0.0);
    for (int b = 0; b < n_bins; ++b) {
        double r_lo = b * rdf_dr;
        double r_hi = (b + 1) * rdf_dr;
        double r_mid = 0.5 * (r_lo + r_hi);
        double shell_vol = (4.0 / 3.0) * M_PI * (r_hi * r_hi * r_hi - r_lo * r_lo * r_lo);
        double ideal = static_cast<double>(s.N) * rho * shell_vol;
        g_r[b] = (ideal > 1e-30) ? hist[b] / ideal : 0.0;
    }

    // Extract peaks (local maxima in g(r))
    for (int b = 1; b < n_bins - 1; ++b) {
        if (g_r[b] > g_r[b - 1] && g_r[b] > g_r[b + 1] && g_r[b] > 1.0) {
            double r_peak = (b + 0.5) * rdf_dr;
            fp.rdf_peak_positions.push_back(r_peak);
            fp.rdf_peak_heights.push_back(g_r[b]);
        }
    }

    // --- Bond angle distribution (triplets within cutoff) ---
    // Sample up to first 500 atoms to keep cost manageable at 4-6nm scale
    const uint32_t N_sample = std::min(s.N, static_cast<uint32_t>(500));
    const int angle_bins = 180;
    std::vector<double> angle_hist(angle_bins, 0.0);
    const double rc2 = cutoff * cutoff;

    for (uint32_t i = 0; i < N_sample; ++i) {
        // Collect neighbours of i
        std::vector<Vec3> neigh_dr;
        for (uint32_t j = 0; j < s.N; ++j) {
            if (i == j) continue;
            Vec3 dr = s.box.delta(s.X[i], s.X[j]);
            if (dot(dr, dr) < rc2) {
                neigh_dr.push_back(dr);
            }
        }
        // All angle triplets centred on i
        for (size_t a = 0; a < neigh_dr.size(); ++a) {
            for (size_t b = a + 1; b < neigh_dr.size(); ++b) {
                double cos_theta = dot(neigh_dr[a], neigh_dr[b])
                    / (norm(neigh_dr[a]) * norm(neigh_dr[b]));
                cos_theta = std::max(-1.0, std::min(1.0, cos_theta));
                double theta_deg = std::acos(cos_theta) * 180.0 / M_PI;
                int bin = static_cast<int>(theta_deg);
                if (bin >= 0 && bin < angle_bins) {
                    angle_hist[bin] += 1.0;
                }
            }
        }
    }

    // Extract BAD peaks
    for (int b = 1; b < angle_bins - 1; ++b) {
        if (angle_hist[b] > angle_hist[b - 1] && angle_hist[b] > angle_hist[b + 1]
            && angle_hist[b] > 0.0) {
            fp.bad_peak_angles.push_back(static_cast<double>(b) + 0.5);
        }
    }

    return fp;
}

// ============================================================================
// compute_dynamics
// ============================================================================

DynamicsObservables compute_dynamics(
    const std::vector<State>& trajectory,
    double dt,
    uint32_t max_lag)
{
    DynamicsObservables dyn{};

    if (trajectory.size() < 2) return dyn;

    const uint32_t N = trajectory[0].N;
    const uint32_t n_frames = static_cast<uint32_t>(trajectory.size());
    if (max_lag == 0) max_lag = n_frames / 2;
    max_lag = std::min(max_lag, n_frames - 1);

    // --- MSD ---
    dyn.msd_times.resize(max_lag);
    dyn.msd_values.resize(max_lag, 0.0);

    for (uint32_t lag = 1; lag <= max_lag; ++lag) {
        double msd_sum = 0.0;
        uint32_t count = 0;
        for (uint32_t t0 = 0; t0 + lag < n_frames; ++t0) {
            for (uint32_t i = 0; i < N; ++i) {
                Vec3 dr = trajectory[t0 + lag].X[i] - trajectory[t0].X[i];
                msd_sum += dot(dr, dr);
                ++count;
            }
        }
        dyn.msd_times[lag - 1]  = lag * dt;
        dyn.msd_values[lag - 1] = (count > 0) ? msd_sum / count : 0.0;
    }

    // Diffusion coefficient: D = lim_{t→∞} MSD / (6t)
    // Linear fit over last half of MSD curve
    if (max_lag >= 4) {
        uint32_t fit_start = max_lag / 2;
        double sum_t = 0, sum_m = 0, sum_t2 = 0, sum_tm = 0;
        uint32_t n_fit = 0;
        for (uint32_t k = fit_start; k < max_lag; ++k) {
            double t = dyn.msd_times[k];
            double m = dyn.msd_values[k];
            sum_t  += t;
            sum_m  += m;
            sum_t2 += t * t;
            sum_tm += t * m;
            ++n_fit;
        }
        // slope = (n*Σtm - Σt*Σm) / (n*Σt² - (Σt)²)
        double denom = n_fit * sum_t2 - sum_t * sum_t;
        if (std::abs(denom) > 1e-30) {
            double slope = (n_fit * sum_tm - sum_t * sum_m) / denom;
            dyn.diffusion_coeff = slope / 6.0;
        }
    }

    // --- Lindemann index ---
    // δ_L = (2 / N(N-1)) Σ_{i<j} sqrt(<r_ij²> - <r_ij>²) / <r_ij>
    // Compute over trajectory
    if (N >= 2) {
        // Only sample a subset for large N to keep O(N²·T) manageable
        const uint32_t N_lind = std::min(N, static_cast<uint32_t>(300));
        double lind_sum = 0.0;
        uint32_t pair_count = 0;

        for (uint32_t i = 0; i < N_lind; ++i) {
            for (uint32_t j = i + 1; j < N_lind; ++j) {
                double sum_r = 0.0, sum_r2 = 0.0;
                for (uint32_t t = 0; t < n_frames; ++t) {
                    Vec3 dr = trajectory[t].box.delta(
                        trajectory[t].X[i], trajectory[t].X[j]);
                    double r = norm(dr);
                    sum_r  += r;
                    sum_r2 += r * r;
                }
                double mean_r  = sum_r  / n_frames;
                double mean_r2 = sum_r2 / n_frames;
                double var = mean_r2 - mean_r * mean_r;
                if (var < 0) var = 0;
                double rms_fluct = std::sqrt(var);
                if (mean_r > 1e-10) {
                    lind_sum += rms_fluct / mean_r;
                    ++pair_count;
                }
            }
        }
        dyn.lindemann_index = (pair_count > 0) ? lind_sum / pair_count : 0.0;
    }

    return dyn;
}

// ============================================================================
// score_against_experiment
// ============================================================================

MatchScore score_against_experiment(
    const GeometricEnergyDecomposition& geo,
    const StructuralFingerprint& struc,
    const DynamicsObservables& dyn,
    const ExperimentalRecord& ref)
{
    MatchScore ms{};
    ms.label = ref.label;

    // --- Cohesive energy ---
    ms.delta_cohesive = geo.E_cohesive_per_atom - ref.cohesive_energy;
    ms.score_cohesive = fractional_score(
        geo.E_cohesive_per_atom, ref.cohesive_energy, ref.tol_energy_frac);

    // --- Surface energy ---
    ms.delta_surface_energy = geo.surface_energy_per_area - ref.surface_energy_per_area;
    ms.score_surface_energy = fractional_score(
        geo.surface_energy_per_area, ref.surface_energy_per_area, ref.tol_energy_frac);

    // --- Coordination number ---
    ms.delta_CN = struc.mean_CN - ref.bulk_CN;
    ms.score_CN = gaussian_score(struc.mean_CN, ref.bulk_CN, ref.tol_CN);

    // --- Diffusion coefficient ---
    if (ref.diffusion_coeff > 0 && dyn.diffusion_coeff > 0) {
        ms.delta_D = dyn.diffusion_coeff - ref.diffusion_coeff;
        ms.score_diffusion = fractional_score(
            dyn.diffusion_coeff, ref.diffusion_coeff, ref.tol_D_frac);
    } else {
        ms.score_diffusion = 1.0; // no data → no penalty
    }

    // --- Lindemann ---
    if (ref.lindemann_threshold > 0) {
        ms.score_lindemann = (dyn.lindemann_index < ref.lindemann_threshold) ? 1.0 : 0.5;
    } else {
        ms.score_lindemann = 1.0;
    }

    // --- Composite: weighted geometric mean ---
    // Weights: cohesive(3), surface(2), CN(2), diffusion(1), lindemann(1)
    double log_sum = 3.0 * std::log(ms.score_cohesive + 1e-30)
                   + 2.0 * std::log(ms.score_surface_energy + 1e-30)
                   + 2.0 * std::log(ms.score_CN + 1e-30)
                   + 1.0 * std::log(ms.score_diffusion + 1e-30)
                   + 1.0 * std::log(ms.score_lindemann + 1e-30);
    ms.composite = std::exp(log_sum / 9.0);

    return ms;
}

} // namespace validation
} // namespace atomistic
