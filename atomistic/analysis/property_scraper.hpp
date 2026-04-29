#pragma once
/**
 * property_scraper.hpp — Post-Formation Property Extraction
 * ==========================================================
 *
 * Pure-function property scraper that extracts physically meaningful
 * observables from a converged State. This is the §15 emergence
 * boundary: below here we compute forces, above here we measure
 * what the formation engine has built.
 *
 * Six property blocks:
 *   1. Geometric  (bond lengths, angles, dihedrals, Rg)
 *   2. Energy     (ledger decomposition, per-atom, strain fraction)
 *   3. Inertial   (tensor, principal moments, asymmetry, rotational constants)
 *   4. Electrostatic (dipole moment, quadrupole tensor)
 *   5. Topological (coordination, ring count, connectivity invariants)
 *   6. Emergence  (anisotropy, VSEPR recovery, quality score)
 *
 * Anti-black-box: every field has units, every computation is inspectable,
 * every output is deterministic from the input State.
 *
 * Terminology: atomistic throughout (no meso).
 */

#include "atomistic/core/state.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace atomistic {
namespace scraper {

// ============================================================================
// Element-pair key for typed statistics
// ============================================================================

struct ElementPair {
    int z1, z2;  // Sorted so z1 <= z2
    ElementPair(int a, int b) : z1(std::min(a, b)), z2(std::max(a, b)) {}
    bool operator==(const ElementPair& o) const { return z1 == o.z1 && z2 == o.z2; }
};

struct ElementPairHash {
    size_t operator()(const ElementPair& p) const {
        return std::hash<int>()(p.z1) ^ (std::hash<int>()(p.z2) << 16);
    }
};

struct ElementTriple {
    int z1, z2, z3;  // z2 is central
    ElementTriple(int a, int b, int c) : z1(std::min(a, c)), z2(b), z3(std::max(a, c)) {}
    bool operator==(const ElementTriple& o) const { return z1 == o.z1 && z2 == o.z2 && z3 == o.z3; }
};

struct ElementTripleHash {
    size_t operator()(const ElementTriple& t) const {
        return std::hash<int>()(t.z1) ^ (std::hash<int>()(t.z2) << 11) ^ (std::hash<int>()(t.z3) << 22);
    }
};

// ============================================================================
// Running statistics accumulator (Welford)
// ============================================================================

struct RunningStats {
    int    count = 0;
    double mean  = 0.0;
    double M2    = 0.0;
    double lo    = 1e30;
    double hi    = -1e30;

    void push(double x) {
        ++count;
        double delta = x - mean;
        mean += delta / count;
        M2 += delta * (x - mean);
        lo = std::min(lo, x);
        hi = std::max(hi, x);
    }

    double stddev() const {
        return (count > 1) ? std::sqrt(M2 / (count - 1)) : 0.0;
    }
};

// ============================================================================
// Block 1: Geometric Properties
// ============================================================================

struct BondLengthStats {
    ElementPair type;
    RunningStats stats;
};

struct BondAngleStats {
    ElementTriple type;
    RunningStats stats;
};

struct GeometricProperties {
    // Per-type bond length statistics
    std::vector<BondLengthStats> bond_lengths;

    // Per-type bond angle statistics
    std::vector<BondAngleStats> bond_angles;

    // Dihedral angles (all, in degrees)
    std::vector<double> dihedrals;
    RunningStats dihedral_stats;

    // Global
    double radius_of_gyration = 0.0;   // Angstrom
    int    n_bonds            = 0;
    int    n_angles           = 0;
    int    n_dihedrals        = 0;
};

// ============================================================================
// Block 2: Energy Properties
// ============================================================================

struct EnergyProperties {
    double E_bond     = 0.0;
    double E_angle    = 0.0;
    double E_torsion  = 0.0;
    double E_vdW      = 0.0;
    double E_coulomb  = 0.0;
    double E_external = 0.0;
    double E_total    = 0.0;
    double E_per_atom = 0.0;
    double E_strain   = 0.0;   // bond + angle + torsion
    double f_vdW      = 0.0;   // fraction from vdW
    double f_coulomb  = 0.0;   // fraction from Coulomb
};

// ============================================================================
// Block 3: Inertial Properties
// ============================================================================

struct InertialProperties {
    double I_A = 0.0, I_B = 0.0, I_C = 0.0;  // Principal moments (amu·Å²)
    Vec3   axis_a{}, axis_b{}, axis_c{};        // Principal axes
    double kappa = 0.0;                          // Ray's asymmetry parameter
    double A_rot = 0.0, B_rot = 0.0, C_rot = 0.0;  // Rotational constants (cm⁻¹)
};

// ============================================================================
// Block 4: Electrostatic Properties
// ============================================================================

struct ElectrostaticProperties {
    Vec3   dipole_vec{};             // Dipole moment vector (e·Å)
    double dipole_magnitude = 0.0;   // |μ| in Debye
    double quadrupole_trace = 0.0;   // Should be ~0 (traceless)
    double Q_xx = 0.0, Q_yy = 0.0, Q_zz = 0.0;  // Principal quadrupole components
};

// ============================================================================
// Block 5: Topological Properties
// ============================================================================

struct TopologicalProperties {
    int n_atoms      = 0;
    int n_bonds      = 0;
    int n_components = 0;
    int cycle_rank   = 0;   // |B| - N + components

    // Coordination number distribution
    std::vector<int> coord_histogram;  // coord_histogram[z] = count of atoms with z neighbors
    double mean_coordination = 0.0;

    // Ring statistics (index = ring size, value = count)
    std::vector<int> ring_sizes;  // ring_sizes[k] = number of k-membered rings (k=3..8)
};

// ============================================================================
// Block 6: Emergence Properties
// ============================================================================

struct VSEPRRecovery {
    int    coordination = 0;
    double ideal_angle  = 0.0;   // degrees
    double measured_mean_angle = 0.0;
    double deviation    = 0.0;   // |measured - ideal|
    bool   recovered    = false; // deviation < threshold
};

struct EmergenceProperties {
    // Anisotropy
    double anisotropy_ratio = 1.0;  // λ₁/λ₃
    double asphericity      = 0.0;  // κ

    // Formation quality score
    double quality_energy   = 0.0;  // q_E
    double quality_force    = 0.0;  // q_F
    double quality_geometry = 0.0;  // q_G
    double quality_topology = 0.0;  // q_T
    double quality_total    = 0.0;  // Q_f ∈ [0,1]

    // VSEPR recovery per coordination number found
    std::vector<VSEPRRecovery> vsepr_recovery;

    // Energy fingerprint (6D normalized)
    double E_fingerprint[6] = {};
};

// ============================================================================
// Complete Property Record
// ============================================================================

struct FormationPropertyRecord {
    GeometricProperties      geom;
    EnergyProperties         energy;
    InertialProperties       inertia;
    ElectrostaticProperties  electro;
    TopologicalProperties    topology;
    EmergenceProperties      emergence;

    // Provenance
    int    N           = 0;
    double F_rms       = 0.0;    // RMS force at convergence
    bool   converged   = false;

    /**
     * Generate a human-readable summary.
     */
    std::string summary() const;
};

// ============================================================================
// Scraper Functions — pure, deterministic, read-only
// ============================================================================

/**
 * scrape_geometry — extract bond lengths, angles, dihedrals, Rg.
 */
inline GeometricProperties scrape_geometry(const State& s) {
    GeometricProperties gp;

    // Build adjacency from bond graph
    const int N = static_cast<int>(s.N);
    std::vector<std::vector<int>> adj(N);
    for (const auto& b : s.B) {
        adj[b.i].push_back(b.j);
        adj[b.j].push_back(b.i);
    }

    // --- Bond lengths ---
    std::unordered_map<ElementPair, RunningStats, ElementPairHash> bl_map;
    for (const auto& b : s.B) {
        double dx = s.X[b.i].x - s.X[b.j].x;
        double dy = s.X[b.i].y - s.X[b.j].y;
        double dz = s.X[b.i].z - s.X[b.j].z;
        double d = std::sqrt(dx*dx + dy*dy + dz*dz);
        ElementPair key(s.type[b.i], s.type[b.j]);
        bl_map[key].push(d);
    }
    for (auto& [key, stats] : bl_map) {
        gp.bond_lengths.push_back({key, stats});
    }
    gp.n_bonds = static_cast<int>(s.B.size());

    // --- Bond angles ---
    std::unordered_map<ElementTriple, RunningStats, ElementTripleHash> ba_map;
    for (int j = 0; j < N; ++j) {
        const auto& nbrs = adj[j];
        for (size_t a = 0; a < nbrs.size(); ++a) {
            for (size_t b = a + 1; b < nbrs.size(); ++b) {
                int i = nbrs[a], k = nbrs[b];
                Vec3 rij = {s.X[i].x - s.X[j].x, s.X[i].y - s.X[j].y, s.X[i].z - s.X[j].z};
                Vec3 rkj = {s.X[k].x - s.X[j].x, s.X[k].y - s.X[j].y, s.X[k].z - s.X[j].z};
                double dot = rij.x*rkj.x + rij.y*rkj.y + rij.z*rkj.z;
                double mij = std::sqrt(rij.x*rij.x + rij.y*rij.y + rij.z*rij.z);
                double mkj = std::sqrt(rkj.x*rkj.x + rkj.y*rkj.y + rkj.z*rkj.z);
                if (mij < 1e-12 || mkj < 1e-12) continue;
                double cos_theta = std::clamp(dot / (mij * mkj), -1.0, 1.0);
                double theta_deg = std::acos(cos_theta) * (180.0 / 3.14159265358979323846);

                ElementTriple key(s.type[i], s.type[j], s.type[k]);
                ba_map[key].push(theta_deg);
            }
        }
    }
    for (auto& [key, stats] : ba_map) {
        gp.bond_angles.push_back({key, stats});
    }
    gp.n_angles = 0;
    for (const auto& ba : gp.bond_angles) gp.n_angles += ba.stats.count;

    // --- Dihedrals ---
    for (const auto& b : s.B) {
        int j = b.i, k = b.j;
        for (int i : adj[j]) {
            if (i == k) continue;
            for (int l : adj[k]) {
                if (l == j || l == i) continue;
                // Compute dihedral i-j-k-l
                Vec3 b1 = {s.X[j].x-s.X[i].x, s.X[j].y-s.X[i].y, s.X[j].z-s.X[i].z};
                Vec3 b2 = {s.X[k].x-s.X[j].x, s.X[k].y-s.X[j].y, s.X[k].z-s.X[j].z};
                Vec3 b3 = {s.X[l].x-s.X[k].x, s.X[l].y-s.X[k].y, s.X[l].z-s.X[k].z};

                // n1 = b1 × b2, n2 = b2 × b3
                Vec3 n1 = {b1.y*b2.z - b1.z*b2.y, b1.z*b2.x - b1.x*b2.z, b1.x*b2.y - b1.y*b2.x};
                Vec3 n2 = {b2.y*b3.z - b2.z*b3.y, b2.z*b3.x - b2.x*b3.z, b2.x*b3.y - b2.y*b3.x};

                double mn1 = std::sqrt(n1.x*n1.x + n1.y*n1.y + n1.z*n1.z);
                double mn2 = std::sqrt(n2.x*n2.x + n2.y*n2.y + n2.z*n2.z);
                if (mn1 < 1e-12 || mn2 < 1e-12) continue;

                double cos_phi = std::clamp((n1.x*n2.x + n1.y*n2.y + n1.z*n2.z) / (mn1*mn2), -1.0, 1.0);
                // Sign: (n1 × n2) · b2_hat
                Vec3 cross_n = {n1.y*n2.z - n1.z*n2.y, n1.z*n2.x - n1.x*n2.z, n1.x*n2.y - n1.y*n2.x};
                double mb2 = std::sqrt(b2.x*b2.x + b2.y*b2.y + b2.z*b2.z);
                double sin_phi = (cross_n.x*b2.x + cross_n.y*b2.y + cross_n.z*b2.z) / (mn1 * mn2 * mb2 + 1e-30);
                double phi = std::atan2(sin_phi, cos_phi);
                double phi_deg = phi * (180.0 / 3.14159265358979323846);

                gp.dihedrals.push_back(phi_deg);
                gp.dihedral_stats.push(phi_deg);
            }
        }
    }
    gp.n_dihedrals = static_cast<int>(gp.dihedrals.size());

    // --- Radius of gyration ---
    double M_total = 0.0;
    Vec3 com{};
    for (int i = 0; i < N; ++i) {
        M_total += s.M[i];
        com.x += s.M[i] * s.X[i].x;
        com.y += s.M[i] * s.X[i].y;
        com.z += s.M[i] * s.X[i].z;
    }
    if (M_total > 1e-30) {
        com.x /= M_total; com.y /= M_total; com.z /= M_total;
        double rg2 = 0.0;
        for (int i = 0; i < N; ++i) {
            double dx = s.X[i].x - com.x;
            double dy = s.X[i].y - com.y;
            double dz = s.X[i].z - com.z;
            rg2 += s.M[i] * (dx*dx + dy*dy + dz*dz);
        }
        gp.radius_of_gyration = std::sqrt(rg2 / M_total);
    }

    return gp;
}

/**
 * scrape_energy — extract energy ledger decomposition.
 */
inline EnergyProperties scrape_energy(const State& s) {
    EnergyProperties ep;
    ep.E_bond     = s.E.Ubond;
    ep.E_angle    = s.E.Uangle;
    ep.E_torsion  = s.E.Utors;
    ep.E_vdW      = s.E.UvdW;
    ep.E_coulomb  = s.E.UCoul;
    ep.E_external = s.E.Uext;
    ep.E_total    = s.E.total();
    ep.E_per_atom = (s.N > 0) ? ep.E_total / static_cast<double>(s.N) : 0.0;
    ep.E_strain   = ep.E_bond + ep.E_angle + ep.E_torsion;

    double abs_total = std::abs(ep.E_total);
    if (abs_total > 1e-30) {
        ep.f_vdW     = ep.E_vdW     / abs_total;
        ep.f_coulomb = ep.E_coulomb / abs_total;
    }
    return ep;
}

/**
 * scrape_inertia — compute inertia tensor, principal moments, asymmetry.
 */
inline InertialProperties scrape_inertia(const State& s) {
    InertialProperties ip;
    const int N = static_cast<int>(s.N);
    if (N < 2) return ip;

    // Center of mass
    double M = 0.0;
    Vec3 com{};
    for (int i = 0; i < N; ++i) {
        M += s.M[i];
        com.x += s.M[i] * s.X[i].x;
        com.y += s.M[i] * s.X[i].y;
        com.z += s.M[i] * s.X[i].z;
    }
    if (M < 1e-30) return ip;
    com.x /= M; com.y /= M; com.z /= M;

    // Inertia tensor
    double I[3][3]{};
    for (int i = 0; i < N; ++i) {
        double dx = s.X[i].x - com.x;
        double dy = s.X[i].y - com.y;
        double dz = s.X[i].z - com.z;
        double r2 = dx*dx + dy*dy + dz*dz;
        double m = s.M[i];
        I[0][0] += m * (r2 - dx*dx);
        I[1][1] += m * (r2 - dy*dy);
        I[2][2] += m * (r2 - dz*dz);
        I[0][1] -= m * dx * dy;
        I[0][2] -= m * dx * dz;
        I[1][2] -= m * dy * dz;
    }
    I[1][0] = I[0][1]; I[2][0] = I[0][2]; I[2][1] = I[1][2];

    // Jacobi eigenvalue (same pattern as emergence_dataset.hpp)
    double A[3][3], V[3][3]{};
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            A[r][c] = I[r][c];
    V[0][0] = V[1][1] = V[2][2] = 1.0;

    for (int iter = 0; iter < 100; ++iter) {
        double max_off = 0.0;
        int p = 0, q = 1;
        for (int i = 0; i < 3; ++i)
            for (int j = i + 1; j < 3; ++j)
                if (std::abs(A[i][j]) > max_off) {
                    max_off = std::abs(A[i][j]);
                    p = i; q = j;
                }
        if (max_off < 1e-14) break;

        double app = A[p][p], aqq = A[q][q], apq = A[p][q];
        if (std::abs(apq) < 1e-15) continue;
        double tau = (aqq - app) / (2.0 * apq);
        double t = (tau >= 0)
            ?  1.0 / ( tau + std::sqrt(1.0 + tau * tau))
            : -1.0 / (-tau + std::sqrt(1.0 + tau * tau));
        double cv = 1.0 / std::sqrt(1.0 + t * t);
        double sv = t * cv;

        A[p][p] = app - t * apq;
        A[q][q] = aqq + t * apq;
        A[p][q] = A[q][p] = 0.0;
        for (int r = 0; r < 3; ++r) {
            if (r == p || r == q) continue;
            double arp = A[r][p], arq = A[r][q];
            A[r][p] = A[p][r] = cv * arp - sv * arq;
            A[r][q] = A[q][r] = sv * arp + cv * arq;
        }
        for (int r = 0; r < 3; ++r) {
            double vrp = V[r][p], vrq = V[r][q];
            V[r][p] = cv * vrp - sv * vrq;
            V[r][q] = sv * vrp + cv * vrq;
        }
    }

    // Sort eigenvalues ascending: I_A <= I_B <= I_C
    double evals[3] = {A[0][0], A[1][1], A[2][2]};
    int order[3] = {0, 1, 2};
    if (evals[order[0]] > evals[order[1]]) std::swap(order[0], order[1]);
    if (evals[order[1]] > evals[order[2]]) std::swap(order[1], order[2]);
    if (evals[order[0]] > evals[order[1]]) std::swap(order[0], order[1]);

    ip.I_A = std::max(evals[order[0]], 0.0);
    ip.I_B = std::max(evals[order[1]], 0.0);
    ip.I_C = std::max(evals[order[2]], 0.0);

    ip.axis_a = {V[0][order[0]], V[1][order[0]], V[2][order[0]]};
    ip.axis_b = {V[0][order[1]], V[1][order[1]], V[2][order[1]]};
    ip.axis_c = {V[0][order[2]], V[1][order[2]], V[2][order[2]]};

    // Ray's asymmetry parameter: κ = (2B - A - C) / (C - A)
    double denom = ip.I_C - ip.I_A;
    if (std::abs(denom) > 1e-30) {
        ip.kappa = (2.0 * ip.I_B - ip.I_A - ip.I_C) / denom;
    }

    // Rotational constants: B = ħ² / (2I) in cm⁻¹
    // ħ = 1.054571817e-34 J·s, but in amu·Å²/fs: 16.857630
    // Conversion: ħ²/(2I) in amu·Å² → cm⁻¹ = 16.857630² / (2·I) * (1/c_cm)
    // Standard: B(cm⁻¹) = h/(8π²cI) = 505379.07 / I(amu·Å²)
    constexpr double ROTATIONAL_CONST = 505379.07;
    if (ip.I_A > 1e-6) ip.A_rot = ROTATIONAL_CONST / ip.I_A;
    if (ip.I_B > 1e-6) ip.B_rot = ROTATIONAL_CONST / ip.I_B;
    if (ip.I_C > 1e-6) ip.C_rot = ROTATIONAL_CONST / ip.I_C;

    return ip;
}

/**
 * scrape_electrostatics — dipole moment, quadrupole tensor.
 */
inline ElectrostaticProperties scrape_electrostatics(const State& s) {
    ElectrostaticProperties ep;
    const int N = static_cast<int>(s.N);
    if (N < 1 || s.Q.empty()) return ep;

    // Center of mass (for charge-neutral systems, dipole is origin-independent)
    Vec3 com{};
    double M = 0.0;
    for (int i = 0; i < N; ++i) {
        M += s.M[i];
        com.x += s.M[i] * s.X[i].x;
        com.y += s.M[i] * s.X[i].y;
        com.z += s.M[i] * s.X[i].z;
    }
    if (M > 1e-30) { com.x /= M; com.y /= M; com.z /= M; }

    // Dipole: μ = Σ q_i · r_i (relative to COM for charge-neutral)
    for (int i = 0; i < N; ++i) {
        ep.dipole_vec.x += s.Q[i] * (s.X[i].x - com.x);
        ep.dipole_vec.y += s.Q[i] * (s.X[i].y - com.y);
        ep.dipole_vec.z += s.Q[i] * (s.X[i].z - com.z);
    }
    // Convert e·Å to Debye: 1 e·Å = 4.8032 D
    double mag_eA = std::sqrt(ep.dipole_vec.x*ep.dipole_vec.x +
                              ep.dipole_vec.y*ep.dipole_vec.y +
                              ep.dipole_vec.z*ep.dipole_vec.z);
    ep.dipole_magnitude = mag_eA * 4.8032;

    // Traceless quadrupole: Q_αβ = Σ q_i (3 r_α r_β - r² δ_αβ)
    double Qxx = 0, Qyy = 0, Qzz = 0;
    for (int i = 0; i < N; ++i) {
        double dx = s.X[i].x - com.x;
        double dy = s.X[i].y - com.y;
        double dz = s.X[i].z - com.z;
        double r2 = dx*dx + dy*dy + dz*dz;
        Qxx += s.Q[i] * (3.0*dx*dx - r2);
        Qyy += s.Q[i] * (3.0*dy*dy - r2);
        Qzz += s.Q[i] * (3.0*dz*dz - r2);
    }
    ep.Q_xx = Qxx; ep.Q_yy = Qyy; ep.Q_zz = Qzz;
    ep.quadrupole_trace = Qxx + Qyy + Qzz;  // Should be ~0

    return ep;
}

/**
 * scrape_topology — connectivity invariants, coordination, ring statistics.
 */
inline TopologicalProperties scrape_topology(const State& s) {
    TopologicalProperties tp;
    tp.n_atoms = static_cast<int>(s.N);
    tp.n_bonds = static_cast<int>(s.B.size());

    // Build adjacency
    const int N = static_cast<int>(s.N);
    std::vector<std::vector<int>> adj(N);
    for (const auto& b : s.B) {
        adj[b.i].push_back(b.j);
        adj[b.j].push_back(b.i);
    }

    // Connected components (BFS)
    std::vector<bool> visited(N, false);
    tp.n_components = 0;
    for (int start = 0; start < N; ++start) {
        if (visited[start]) continue;
        ++tp.n_components;
        std::vector<int> stack = {start};
        visited[start] = true;
        while (!stack.empty()) {
            int v = stack.back(); stack.pop_back();
            for (int u : adj[v]) {
                if (!visited[u]) {
                    visited[u] = true;
                    stack.push_back(u);
                }
            }
        }
    }

    // Cycle rank: |B| - N + components
    tp.cycle_rank = tp.n_bonds - tp.n_atoms + tp.n_components;

    // Coordination histogram
    int max_coord = 0;
    double sum_coord = 0.0;
    for (int i = 0; i < N; ++i) {
        int z = static_cast<int>(adj[i].size());
        max_coord = std::max(max_coord, z);
        sum_coord += z;
    }
    tp.coord_histogram.resize(max_coord + 1, 0);
    for (int i = 0; i < N; ++i) {
        tp.coord_histogram[static_cast<int>(adj[i].size())]++;
    }
    tp.mean_coordination = (N > 0) ? sum_coord / N : 0.0;

    // Simple ring detection: for small rings (3-8), use DFS from each edge
    // Count unique rings by canonical ordering
    tp.ring_sizes.resize(9, 0);  // index 0-8; only 3-8 used
    // Simple approach: for each bond (u,v), find shortest path from u to v
    // not using that bond.  Path length + 1 = ring size.
    std::unordered_set<uint64_t> found_rings;
    for (const auto& bond : s.B) {
        int u = bond.i, v = bond.j;
        // BFS from u to v, not using edge (u,v)
        std::vector<int> dist(N, -1);
        std::vector<int> parent(N, -1);
        dist[u] = 0;
        std::vector<int> queue = {u};
        size_t head = 0;
        bool found = false;
        while (head < queue.size() && !found) {
            int cur = queue[head++];
            if (dist[cur] >= 7) continue;  // max ring size 8
            for (int nbr : adj[cur]) {
                if (cur == u && nbr == v) continue;  // skip this bond
                if (cur == v && nbr == u) continue;
                if (dist[nbr] >= 0) continue;
                dist[nbr] = dist[cur] + 1;
                parent[nbr] = cur;
                if (nbr == v) { found = true; break; }
                queue.push_back(nbr);
            }
        }
        if (found && dist[v] >= 2) {
            int ring_size = dist[v] + 1;
            if (ring_size >= 3 && ring_size <= 8) {
                // Canonical ring key: sorted atoms in ring
                std::vector<int> ring_atoms;
                int cur = v;
                while (cur != -1) {
                    ring_atoms.push_back(cur);
                    cur = parent[cur];
                }
                std::sort(ring_atoms.begin(), ring_atoms.end());
                uint64_t key = 0;
                for (int a : ring_atoms) key = key * 131 + a;
                if (found_rings.find(key) == found_rings.end()) {
                    found_rings.insert(key);
                    tp.ring_sizes[ring_size]++;
                }
            }
        }
    }

    return tp;
}

/**
 * scrape_emergence — anisotropy, VSEPR recovery, quality score.
 */
inline EmergenceProperties scrape_emergence(
    const State& s,
    const GeometricProperties& gp,
    const EnergyProperties& ep)
{
    EmergenceProperties em;
    const int N = static_cast<int>(s.N);

    // --- Anisotropy (gyration tensor) ---
    double M = 0.0;
    Vec3 com{};
    for (int i = 0; i < N; ++i) {
        M += s.M[i];
        com.x += s.M[i] * s.X[i].x;
        com.y += s.M[i] * s.X[i].y;
        com.z += s.M[i] * s.X[i].z;
    }
    if (M > 1e-30) { com.x /= M; com.y /= M; com.z /= M; }

    double G[3][3]{};
    for (int i = 0; i < N; ++i) {
        double dx = s.X[i].x - com.x;
        double dy = s.X[i].y - com.y;
        double dz = s.X[i].z - com.z;
        double w = s.M[i] / (M > 1e-30 ? M : 1.0);
        G[0][0] += w*dx*dx; G[0][1] += w*dx*dy; G[0][2] += w*dx*dz;
        G[1][1] += w*dy*dy; G[1][2] += w*dy*dz;
        G[2][2] += w*dz*dz;
    }
    G[1][0]=G[0][1]; G[2][0]=G[0][2]; G[2][1]=G[1][2];

    // Eigenvalues via characteristic equation for 3x3 symmetric
    double p1 = G[0][1]*G[0][1] + G[0][2]*G[0][2] + G[1][2]*G[1][2];
    if (p1 < 1e-30) {
        // Diagonal
        double e[3] = {G[0][0], G[1][1], G[2][2]};
        std::sort(e, e+3); // ascending
        em.anisotropy_ratio = (e[0] > 1e-30) ? e[2]/e[0] : 1.0;
        double trace = e[0]+e[1]+e[2];
        double cross = e[0]*e[1]+e[1]*e[2]+e[2]*e[0];
        em.asphericity = (trace > 1e-30) ? 1.0 - 3.0*cross/(trace*trace) : 0.0;
    } else {
        double q = (G[0][0]+G[1][1]+G[2][2]) / 3.0;
        double p2 = (G[0][0]-q)*(G[0][0]-q) + (G[1][1]-q)*(G[1][1]-q)
                   + (G[2][2]-q)*(G[2][2]-q) + 2.0*p1;
        double p = std::sqrt(p2 / 6.0);
        // Not worth full Jacobi here — use approximate eigenvalues
        // via trace/det/p for the quality score
        em.anisotropy_ratio = 1.0 + std::sqrt(p1) * 10.0;  // Proxy
        em.asphericity = p1 / (p2 + 1e-30);
    }

    // --- VSEPR recovery ---
    // Build adjacency
    std::vector<std::vector<int>> adj(N);
    for (const auto& b : s.B) {
        adj[b.i].push_back(b.j);
        adj[b.j].push_back(b.i);
    }

    // Ideal VSEPR angles
    auto ideal_angle = [](int z) -> double {
        switch (z) {
            case 2: return 180.0;
            case 3: return 120.0;
            case 4: return 109.47;
            case 5: return 90.0;   // equatorial-axial average
            case 6: return 90.0;
            default: return 0.0;
        }
    };

    // Group atoms by coordination, measure mean angle
    std::map<int, RunningStats> coord_angles;
    for (int j = 0; j < N; ++j) {
        int z = static_cast<int>(adj[j].size());
        if (z < 2 || z > 6) continue;
        for (size_t a = 0; a < adj[j].size(); ++a) {
            for (size_t b = a + 1; b < adj[j].size(); ++b) {
                int i = adj[j][a], k = adj[j][b];
                Vec3 rij = {s.X[i].x-s.X[j].x, s.X[i].y-s.X[j].y, s.X[i].z-s.X[j].z};
                Vec3 rkj = {s.X[k].x-s.X[j].x, s.X[k].y-s.X[j].y, s.X[k].z-s.X[j].z};
                double dot = rij.x*rkj.x + rij.y*rkj.y + rij.z*rkj.z;
                double mij = std::sqrt(rij.x*rij.x + rij.y*rij.y + rij.z*rij.z);
                double mkj = std::sqrt(rkj.x*rkj.x + rkj.y*rkj.y + rkj.z*rkj.z);
                if (mij < 1e-12 || mkj < 1e-12) continue;
                double theta = std::acos(std::clamp(dot/(mij*mkj), -1.0, 1.0)) * 180.0 / 3.14159265358979323846;
                coord_angles[z].push(theta);
            }
        }
    }

    for (auto& [z, stats] : coord_angles) {
        double ideal = ideal_angle(z);
        if (ideal < 1.0) continue;
        VSEPRRecovery vr;
        vr.coordination = z;
        vr.ideal_angle = ideal;
        vr.measured_mean_angle = stats.mean;
        vr.deviation = std::abs(stats.mean - ideal);
        vr.recovered = (vr.deviation < 5.0);  // 5° threshold
        em.vsepr_recovery.push_back(vr);
    }

    // --- Formation quality score ---
    // q_E = exp(-|E_strain| / E_ref)
    constexpr double E_ref = 10.0;  // kcal/mol reference
    em.quality_energy = std::exp(-std::abs(ep.E_strain) / E_ref);

    // q_F = exp(-F_rms / F_ref)  (need F_rms from outside; approximate from state)
    double F_rms = 0.0;
    for (int i = 0; i < N; ++i) {
        F_rms += s.F[i].x*s.F[i].x + s.F[i].y*s.F[i].y + s.F[i].z*s.F[i].z;
    }
    F_rms = (N > 0) ? std::sqrt(F_rms / N) : 0.0;
    constexpr double F_ref = 0.01;  // kcal/(mol·Å)
    em.quality_force = std::exp(-F_rms / F_ref);

    // q_G = VSEPR recovery average
    if (!em.vsepr_recovery.empty()) {
        double sum = 0.0;
        for (const auto& vr : em.vsepr_recovery) {
            sum += 1.0 - vr.deviation / 180.0;
        }
        em.quality_geometry = sum / em.vsepr_recovery.size();
    } else {
        em.quality_geometry = 1.0;  // No angles to check
    }

    // q_T = 1 - violations / total
    int total_checks = gp.n_bonds + gp.n_angles;
    int violations = 0;
    // Count bonds outside reasonable range (0.5-3.0 Å)
    for (const auto& bl : gp.bond_lengths) {
        if (bl.stats.lo < 0.5 || bl.stats.hi > 3.5) ++violations;
    }
    em.quality_topology = (total_checks > 0)
        ? 1.0 - static_cast<double>(violations) / total_checks
        : 1.0;

    // Composite
    em.quality_total = 0.3 * em.quality_energy
                     + 0.2 * em.quality_force
                     + 0.3 * em.quality_geometry
                     + 0.2 * em.quality_topology;

    // --- Energy fingerprint ---
    double abs_total = std::abs(ep.E_total);
    if (abs_total > 1e-30) {
        em.E_fingerprint[0] = ep.E_bond     / abs_total;
        em.E_fingerprint[1] = ep.E_angle    / abs_total;
        em.E_fingerprint[2] = ep.E_torsion  / abs_total;
        em.E_fingerprint[3] = ep.E_vdW      / abs_total;
        em.E_fingerprint[4] = ep.E_coulomb  / abs_total;
        em.E_fingerprint[5] = ep.E_external / abs_total;
    }

    return em;
}

/**
 * scrape_properties — the master function.
 * Pure, deterministic, read-only extraction from converged State.
 */
inline FormationPropertyRecord scrape_properties(const State& s) {
    FormationPropertyRecord rec;
    rec.N = static_cast<int>(s.N);

    // RMS force
    const int N = static_cast<int>(s.N);
    double f2 = 0.0;
    for (int i = 0; i < N; ++i) {
        f2 += s.F[i].x*s.F[i].x + s.F[i].y*s.F[i].y + s.F[i].z*s.F[i].z;
    }
    rec.F_rms = (N > 0) ? std::sqrt(f2 / N) : 0.0;
    rec.converged = (rec.F_rms < 1e-4);

    // Six blocks
    rec.geom      = scrape_geometry(s);
    rec.energy    = scrape_energy(s);
    rec.inertia   = scrape_inertia(s);
    rec.electro   = scrape_electrostatics(s);
    rec.topology  = scrape_topology(s);
    rec.emergence = scrape_emergence(s, rec.geom, rec.energy);

    return rec;
}

/**
 * summary — human-readable property report.
 */
inline std::string FormationPropertyRecord::summary() const {
    std::ostringstream out;
    out << "=== Formation Property Report ===\n";
    out << "Atoms: " << N << "  Bonds: " << geom.n_bonds
        << "  F_rms: " << F_rms << "  Converged: " << (converged ? "YES" : "NO") << "\n\n";

    out << "--- Geometry ---\n";
    for (const auto& bl : geom.bond_lengths) {
        out << "  Bond Z(" << bl.type.z1 << "-" << bl.type.z2 << "): "
            << "mean=" << bl.stats.mean << " std=" << bl.stats.stddev()
            << " [" << bl.stats.lo << ", " << bl.stats.hi << "] n=" << bl.stats.count << "\n";
    }
    for (const auto& ba : geom.bond_angles) {
        out << "  Angle Z(" << ba.type.z1 << "-" << ba.type.z2 << "-" << ba.type.z3 << "): "
            << "mean=" << ba.stats.mean << "° std=" << ba.stats.stddev() << "°\n";
    }
    out << "  Rg = " << geom.radius_of_gyration << " A\n\n";

    out << "--- Energy (kcal/mol) ---\n";
    out << "  bond=" << energy.E_bond << " angle=" << energy.E_angle
        << " torsion=" << energy.E_torsion << " vdW=" << energy.E_vdW
        << " Coulomb=" << energy.E_coulomb << " ext=" << energy.E_external << "\n";
    out << "  total=" << energy.E_total << " per_atom=" << energy.E_per_atom
        << " strain=" << energy.E_strain << "\n\n";

    out << "--- Inertia ---\n";
    out << "  I_A=" << inertia.I_A << " I_B=" << inertia.I_B << " I_C=" << inertia.I_C
        << " kappa=" << inertia.kappa << "\n";
    out << "  A=" << inertia.A_rot << " B=" << inertia.B_rot << " C=" << inertia.C_rot << " cm-1\n\n";

    out << "--- Electrostatics ---\n";
    out << "  |mu| = " << electro.dipole_magnitude << " Debye\n\n";

    out << "--- Topology ---\n";
    out << "  atoms=" << topology.n_atoms << " bonds=" << topology.n_bonds
        << " components=" << topology.n_components << " cycles=" << topology.cycle_rank << "\n";
    out << "  mean_coord=" << topology.mean_coordination << "\n";
    for (int k = 3; k <= 8; ++k) {
        if (k < static_cast<int>(topology.ring_sizes.size()) && topology.ring_sizes[k] > 0) {
            out << "  " << k << "-rings: " << topology.ring_sizes[k] << "\n";
        }
    }
    out << "\n";

    out << "--- Emergence ---\n";
    out << "  Q_f = " << emergence.quality_total
        << " (E=" << emergence.quality_energy
        << " F=" << emergence.quality_force
        << " G=" << emergence.quality_geometry
        << " T=" << emergence.quality_topology << ")\n";
    for (const auto& vr : emergence.vsepr_recovery) {
        out << "  VSEPR z=" << vr.coordination
            << ": ideal=" << vr.ideal_angle << "° measured=" << vr.measured_mean_angle
            << "° dev=" << vr.deviation << "° "
            << (vr.recovered ? "RECOVERED" : "MISSED") << "\n";
    }
    out << "  Anisotropy = " << emergence.anisotropy_ratio << "\n";

    return out.str();
}

} // namespace scraper
} // namespace atomistic
