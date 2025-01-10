#include "atomistic/crystal/crystal_metrics.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <set>
#include <queue>

namespace atomistic {
namespace crystal {

// ============================================================================
// Element Symbol Table (Z → symbol)
// ============================================================================

static const char* element_symbol(uint32_t Z) {
    static const char* sym[] = {
        "?",
        "H","He","Li","Be","B","C","N","O","F","Ne",
        "Na","Mg","Al","Si","P","S","Cl","Ar","K","Ca",
        "Sc","Ti","V","Cr","Mn","Fe","Co","Ni","Cu","Zn",
        "Ga","Ge","As","Se","Br","Kr","Rb","Sr","Y","Zr",
        "Nb","Mo","Tc","Ru","Rh","Pd","Ag","Cd","In","Sn",
        "Sb","Te","I","Xe","Cs","Ba","La","Ce","Pr","Nd",
        "Pm","Sm","Eu","Gd","Tb","Dy","Ho","Er","Tm","Yb",
        "Lu","Hf","Ta","W","Re","Os","Ir","Pt","Au","Hg",
        "Tl","Pb","Bi","Po","At","Rn","Fr","Ra","Ac","Th",
        "Pa","U","Np","Pu","Am","Cm","Bk","Cf","Es","Fm"
    };
    return (Z < 101) ? sym[Z] : "?";
}

// ============================================================================
// 1. Identity Metrics
// ============================================================================

IdentityMetrics compute_identity_metrics(const UnitCell& uc) {
    IdentityMetrics im;
    
    // Element counts
    for (const auto& atom : uc.basis) {
        im.element_counts[atom.type]++;
    }
    
    // GCD of all counts for reduced formula
    int gcd = 0;
    for (const auto& [Z, count] : im.element_counts) {
        gcd = (gcd == 0) ? count : std::gcd(gcd, count);
    }
    im.formula_units_Z = gcd;
    
    // Build reduced formula string (sorted by Z)
    std::ostringstream oss;
    std::vector<std::pair<uint32_t, int>> sorted_elems(
        im.element_counts.begin(), im.element_counts.end());
    std::sort(sorted_elems.begin(), sorted_elems.end());
    
    for (const auto& [Z, count] : sorted_elems) {
        oss << element_symbol(Z);
        int reduced = count / gcd;
        if (reduced > 1) oss << reduced;
    }
    im.reduced_formula = oss.str();
    
    // Charge
    im.net_charge = 0.0;
    for (const auto& atom : uc.basis) {
        im.net_charge += atom.charge;
    }
    im.charge_neutral = (std::abs(im.net_charge) < 0.01);
    
    // Volume
    im.cell_volume = uc.lattice.V;
    
    // Density: ρ = Σ(mass) / (N_A · V)
    im.total_mass = 0.0;
    for (const auto& atom : uc.basis) {
        im.total_mass += atom.mass;
    }
    
    // Molar mass per formula unit
    im.molar_mass = im.total_mass / im.formula_units_Z;
    
    // ρ = total_mass(g/mol) / (N_A × V(Å³) × 1e-24(cm³/Å³))
    im.density_gcc = im.total_mass / (AVOGADRO * im.cell_volume * ANG3_TO_CM3);
    
    return im;
}

IdentityMetrics compute_identity_metrics(const State& s, const Lattice& lat) {
    IdentityMetrics im;
    
    for (uint32_t t : s.type) {
        im.element_counts[t]++;
    }
    
    int gcd = 0;
    for (const auto& [Z, count] : im.element_counts) {
        gcd = (gcd == 0) ? count : std::gcd(gcd, count);
    }
    im.formula_units_Z = gcd;
    
    std::ostringstream oss;
    std::vector<std::pair<uint32_t, int>> sorted_elems(
        im.element_counts.begin(), im.element_counts.end());
    std::sort(sorted_elems.begin(), sorted_elems.end());
    for (const auto& [Z, count] : sorted_elems) {
        oss << element_symbol(Z);
        int reduced = count / gcd;
        if (reduced > 1) oss << reduced;
    }
    im.reduced_formula = oss.str();
    
    im.net_charge = 0.0;
    for (double q : s.Q) im.net_charge += q;
    im.charge_neutral = (std::abs(im.net_charge) < 0.01);
    
    im.cell_volume = lat.V;
    
    im.total_mass = 0.0;
    for (double m : s.M) im.total_mass += m;
    im.molar_mass = im.total_mass / im.formula_units_Z;
    im.density_gcc = im.total_mass / (AVOGADRO * im.cell_volume * ANG3_TO_CM3);
    
    return im;
}

// ============================================================================
// 2. Symmetry Metrics
// ============================================================================

SymmetryMetrics compute_symmetry_metrics(const UnitCell& uc) {
    SymmetryMetrics sm;
    
    sm.space_group_number = uc.space_group_number;
    sm.space_group_symbol = uc.space_group_symbol;
    
    // Lattice system detection from parameters
    double a = uc.lattice.a_len();
    double b = uc.lattice.b_len();
    double c = uc.lattice.c_len();
    double alpha = uc.lattice.alpha_deg();
    double beta = uc.lattice.beta_deg();
    double gamma = uc.lattice.gamma_deg();
    
    constexpr double LEN_TOL = 0.01;   // Å
    constexpr double ANG_TOL = 0.5;    // degrees
    
    bool ab_eq = std::abs(a - b) < LEN_TOL;
    bool bc_eq = std::abs(b - c) < LEN_TOL;
    bool all_eq = ab_eq && bc_eq;
    bool all_90 = std::abs(alpha - 90.0) < ANG_TOL &&
                  std::abs(beta - 90.0) < ANG_TOL &&
                  std::abs(gamma - 90.0) < ANG_TOL;
    bool gamma_120 = std::abs(gamma - 120.0) < ANG_TOL;
    
    sm.is_cubic = all_eq && all_90;
    sm.is_tetragonal = ab_eq && !bc_eq && all_90;
    sm.is_orthorhombic = !ab_eq && !bc_eq && all_90;
    sm.is_hexagonal = ab_eq && !bc_eq && 
                      std::abs(alpha - 90.0) < ANG_TOL &&
                      std::abs(beta - 90.0) < ANG_TOL && gamma_120;
    
    if (sm.is_cubic) sm.lattice_system = "cubic";
    else if (sm.is_tetragonal) sm.lattice_system = "tetragonal";
    else if (sm.is_hexagonal) sm.lattice_system = "hexagonal";
    else if (sm.is_orthorhombic) sm.lattice_system = "orthorhombic";
    else sm.lattice_system = "other";
    
    // Site class analysis: group atoms by (type, CN-environment)
    // Build neighbor list for site classification
    std::map<uint32_t, int> type_multiplicities;
    for (const auto& atom : uc.basis) {
        type_multiplicities[atom.type]++;
    }
    
    sm.num_unique_sites = static_cast<int>(type_multiplicities.size());
    for (const auto& [Z, mult] : type_multiplicities) {
        sm.site_multiplicities.push_back(mult);
    }
    
    return sm;
}

// ============================================================================
// 3. Local Geometry Metrics
// ============================================================================

// Internal: build neighbor list from UnitCell using MIC
struct NeighborEntry {
    int i, j;
    double distance;
};

static std::vector<NeighborEntry> build_neighbor_list_uc(const UnitCell& uc, double cutoff) {
    std::vector<NeighborEntry> neighbors;
    double cutoff2 = cutoff * cutoff;
    
    int N = static_cast<int>(uc.basis.size());
    
    // Check all pairs including periodic images (±1 cell in each direction)
    for (int i = 0; i < N; ++i) {
        for (int j = i; j < N; ++j) {
            for (int pa = -1; pa <= 1; ++pa) {
                for (int pb = -1; pb <= 1; ++pb) {
                    for (int pc = -1; pc <= 1; ++pc) {
                        if (i == j && pa == 0 && pb == 0 && pc == 0) continue;
                        
                        Vec3 fj_shifted = {
                            uc.basis[j].frac.x + pa,
                            uc.basis[j].frac.y + pb,
                            uc.basis[j].frac.z + pc
                        };
                        
                        Vec3 ri = uc.lattice.to_cartesian(uc.basis[i].frac);
                        Vec3 rj = uc.lattice.to_cartesian(fj_shifted);
                        Vec3 dr = rj - ri;
                        double r2 = dot(dr, dr);
                        
                        if (r2 < cutoff2 && r2 > 0.01) {
                            neighbors.push_back({i, j, std::sqrt(r2)});
                        }
                    }
                }
            }
        }
    }
    
    return neighbors;
}

static std::vector<NeighborEntry> build_neighbor_list_state(
    const State& s, const Lattice& lat, double cutoff) 
{
    std::vector<NeighborEntry> neighbors;
    double cutoff2 = cutoff * cutoff;
    
    for (int i = 0; i < static_cast<int>(s.N); ++i) {
        Vec3 fi = lat.to_fractional(s.X[i]);
        
        for (int j = i + 1; j < static_cast<int>(s.N); ++j) {
            Vec3 fj = lat.to_fractional(s.X[j]);
            
            double r2 = lat.distance2_metric(fi, fj);
            
            if (r2 < cutoff2 && r2 > 0.01) {
                neighbors.push_back({i, j, std::sqrt(r2)});
            }
        }
    }
    
    return neighbors;
}

LocalGeometryMetrics compute_local_geometry(const UnitCell& uc, double cutoff) {
    LocalGeometryMetrics lgm;
    
    int N = static_cast<int>(uc.basis.size());
    auto neighbors = build_neighbor_list_uc(uc, cutoff);
    
    // Per-atom CN and mean bond length
    std::vector<int> CN(N, 0);
    std::vector<double> sum_r(N, 0.0);
    
    for (const auto& nb : neighbors) {
        CN[nb.i]++;
        sum_r[nb.i] += nb.distance;
        if (nb.i != nb.j) {
            CN[nb.j]++;
            sum_r[nb.j] += nb.distance;
        }
    }
    
    lgm.site_geometries.resize(N);
    for (int i = 0; i < N; ++i) {
        auto& sg = lgm.site_geometries[i];
        sg.atom_type = uc.basis[i].type;
        sg.coordination_number = CN[i];
        sg.mean_bond_length = (CN[i] > 0) ? sum_r[i] / CN[i] : 0.0;
        
        // Distortion index: std dev of bond lengths / mean
        std::vector<double> my_bonds;
        for (const auto& nb : neighbors) {
            if (nb.i == i || nb.j == i) {
                my_bonds.push_back(nb.distance);
            }
        }
        
        if (my_bonds.size() > 1) {
            double mean = sg.mean_bond_length;
            double sum_sq = 0.0;
            for (double r : my_bonds) {
                sum_sq += (r - mean) * (r - mean);
            }
            sg.distortion_index = std::sqrt(sum_sq / my_bonds.size()) / mean;
        } else {
            sg.distortion_index = 0.0;
        }
    }
    
    // Mean CN by type
    std::map<uint32_t, double> cn_sum;
    std::map<uint32_t, int> cn_count;
    for (int i = 0; i < N; ++i) {
        cn_sum[uc.basis[i].type] += CN[i];
        cn_count[uc.basis[i].type]++;
    }
    for (const auto& [Z, sum] : cn_sum) {
        lgm.mean_CN_by_type[Z] = sum / cn_count[Z];
    }
    
    // Bond statistics per pair type
    std::map<std::pair<uint32_t,uint32_t>, std::vector<double>> pair_bonds;
    for (const auto& nb : neighbors) {
        uint32_t ti = uc.basis[nb.i].type;
        uint32_t tj = uc.basis[nb.j].type;
        auto key = std::make_pair(std::min(ti, tj), std::max(ti, tj));
        pair_bonds[key].push_back(nb.distance);
    }
    
    for (const auto& [pair, lengths] : pair_bonds) {
        BondStats bs;
        bs.type_i = pair.first;
        bs.type_j = pair.second;
        bs.count = static_cast<int>(lengths.size());
        bs.min_length = *std::min_element(lengths.begin(), lengths.end());
        bs.max_length = *std::max_element(lengths.begin(), lengths.end());
        bs.mean_length = std::accumulate(lengths.begin(), lengths.end(), 0.0) / lengths.size();
        
        double var = 0.0;
        for (double r : lengths) {
            var += (r - bs.mean_length) * (r - bs.mean_length);
        }
        bs.std_dev = std::sqrt(var / lengths.size());
        
        lgm.bond_statistics.push_back(bs);
    }
    
    // Aggregate
    lgm.max_distortion = 0.0;
    for (const auto& sg : lgm.site_geometries) {
        lgm.max_distortion = std::max(lgm.max_distortion, sg.distortion_index);
    }
    lgm.rms_bond_error = 0.0;  // Requires reference data
    
    return lgm;
}

LocalGeometryMetrics compute_local_geometry(const State& s, const Lattice& lat, double cutoff) {
    // Build UnitCell-like data from State for reuse
    LocalGeometryMetrics lgm;
    
    auto neighbors = build_neighbor_list_state(s, lat, cutoff);
    
    int N = static_cast<int>(s.N);
    std::vector<int> CN(N, 0);
    std::vector<double> sum_r(N, 0.0);
    
    for (const auto& nb : neighbors) {
        CN[nb.i]++;
        CN[nb.j]++;
        sum_r[nb.i] += nb.distance;
        sum_r[nb.j] += nb.distance;
    }
    
    lgm.site_geometries.resize(N);
    for (int i = 0; i < N; ++i) {
        auto& sg = lgm.site_geometries[i];
        sg.atom_type = s.type[i];
        sg.coordination_number = CN[i];
        sg.mean_bond_length = (CN[i] > 0) ? sum_r[i] / CN[i] : 0.0;
        sg.distortion_index = 0.0;
    }
    
    // Mean CN by type
    std::map<uint32_t, double> cn_sum;
    std::map<uint32_t, int> cn_count;
    for (int i = 0; i < N; ++i) {
        cn_sum[s.type[i]] += CN[i];
        cn_count[s.type[i]]++;
    }
    for (const auto& [Z, sum] : cn_sum) {
        lgm.mean_CN_by_type[Z] = sum / cn_count[Z];
    }
    
    // Bond statistics per pair
    std::map<std::pair<uint32_t,uint32_t>, std::vector<double>> pair_bonds;
    for (const auto& nb : neighbors) {
        uint32_t ti = s.type[nb.i];
        uint32_t tj = s.type[nb.j];
        auto key = std::make_pair(std::min(ti, tj), std::max(ti, tj));
        pair_bonds[key].push_back(nb.distance);
    }
    
    for (const auto& [pair, lengths] : pair_bonds) {
        BondStats bs;
        bs.type_i = pair.first;
        bs.type_j = pair.second;
        bs.count = static_cast<int>(lengths.size());
        bs.min_length = *std::min_element(lengths.begin(), lengths.end());
        bs.max_length = *std::max_element(lengths.begin(), lengths.end());
        bs.mean_length = std::accumulate(lengths.begin(), lengths.end(), 0.0) / lengths.size();
        double var = 0.0;
        for (double r : lengths) var += (r - bs.mean_length) * (r - bs.mean_length);
        bs.std_dev = std::sqrt(var / lengths.size());
        lgm.bond_statistics.push_back(bs);
    }
    
    lgm.max_distortion = 0.0;
    lgm.rms_bond_error = 0.0;
    
    return lgm;
}

// ============================================================================
// 4. Topology Metrics
// ============================================================================

TopologyMetrics compute_topology_metrics(const UnitCell& uc, double cutoff) {
    TopologyMetrics tm;
    
    int N = static_cast<int>(uc.basis.size());
    auto neighbors = build_neighbor_list_uc(uc, cutoff);
    
    tm.total_bonds = static_cast<int>(neighbors.size());
    
    // Bonds per type
    for (const auto& nb : neighbors) {
        tm.bonds_per_type[uc.basis[nb.i].type]++;
        if (nb.i != nb.j) {
            tm.bonds_per_type[uc.basis[nb.j].type]++;
        }
    }
    
    // Connected components via BFS
    std::vector<std::vector<int>> adj(N);
    for (const auto& nb : neighbors) {
        adj[nb.i].push_back(nb.j);
        if (nb.i != nb.j) {
            adj[nb.j].push_back(nb.i);
        }
    }
    
    std::vector<bool> visited(N, false);
    tm.num_connected_components = 0;
    
    for (int start = 0; start < N; ++start) {
        if (visited[start]) continue;
        tm.num_connected_components++;
        
        std::queue<int> q;
        q.push(start);
        visited[start] = true;
        
        while (!q.empty()) {
            int curr = q.front();
            q.pop();
            
            for (int next : adj[curr]) {
                if (!visited[next]) {
                    visited[next] = true;
                    q.push(next);
                }
            }
        }
    }
    
    tm.fully_connected = (tm.num_connected_components == 1);
    
    // Sublattice connectivity
    std::map<uint32_t, std::vector<int>> type_indices;
    for (int i = 0; i < N; ++i) {
        type_indices[uc.basis[i].type].push_back(i);
    }
    
    for (const auto& [Z, indices] : type_indices) {
        // Build sublattice adjacency
        std::set<int> index_set(indices.begin(), indices.end());
        std::vector<bool> sub_visited(N, false);
        
        if (indices.empty()) {
            tm.sublattice_connected[Z] = true;
            continue;
        }
        
        std::queue<int> q;
        q.push(indices[0]);
        sub_visited[indices[0]] = true;
        int visited_count = 1;
        
        while (!q.empty()) {
            int curr = q.front();
            q.pop();
            
            for (int next : adj[curr]) {
                if (index_set.count(next) && !sub_visited[next]) {
                    sub_visited[next] = true;
                    visited_count++;
                    q.push(next);
                }
            }
        }
        
        tm.sublattice_connected[Z] = (visited_count == static_cast<int>(indices.size()));
    }
    
    // Topology hash (simplified WL)
    std::vector<uint64_t> labels(N);
    for (int i = 0; i < N; ++i) {
        labels[i] = static_cast<uint64_t>(adj[i].size()) * 1000 + uc.basis[i].type;
    }
    
    for (int iter = 0; iter < 3; ++iter) {
        std::vector<uint64_t> new_labels(N);
        for (int i = 0; i < N; ++i) {
            std::vector<uint64_t> nb_labels;
            nb_labels.push_back(labels[i]);
            for (int j : adj[i]) nb_labels.push_back(labels[j]);
            std::sort(nb_labels.begin(), nb_labels.end());
            
            uint64_t hash = 0;
            for (uint64_t lbl : nb_labels) hash = hash * 31 + lbl;
            new_labels[i] = hash;
        }
        labels = new_labels;
    }
    
    std::sort(labels.begin(), labels.end());
    tm.topology_hash = 0;
    for (uint64_t lbl : labels) tm.topology_hash = tm.topology_hash * 31 + lbl;
    
    return tm;
}

TopologyMetrics compute_topology_metrics(const State& s, const Lattice& lat, double cutoff) {
    TopologyMetrics tm;
    
    int N = static_cast<int>(s.N);
    auto neighbors = build_neighbor_list_state(s, lat, cutoff);
    
    tm.total_bonds = static_cast<int>(neighbors.size());
    
    for (const auto& nb : neighbors) {
        tm.bonds_per_type[s.type[nb.i]]++;
        tm.bonds_per_type[s.type[nb.j]]++;
    }
    
    std::vector<std::vector<int>> adj(N);
    for (const auto& nb : neighbors) {
        adj[nb.i].push_back(nb.j);
        adj[nb.j].push_back(nb.i);
    }
    
    std::vector<bool> visited(N, false);
    tm.num_connected_components = 0;
    
    for (int start = 0; start < N; ++start) {
        if (visited[start]) continue;
        tm.num_connected_components++;
        std::queue<int> q;
        q.push(start);
        visited[start] = true;
        while (!q.empty()) {
            int curr = q.front(); q.pop();
            for (int next : adj[curr]) {
                if (!visited[next]) { visited[next] = true; q.push(next); }
            }
        }
    }
    
    tm.fully_connected = (tm.num_connected_components == 1);
    tm.topology_hash = 0;
    
    return tm;
}

// ============================================================================
// 5. Reciprocal Space Metrics
// ============================================================================

ReciprocalMetrics compute_reciprocal_metrics(const Lattice& lat, 
                                              double two_theta_max,
                                              double wavelength) {
    ReciprocalMetrics rm;
    
    // Reciprocal lattice vectors: b_i = (2π/V) * (a_j × a_k)
    // For d-spacings we use: 1/d² = h²a*² + k²b*² + l²c*² + cross terms
    // Simplified for orthogonal: 1/d² = (h/a)² + (k/b)² + (l/c)²
    
    double a = lat.a_len();
    double b = lat.b_len();
    double c = lat.c_len();
    
    rm.a_star = 1.0 / a;
    rm.b_star = 1.0 / b;
    rm.c_star = 1.0 / c;
    
    // Compute d-spacings for Miller indices up to hmax
    double d_min = wavelength / (2.0 * std::sin(two_theta_max * M_PI / 360.0));
    int hmax = static_cast<int>(a / d_min) + 1;
    int kmax = static_cast<int>(b / d_min) + 1;
    int lmax = static_cast<int>(c / d_min) + 1;
    
    // Use metric tensor for general case: 1/d² = hᵀ G* h
    // G* = (Aᵀ)⁻¹ · A⁻¹ = (AᵀA)⁻¹ = G⁻¹
    Mat3 G_inv = lat.G.inverse();
    
    std::set<std::tuple<double, int, int, int>> unique_peaks;  // {-d, h, k, l}
    
    for (int h = 0; h <= hmax; ++h) {
        for (int k = (h == 0 ? 0 : -kmax); k <= kmax; ++k) {
            for (int l = (h == 0 && k == 0 ? 1 : -lmax); l <= lmax; ++l) {
                Vec3 hkl = {static_cast<double>(h), static_cast<double>(k), 
                           static_cast<double>(l)};
                Vec3 G_hkl = G_inv.mul(hkl);
                double inv_d2 = dot(hkl, G_hkl);
                
                if (inv_d2 < 1e-10) continue;
                
                double d = 1.0 / std::sqrt(inv_d2);
                double sin_theta = wavelength / (2.0 * d);
                
                if (sin_theta > 1.0 || sin_theta < 0.0) continue;
                
                double two_theta = 2.0 * std::asin(sin_theta) * 180.0 / M_PI;
                
                if (two_theta <= two_theta_max) {
                    unique_peaks.insert({-d, h, k, l});  // Negative d for descending sort
                }
            }
        }
    }
    
    for (const auto& [neg_d, h, k, l] : unique_peaks) {
        DSpacing ds;
        ds.h = h;
        ds.k = k;
        ds.l = l;
        ds.d = -neg_d;
        ds.two_theta = 2.0 * std::asin(wavelength / (2.0 * ds.d)) * 180.0 / M_PI;
        rm.d_spacings.push_back(ds);
    }
    
    rm.num_peaks = static_cast<int>(rm.d_spacings.size());
    
    return rm;
}

// ============================================================================
// 6. Relaxation Stability Metrics
// ============================================================================

RelaxationMetrics compute_relaxation_metrics(const UnitCell& uc_before,
                                              const State& s_after,
                                              const Lattice& lat_after) {
    RelaxationMetrics rm;
    
    // Volume drift
    double V_before = uc_before.lattice.V;
    double V_after = lat_after.V;
    rm.volume_drift = (V_after - V_before) / V_before;
    
    // Lattice parameter drifts
    rm.a_drift = (lat_after.a_len() - uc_before.lattice.a_len()) / uc_before.lattice.a_len();
    rm.b_drift = (lat_after.b_len() - uc_before.lattice.b_len()) / uc_before.lattice.b_len();
    rm.c_drift = (lat_after.c_len() - uc_before.lattice.c_len()) / uc_before.lattice.c_len();
    
    // Atom displacements
    rm.max_displacement = 0.0;
    double sum_sq_disp = 0.0;
    
    int N = std::min(static_cast<int>(uc_before.basis.size()), static_cast<int>(s_after.N));
    
    for (int i = 0; i < N; ++i) {
        Vec3 r_before = uc_before.lattice.to_cartesian(uc_before.basis[i].frac);
        Vec3 r_after = s_after.X[i];
        Vec3 dr = r_after - r_before;
        double d = norm(dr);
        
        rm.max_displacement = std::max(rm.max_displacement, d);
        sum_sq_disp += d * d;
    }
    
    rm.rms_displacement = (N > 0) ? std::sqrt(sum_sq_disp / N) : 0.0;
    
    // Energy per atom
    rm.energy_per_atom = s_after.E.total() / s_after.N;
    
    // Topology preservation check
    auto topo_before = compute_topology_metrics(uc_before, 3.5);
    auto topo_after = compute_topology_metrics(s_after, lat_after, 3.5);
    rm.topology_preserved = (topo_before.topology_hash == topo_after.topology_hash) ||
                            (topo_before.num_connected_components == topo_after.num_connected_components);
    
    rm.converged = true;  // Set by caller after FIRE
    
    return rm;
}

// ============================================================================
// Verification Scorecard
// ============================================================================

void VerificationScorecard::add(const std::string& name, bool pass, double val, 
                                 double expected, double err_pct, 
                                 const std::string& note) {
    entries.push_back({name, pass, val, expected, err_pct, note});
    total_checks++;
    if (pass) passed++;
    else failed++;
    pass_rate = (total_checks > 0) ? 100.0 * passed / total_checks : 0.0;
}

void VerificationScorecard::print() const {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  CRYSTAL VERIFICATION SCORECARD: " << std::setw(30) << std::left 
              << crystal_name << "  ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  " << std::setw(28) << std::left << "Check"
              << std::setw(10) << "Status"
              << std::setw(10) << "Value"
              << std::setw(10) << "Expected"
              << std::setw(8) << "Err%"
              << "  ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════════╣\n";
    
    for (const auto& e : entries) {
        std::cout << "║  " << std::setw(28) << std::left << e.check_name
                  << (e.passed ? "  ✓  " : "  ✗  ")
                  << std::setw(10) << std::fixed << std::setprecision(3) << e.value
                  << std::setw(10) << e.expected
                  << std::setw(8) << std::setprecision(1) << e.error_pct
                  << "  ║\n";
    }
    
    std::cout << "╠══════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  PASSED: " << passed << " / " << total_checks 
              << "  (" << std::fixed << std::setprecision(1) << pass_rate << "%)";
    
    int pad = 50 - 12 - std::to_string(passed).length() - std::to_string(total_checks).length();
    for (int i = 0; i < pad; ++i) std::cout << " ";
    std::cout << "  ║\n";
    
    if (failed == 0) {
        std::cout << "║  ✓ ALL CHECKS PASSED                                            ║\n";
    } else {
        std::cout << "║  ✗ " << failed << " CHECK(S) FAILED                                          ║\n";
    }
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";
}

VerificationScorecard verify_against_reference(const UnitCell& uc,
                                                const ReferenceData& ref,
                                                double cutoff) {
    VerificationScorecard sc;
    sc.crystal_name = uc.name;
    sc.total_checks = 0;
    sc.passed = 0;
    sc.failed = 0;
    
    auto id = compute_identity_metrics(uc);
    auto sym = compute_symmetry_metrics(uc);
    auto geo = compute_local_geometry(uc, cutoff);
    auto recip = compute_reciprocal_metrics(uc.lattice);
    
    // 1. Formula match
    sc.add("Formula match", id.reduced_formula == ref.formula,
           0, 0, 0, id.reduced_formula + " vs " + ref.formula);
    
    // 2. Cell volume error
    double V_ref = ref.a * ref.b * ref.c;  // Approximate for orthogonal
    double V_err = 100.0 * std::abs(id.cell_volume - V_ref) / V_ref;
    sc.add("Cell volume", V_err < 5.0, id.cell_volume, V_ref, V_err);
    
    // 3. Density error
    double rho_err = 100.0 * std::abs(id.density_gcc - ref.density_gcc) / ref.density_gcc;
    sc.add("Density (g/cm³)", rho_err < 5.0, id.density_gcc, ref.density_gcc, rho_err);
    
    // 4. Lattice parameter errors
    double a_err = 100.0 * std::abs(uc.lattice.a_len() - ref.a) / ref.a;
    double b_err = 100.0 * std::abs(uc.lattice.b_len() - ref.b) / ref.b;
    double c_err = 100.0 * std::abs(uc.lattice.c_len() - ref.c) / ref.c;
    sc.add("Lattice a", a_err < 2.0, uc.lattice.a_len(), ref.a, a_err);
    sc.add("Lattice b", b_err < 2.0, uc.lattice.b_len(), ref.b, b_err);
    sc.add("Lattice c", c_err < 2.0, uc.lattice.c_len(), ref.c, c_err);
    
    // 5. Space group match
    sc.add("Space group", sym.space_group_number == ref.space_group,
           static_cast<double>(sym.space_group_number), 
           static_cast<double>(ref.space_group), 0);
    
    // 6. Formula units (Z)
    sc.add("Formula units Z", id.formula_units_Z == ref.Z,
           static_cast<double>(id.formula_units_Z), 
           static_cast<double>(ref.Z), 0);
    
    // 7. CN match per site
    for (const auto& [Z, expected_cn] : ref.expected_CN) {
        if (geo.mean_CN_by_type.count(Z)) {
            double actual_cn = geo.mean_CN_by_type.at(Z);
            bool cn_ok = std::abs(actual_cn - expected_cn) < 1.0;
            sc.add(std::string("CN ") + element_symbol(Z), cn_ok, 
                   actual_cn, static_cast<double>(expected_cn),
                   100.0 * std::abs(actual_cn - expected_cn) / expected_cn);
        }
    }
    
    // 8. Bond length ranges
    for (const auto& [pair, range] : ref.expected_bonds) {
        for (const auto& bs : geo.bond_statistics) {
            if ((bs.type_i == pair.first && bs.type_j == pair.second) ||
                (bs.type_i == pair.second && bs.type_j == pair.first)) {
                bool in_range = (bs.mean_length >= range.first * 0.95) && 
                               (bs.mean_length <= range.second * 1.05);
                sc.add(std::string(element_symbol(pair.first)) + "-" + 
                       element_symbol(pair.second) + " bond",
                       in_range, bs.mean_length, 
                       (range.first + range.second) / 2.0, 0);
            }
        }
    }
    
    // 9. XRD peak positions
    if (!ref.expected_peaks.empty() && !recip.d_spacings.empty()) {
        int peaks_matched = 0;
        for (double ref_peak : ref.expected_peaks) {
            double best_err = 1e9;
            for (const auto& ds : recip.d_spacings) {
                double err = std::abs(ds.two_theta - ref_peak);
                best_err = std::min(best_err, err);
            }
            if (best_err < 1.0) peaks_matched++;
        }
        double match_frac = static_cast<double>(peaks_matched) / ref.expected_peaks.size();
        sc.add("XRD peak match", match_frac > 0.8,
               match_frac * 100.0, 100.0, (1.0 - match_frac) * 100.0);
    }
    
    // 10. Charge neutrality
    sc.add("Charge neutral", id.charge_neutral, 
           id.net_charge, 0.0, 0);
    
    return sc;
}

// ============================================================================
// Full Metrics Bundle
// ============================================================================

CrystalMetrics compute_all_metrics(const UnitCell& uc, double cutoff) {
    CrystalMetrics cm;
    cm.identity = compute_identity_metrics(uc);
    cm.symmetry = compute_symmetry_metrics(uc);
    cm.geometry = compute_local_geometry(uc, cutoff);
    cm.topology = compute_topology_metrics(uc, cutoff);
    cm.reciprocal = compute_reciprocal_metrics(uc.lattice);
    return cm;
}

void CrystalMetrics::print_summary() const {
    std::cout << "\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "  CRYSTAL METRICS SUMMARY\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
    
    // Identity
    std::cout << "  ── Identity ──\n";
    std::cout << "    Formula:        " << identity.reduced_formula << "\n";
    std::cout << "    Z (fu/cell):    " << identity.formula_units_Z << "\n";
    std::cout << "    Cell volume:    " << std::fixed << std::setprecision(3) 
              << identity.cell_volume << " Å³\n";
    std::cout << "    Density:        " << identity.density_gcc << " g/cm³\n";
    std::cout << "    Net charge:     " << identity.net_charge << " e";
    std::cout << (identity.charge_neutral ? "  ✓" : "  ✗") << "\n\n";
    
    // Symmetry
    std::cout << "  ── Symmetry ──\n";
    std::cout << "    Lattice system: " << symmetry.lattice_system << "\n";
    std::cout << "    Space group:    " << symmetry.space_group_number 
              << " (" << symmetry.space_group_symbol << ")\n";
    std::cout << "    Unique sites:   " << symmetry.num_unique_sites << "\n\n";
    
    // Geometry
    std::cout << "  ── Local Geometry ──\n";
    for (const auto& [Z, cn] : geometry.mean_CN_by_type) {
        std::cout << "    CN(" << element_symbol(Z) << "):        " 
                  << std::setprecision(1) << cn << "\n";
    }
    
    for (const auto& bs : geometry.bond_statistics) {
        std::cout << "    " << element_symbol(bs.type_i) << "-" << element_symbol(bs.type_j) 
                  << ":         " << std::setprecision(3) << bs.mean_length 
                  << " Å  [" << bs.min_length << ", " << bs.max_length << "]\n";
    }
    std::cout << "    Max distortion: " << std::setprecision(4) 
              << geometry.max_distortion << "\n\n";
    
    // Topology
    std::cout << "  ── Topology ──\n";
    std::cout << "    Total bonds:    " << topology.total_bonds << "\n";
    std::cout << "    Connected:      " << (topology.fully_connected ? "yes" : "no") 
              << " (" << topology.num_connected_components << " component(s))\n";
    std::cout << "    Hash:           0x" << std::hex << topology.topology_hash 
              << std::dec << "\n\n";
    
    // Reciprocal
    std::cout << "  ── Reciprocal Space ──\n";
    std::cout << "    Peaks (2θ<90°): " << reciprocal.num_peaks << "\n";
    if (!reciprocal.d_spacings.empty()) {
        int show = std::min(5, static_cast<int>(reciprocal.d_spacings.size()));
        std::cout << "    Top " << show << " d-spacings:\n";
        for (int i = 0; i < show; ++i) {
            const auto& ds = reciprocal.d_spacings[i];
            std::cout << "      (" << ds.h << ds.k << ds.l << ")  d=" 
                      << std::setprecision(3) << ds.d 
                      << " Å   2θ=" << ds.two_theta << "°\n";
        }
    }
    
    std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
}

} // namespace crystal
} // namespace atomistic
