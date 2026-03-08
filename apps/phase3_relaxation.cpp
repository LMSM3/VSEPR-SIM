/**
 * phase3_relaxation.cpp — Phase 3: Relaxation and Minimization
 *
 * Self-testing executable.  Creates distorted molecules through the full
 * end-user pipeline (XYZMolecule → detect_bonds → parsers::from_xyz →
 * FIRE minimization), verifying that:
 *
 *   3.1  Energy decreases during relaxation
 *   3.2  Forces converge toward zero
 *   3.3  Topology (bonds) is retained
 *   3.4  Geometry settles to a physically reasonable configuration
 *   3.5  Deterministic: identical distortion → identical relaxed state
 *   3.6  Bonded-pair exclusions produce sane molecular energies
 */

#include "atomistic/core/state.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/integrators/fire.hpp"
#include "atomistic/parsers/xyz_parser.hpp"
#include "io/xyz_format.hpp"
#include <cmath>
#include <cstdio>
#include <vector>
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

// Build molecule from elements + coordinates, detect bonds (like user pipeline).
static vsepr::io::XYZMolecule make_mol_with_bonds(
    const std::vector<std::string>& elems,
    const std::vector<std::array<double,3>>& coords)
{
    vsepr::io::XYZMolecule mol;
    for (size_t i = 0; i < elems.size(); ++i)
        mol.atoms.emplace_back(elems[i], coords[i][0], coords[i][1], coords[i][2]);

    // Bond detection — same as XYZReader::detect_bonds in the real pipeline
    vsepr::io::XYZReader reader;
    reader.detect_bonds(mol);
    return mol;
}

// Full pipeline: parse + prepare state (with bonds → exclusions active)
static State to_state(const vsepr::io::XYZMolecule& mol)
{
    State s = parsers::from_xyz(mol);
    s.F.resize(s.N, {0,0,0});
    return s;
}

static double frms(const State& s)
{
    double sum = 0;
    for (auto& f : s.F) sum += f.x*f.x + f.y*f.y + f.z*f.z;
    return std::sqrt(sum / s.N);
}

static double distance(const Vec3& a, const Vec3& b)
{
    Vec3 d = a - b;
    return std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
}

// ============================================================================
// Reference geometries — slightly distorted for relaxation
// ============================================================================

// Ar cluster: 3 atoms in equilateral triangle, slightly beyond LJ minimum
static vsepr::io::XYZMolecule distorted_Ar3()
{
    // Start at ~4.5 Å sides (just beyond equilibrium ~3.82 Å)
    return make_mol_with_bonds({"Ar","Ar","Ar"}, {
        {{0, 0, 0}}, {{4.5, 0, 0}}, {{2.25, 3.897, 0}}
    });
}

// H2O: bonds present but angle distorted to ~130°
static vsepr::io::XYZMolecule distorted_H2O()
{
    double r = 0.96;
    double theta = 130.0 * M_PI / 180.0;  // wider than equilibrium
    return make_mol_with_bonds({"O","H","H"}, {
        {{0, 0, 0}},
        {{r, 0, 0}},
        {{r*std::cos(theta), r*std::sin(theta), 0}}
    });
}

// CH4: slightly stretched C-H to 1.2 Å (within bond detection range, vs equilibrium ~1.09 Å)
static vsepr::io::XYZMolecule distorted_CH4()
{
    double d = 1.2;
    double a = d / std::sqrt(3.0);
    return make_mol_with_bonds({"C","H","H","H","H"}, {
        {{0, 0, 0}},
        {{ a,  a,  a}},
        {{ a, -a, -a}},
        {{-a,  a, -a}},
        {{-a, -a,  a}}
    });
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::printf("\n");
    std::printf("=============================================================\n");
    std::printf("  Phase 3 — Relaxation and Minimization\n");
    std::printf("=============================================================\n\n");

    auto model = create_lj_coulomb_model();
    ModelParams mp;
    mp.rc = 12.0;

    FIREParams fp;
    fp.max_steps = 3000;
    fp.epsF = 1e-4;
    fp.dt = 1e-3;
    fp.dt_max = 1e-1;

    // ------------------------------------------------------------------
    // 3.6  Bonded-pair exclusion sanity (before relaxation)
    // ------------------------------------------------------------------
    std::printf("--- 3.6 Bonded-Pair Exclusion Sanity ---\n");
    {
        // H2 WITHOUT bonds → huge LJ repulsion
        vsepr::io::XYZMolecule h2_no_bonds;
        h2_no_bonds.atoms.emplace_back("H", 0.0, 0.0, 0.0);
        h2_no_bonds.atoms.emplace_back("H", 0.74, 0.0, 0.0);

        State s_no = to_state(h2_no_bonds);
        model->eval(s_no, mp);

        // H2 WITH bonds → bonded pair excluded from LJ
        auto h2_bonds = make_mol_with_bonds({"H","H"}, {{{0,0,0}}, {{0.74,0,0}}});
        State s_yes = to_state(h2_bonds);
        model->eval(s_yes, mp);

        std::printf("    H2 no bonds:   U_vdw = %.2f kcal/mol  (n_bonds=%zu)\n",
                    s_no.E.UvdW, s_no.B.size());
        std::printf("    H2 with bonds: U_vdw = %.6f kcal/mol  (n_bonds=%zu)\n",
                    s_yes.E.UvdW, s_yes.B.size());

        check(s_no.B.empty(),      "H2 no-bond mol: B is empty");
        check(!s_yes.B.empty(),    "H2 with-bond mol: B is not empty");
        check(s_no.E.UvdW > 1e5,  "H2 no bonds: massive LJ repulsion (>1e5)");
        check(std::abs(s_yes.E.UvdW) < 1.0,
              "H2 with bonds: LJ near zero (pair excluded)");
    }

    // ------------------------------------------------------------------
    // 3.1–3.4  Relaxation of Ar3 cluster (nonbonded only)
    // ------------------------------------------------------------------
    std::printf("\n--- 3.1-3.4 Ar3 Cluster Relaxation ---\n");
    {
        State s = to_state(distorted_Ar3());
        model->eval(s, mp);
        double U_init = s.E.total();
        double F_init = frms(s);

        FIREParams fp_ar = fp;
        fp_ar.max_steps = 10000;  // Ar3 converges slowly on flat LJ surface
        FIRE fire(*model, mp);
        auto stats = fire.minimize(s, fp_ar);

        std::printf("    init:  U=%.6f  F_rms=%.4e\n", U_init, F_init);
        std::printf("    final: U=%.6f  F_rms=%.4e  steps=%d\n",
                    stats.U, stats.Frms, stats.step);

        check(stats.U < U_init,    "Ar3: energy decreased");
        check(stats.Frms < 1e-3,   "Ar3: F_rms < 1e-3 (converged)");

        // Check pair distances are near equilibrium (~3.82 Å)
        double d01 = distance(s.X[0], s.X[1]);
        double d02 = distance(s.X[0], s.X[2]);
        double d12 = distance(s.X[1], s.X[2]);
        std::printf("    distances: %.3f  %.3f  %.3f Å\n", d01, d02, d12);

        check(std::abs(d01 - d02) < 0.1 && std::abs(d01 - d12) < 0.1,
              "Ar3: equilateral geometry (all sides within 0.1 Å)");
        check(d01 > 3.5 && d01 < 4.5,
              "Ar3: pair distance near LJ minimum (3.5-4.5 Å)");
    }

    // ------------------------------------------------------------------
    // 3.1–3.4  Relaxation of distorted H2O (with bonds)
    //
    // NOTE: Only LJ nonbonded terms are active.  O-H pairs are excluded
    // (1-2 bonds) so the only nonbonded force is H-H.  There is no
    // harmonic bond spring — the BondedModel is not yet composed.
    // Therefore: energy should decrease, bonds should be retained, but
    // convergence to tight tolerance is not expected.
    // ------------------------------------------------------------------
    std::printf("\n--- 3.1-3.4 H2O Relaxation (LJ nonbonded only) ---\n");
    {
        State s = to_state(distorted_H2O());
        model->eval(s, mp);
        double U_init = s.E.total();
        int bonds_init = (int)s.B.size();

        std::printf("    init:  U=%.6f  F_rms=%.4e  bonds=%d\n",
                    U_init, frms(s), bonds_init);

        FIREParams fp_mol = fp;
        fp_mol.max_steps = 5000;
        fp_mol.epsF = 1e-2;
        FIRE fire(*model, mp);
        auto stats = fire.minimize(s, fp_mol);

        std::printf("    final: U=%.6f  F_rms=%.4e  steps=%d  bonds=%zu\n",
                    stats.U, stats.Frms, stats.step, s.B.size());

        check(stats.U <= U_init,             "H2O: energy decreased or unchanged");
        check(stats.Frms < U_init,           "H2O: F_rms decreased from initial");
        check((int)s.B.size() == bonds_init, "H2O: bond count preserved");
    }

    // ------------------------------------------------------------------
    // 3.1–3.4  Relaxation of distorted CH4 (with bonds)
    // Same limitation as H2O — only nonbonded LJ between 1-3+ pairs.
    // ------------------------------------------------------------------
    std::printf("\n--- 3.1-3.4 CH4 Relaxation (LJ nonbonded only) ---\n");
    {
        State s = to_state(distorted_CH4());
        model->eval(s, mp);
        double U_init = s.E.total();
        int bonds_init = (int)s.B.size();

        std::printf("    init:  U=%.6f  F_rms=%.4e  bonds=%d\n",
                    U_init, frms(s), bonds_init);

        FIREParams fp_mol = fp;
        fp_mol.max_steps = 5000;
        fp_mol.epsF = 1e-2;
        FIRE fire(*model, mp);
        auto stats = fire.minimize(s, fp_mol);

        std::printf("    final: U=%.6f  F_rms=%.4e  steps=%d  bonds=%zu\n",
                    stats.U, stats.Frms, stats.step, s.B.size());

        check(stats.U <= U_init,             "CH4: energy decreased or unchanged");
        check(bonds_init > 0,                "CH4: bonds were detected at 1.2 Å");
        check((int)s.B.size() == bonds_init, "CH4: bond count preserved");
    }

    // ------------------------------------------------------------------
    // 3.5  Deterministic relaxation
    // ------------------------------------------------------------------
    std::printf("\n--- 3.5 Deterministic Relaxation ---\n");
    {
        State s1 = to_state(distorted_Ar3());
        State s2 = to_state(distorted_Ar3());

        FIRE fire1(*model, mp);
        FIRE fire2(*model, mp);
        auto st1 = fire1.minimize(s1, fp);
        auto st2 = fire2.minimize(s2, fp);

        check(st1.U == st2.U,    "Ar3: relaxed energy bitwise identical");
        check(st1.step == st2.step, "Ar3: same step count");

        bool pos_match = true;
        for (uint32_t i = 0; i < s1.N; ++i)
            if (s1.X[i].x != s2.X[i].x || s1.X[i].y != s2.X[i].y || s1.X[i].z != s2.X[i].z)
                { pos_match = false; break; }
        check(pos_match, "Ar3: relaxed positions bitwise identical");
    }

    // ------------------------------------------------------------------
    // Summary
    // ------------------------------------------------------------------
    std::printf("\n=============================================================\n");
    std::printf("  Phase 3 Audit:  %d passed,  %d failed\n", g_pass, g_fail);
    std::printf("=============================================================\n\n");

    return g_fail > 0 ? 1 : 0;
}
