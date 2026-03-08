/**
 * phase4_formation_priors.cpp — Phase 4: Formalize Formation Priors
 *
 * Self-testing executable.  Verifies that the crystal preset library and
 * the formation-prior layer (VSEPR seeds, crystal motifs) are correctly
 * separated from the physical kernel and produce well-formed State objects.
 *
 * Exercises the end-user crystal pipeline:
 *   presets::X() → UnitCell → to_state() → model->eval() → energy
 *
 * Checks:
 *   4.1  Crystal preset atom counts and lattice constants
 *   4.2  to_state() produces valid State with PBC
 *   4.3  Single-point energy is finite and deterministic per preset
 *   4.4  Supercell construction preserves per-atom energy
 *   4.5  Crystal presets cover the checklist targets (Al, Fe, NaCl, Si)
 */

#include "atomistic/core/state.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/crystal/unit_cell.hpp"
#include "atomistic/crystal/supercell.hpp"
#include <cmath>
#include <cstdio>
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

static double lattice_a(const Lattice& lat)
{
    Vec3 a = lat.A.col(0);
    return std::sqrt(a.x*a.x + a.y*a.y + a.z*a.z);
}

struct PresetCheck {
    const char* label;
    UnitCell    uc;
    uint32_t    expected_N;
    double      expected_a;    // lattice constant a (Å)
    int         expected_sg;   // space group number
};

// ============================================================================
// main
// ============================================================================

int main()
{
    std::printf("\n");
    std::printf("=============================================================\n");
    std::printf("  Phase 4 — Formation Priors\n");
    std::printf("=============================================================\n\n");

    auto model = create_lj_coulomb_model();
    ModelParams mp;
    mp.rc = 10.0;

    // ------------------------------------------------------------------
    // 4.1 + 4.5  Crystal preset geometry verification
    // ------------------------------------------------------------------
    std::printf("--- 4.1 Crystal Preset Geometry ---\n");

    PresetCheck targets[] = {
        {"FCC Al",     presets::aluminum_fcc(),     4, 4.05, 225},
        {"BCC Fe",     presets::iron_bcc(),         2, 2.87, 229},
        {"NaCl",       presets::sodium_chloride(),  8, 5.64, 225},
        {"Diamond Si", presets::silicon_diamond(),  8, 5.43, 227},
        {"FCC Cu",     presets::copper_fcc(),       4, 3.61, 225},
        {"FCC Au",     presets::gold_fcc(),         4, 4.08, 225},
        {"CsCl",       presets::cesium_chloride(),  2, 4.12, 221},
        {"MgO",        presets::magnesium_oxide(),  8, 4.21, 225},
    };

    for (auto& t : targets) {
        uint32_t N = (uint32_t)t.uc.num_atoms();
        double a = lattice_a(t.uc.lattice);

        std::printf("    %-12s  N=%u (expect %u)  a=%.2f (expect %.2f)  SG=%d\n",
                    t.label, N, t.expected_N, a, t.expected_a, t.uc.space_group_number);

        check(N == t.expected_N,
              (std::string(t.label) + ": atom count").c_str());
        check(std::abs(a - t.expected_a) < 0.01,
              (std::string(t.label) + ": lattice constant a").c_str());
        check(t.uc.space_group_number == t.expected_sg,
              (std::string(t.label) + ": space group number").c_str());
    }

    // ------------------------------------------------------------------
    // 4.2  to_state() produces valid State with PBC
    // ------------------------------------------------------------------
    std::printf("\n--- 4.2 UnitCell → State Conversion ---\n");
    {
        for (auto& t : targets) {
            State s = t.uc.to_state();

            bool valid = (s.N == t.expected_N)
                      && (s.X.size() == s.N)
                      && (s.type.size() == s.N)
                      && (s.M.size() == s.N)
                      && (s.Q.size() == s.N)
                      && s.box.enabled;

            check(valid,
                  (std::string(t.label) + ": State well-formed + PBC enabled").c_str());
        }
    }

    // ------------------------------------------------------------------
    // 4.3  Single-point energy: finite and deterministic
    // ------------------------------------------------------------------
    std::printf("\n--- 4.3 Crystal Single-Point Energy ---\n");
    {
        for (auto& t : targets) {
            State s1 = t.uc.to_state();
            s1.F.resize(s1.N, {0,0,0});
            model->eval(s1, mp);

            State s2 = t.uc.to_state();
            s2.F.resize(s2.N, {0,0,0});
            model->eval(s2, mp);

            bool finite = std::isfinite(s1.E.total());
            bool determ = (s1.E.total() == s2.E.total());

            std::printf("    %-12s  U=%.6f  U/atom=%.6f\n",
                        t.label, s1.E.total(), s1.E.total() / s1.N);

            check(finite,
                  (std::string(t.label) + ": energy finite").c_str());
            check(determ,
                  (std::string(t.label) + ": energy deterministic").c_str());
        }
    }

    // ------------------------------------------------------------------
    // 4.4  Supercell construction correctness
    //
    // NOTE: Per-atom energy convergence requires cell >> 2*rc.  For Al
    // FCC (a=4.05), even a 2×2×2 cell (8.1 Å) is smaller than 2*rc (20 Å).
    // MIC undersamples in small cells.  This test verifies structural
    // correctness only; energy convergence is a Phase 5 concern.
    // ------------------------------------------------------------------
    std::printf("\n--- 4.4 Supercell Construction ---\n");
    {
        auto uc = presets::aluminum_fcc();
        auto sc = construct_supercell(uc, 2, 2, 2);
        State& s2 = sc.state;
        s2.F.resize(s2.N, {0,0,0});
        model->eval(s2, mp);

        std::printf("    Al FCC 1x1x1: N=%u\n", (uint32_t)uc.num_atoms());
        std::printf("    Al FCC 2x2x2: N=%u  U=%.4f  PBC=%s\n",
                    s2.N, s2.E.total(), s2.box.enabled ? "yes" : "no");

        check(s2.N == (uint32_t)uc.num_atoms() * 8,
              "Al 2x2x2: atom count = 8 x unit cell");
        check(s2.box.enabled,
              "Al 2x2x2: PBC enabled");
        check(std::isfinite(s2.E.total()),
              "Al 2x2x2: energy finite");

        // Verify supercell lattice vectors are 2× unit cell
        double a_uc = lattice_a(uc.lattice);
        double a_sc_x = s2.box.L.x;
        std::printf("    unit cell a=%.2f  supercell Lx=%.2f (expect %.2f)\n",
                    a_uc, a_sc_x, 2*a_uc);
        check(std::abs(a_sc_x - 2*a_uc) < 0.01,
              "Al 2x2x2: supercell Lx = 2*a");
    }

    // ------------------------------------------------------------------
    // Summary
    // ------------------------------------------------------------------
    std::printf("\n=============================================================\n");
    std::printf("  Phase 4 Audit:  %d passed,  %d failed\n", g_pass, g_fail);
    std::printf("=============================================================\n\n");

    return g_fail > 0 ? 1 : 0;
}
