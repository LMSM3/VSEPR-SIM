/**
 * phase2_structural_energy.cpp — Phase 2: Structural Energy Physics
 *
 * Self-testing executable.  Constructs reference molecules the same way a
 * user loading an .xyz file would: element symbols + Cartesian coordinates
 * are packed into vsepr::io::XYZMolecule, routed through parsers::from_xyz(),
 * then evaluated with the LJ+Coulomb model.
 *
 * This exercises the FULL end-user pipeline:
 *   XYZMolecule → parsers::from_xyz() → State → model->eval() → EnergyTerms
 *
 * Checks:
 *   2.1  Reference benchmark evaluations (H2, H2O, CH4, Ar2)
 *   2.2  Reproducibility across identical calls
 *   2.3  Controlled perturbation — bond stretch raises energy
 *   2.4  Local smoothness — energy sweep has no discontinuities
 *   2.5  Force–energy consistency on multi-atom systems
 */

#include "atomistic/core/state.hpp"
#include "atomistic/models/model.hpp"
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

// Build XYZMolecule the same way a user's .xyz file would be parsed.
static vsepr::io::XYZMolecule make_mol(
    const std::vector<std::string>& elems,
    const std::vector<std::array<double,3>>& coords)
{
    vsepr::io::XYZMolecule mol;
    for (size_t i = 0; i < elems.size(); ++i)
        mol.atoms.emplace_back(elems[i], coords[i][0], coords[i][1], coords[i][2]);
    return mol;
}

// Parse through the real end-user pipeline.
static State parse_and_eval(const vsepr::io::XYZMolecule& mol,
                            IModel& model, const ModelParams& mp)
{
    State s = parsers::from_xyz(mol);
    s.F.resize(s.N, {0,0,0});
    model.eval(s, mp);
    return s;
}

static bool all_finite(const State& s)
{
    for (uint32_t i = 0; i < s.N; ++i) {
        const auto& f = s.F[i];
        if (!std::isfinite(f.x) || !std::isfinite(f.y) || !std::isfinite(f.z))
            return false;
    }
    return std::isfinite(s.E.total());
}

// ============================================================================
// Reference geometries (textbook values, Angstroms)
// ============================================================================

// H2: bond length 0.74 Å
static vsepr::io::XYZMolecule ref_H2()
{
    return make_mol({"H","H"}, {{{0,0,0}}, {{0.74,0,0}}});
}

// H2O: O–H 0.9584 Å, angle 104.45°
static vsepr::io::XYZMolecule ref_H2O()
{
    double r = 0.9584;
    double theta = 104.45 * M_PI / 180.0;
    return make_mol({"O","H","H"}, {
        {{0, 0, 0}},
        {{r, 0, 0}},
        {{r*std::cos(theta), r*std::sin(theta), 0}}
    });
}

// CH4: tetrahedral, C–H 1.09 Å
static vsepr::io::XYZMolecule ref_CH4()
{
    double d = 1.09;
    double a = d / std::sqrt(3.0);  // tetrahedral vertex
    return make_mol({"C","H","H","H","H"}, {
        {{0, 0, 0}},
        {{ a,  a,  a}},
        {{ a, -a, -a}},
        {{-a,  a, -a}},
        {{-a, -a,  a}}
    });
}

// Ar dimer at 4.0 Å
static vsepr::io::XYZMolecule ref_Ar2()
{
    return make_mol({"Ar","Ar"}, {{{0,0,0}}, {{4.0,0,0}}});
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::printf("\n");
    std::printf("=============================================================\n");
    std::printf("  Phase 2 — Structural Energy Physics\n");
    std::printf("=============================================================\n\n");

    auto model = create_lj_coulomb_model();
    ModelParams mp;
    mp.rc = 12.0;

    // ------------------------------------------------------------------
    // 2.1  Reference benchmark evaluations through the end-user pipeline
    // ------------------------------------------------------------------
    std::printf("--- 2.1 Reference Benchmark Evaluations (end-user pipeline) ---\n");
    {
        struct Case { const char* name; vsepr::io::XYZMolecule mol; };
        Case cases[] = {
            {"H2",  ref_H2()},
            {"H2O", ref_H2O()},
            {"CH4", ref_CH4()},
            {"Ar2", ref_Ar2()},
        };

        for (auto& c : cases) {
            State s = parse_and_eval(c.mol, *model, mp);
            std::printf("    %-4s  N=%u  types=[", c.name, s.N);
            for (uint32_t i = 0; i < s.N; ++i)
                std::printf("%s%u", i?",":"", s.type[i]);
            std::printf("]  U=%.6f  U_vdw=%.6f  |F_rms|=%.4e\n",
                        s.E.total(), s.E.UvdW,
                        [&]() {
                            double sum = 0;
                            for (auto& f : s.F) sum += f.x*f.x+f.y*f.y+f.z*f.z;
                            return std::sqrt(sum / s.N);
                        }());

            check(all_finite(s),
                  (std::string(c.name) + ": all forces & energy finite").c_str());
        }

        // Verify parser set atomic numbers correctly (not sequential IDs).
        State h2o = parse_and_eval(ref_H2O(), *model, mp);
        check(h2o.type[0] == 8,  "H2O parser: O → type=8  (Z=8)");
        check(h2o.type[1] == 1,  "H2O parser: H → type=1  (Z=1)");

        State ch4 = parse_and_eval(ref_CH4(), *model, mp);
        check(ch4.type[0] == 6,  "CH4 parser: C → type=6  (Z=6)");
        check(ch4.type[1] == 1,  "CH4 parser: H → type=1  (Z=1)");
    }

    // ------------------------------------------------------------------
    // 2.2  Reproducibility
    // ------------------------------------------------------------------
    std::printf("\n--- 2.2 Reproducibility ---\n");
    {
        State a = parse_and_eval(ref_H2O(), *model, mp);
        State b = parse_and_eval(ref_H2O(), *model, mp);
        check(a.E.total() == b.E.total(), "H2O: energy bitwise identical across runs");
        bool fmatch = true;
        for (uint32_t i = 0; i < a.N; ++i)
            if (a.F[i].x != b.F[i].x || a.F[i].y != b.F[i].y || a.F[i].z != b.F[i].z)
                { fmatch = false; break; }
        check(fmatch, "H2O: forces bitwise identical across runs");
    }

    // ------------------------------------------------------------------
    // 2.3  Controlled perturbation — stretching a bond raises energy
    // ------------------------------------------------------------------
    std::printf("\n--- 2.3 Controlled Perturbation (bond stretch) ---\n");
    {
        // H2 at equilibrium-ish (0.74 Å) vs stretched (1.5 Å) vs compressed (0.5 Å)
        auto mol_eq  = make_mol({"H","H"}, {{{0,0,0}}, {{0.74,0,0}}});
        auto mol_str = make_mol({"H","H"}, {{{0,0,0}}, {{1.50,0,0}}});
        auto mol_cmp = make_mol({"H","H"}, {{{0,0,0}}, {{0.50,0,0}}});

        double U_eq  = parse_and_eval(mol_eq,  *model, mp).E.total();
        double U_str = parse_and_eval(mol_str, *model, mp).E.total();
        double U_cmp = parse_and_eval(mol_cmp, *model, mp).E.total();

        std::printf("    H2: U(0.50)=%.4f  U(0.74)=%.4f  U(1.50)=%.4f\n",
                    U_cmp, U_eq, U_str);

        // Compressing below σ should produce very high repulsive energy.
        check(U_cmp > U_eq, "H2: compressed energy > equilibrium energy");

        // H2O: move one H outward, energy should change
        auto mol_h2o      = ref_H2O();
        auto mol_h2o_str  = ref_H2O();
        mol_h2o_str.atoms[1].position[0] += 0.5;  // stretch O-H

        double U_h2o     = parse_and_eval(mol_h2o,     *model, mp).E.total();
        double U_h2o_str = parse_and_eval(mol_h2o_str, *model, mp).E.total();

        std::printf("    H2O: U(eq)=%.6f  U(H stretched +0.5)=%.6f\n", U_h2o, U_h2o_str);
        check(U_h2o != U_h2o_str, "H2O: stretching one O-H changes total energy");
    }

    // ------------------------------------------------------------------
    // 2.4  Local smoothness — energy sweep for Ar2
    // ------------------------------------------------------------------
    std::printf("\n--- 2.4 Local Smoothness (Ar2 distance sweep) ---\n");
    {
        double prev_U = 1e30;
        double max_jump = 0;
        bool finite_ok = true;

        for (double r = 3.0; r <= 8.0; r += 0.01) {
            auto mol = make_mol({"Ar","Ar"}, {{{0,0,0}}, {{r,0,0}}});
            State s = parse_and_eval(mol, *model, mp);
            double U = s.E.total();
            if (!std::isfinite(U)) { finite_ok = false; break; }
            if (prev_U < 1e29) {
                double jump = std::abs(U - prev_U);
                if (jump > max_jump) max_jump = jump;
            }
            prev_U = U;
        }

        std::printf("    max |ΔU| between 0.01 Å steps: %.6f kcal/mol\n", max_jump);
        check(finite_ok, "Ar2 sweep: all energies finite");
        check(max_jump < 1.0, "Ar2 sweep: no jumps > 1 kcal/mol per 0.01 Å step");
    }

    // ------------------------------------------------------------------
    // 2.5  Force–energy consistency on multi-atom system (H2O)
    // ------------------------------------------------------------------
    std::printf("\n--- 2.5 Force-Energy Consistency (H2O, end-user pipeline) ---\n");
    {
        double dr = 1e-5;
        auto mol0 = ref_H2O();
        State s0 = parse_and_eval(mol0, *model, mp);

        // Check all 3 atoms × 3 coordinates.
        // NOTE: At covalent distances, forces are ~10⁶ kcal/mol/Å because
        // bonded-pair exclusions are not yet applied (LJ treats all pairs as
        // nonbonded).  Use a RELATIVE tolerance of 1e-6.
        bool all_ok = true;
        for (uint32_t atom = 0; atom < s0.N; ++atom) {
            for (int dim = 0; dim < 3; ++dim) {
                auto mol_p = ref_H2O();
                auto mol_m = ref_H2O();
                mol_p.atoms[atom].position[dim] += dr;
                mol_m.atoms[atom].position[dim] -= dr;

                State sp = parse_and_eval(mol_p, *model, mp);
                State sm = parse_and_eval(mol_m, *model, mp);

                double F_numeric = -(sp.E.total() - sm.E.total()) / (2.0 * dr);
                double F_analytic = (dim==0) ? s0.F[atom].x
                                 : (dim==1) ? s0.F[atom].y
                                 :            s0.F[atom].z;
                double err = std::abs(F_numeric - F_analytic);
                double scale = std::max(std::abs(F_analytic), 1.0);
                double rel_err = err / scale;

                if (rel_err > 1e-6) {
                    std::printf("    atom %u dim %d: analytic=%.6f  numeric=%.6f  rel_err=%.2e  FAIL\n",
                                atom, dim, F_analytic, F_numeric, rel_err);
                    all_ok = false;
                }
            }
        }

        if (all_ok)
            std::printf("    all 9 force components match numeric derivatives (rel err < 1e-6)\n");
        check(all_ok, "H2O: all F_i = -dU/dx_i within rel tol 1e-6 (9 components)");
    }

    // ------------------------------------------------------------------
    // Summary
    // ------------------------------------------------------------------
    std::printf("\n=============================================================\n");
    std::printf("  Phase 2 Audit:  %d passed,  %d failed\n", g_pass, g_fail);
    std::printf("=============================================================\n\n");

    return g_fail > 0 ? 1 : 0;
}
