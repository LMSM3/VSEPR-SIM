/**
 * phase5_crystal_validation.cpp — Phase 5: Crystal Structure Validation
 *
 * Self-testing executable.  Emits crystal presets, evaluates them under
 * PBC with the LJ model, relaxes with FIRE, and verifies that:
 *
 *   5.1  Nearest-neighbor distances match crystallographic expectations
 *   5.2  Coordination numbers are correct for each motif
 *   5.3  Relaxation preserves motif (distances remain within tolerance)
 *   5.4  Supercell 2×2×2 vs 3×3×3 per-atom energy converges
 *   5.5  All results are deterministic
 */

#include "atomistic/core/state.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/integrators/fire.hpp"
#include "atomistic/crystal/unit_cell.hpp"
#include "atomistic/crystal/supercell.hpp"
#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <string>

using namespace atomistic;
using namespace atomistic::crystal;

// ============================================================================
// Helpers
// ============================================================================

static int g_pass = 0, g_fail = 0;

static void check(bool ok, const char* name)
{
    if (ok) { std::printf("  [PASS]  %s\n", name); ++g_pass; }
    else    { std::printf("  [FAIL]  %s\n", name); ++g_fail; }
}

// Minimum-image distance for PBC
static double mic_dist(const Vec3& a, const Vec3& b, const BoxPBC& box)
{
    Vec3 d = box.delta(a, b);
    return std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
}

// Find nearest-neighbor distance in a State under PBC
static double nn_distance(const State& s)
{
    double dmin = 1e30;
    for (uint32_t i = 0; i < s.N; ++i)
        for (uint32_t j = i+1; j < s.N; ++j) {
            double d = mic_dist(s.X[i], s.X[j], s.box);
            if (d < dmin) dmin = d;
        }
    return dmin;
}

// Count neighbors within cutoff for a specific atom
static int coordination(const State& s, uint32_t idx, double rcut)
{
    int cn = 0;
    for (uint32_t j = 0; j < s.N; ++j) {
        if (j == idx) continue;
        if (mic_dist(s.X[idx], s.X[j], s.box) < rcut) ++cn;
    }
    return cn;
}

static double frms(const State& s)
{
    double sum = 0;
    for (auto& f : s.F) sum += f.x*f.x + f.y*f.y + f.z*f.z;
    return std::sqrt(sum / s.N);
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::printf("\n");
    std::printf("=============================================================\n");
    std::printf("  Phase 5 — Crystal Structure Validation\n");
    std::printf("=============================================================\n\n");

    auto model = create_lj_coulomb_model();
    ModelParams mp;
    mp.rc = 10.0;

    // ------------------------------------------------------------------
    // 5.1 + 5.2  Nearest-neighbor distances and coordination numbers
    //
    // Use supercells large enough that the unit cell is replicated
    // sufficiently for nn detection under MIC.
    // ------------------------------------------------------------------
    std::printf("--- 5.1-5.2 Nearest-Neighbor Distances & Coordination ---\n");
    {
        struct CrystalCase {
            const char* label;
            UnitCell uc;
            int sc_n;          // supercell factor
            double nn_expect;  // nearest-neighbor distance (Å)
            double nn_cut;     // cutoff for coordination counting
            int cn_expect;     // expected coordination number
        };

        // FCC nn = a/sqrt(2), CN=12.  BCC nn = a*sqrt(3)/2, CN=8.
        // NaCl nn = a/2, CN=6 (unlike neighbors).  Diamond nn = a*sqrt(3)/4, CN=4.
        CrystalCase cases[] = {
            {"FCC Al", presets::aluminum_fcc(), 3, 4.05/std::sqrt(2.0), 3.0, 12},
            {"BCC Fe", presets::iron_bcc(),     4, 2.87*std::sqrt(3.0)/2.0, 2.6, 8},
            {"NaCl",   presets::sodium_chloride(), 2, 5.64/2.0, 3.0, 6},
            {"Diamond Si", presets::silicon_diamond(), 2, 5.43*std::sqrt(3.0)/4.0, 2.5, 4},
        };

        for (auto& c : cases) {
            auto sc = construct_supercell(c.uc, c.sc_n, c.sc_n, c.sc_n);
            State& s = sc.state;

            double nn = nn_distance(s);
            int cn = coordination(s, 0, c.nn_cut);

            std::printf("    %-12s nn=%.3f (expect %.3f)  CN=%d (expect %d)  N=%u\n",
                        c.label, nn, c.nn_expect, cn, c.cn_expect, s.N);

            check(std::abs(nn - c.nn_expect) < 0.02,
                  (std::string(c.label) + ": nn distance").c_str());
            check(cn == c.cn_expect,
                  (std::string(c.label) + ": coordination number").c_str());
        }
    }

    // ------------------------------------------------------------------
    // 5.3  Relaxation preserves motif (BCC Fe under PBC)
    //
    // BCC Fe has a=2.87, nn=2.485 Å.  Use a 4×4×4 supercell (128 atoms,
    // L=11.48 Å > rc) so PBC energy is well-defined.  Relax with FIRE
    // and verify nn distance is preserved.
    // ------------------------------------------------------------------
    std::printf("\n--- 5.3 Crystal Relaxation (BCC Fe 4x4x4) ---\n");
    {
        auto uc = presets::iron_bcc();
        auto sc = construct_supercell(uc, 4, 4, 4);
        State& s = sc.state;
        s.F.resize(s.N, {0,0,0});
        model->eval(s, mp);

        double nn_before = nn_distance(s);
        double U_before  = s.E.total();

        FIREParams fp;
        fp.max_steps = 5000;
        fp.epsF = 1e-4;
        fp.dt = 1e-3;
        fp.dt_max = 1e-1;

        FIRE fire(*model, mp);
        auto stats = fire.minimize(s, fp);

        double nn_after = nn_distance(s);

        std::printf("    before: U=%.4f  nn=%.4f  N=%u\n", U_before, nn_before, s.N);
        std::printf("    after:  U=%.4f  nn=%.4f  F_rms=%.4e  steps=%d\n",
                    stats.U, nn_after, stats.Frms, stats.step);

        check(stats.U <= U_before + 1e-6,
              "BCC Fe: energy decreased or unchanged");
        check(std::abs(nn_after - nn_before) < 0.1,
              "BCC Fe: nn distance preserved within 0.1 Å");
        check(stats.Frms < 1e-2,
              "BCC Fe: F_rms converged < 0.01");
    }

    // ------------------------------------------------------------------
    // 5.4  Per-atom energy convergence (FCC Cu: 3×3×3 vs 4×4×4)
    //
    // Cu a=3.61, so 3×3×3 → L=10.83 > rc, 4×4×4 → L=14.44.
    // Per-atom energy should converge as both cells are > rc.
    // ------------------------------------------------------------------
    std::printf("\n--- 5.4 Per-Atom Energy Convergence (FCC Cu) ---\n");
    {
        auto uc = presets::copper_fcc();

        auto sc3 = construct_supercell(uc, 3, 3, 3);
        sc3.state.F.resize(sc3.state.N, {0,0,0});
        model->eval(sc3.state, mp);
        double u3 = sc3.state.E.total() / sc3.state.N;

        auto sc4 = construct_supercell(uc, 4, 4, 4);
        sc4.state.F.resize(sc4.state.N, {0,0,0});
        model->eval(sc4.state, mp);
        double u4 = sc4.state.E.total() / sc4.state.N;

        double diff = std::abs(u4 - u3);
        std::printf("    3x3x3: N=%u  U/atom=%.6f\n", sc3.state.N, u3);
        std::printf("    4x4x4: N=%u  U/atom=%.6f\n", sc4.state.N, u4);
        std::printf("    |diff| = %.6f kcal/mol/atom\n", diff);

        check(diff < std::abs(u3) * 0.05,
              "FCC Cu: per-atom energy converges within 5% (3x3x3 vs 4x4x4)");
    }

    // ------------------------------------------------------------------
    // 5.5  Deterministic crystal evaluation
    // ------------------------------------------------------------------
    std::printf("\n--- 5.5 Deterministic Crystal Evaluation ---\n");
    {
        auto uc = presets::iron_bcc();
        auto sc1 = construct_supercell(uc, 3, 3, 3);
        sc1.state.F.resize(sc1.state.N, {0,0,0});
        model->eval(sc1.state, mp);

        auto sc2 = construct_supercell(uc, 3, 3, 3);
        sc2.state.F.resize(sc2.state.N, {0,0,0});
        model->eval(sc2.state, mp);

        check(sc1.state.E.total() == sc2.state.E.total(),
              "BCC Fe 3x3x3: energy bitwise deterministic");
    }

    // ------------------------------------------------------------------
    // Summary
    // ------------------------------------------------------------------
    std::printf("\n=============================================================\n");
    std::printf("  Phase 5 Audit:  %d passed,  %d failed\n", g_pass, g_fail);
    std::printf("=============================================================\n\n");

    return g_fail > 0 ? 1 : 0;
}
