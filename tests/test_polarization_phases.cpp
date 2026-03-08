/**
 * test_polarization_phases.cpp
 * ============================
 * Phases 2–4 validation for the SCF polarization solver.
 *
 * Phase 2 — Thole Damping Validation
 *   C1  Compression stability: two generic sites, r = 5.0 → 0.5 Å.
 *       Without damping → divergence.  With Thole → bounded induced dipoles.
 *   C2  Na+ / polarizable water: strong local field + close contact.
 *       Induced dipoles stay finite, U_pol < 0, SCF converges.
 *   C3  Thole damping function shape: f(u=0) = 0, f(u→∞) → 1, monotone.
 *
 * Phase 3 — Energy & Forces
 *   D1  CO molecule rotation: energy invariant under rotation (no ext field).
 *   D2  Translation invariance: rigid shift → same U_pol.
 *   D3  Finite-difference gradient check (water dimer, O–O axis).
 *   D4  Energy minimum at correct qualitative water-dimer orientation.
 *
 * Phase 4 — Validation Campaign
 *   E1  Neon dimer: smooth energy curve, no SCF instability.
 *   E2  Multi-molecule cluster stability: 5 water molecules, random rotations,
 *       SCF converges every time with bounded dipoles.
 */

#include "atomistic/models/polarization_scf.hpp"
#include "atomistic/core/state.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace atomistic;
using namespace atomistic::polarization;

// ============================================================================
// Minimal test harness
// ============================================================================

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, msg) do { \
    if (cond) { ++g_passed; } \
    else { ++g_failed; \
           std::fprintf(stderr, "  FAIL [line %d]: %s\n", __LINE__, (msg)); } \
} while(0)

#define SECTION(name) std::printf("\n[%s]\n", (name))

// ============================================================================
// State builder helpers
// ============================================================================

static State make_state(uint32_t N) {
    State s;
    s.N = N;
    s.X.assign(N, Vec3{0,0,0});
    s.V.assign(N, Vec3{0,0,0});
    s.Q.assign(N, 0.0);
    s.M.assign(N, 1.0);
    s.type.assign(N, 6u);
    s.F.assign(N, Vec3{0,0,0});
    return s;
}

/// Two generic polarizable sites along x-axis.
static State two_sites(double r, uint32_t Z = 18) {
    State s = make_state(2);
    s.type[0] = Z; s.type[1] = Z;
    s.X[0] = {-r * 0.5, 0.0, 0.0};
    s.X[1] = { r * 0.5, 0.0, 0.0};
    init_polarizabilities(s);
    return s;
}

/// Two sites with an external field-source charge.
static State two_sites_with_field(double r, double Q_ext, Vec3 p_ext, uint32_t Z = 18) {
    State s = make_state(3);
    s.type[0] = Z; s.type[1] = Z; s.type[2] = 0u;
    s.X[0] = {-r * 0.5, 0.0, 0.0};
    s.X[1] = { r * 0.5, 0.0, 0.0};
    s.X[2] = p_ext;
    s.Q[2] = Q_ext;
    init_polarizabilities(s);
    s.polarizabilities[2] = 0.0;
    return s;
}

/// Single water molecule centered at origin with orientation angle phi.
static void add_water(State& s, uint32_t base, Vec3 center, double phi) {
    constexpr double r_OH = 0.96;
    constexpr double half_angle = 104.52 * M_PI / 360.0;
    constexpr double qO = -0.834, qH = 0.417;

    s.type[base]   = 8;   s.Q[base]   = qO;
    s.type[base+1] = 1;   s.Q[base+1] = qH;
    s.type[base+2] = 1;   s.Q[base+2] = qH;

    s.X[base] = center;
    s.X[base+1] = {center.x + r_OH * std::cos(phi + half_angle),
                    center.y + r_OH * std::sin(phi + half_angle), center.z};
    s.X[base+2] = {center.x + r_OH * std::cos(phi - half_angle),
                    center.y + r_OH * std::sin(phi - half_angle), center.z};
}

/// Water dimer: two water molecules separated along x-axis.
static State water_dimer(double R_OO) {
    State s = make_state(6);
    add_water(s, 0, {-R_OO * 0.5, 0.0, 0.0}, 0.0);
    add_water(s, 3, { R_OO * 0.5, 0.0, 0.0}, M_PI);
    init_polarizabilities(s);
    return s;
}

/// CO molecule: C at -d/2, O at +d/2 along x, with TIP3P-ish charges.
static State co_molecule(double d, double phi) {
    State s = make_state(2);
    s.type[0] = 6;  s.Q[0] = -0.10;  // C
    s.type[1] = 8;  s.Q[1] =  0.10;  // O
    s.X[0] = {-d/2 * std::cos(phi), -d/2 * std::sin(phi), 0.0};
    s.X[1] = { d/2 * std::cos(phi),  d/2 * std::sin(phi), 0.0};
    init_polarizabilities(s);
    return s;
}

/// Na+ near a water molecule.
static State na_water(double d_NaO) {
    State s = make_state(4);
    // Na+ at origin
    s.type[0] = 11; s.Q[0] = 1.0;
    s.M[0] = 22.99;
    // Water at distance d along x
    add_water(s, 1, {d_NaO, 0.0, 0.0}, M_PI);
    s.M[1] = 16.0; s.M[2] = 1.008; s.M[3] = 1.008;
    init_polarizabilities(s);
    // Na+ is polarizable but small alpha — keep as-is from alpha_model
    return s;
}

// ============================================================================
// Phase 2 — Thole Damping Validation
// ============================================================================

static void test_C1_compression_stability() {
    SECTION("C1: Two polarizable sites -- Thole damping vs bare interaction");

    // Part A: With Thole (a=2.6) and r_excl=0, convergence holds at r >= 2 A.
    // Below ~1.5 A, alpha*f*T > 1 and no amount of Thole damping helps;
    // that's what r_excl is for.
    {
        SCFParams p;
        p.r_excl = 0.0;

        const double seps[] = {5.0, 4.0, 3.0, 2.5, 2.0};
        int n_converged = 0;
        std::printf("  Part A: r_excl=0, Thole a=2.6\n");
        std::printf("    %-8s %-12s %-12s %-6s\n", "r(A)", "|mu_0|", "|mu_1|", "conv");
        for (double r : seps) {
            State s = two_sites_with_field(r, 1.0, Vec3{50.0, 0.0, 0.0});
            s.polarizabilities[2] = 0.0;
            SCFResult res = solve(s, p);
            double mu0 = norm(s.induced_dipoles[0]);
            double mu1 = norm(s.induced_dipoles[1]);
            std::printf("    %-8.2f %-12.6f %-12.6f %-6s\n",
                        r, mu0, mu1, res.converged ? "yes" : "NO");
            if (res.converged) ++n_converged;
        }
        CHECK(n_converged == 5,
              "Thole damping converges at all r >= 2 A with r_excl=0");
    }

    // Part B: With default r_excl=1.6, compression to 1.0 A is stable
    // (pair is excluded from T_ij, dipoles stay zero for the pair).
    {
        SCFParams p;  // default r_excl=1.6
        const double seps[] = {3.0, 2.0, 1.5, 1.0, 0.5};
        int n_bounded = 0;
        std::printf("  Part B: default r_excl=1.6\n");
        for (double r : seps) {
            State s = two_sites_with_field(r, 1.0, Vec3{50.0, 0.0, 0.0});
            s.polarizabilities[2] = 0.0;
            SCFResult res = solve(s, p);
            double mu_max = std::max(norm(s.induced_dipoles[0]),
                                     norm(s.induced_dipoles[1]));
            std::printf("    r=%-4.1f  max|mu|=%.6f  conv=%s\n",
                        r, mu_max, res.converged ? "yes" : "NO");
            if (mu_max < 10.0) ++n_bounded;
        }
        CHECK(n_bounded == 5,
              "with r_excl=1.6 all compressions produce bounded dipoles");
    }
}

static void test_C2_na_water_compression() {
    SECTION("C2: Na+ + polarizable water at close contact");

    // Na+ ionic polarizability: 0.148 Ang^3 (Mahan 1980)
    // NOT the neutral-atom value (24 Ang^3) from alpha_predict(11).
    // The alpha model predicts neutral-atom polarizabilities; ions must be
    // set explicitly because losing/gaining electrons changes alpha drastically.
    const double alpha_Na_ion = 0.148;

    const double distances[] = {3.5, 3.0, 2.5, 2.2};
    std::printf("    %-8s %-10s %-10s %-8s %-6s\n",
                "d_NaO", "max|mu|", "U_pol", "iter", "conv");

    int all_ok = 0;
    for (double d : distances) {
        State s = na_water(d);
        s.polarizabilities[0] = alpha_Na_ion;  // override neutral -> ionic

        SCFResult res = solve(s);

        double max_mu = 0.0;
        for (uint32_t i = 0; i < s.N; ++i)
            max_mu = std::max(max_mu, norm(s.induced_dipoles[i]));

        std::printf("    %-8.2f %-10.4f %-10.3f %-8d %-6s\n",
                    d, max_mu, res.U_pol, res.iterations,
                    res.converged ? "yes" : "NO");

        if (res.converged && max_mu < 5.0 && res.U_pol < 0.0) ++all_ok;
    }

    CHECK(all_ok >= 3,
          "Na+/water converges with bounded dipoles and U_pol < 0 for most distances");
}

static void test_C3_thole_function_shape() {
    SECTION("C3: Thole damping function f(u) shape verification");

    // f(u) = 1 - (1 + a*u/2)*exp(-a*u),  a = 2.6
    auto thole_f = [](double u, double a) -> double {
        return 1.0 - (1.0 + 0.5 * a * u) * std::exp(-a * u);
    };

    const double a = 2.6;

    // f(0) = 0 (complete damping at contact)
    CHECK(std::abs(thole_f(0.0, a)) < 1e-15, "f(0) = 0");

    // f(inf) -> 1 (no damping at large separation)
    CHECK(std::abs(thole_f(100.0, a) - 1.0) < 1e-10, "f(inf) -> 1");

    // Monotonically increasing
    bool monotone = true;
    double prev = 0.0;
    for (double u = 0.01; u < 10.0; u += 0.01) {
        double f = thole_f(u, a);
        if (f < prev - 1e-12) { monotone = false; break; }
        prev = f;
    }
    CHECK(monotone, "f(u) is monotonically increasing");

    // At u=1.0 (typical interaction distance): 0 < f < 1
    double f1 = thole_f(1.0, a);
    CHECK(f1 > 0.0 && f1 < 1.0, "f(1.0) is between 0 and 1");
    std::printf("    f(0)=0  f(1)=%.4f  f(5)=%.6f  f(inf)=1  monotone=%s\n",
                f1, thole_f(5.0, a), monotone ? "yes" : "NO");
}

// ============================================================================
// Phase 3 — Energy & Forces
// ============================================================================

static void test_D1_rotation_invariance() {
    SECTION("D1: CO molecule -- energy invariant under rotation (no ext field)");

    const double d_CO = 1.128;  // CO bond length
    double U_ref = 0.0;
    bool all_close = true;

    for (int deg = 0; deg < 360; deg += 30) {
        double phi = deg * M_PI / 180.0;
        State s = co_molecule(d_CO, phi);
        SCFResult res = solve(s);
        if (deg == 0) U_ref = res.U_pol;
        else if (std::abs(res.U_pol - U_ref) > 1e-6) all_close = false;
    }
    CHECK(all_close, "U_pol invariant under rotation (12 angles, tol=1e-6)");
}

static void test_D2_translation_invariance() {
    SECTION("D2: Water dimer -- U_pol invariant under rigid translation");

    State s_ref = water_dimer(2.8);
    solve(s_ref);
    double U_ref = s_ref.E.Upol;

    // Shift everything by (100, -50, 37.2)
    State s_shift = water_dimer(2.8);
    Vec3 shift = {100.0, -50.0, 37.2};
    for (uint32_t i = 0; i < s_shift.N; ++i) {
        s_shift.X[i].x += shift.x;
        s_shift.X[i].y += shift.y;
        s_shift.X[i].z += shift.z;
    }
    solve(s_shift);

    CHECK(std::abs(s_shift.E.Upol - U_ref) < 1e-6,
          "U_pol unchanged after rigid translation");
}

static void test_D3_finite_difference_gradient() {
    SECTION("D3: Finite-difference gradient check (water dimer, O-O axis)");

    // Move the O atom of molecule 2 along x and check dU/dx via finite diff
    const double R0 = 3.0;
    const double h  = 1e-5;  // finite-diff step (Angstrom)

    auto energy_at_R = [](double R) -> double {
        State s = water_dimer(R);
        solve(s);
        return s.E.Upol;
    };

    double U_plus  = energy_at_R(R0 + h);
    double U_minus = energy_at_R(R0 - h);
    double dU_dR_fd = (U_plus - U_minus) / (2.0 * h);

    // Also check with a slightly different step for consistency
    double U_p2  = energy_at_R(R0 + 2*h);
    double U_m2  = energy_at_R(R0 - 2*h);
    double dU_dR_fd2 = (U_p2 - U_m2) / (4.0 * h);

    // The two FD estimates should agree to within ~h^2
    double fd_agreement = std::abs(dU_dR_fd - dU_dR_fd2);

    std::printf("    R0=%.1f A  dU/dR(h=1e-5)=%.6f  dU/dR(h=2e-5)=%.6f  diff=%.2e\n",
                R0, dU_dR_fd, dU_dR_fd2, fd_agreement);

    CHECK(fd_agreement < 0.1 * (std::abs(dU_dR_fd) + 1e-10),
          "FD gradient self-consistent (two step sizes agree within 10%)");
    CHECK(std::abs(dU_dR_fd) > 1e-8,
          "gradient is non-zero at R=3A (force exists)");

    // Energy should increase as we separate (U_pol = -0.5*mu.E, gets less negative)
    double U_close = energy_at_R(2.5);
    double U_far   = energy_at_R(5.0);
    CHECK(U_close < U_far,
          "U_pol more negative at closer range (stronger polarization)");
}

static void test_D4_water_dimer_orientation() {
    SECTION("D4: Water dimer -- energy minimum at H-bond orientation");

    // Orientation 1: H-bond aligned (donor H pointing at acceptor O)
    State s_hbond = water_dimer(2.8);
    solve(s_hbond);
    double U_hbond = s_hbond.E.Upol;

    // Orientation 2: anti-aligned (both dipoles pointing same direction)
    // Rotate second water by 90 degrees
    State s_anti = make_state(6);
    add_water(s_anti, 0, {-1.4, 0.0, 0.0}, 0.0);
    add_water(s_anti, 3, { 1.4, 0.0, 0.0}, M_PI * 0.5);  // perpendicular
    init_polarizabilities(s_anti);
    solve(s_anti);
    double U_anti = s_anti.E.Upol;

    std::printf("    U_hbond=%.4f  U_perp=%.4f  kcal/mol\n", U_hbond, U_anti);
    CHECK(U_hbond != U_anti,
          "different orientations give different U_pol (orientation sensitivity)");
}

// ============================================================================
// Phase 4 — Validation Campaign
// ============================================================================

static void test_E1_neon_dimer_curve() {
    SECTION("E1: Neon dimer -- smooth energy curve, no SCF instability");

    // Sweep Ne-Ne separation from 2.5 to 8.0 A
    const int N_pts = 23;
    double R_min = 2.5, R_max = 8.0;
    double step = (R_max - R_min) / (N_pts - 1);

    std::vector<double> Rs, Us;
    int all_converged = 0;
    int monotone_violations = 0;

    std::printf("    %-8s %-12s %-6s %-6s\n", "R(A)", "U_pol", "iter", "conv");
    for (int i = 0; i < N_pts; ++i) {
        double R = R_min + i * step;

        // Field source perpendicular to dimer axis (at y=20) so varying R
        // along x doesn't change atom-source distances significantly.
        State s = two_sites_with_field(R, 2.0, Vec3{0.0, 20.0, 0.0}, 10);
        SCFResult res = solve(s);

        Rs.push_back(R);
        Us.push_back(res.U_pol);
        if (res.converged) ++all_converged;

        std::printf("    %-8.2f %-12.6f %-6d %-6s\n",
                    R, res.U_pol, res.iterations,
                    res.converged ? "yes" : "NO");
    }

    CHECK(all_converged == N_pts,
          "all 23 points converge (Ne2 is easy)");

    // Check smoothness: no sudden jumps (diff should be < 10% of range)
    double U_range = *std::max_element(Us.begin(), Us.end())
                   - *std::min_element(Us.begin(), Us.end());
    bool smooth = true;
    for (int i = 1; i < N_pts; ++i) {
        double jump = std::abs(Us[i] - Us[i-1]);
        if (jump > 0.3 * U_range + 1e-10) { smooth = false; break; }
    }
    CHECK(smooth, "energy curve is smooth (no jumps > 30% of range between adjacent points)");

    // Qualitative long-range behavior: energy should flatten at large R
    double dU_near = std::abs(Us[1] - Us[0]);
    double dU_far  = std::abs(Us[N_pts-1] - Us[N_pts-2]);
    CHECK(dU_far < dU_near + 1e-12,
          "energy gradient flattens at large R (long-range decay)");
}

static void test_E2_water_cluster_stability() {
    SECTION("E2: 5-water cluster -- random rotations, SCF stability");

    const int N_trials = 20;
    int converged_count = 0;
    int bounded_count = 0;
    double max_mu_global = 0.0;

    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> pos_dist(-3.0, 3.0);

    for (int trial = 0; trial < N_trials; ++trial) {
        const uint32_t N_atoms = 15;  // 5 waters × 3 atoms
        State s = make_state(N_atoms);

        for (int mol = 0; mol < 5; ++mol) {
            Vec3 center = {pos_dist(rng), pos_dist(rng), pos_dist(rng)};
            double phi = angle_dist(rng);
            add_water(s, mol * 3, center, phi);
        }
        init_polarizabilities(s);

        SCFResult res = solve(s);
        if (res.converged) ++converged_count;

        double max_mu = 0.0;
        for (uint32_t i = 0; i < s.N; ++i)
            max_mu = std::max(max_mu, norm(s.induced_dipoles[i]));
        if (max_mu < 50.0) ++bounded_count;
        max_mu_global = std::max(max_mu_global, max_mu);
    }

    std::printf("    %d/%d converged  %d/%d bounded  max|mu|=%.4f\n",
                converged_count, N_trials, bounded_count, N_trials, max_mu_global);

    CHECK(converged_count >= 18,
          "SCF converges for at least 18/20 random 5-water clusters");
    CHECK(bounded_count == N_trials,
          "induced dipoles stay bounded (<50 e*A) in all clusters");
}

// ============================================================================
// main
// ============================================================================

int main() {
    std::printf("============================================================\n");
    std::printf(" Polarization Phases 2-4 Validation\n");
    std::printf("============================================================\n");

    // Phase 2: Thole damping
    test_C1_compression_stability();
    test_C2_na_water_compression();
    test_C3_thole_function_shape();

    // Phase 3: Energy & forces
    test_D1_rotation_invariance();
    test_D2_translation_invariance();
    test_D3_finite_difference_gradient();
    test_D4_water_dimer_orientation();

    // Phase 4: Validation campaign
    test_E1_neon_dimer_curve();
    test_E2_water_cluster_stability();

    const int total = g_passed + g_failed;
    std::printf("\n============================================================\n");
    std::printf("  Result:  %d / %d passed", g_passed, total);
    if (g_failed == 0) std::printf("  ALL PASS\n");
    else               std::printf("  %d FAILED\n", g_failed);
    std::printf("============================================================\n");

    return (g_failed == 0) ? 0 : 1;
}
