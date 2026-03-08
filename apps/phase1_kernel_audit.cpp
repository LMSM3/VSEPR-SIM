/**
 * phase1_kernel_audit.cpp — Phase 1: Revalidate the Core Kernel
 *
 * Self-testing executable.  Reports pass/fail for every check.
 * No external test framework, no scripts, no JSON config.
 *
 * Checks:
 *   1.1  Energy ledger consistency  (total == sum of components)
 *   1.2  Deterministic evaluation   (two identical calls → identical output)
 *   1.3a Force finiteness           (no NaN / Inf anywhere)
 *   1.3b Physical sanity            (Ar dimer at equilibrium: F ≈ 0, U ≈ −ε)
 *   1.3c Force–energy consistency   (F ≈ −dU/dx via finite difference)
 */

#include "atomistic/core/state.hpp"
#include "atomistic/models/model.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

using namespace atomistic;

// ============================================================================
// Helpers
// ============================================================================

static int g_pass = 0, g_fail = 0;

static void check(bool ok, const char* name)
{
    if (ok) { std::printf("  [PASS]  %s\n", name); ++g_pass; }
    else    { std::printf("  [FAIL]  %s\n", name); ++g_fail; }
}

static bool all_finite(const State& s)
{
    for (uint32_t i = 0; i < s.N; ++i) {
        const auto& f = s.F[i];
        if (!std::isfinite(f.x) || !std::isfinite(f.y) || !std::isfinite(f.z))
            return false;
    }
    return true;
}

static bool energy_finite(const EnergyTerms& e)
{
    return std::isfinite(e.Ubond) && std::isfinite(e.Uangle)
        && std::isfinite(e.Utors) && std::isfinite(e.UvdW)
        && std::isfinite(e.UCoul) && std::isfinite(e.Uext)
        && std::isfinite(e.Upol)  && std::isfinite(e.total());
}

// ============================================================================
// Build an Ar dimer at a given separation along x
// ============================================================================

static State make_ar_dimer(double separation)
{
    State s;
    s.N = 2;
    s.X = {{0, 0, 0}, {separation, 0, 0}};
    s.V.resize(s.N, {0, 0, 0});
    s.Q = {0, 0};
    s.M = {39.948, 39.948};
    s.type = {18, 18};   // Z=18 for Ar
    s.F.resize(s.N, {0, 0, 0});
    return s;
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::printf("\n");
    std::printf("=============================================================\n");
    std::printf("  Phase 1 — Core Kernel Audit\n");
    std::printf("=============================================================\n\n");

    auto model = create_lj_coulomb_model();
    ModelParams mp;
    mp.rc = 12.0;

    // ------------------------------------------------------------------
    // 1.1  Energy ledger consistency
    // ------------------------------------------------------------------
    std::printf("--- 1.1 Energy Ledger Consistency ---\n");
    {
        State s = make_ar_dimer(4.0);
        model->eval(s, mp);

        double sum = s.E.Ubond + s.E.Uangle + s.E.Utors
                   + s.E.UvdW + s.E.UCoul + s.E.Uext + s.E.Upol;
        double tot = s.E.total();

        check(std::abs(sum - tot) < 1e-15,
              "total() == sum of components");
        check(energy_finite(s.E),
              "all energy terms are finite");

        std::printf("    U_total = %.8f  U_vdw = %.8f  U_coul = %.8f\n",
                    tot, s.E.UvdW, s.E.UCoul);
        std::printf("    U_bond  = %.8f  U_angle = %.8f  U_tors = %.8f\n",
                    s.E.Ubond, s.E.Uangle, s.E.Utors);

        // NOTE: Coulomb is currently disabled in the kernel.
        // Record this fact as a declared limitation.
        check(s.E.UCoul == 0.0,
              "Coulomb term is zero (disabled — declared limitation)");
    }

    // ------------------------------------------------------------------
    // 1.2  Deterministic evaluation
    // ------------------------------------------------------------------
    std::printf("\n--- 1.2 Deterministic Evaluation ---\n");
    {
        State s1 = make_ar_dimer(4.0);
        State s2 = make_ar_dimer(4.0);

        model->eval(s1, mp);
        model->eval(s2, mp);

        bool e_match = (s1.E.total() == s2.E.total());
        bool f_match = true;
        for (uint32_t i = 0; i < s1.N; ++i) {
            if (s1.F[i].x != s2.F[i].x ||
                s1.F[i].y != s2.F[i].y ||
                s1.F[i].z != s2.F[i].z) { f_match = false; break; }
        }

        check(e_match, "identical inputs → identical energy (bitwise)");
        check(f_match, "identical inputs → identical forces (bitwise)");
    }

    // ------------------------------------------------------------------
    // 1.3a  Force finiteness over a sweep
    // ------------------------------------------------------------------
    std::printf("\n--- 1.3a Force Finiteness ---\n");
    {
        bool ok = true;
        // Sweep from 2.5 Å to 10 Å in 0.1 Å steps
        for (double r = 2.5; r <= 10.0; r += 0.1) {
            State s = make_ar_dimer(r);
            model->eval(s, mp);
            if (!all_finite(s) || !energy_finite(s.E)) { ok = false; break; }
        }
        check(ok, "all forces & energies finite for r ∈ [2.5, 10.0] Å");
    }

    // ------------------------------------------------------------------
    // 1.3b  Physical sanity — Ar dimer at equilibrium
    // ------------------------------------------------------------------
    std::printf("\n--- 1.3b Physical Sanity (Ar Dimer) ---\n");
    {
        // UFF Ar: ε = 0.185 kcal/mol, σ = 3.868 Å (from uff_params.hpp)
        // r_min = 2^(1/6) * σ ≈ 4.342 Å
        // At r_min: U ≈ −ε, F ≈ 0
        // We do NOT hardcode the exact UFF values here; instead we search.

        double best_r = 0, best_U = 1e30;
        for (double r = 3.0; r < 6.0; r += 0.001) {
            State s = make_ar_dimer(r);
            model->eval(s, mp);
            if (s.E.total() < best_U) { best_U = s.E.total(); best_r = r; }
        }

        State eq = make_ar_dimer(best_r);
        model->eval(eq, mp);
        double Fmag = std::sqrt(eq.F[0].x*eq.F[0].x + eq.F[0].y*eq.F[0].y + eq.F[0].z*eq.F[0].z);

        std::printf("    r_min = %.4f Å   U_min = %.8f kcal/mol   |F| = %.2e\n",
                    best_r, best_U, Fmag);

        check(best_U < 0,   "U(r_min) is negative (bound state)");
        check(Fmag < 0.01,  "|F(r_min)| < 0.01 kcal/mol/Å (near zero)");

        // Force should be repulsive at r < r_min and attractive at r > r_min
        State inner = make_ar_dimer(best_r - 0.5);
        model->eval(inner, mp);
        check(inner.F[0].x < 0,
              "force is repulsive (−x) at r < r_min");

        State outer = make_ar_dimer(best_r + 0.5);
        model->eval(outer, mp);
        check(outer.F[0].x > 0,
              "force is attractive (+x) at r > r_min");
    }

    // ------------------------------------------------------------------
    // 1.3c  Force–energy consistency (finite-difference check)
    // ------------------------------------------------------------------
    std::printf("\n--- 1.3c Force-Energy Consistency ---\n");
    {
        double r0 = 4.5;
        double dr = 1e-5;

        // Perturb atom 0 in +x directly (not separation) so the numeric
        // derivative gives F_x = -dU/dx_0 with the correct sign.
        State s_plus  = make_ar_dimer(r0);
        s_plus.X[0].x += dr;
        model->eval(s_plus, mp);

        State s_minus = make_ar_dimer(r0);
        s_minus.X[0].x -= dr;
        model->eval(s_minus, mp);

        State s_mid   = make_ar_dimer(r0);
        model->eval(s_mid, mp);

        double Fx_numeric  = -(s_plus.E.total() - s_minus.E.total()) / (2.0 * dr);
        double Fx_analytic = s_mid.F[0].x;
        double err = std::abs(Fx_numeric - Fx_analytic);

        std::printf("    r = %.2f Å:  F_analytic = %.8f  F_numeric = %.8f  |err| = %.2e\n",
                    r0, Fx_analytic, Fx_numeric, err);

        check(err < 1e-4,
              "F_x matches -dU/dx within 1e-4 kcal/mol/Å");
    }

    // ------------------------------------------------------------------
    // Summary
    // ------------------------------------------------------------------
    std::printf("\n=============================================================\n");
    std::printf("  Phase 1 Audit:  %d passed,  %d failed\n", g_pass, g_fail);
    std::printf("=============================================================\n\n");

    return g_fail > 0 ? 1 : 0;
}
