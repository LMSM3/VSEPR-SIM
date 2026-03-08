/**
 * test_experiment_match.cpp
 * -------------------------
 * Validates the experimental data-matching module for the near-atomistic
 * nanoparticle regime (4–6 nm).
 *
 * Tests:
 *   T1  Site classification (bulk / surface / edge / vertex)
 *   T2  Geometric energy decomposition (energy partitioning)
 *   T3  Structural fingerprint (CN histogram, RDF peaks)
 *   T4  Dynamics observables (MSD, diffusion coefficient, Lindemann)
 *   T5  Scoring against experimental reference (Au 5nm)
 *   T6  Reference data sanity (all built-in records)
 */

#include "atomistic/validation/experiment_match.hpp"
#include "atomistic/validation/nanoparticle_ref.hpp"
#include "atomistic/models/model.hpp"
#include <iostream>
#include <cmath>
#include <cassert>
#include <vector>
#include <cstdlib>

using namespace atomistic;
using namespace atomistic::validation;

static int g_pass = 0, g_fail = 0;

static void check(bool cond, const char* name) {
    if (cond) {
        std::cout << "  [PASS] " << name << "\n";
        ++g_pass;
    } else {
        std::cout << "  [FAIL] " << name << "\n";
        ++g_fail;
    }
}

// Build a simple FCC cluster (truncated sphere) for testing
static State build_fcc_cluster(uint32_t Z, double a0, double radius) {
    State s{};

    // Generate FCC lattice points within sphere
    int n_cells = static_cast<int>(std::ceil(radius / a0)) + 1;
    std::vector<Vec3> positions;

    // FCC basis: (0,0,0), (0.5,0.5,0), (0.5,0,0.5), (0,0.5,0.5)
    double basis[4][3] = {
        {0.0, 0.0, 0.0},
        {0.5, 0.5, 0.0},
        {0.5, 0.0, 0.5},
        {0.0, 0.5, 0.5}
    };

    for (int ix = -n_cells; ix <= n_cells; ++ix) {
        for (int iy = -n_cells; iy <= n_cells; ++iy) {
            for (int iz = -n_cells; iz <= n_cells; ++iz) {
                for (int b = 0; b < 4; ++b) {
                    double x = (ix + basis[b][0]) * a0;
                    double y = (iy + basis[b][1]) * a0;
                    double z = (iz + basis[b][2]) * a0;
                    double r2 = x*x + y*y + z*z;
                    if (r2 <= radius * radius) {
                        positions.push_back({x, y, z});
                    }
                }
            }
        }
    }

    s.N = static_cast<uint32_t>(positions.size());
    s.X = positions;
    s.V.resize(s.N, {0, 0, 0});
    s.Q.resize(s.N, 0.0);
    s.M.resize(s.N, 196.97);  // Au mass (amu)
    s.type.resize(s.N, Z);
    s.F.resize(s.N, {0, 0, 0});

    // Fake per-atom forces for energy decomposition testing
    // Bulk atoms deeper in potential well, surface atoms shallower
    double E_bulk_atom = -3.5;  // kcal/mol (arbitrary test value)
    for (uint32_t i = 0; i < s.N; ++i) {
        double r = std::sqrt(s.X[i].x * s.X[i].x +
                             s.X[i].y * s.X[i].y +
                             s.X[i].z * s.X[i].z);
        double frac = r / radius;
        // Atoms near surface have less binding
        double e_i = E_bulk_atom * (1.0 - 0.4 * frac * frac);
        // Store as radial force for virial estimation
        if (r > 0.1) {
            double f_mag = e_i / r;
            s.F[i] = {f_mag * s.X[i].x / r,
                       f_mag * s.X[i].y / r,
                       f_mag * s.X[i].z / r};
        }
    }

    // Set total energy
    s.E.UvdW = E_bulk_atom * s.N * 0.85;  // slightly less due to surface
    return s;
}

// ============================================================================
// T1: Site classification
// ============================================================================
static void test_site_classification() {
    std::cout << "\n=== T1: Site Classification ===\n";

    // Small FCC cluster (radius ~10 Å ≈ 1nm, for fast testing)
    State s = build_fcc_cluster(79, 4.078, 10.0);
    check(s.N > 10, "cluster has atoms");

    double cutoff = 4.078 * 0.75;  // ~3.06 Å (NN distance in FCC Au)
    auto classes = classify_sites(s, cutoff, 12);

    uint32_t n_bulk = 0, n_surf = 0, n_edge = 0, n_vert = 0;
    for (auto c : classes) {
        switch (c) {
            case SiteClass::Bulk:    ++n_bulk; break;
            case SiteClass::Surface: ++n_surf; break;
            case SiteClass::Edge:    ++n_edge; break;
            case SiteClass::Vertex:  ++n_vert; break;
        }
    }

    std::cout << "    N=" << s.N << "  bulk=" << n_bulk
              << "  surface=" << n_surf << "  edge=" << n_edge
              << "  vertex=" << n_vert << "\n";

    // For a small cluster, most atoms should be surface/edge/vertex
    check(n_surf + n_edge + n_vert > n_bulk,
          "small cluster: more surface than bulk atoms");
    check(n_bulk + n_surf + n_edge + n_vert == s.N,
          "all atoms classified");
}

// ============================================================================
// T2: Geometric energy decomposition
// ============================================================================
static void test_geometric_energy() {
    std::cout << "\n=== T2: Geometric Energy Decomposition ===\n";

    State s = build_fcc_cluster(79, 4.078, 10.0);
    double cutoff = 4.078 * 0.75;
    auto classes = classify_sites(s, cutoff, 12);

    auto geo = compute_geometric_energy(s, classes, cutoff);

    std::cout << "    E_cohesive/atom = " << geo.E_cohesive_per_atom << " kcal/mol\n";
    std::cout << "    E_bulk          = " << geo.E_bulk << " kcal/mol/atom\n";
    std::cout << "    E_surface       = " << geo.E_surface << " kcal/mol/atom\n";
    std::cout << "    surface_area    = " << geo.surface_area_est << " Å²\n";
    std::cout << "    σ               = " << geo.surface_energy_per_area << " kcal/mol/Å²\n";

    check(geo.E_cohesive_per_atom < 0, "cohesive energy is negative");
    check(geo.surface_area_est > 0, "surface area estimate positive");
    check(geo.n_bulk + geo.n_surface + geo.n_edge + geo.n_vertex == s.N,
          "atom count conserved in decomposition");
}

// ============================================================================
// T3: Structural fingerprint
// ============================================================================
static void test_structural_fingerprint() {
    std::cout << "\n=== T3: Structural Fingerprint ===\n";

    State s = build_fcc_cluster(79, 4.078, 10.0);
    double cutoff = 4.078 * 0.75;

    auto fp = compute_structural_fingerprint(s, cutoff, 0.1, 12.0);

    std::cout << "    mean_CN = " << fp.mean_CN << "\n";
    std::cout << "    CN histogram entries: " << fp.cn_histogram.size() << "\n";
    std::cout << "    RDF peaks found: " << fp.rdf_peak_positions.size() << "\n";
    std::cout << "    BAD peaks found: " << fp.bad_peak_angles.size() << "\n";

    check(fp.mean_CN > 0.0, "mean CN > 0");
    check(!fp.cn_histogram.empty(), "CN histogram non-empty");
    // FCC should have RDF peaks near a0/√2 ≈ 2.88 Å
    if (!fp.rdf_peak_positions.empty()) {
        double first_peak = fp.rdf_peak_positions[0];
        std::cout << "    first RDF peak at " << first_peak << " Å\n";
        check(first_peak > 2.5 && first_peak < 3.5,
              "first RDF peak near NN distance (~2.88 Å for Au)");
    } else {
        check(false, "at least one RDF peak expected");
    }
}

// ============================================================================
// T4: Dynamics observables
// ============================================================================
static void test_dynamics() {
    std::cout << "\n=== T4: Dynamics Observables ===\n";

    // Create a 3-frame trajectory with small displacements
    State s0 = build_fcc_cluster(79, 4.078, 6.0);
    State s1 = s0, s2 = s0;

    // Apply small random displacements to simulate thermal motion
    std::srand(42);
    for (uint32_t i = 0; i < s0.N; ++i) {
        double dx = 0.01 * (std::rand() / (double)RAND_MAX - 0.5);
        double dy = 0.01 * (std::rand() / (double)RAND_MAX - 0.5);
        double dz = 0.01 * (std::rand() / (double)RAND_MAX - 0.5);
        s1.X[i].x += dx; s1.X[i].y += dy; s1.X[i].z += dz;
        s2.X[i].x += 2*dx; s2.X[i].y += 2*dy; s2.X[i].z += 2*dz;
    }

    std::vector<State> traj = {s0, s1, s2};
    auto dyn = compute_dynamics(traj, 1.0 /* fs */, 0);

    std::cout << "    D = " << dyn.diffusion_coeff << " Å²/fs\n";
    std::cout << "    Lindemann = " << dyn.lindemann_index << "\n";
    std::cout << "    MSD points = " << dyn.msd_values.size() << "\n";

    check(dyn.msd_values.size() >= 1, "MSD has data points");
    check(dyn.msd_values[0] >= 0.0, "MSD non-negative");
    check(dyn.lindemann_index >= 0.0, "Lindemann non-negative");
}

// ============================================================================
// T5: Scoring against experiment
// ============================================================================
static void test_scoring() {
    std::cout << "\n=== T5: Scoring Against Experiment ===\n";

    State s = build_fcc_cluster(79, 4.078, 10.0);
    double cutoff = 4.078 * 0.75;

    auto classes = classify_sites(s, cutoff, 12);
    auto geo     = compute_geometric_energy(s, classes, cutoff);
    auto struc   = compute_structural_fingerprint(s, cutoff);
    DynamicsObservables dyn{};  // no trajectory, dynamics scores default to 1.0

    auto ref = reference::gold_5nm_fcc();
    auto ms  = score_against_experiment(geo, struc, dyn, ref);

    std::cout << "    score_cohesive       = " << ms.score_cohesive << "\n";
    std::cout << "    score_surface_energy = " << ms.score_surface_energy << "\n";
    std::cout << "    score_CN             = " << ms.score_CN << "\n";
    std::cout << "    score_diffusion      = " << ms.score_diffusion << "\n";
    std::cout << "    score_lindemann      = " << ms.score_lindemann << "\n";
    std::cout << "    composite            = " << ms.composite << "\n";

    check(ms.composite >= 0.0 && ms.composite <= 1.0, "composite in [0,1]");
    check(ms.score_cohesive >= 0.0, "cohesive score non-negative");
    check(ms.score_CN >= 0.0, "CN score non-negative");
}

// ============================================================================
// T6: Reference data sanity
// ============================================================================
static void test_reference_sanity() {
    std::cout << "\n=== T6: Reference Data Sanity ===\n";

    auto refs = reference::all_nanoparticle_refs();
    check(refs.size() >= 6, "at least 6 built-in references");

    for (const auto& r : refs) {
        bool sane = r.Z > 0 && r.diameter_ang > 0 && r.approx_N > 0
                 && r.cohesive_energy < 0 && r.bulk_CN > 0;
        std::cout << "    " << r.label << ": Z=" << r.Z
                  << "  d=" << r.diameter_ang << "Å"
                  << "  N≈" << r.approx_N
                  << "  E_coh=" << r.cohesive_energy << " kcal/mol"
                  << (sane ? "  ✓" : "  ✗") << "\n";
        check(sane, (std::string("reference sane: ") + r.label).c_str());
    }
}

// ============================================================================

int main() {
    std::cout << "============================================================\n";
    std::cout << " Experimental Data Matching — Near-Atomistic Regime (4-6nm)\n";
    std::cout << "============================================================\n";

    test_site_classification();
    test_geometric_energy();
    test_structural_fingerprint();
    test_dynamics();
    test_scoring();
    test_reference_sanity();

    std::cout << "\n============================================================\n";
    std::cout << " Results: " << g_pass << " passed, " << g_fail << " failed\n";
    std::cout << "============================================================\n";

    return (g_fail == 0) ? 0 : 1;
}
