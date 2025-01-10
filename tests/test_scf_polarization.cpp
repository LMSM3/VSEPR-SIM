/**
 * test_scf_polarization.cpp
 * =========================
 * Phase 1 validation for the SCF induced-dipole polarization solver.
 *
 * Test groups:
 *
 *  A — Argon Dimer (Ar2)          pure induced-dipole response
 *  ─────────────────────────────────────────────────────────────
 *  A1  Field-off symmetry: no external source -> mu = 0
 *  A2  External field symmetry: field along bond, mu_0 ~= mu_1
 *      (charge at 500 A gives <2% asymmetry across the 4 A dimer)
 *  A3  Linear response: mu scales linearly with Q_ext
 *  A4  Convergence rate vs separation (3, 4, 5, 7, 10 A)
 *  A5  50-seed determinism: any initial guess -> same result
 *
 *  B — Water Trimer (cyclic (H2O)3)   permanent dipoles + coupling
 *  ─────────────────────────────────────────────────────────────
 *  B1  Converges within max_iter with default params
 *  B2  Induced dipoles non-zero; U_pol < 0 (stabilising)
 *  B3  50-seed determinism: same converged dipoles for all seeds
 *  B4  Permutation invariance: swapping molecules gives same U_pol
 *
 * Notes on geometry:
 *  - "External field" is injected via a virtual non-polarisable charge
 *    placed far from the system.  Moving it to 500 A (vs. 50 A used
 *    earlier) reduces the field asymmetry across the 4 A dimer from
 *    15% to ~1.6%, within the 3% test tolerance.
 *  - Water geometry: O at ring radius 2.8 A, r_OH = 0.96 A, HOH = 104.52 deg.
 *  - TIP3P-like charges: qO = -0.834 e, qH = +0.417 e.
 *  - Intramolecular O-H and H-H pairs (r < r_excl = 1.6 A) are excluded
 *    from both E_perm and the dipole-dipole tensor, preventing divergence.
 */

#include "atomistic/models/polarization_scf.hpp"
#include "atomistic/core/state.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
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
           std::cerr << "  FAIL [" << __LINE__ << "]: " << (msg) << "\n"; } \
} while(0)

#define CHECK_CLOSE(a, b, tol, msg) \
    CHECK(std::abs((double)(a) - (double)(b)) < (double)(tol), \
          std::string(msg) + "  got=" + std::to_string(a) \
                           + "  exp~=" + std::to_string(b))

#define SECTION(name) do { std::cout << "\n[" << (name) << "]\n"; } while(0)

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

/// Ar dimer centred at origin, separation r along x-axis.
static State ar_dimer(double r) {
    State s = make_state(2);
    s.type[0] = 18; s.type[1] = 18;
    s.X[0] = {-r * 0.5, 0.0, 0.0};
    s.X[1] = { r * 0.5, 0.0, 0.0};
    init_polarizabilities(s);
    return s;
}

/// Ar dimer plus a virtual non-polarisable field-source charge.
/// The field-source is excluded from dipole induction (alpha = 0).
static State ar_dimer_with_field(double r, double Q_ext, Vec3 p_ext) {
    State s = make_state(3);
    s.type[0] = 18; s.type[1] = 18; s.type[2] = 0u;
    s.X[0] = {-r * 0.5, 0.0, 0.0};
    s.X[1] = { r * 0.5, 0.0, 0.0};
    s.X[2] = p_ext;
    s.Q[2] = Q_ext;
    init_polarizabilities(s);
    s.polarizabilities[2] = 0.0;   // field-source only, not polarisable
    return s;
}

/**
 * Cyclic water trimer  (H2O)3.
 * Three molecules at 120 deg, O at ring radius R_O.
 * TIP3P-like charges: qO = -0.834 e, qH = +0.417 e.
 */
static State water_trimer() {
    constexpr double R_O       = 2.80;
    constexpr double r_OH      = 0.96;
    constexpr double theta_HOH = 104.52 * M_PI / 180.0;
    constexpr double qO        = -0.834;
    constexpr double qH        =  0.417;

    State s = make_state(9);
    const uint32_t types[9] = {8,1,1, 8,1,1, 8,1,1};
    const double   charges[9] = {qO,qH,qH, qO,qH,qH, qO,qH,qH};
    for (int i = 0; i < 9; ++i) { s.type[i] = types[i]; s.Q[i] = charges[i]; }

    for (int m = 0; m < 3; ++m) {
        const double phi   = m * 2.0 * M_PI / 3.0;
        const double Ox    = R_O * std::cos(phi);
        const double Oy    = R_O * std::sin(phi);
        const double half  = theta_HOH * 0.5;
        const double phi_h1 = phi + M_PI + half;
        const double phi_h2 = phi + M_PI - half;

        s.X[m*3+0] = {Ox, Oy, 0.0};
        s.X[m*3+1] = {Ox + r_OH * std::cos(phi_h1), Oy + r_OH * std::sin(phi_h1), 0.0};
        s.X[m*3+2] = {Ox + r_OH * std::cos(phi_h2), Oy + r_OH * std::sin(phi_h2), 0.0};
    }
    init_polarizabilities(s);
    return s;
}

/// Randomise s.induced_dipoles with amplitude `amp` using seed.
static void perturb_dipoles(State& s, uint64_t seed, double amp = 0.1) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dist(-amp, amp);
    for (auto& mu : s.induced_dipoles) {
        mu.x = dist(rng);
        mu.y = dist(rng);
        mu.z = dist(rng);
    }
}

// ============================================================================
// Group A — Argon dimer
// ============================================================================

void test_A1_field_off() {
    SECTION("A1: Ar2 -- no external source -> mu = 0");
    State s = ar_dimer(4.0);
    SCFResult r = solve(s);
    CHECK(r.converged, "converges");
    CHECK_CLOSE(norm(s.induced_dipoles[0]), 0.0, 1e-12, "|mu_0| == 0");
    CHECK_CLOSE(norm(s.induced_dipoles[1]), 0.0, 1e-12, "|mu_1| == 0");
    CHECK_CLOSE(r.U_pol, 0.0, 1e-12, "U_pol == 0");
}

void test_A2_field_symmetry() {
    SECTION("A2: Ar2 -- external field along x: mu_0 ~= mu_1 (< 3% asymmetry)");
    // Field charge at x=500 A -> field asymmetry across 4 A dimer is ~1.6%
    const double sep   = 4.0;
    const Vec3   p_ext = {500.0, 0.0, 0.0};
    State s = ar_dimer_with_field(sep, 1.0, p_ext);
    SCFResult r = solve(s);
    CHECK(r.converged, "converges with field");

    const double mu0x = s.induced_dipoles[0].x;
    const double mu1x = s.induced_dipoles[1].x;
    const double mu0y = std::abs(s.induced_dipoles[0].y);
    const double mu1y = std::abs(s.induced_dipoles[1].y);
    const double avg  = 0.5 * (std::abs(mu0x) + std::abs(mu1x));

    CHECK(mu0x > 0.0, "mu_0.x > 0");
    CHECK(mu1x > 0.0, "mu_1.x > 0");
    CHECK(avg > 1e-10, "induced dipoles are non-zero");
    // Asymmetry < 3% (theoretical limit at 500 A: ~1.6%)
    CHECK_CLOSE(mu0x, mu1x, 0.03 * avg, "mu_0.x ~= mu_1.x  (< 3% asymmetry)");
    CHECK_CLOSE(mu0y, 0.0, 1e-8, "mu_0.y ~ 0");
    CHECK_CLOSE(mu1y, 0.0, 1e-8, "mu_1.y ~ 0");
}

void test_A3_linear_response() {
    SECTION("A3: Ar2 -- mu scales linearly with Q_ext");
    const double sep   = 4.0;
    const Vec3   p_ext = {500.0, 0.0, 0.0};

    // Use tight tolerance: at 500 A the induced dipoles are very small
    // (mu ~ 1e-5 e*Ang), so the default 1e-6 tolerance is ~10% relative.
    SCFParams tight;
    tight.tolerance = 1e-10;

    State s1 = ar_dimer_with_field(sep, 1.0, p_ext); solve(s1, tight);
    State s2 = ar_dimer_with_field(sep, 2.0, p_ext); solve(s2, tight);

    const double mu1 = s1.induced_dipoles[0].x;
    const double mu2 = s2.induced_dipoles[0].x;
    const double ratio = (std::abs(mu1) > 1e-15) ? mu2 / mu1 : 0.0;
    // Should be 2.0 in the linear regime (small polarisation relative to field)
    CHECK_CLOSE(ratio, 2.0, 0.05, "dipole scales linearly with Q_ext");
}

void test_A4_convergence_vs_sep() {
    SECTION("A4: Ar2 -- convergence rate vs separation");
    const double seps[] = {3.0, 4.0, 5.0, 7.0, 10.0};
    for (double r : seps) {
        State s = ar_dimer_with_field(r, 1.0, Vec3{500.0, 0.0, 0.0});
        SCFResult res = solve(s);
        std::cout << "    r=" << r << " A: iter=" << res.iterations
                  << "  residual=" << res.residual
                  << (res.converged ? "  PASS" : "  FAIL") << "\n";
        CHECK(res.converged, "converges at r=" + std::to_string(r));
        CHECK(res.iterations < 100, "fast (<100 iter) at r=" + std::to_string(r));
    }
}

void test_A5_determinism_50seeds() {
    SECTION("A5: Ar2 -- 50-seed determinism");
    const double sep   = 4.0;
    const Vec3   p_ext = {500.0, 0.0, 0.0};

    State ref = ar_dimer_with_field(sep, 1.0, p_ext);
    solve(ref);
    const double ref0 = ref.induced_dipoles[0].x;
    const double ref1 = ref.induced_dipoles[1].x;

    int bad = 0;
    for (int seed = 0; seed < 50; ++seed) {
        State s = ar_dimer_with_field(sep, 1.0, p_ext);
        perturb_dipoles(s, static_cast<uint64_t>(seed), 0.5);
        SCFResult r = solve(s);
        if (!r.converged) { ++bad; continue; }
        if (std::abs(s.induced_dipoles[0].x - ref0) > 1e-5 ||
            std::abs(s.induced_dipoles[1].x - ref1) > 1e-5) { ++bad; }
    }
    CHECK(bad == 0, "all 50 seeds agree (bad=" + std::to_string(bad) + ")");
}

// ============================================================================
// Group B — Water trimer
// ============================================================================

void test_B1_convergence() {
    SECTION("B1: Water trimer -- SCF converges");
    State s = water_trimer();
    SCFResult r = solve(s);
    std::cout << "    iter=" << r.iterations
              << "  residual=" << r.residual
              << "  U_pol=" << r.U_pol << " kcal/mol\n";
    CHECK(r.converged, "trimer SCF converges");
    CHECK(r.iterations < 150, "trimer converges quickly (< 150 iter)");
}

void test_B2_nonzero_stabilising() {
    SECTION("B2: Water trimer -- non-zero dipoles, U_pol < 0");
    State s = water_trimer();
    solve(s);
    double total_mu = 0.0;
    for (auto& mu : s.induced_dipoles) total_mu += norm(mu);
    CHECK(total_mu > 1e-6, "at least one non-zero induced dipole");
    CHECK(s.E.Upol < 0.0, "U_pol < 0 (stabilising)");
}

void test_B3_determinism_50seeds() {
    SECTION("B3: Water trimer -- 50-seed determinism");
    State ref = water_trimer();
    solve(ref);

    int bad = 0;
    for (int seed = 0; seed < 50; ++seed) {
        State s = water_trimer();
        perturb_dipoles(s, static_cast<uint64_t>(seed + 200), 0.3);
        SCFResult r = solve(s);
        if (!r.converged) { ++bad; continue; }
        for (uint32_t i = 0; i < s.N; ++i) {
            Vec3 d{ s.induced_dipoles[i].x - ref.induced_dipoles[i].x,
                    s.induced_dipoles[i].y - ref.induced_dipoles[i].y,
                    s.induced_dipoles[i].z - ref.induced_dipoles[i].z };
            if (norm(d) > 1e-5) { ++bad; break; }
        }
    }
    CHECK(bad == 0, "all 50 seeds agree (bad=" + std::to_string(bad) + ")");
}

void test_B4_permutation_invariance() {
    SECTION("B4: Water trimer -- permutation invariance");
    State s_orig = water_trimer();
    solve(s_orig);

    // Swap molecule 0 [atoms 0,1,2] with molecule 2 [atoms 6,7,8]
    State s_perm = water_trimer();
    for (int k = 0; k < 3; ++k) {
        std::swap(s_perm.X[k],               s_perm.X[6+k]);
        std::swap(s_perm.Q[k],               s_perm.Q[6+k]);
        std::swap(s_perm.type[k],            s_perm.type[6+k]);
        std::swap(s_perm.polarizabilities[k],s_perm.polarizabilities[6+k]);
    }
    solve(s_perm);

    CHECK_CLOSE(s_orig.E.Upol, s_perm.E.Upol, 1e-6,
                "U_pol invariant under permutation");

    double sum_orig = 0.0, sum_perm = 0.0;
    for (uint32_t i = 0; i < s_orig.N; ++i) {
        sum_orig += norm(s_orig.induced_dipoles[i]);
        sum_perm += norm(s_perm.induced_dipoles[i]);
    }
    CHECK_CLOSE(sum_orig, sum_perm, 1e-6,
                "SUM|mu_i| invariant under permutation");
}

// ============================================================================
// main
// ============================================================================

int main() {
    std::cout << "============================================================\n";
    std::cout << " SCF Polarization  --  Phase 1 Validation\n";
    std::cout << "============================================================\n";

    test_A1_field_off();
    test_A2_field_symmetry();
    test_A3_linear_response();
    test_A4_convergence_vs_sep();
    test_A5_determinism_50seeds();

    test_B1_convergence();
    test_B2_nonzero_stabilising();
    test_B3_determinism_50seeds();
    test_B4_permutation_invariance();

    const int total = g_passed + g_failed;
    std::cout << "\n============================================================\n";
    std::cout << "  Result:  " << g_passed << " / " << total << " passed";
    if (g_failed == 0) std::cout << "  ALL PASS\n";
    else               std::cout << "  " << g_failed << " FAILED\n";
    std::cout << "============================================================\n";

    return (g_failed == 0) ? 0 : 1;
}
