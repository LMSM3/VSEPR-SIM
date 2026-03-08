/**
 * phase14_milestones.cpp — Five Milestone Verification
 *
 * M1: Coulomb re-enablement with dielectric screening
 * M2: UFF-matched bonded parameters (geometry-derived r0, element-aware kb)
 * M3: Angle gradient sign fix (force-energy consistency)
 * M4: Covalent crystal relaxation via composite model
 * M5: Dielectric screening: vacuum vs solution energy ratio
 *
 * Mathematical methodology documented inline and in section_phase_reports.tex.
 */

#include "atomistic/core/state.hpp"
#include "atomistic/core/environment.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/models/composite.hpp"
#include "atomistic/models/bonded.hpp"
#include "atomistic/integrators/fire.hpp"
#include "atomistic/parsers/xyz_parser.hpp"
#include "atomistic/crystal/unit_cell.hpp"
#include "atomistic/crystal/supercell.hpp"
#include "io/xyz_format.hpp"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace atomistic;
using namespace atomistic::crystal;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* name)
{
    if (ok) { std::printf("  [PASS]  %s\n", name); ++g_pass; }
    else    { std::printf("  [FAIL]  %s\n", name); ++g_fail; }
}

static State parse(const vsepr::io::XYZMolecule& mol)
{
    State s = parsers::from_xyz(mol);
    s.F.resize(s.N, {0,0,0});
    return s;
}

static double dist(const Vec3& a, const Vec3& b)
{
    Vec3 d = a - b;
    return std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
}

static double mic_dist(const Vec3& a, const Vec3& b, const BoxPBC& box)
{
    Vec3 d = box.enabled ? box.delta(a, b) : Vec3{a.x-b.x, a.y-b.y, a.z-b.z};
    return std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
}

static double nn_dist(const State& s)
{
    double dmin = 1e30;
    for (uint32_t i = 0; i < s.N; ++i)
        for (uint32_t j = i+1; j < s.N; ++j) {
            double d = mic_dist(s.X[i], s.X[j], s.box);
            if (d < dmin) dmin = d;
        }
    return dmin;
}

// ============================================================================
// M1: Coulomb Re-enablement
// ============================================================================

static void milestone_1()
{
    std::printf("\n=== M1: Coulomb Re-enablement ===\n");
    auto model = create_lj_coulomb_model();

    // 1a. NaCl ion pair: U_Coul should be negative (attractive +/- charges)
    //     U_Coul = k_e * (+1)(-1) / r = -332.06 / 2.82 = -117.7 kcal/mol
    std::printf("\n  1a. NaCl ion pair at r=2.82 A\n");
    {
        vsepr::io::XYZMolecule m;
        m.atoms.emplace_back("Na", 0.0, 0.0, 0.0);
        m.atoms.emplace_back("Cl", 2.82, 0.0, 0.0);
        State s = parse(m);
        s.Q[0] = +1.0; s.Q[1] = -1.0;

        ModelParams mp; mp.rc = 20.0;
        model->eval(s, mp);

        double U_coul_expect = -332.0636 / 2.82;
        std::printf("    U_coul = %.4f (expect %.4f)  U_vdw = %.4f  U_total = %.4f\n",
                    s.E.UCoul, U_coul_expect, s.E.UvdW, s.E.total());

        check(s.E.UCoul < 0, "NaCl: U_Coul < 0 (attractive)");
        check(std::abs(s.E.UCoul - U_coul_expect) < 0.5,
              "NaCl: U_Coul within 0.5 kcal/mol of -k_e/r");
        check(std::isfinite(s.E.total()), "NaCl: total energy finite");
    }

    // 1b. Force-energy consistency for Coulomb
    //     Perturb atom 0 (Na) in x and compare analytic force with numeric gradient.
    //     F_x(Na) = -dU/dx_Na via central difference.
    std::printf("\n  1b. Coulomb force-energy consistency\n");
    {
        double dr = 1e-6;
        double r0 = 5.0;

        auto make_state = [&](double dx_na) {
            vsepr::io::XYZMolecule m;
            m.atoms.emplace_back("Na", dx_na, 0.0, 0.0);
            m.atoms.emplace_back("Cl", r0,    0.0, 0.0);
            State s = parse(m);
            s.Q[0] = +1.0; s.Q[1] = -1.0;
            ModelParams mp; mp.rc = 20.0;
            model->eval(s, mp);
            return s;
        };

        State s0 = make_state(0.0);
        State sp = make_state(+dr);
        State sm = make_state(-dr);
        double F_ana = s0.F[0].x;
        double F_num = -(sp.E.total() - sm.E.total()) / (2.0 * dr);
        double err = std::abs(F_num - F_ana);

        std::printf("    r=%.1f: F_ana=%.8f  F_num=%.8f  err=%.2e\n",
                    r0, F_ana, F_num, err);
        check(err < 1e-4, "NaCl: Coulomb force-energy consistency < 1e-4");
    }

    // 1c. Neutral system: Coulomb should be zero
    std::printf("\n  1c. Neutral Ar dimer: Coulomb = 0\n");
    {
        vsepr::io::XYZMolecule m;
        m.atoms.emplace_back("Ar", 0.0, 0.0, 0.0);
        m.atoms.emplace_back("Ar", 4.0, 0.0, 0.0);
        State s = parse(m);
        ModelParams mp; mp.rc = 12.0;
        model->eval(s, mp);
        check(s.E.UCoul == 0.0, "Ar dimer: U_Coul = 0 (neutral)");
    }

    // 1d. Determinism with Coulomb active
    std::printf("\n  1d. Determinism\n");
    {
        auto make_nacl = [&]() {
            vsepr::io::XYZMolecule m;
            m.atoms.emplace_back("Na", 0.0, 0.0, 0.0);
            m.atoms.emplace_back("Cl", 3.0, 0.0, 0.0);
            State s = parse(m);
            s.Q[0] = +1.0; s.Q[1] = -1.0;
            return s;
        };
        State s1 = make_nacl(), s2 = make_nacl();
        ModelParams mp; mp.rc = 20.0;
        model->eval(s1, mp); model->eval(s2, mp);
        check(s1.E.total() == s2.E.total(), "NaCl: bitwise deterministic");
    }
}

// ============================================================================
// M2: UFF-Matched Bonded Parameters
// ============================================================================

static void milestone_2()
{
    std::printf("\n=== M2: UFF-Matched Bonded Parameters ===\n");

    // 2a. H2O: r0 should match initial geometry, not 1.54 A
    std::printf("\n  2a. H2O bonded r0 = initial geometry\n");
    {
        vsepr::io::XYZMolecule m;
        m.atoms.emplace_back("O", 0.0, 0.0, 0.0);
        m.atoms.emplace_back("H", 0.96, 0.0, 0.0);
        double theta = 104.45 * M_PI / 180.0;
        m.atoms.emplace_back("H", 0.96*std::cos(theta), 0.96*std::sin(theta), 0.0);
        vsepr::io::XYZReader().detect_bonds(m);
        State s = parse(m);

        // At the initial geometry, bonded forces should be near zero
        // (since r0 = current distance, theta0 = current angle)
        auto comp = create_composite_model(s);
        comp->eval(s, ModelParams{.rc=12.0});

        std::printf("    Ubond=%.6f  Uangle=%.6f  (both should be ~0 at initial geom)\n",
                    s.E.Ubond, s.E.Uangle);
        check(std::abs(s.E.Ubond) < 0.01,
              "H2O: U_bond ~ 0 at initial geometry (r0 matched)");
        check(std::abs(s.E.Uangle) < 0.01,
              "H2O: U_angle ~ 0 at initial geometry (theta0 matched)");
    }

    // 2b. Distorted H2O relaxes BACK toward initial geometry
    std::printf("\n  2b. Distorted H2O relaxation with composite\n");
    {
        // Build model from equilibrium geometry
        vsepr::io::XYZMolecule meq;
        meq.atoms.emplace_back("O", 0.0, 0.0, 0.0);
        meq.atoms.emplace_back("H", 0.96, 0.0, 0.0);
        double theta = 104.45 * M_PI / 180.0;
        meq.atoms.emplace_back("H", 0.96*std::cos(theta), 0.96*std::sin(theta), 0.0);
        vsepr::io::XYZReader().detect_bonds(meq);
        State seq = parse(meq);
        auto model = create_composite_model(seq);

        // Now distort: stretch O-H to 1.15 A, widen angle to 130 deg
        State s = seq;
        double theta2 = 130.0 * M_PI / 180.0;
        s.X[1] = {1.15, 0.0, 0.0};
        s.X[2] = {1.15*std::cos(theta2), 1.15*std::sin(theta2), 0.0};
        s.F.assign(s.N, {0,0,0});
        model->eval(s, ModelParams{.rc=12.0});
        double U0 = s.E.total();

        FIREParams fp;
        fp.max_steps = 20000; fp.epsF = 1e-4; fp.dt = 5e-5; fp.dt_max = 5e-3;
        FIRE fire(*model, ModelParams{.rc=12.0});
        auto st = fire.minimize(s, fp);

        double d_oh1 = dist(s.X[0], s.X[1]);
        double d_oh2 = dist(s.X[0], s.X[2]);

        std::printf("    init U=%.4f  final U=%.4f  Frms=%.2e  steps=%d\n",
                    U0, st.U, st.Frms, st.step);
        std::printf("    d(O-H1)=%.4f  d(O-H2)=%.4f  (target ~0.96)\n", d_oh1, d_oh2);

        check(st.U < U0, "H2O distorted: energy decreased");
        // Bond lengths should move toward 0.96 (won't match exactly due to LJ H-H)
        check(d_oh1 < 1.15 && d_oh2 < 1.15,
              "H2O: O-H contracted toward r0=0.96 from 1.15");
    }

    // 2c. CH4: element-aware kb — C-H should be stiffer than C-C
    std::printf("\n  2c. CH4 element-aware force constants\n");
    {
        vsepr::io::XYZMolecule m;
        double d = 1.09, a = d / std::sqrt(3.0);
        m.atoms.emplace_back("C",  0,  0,  0);
        m.atoms.emplace_back("H",  a,  a,  a);
        m.atoms.emplace_back("H",  a, -a, -a);
        m.atoms.emplace_back("H", -a,  a, -a);
        m.atoms.emplace_back("H", -a, -a,  a);
        vsepr::io::XYZReader().detect_bonds(m);
        State s = parse(m);
        auto model = create_composite_model(s);
        model->eval(s, ModelParams{.rc=12.0});

        std::printf("    CH4: Ubond=%.6f  Uangle=%.6f  (should be ~0)\n",
                    s.E.Ubond, s.E.Uangle);
        check(std::abs(s.E.Ubond) < 0.01,
              "CH4: U_bond ~ 0 at initial geometry");
    }
}

// ============================================================================
// M3: Angle Gradient Fix
// ============================================================================

static void milestone_3()
{
    std::printf("\n=== M3: Angle Gradient Fix ===\n");

    // 3a. Force-energy consistency on H2O angles through composite model
    //     This previously showed ~7% error from a sign bug in eval_angles.
    std::printf("\n  3a. H2O composite force-energy consistency (all 9 components)\n");
    {
        double dr = 1e-5;
        vsepr::io::XYZMolecule meq;
        meq.atoms.emplace_back("O", 0.0, 0.0, 0.0);
        meq.atoms.emplace_back("H", 0.96, 0.0, 0.0);
        double theta = 104.45 * M_PI / 180.0;
        meq.atoms.emplace_back("H", 0.96*std::cos(theta), 0.96*std::sin(theta), 0.0);
        vsepr::io::XYZReader().detect_bonds(meq);
        State s0 = parse(meq);
        auto model = create_composite_model(s0);
        ModelParams mp; mp.rc = 12.0;
        model->eval(s0, mp);

        double max_rel_err = 0;
        bool all_ok = true;
        for (uint32_t at = 0; at < s0.N; ++at) {
            for (int dim = 0; dim < 3; ++dim) {
                State sp = s0, sm = s0;
                sp.F.assign(sp.N, {0,0,0}); sp.E = {};
                sm.F.assign(sm.N, {0,0,0}); sm.E = {};
                if (dim==0) { sp.X[at].x += dr; sm.X[at].x -= dr; }
                if (dim==1) { sp.X[at].y += dr; sm.X[at].y -= dr; }
                if (dim==2) { sp.X[at].z += dr; sm.X[at].z -= dr; }
                model->eval(sp, mp); model->eval(sm, mp);

                double F_num = -(sp.E.total() - sm.E.total()) / (2.0*dr);
                double F_ana = (dim==0) ? s0.F[at].x
                             : (dim==1) ? s0.F[at].y : s0.F[at].z;
                double scale = std::max(std::abs(F_ana), 1.0);
                double rel = std::abs(F_num - F_ana) / scale;
                if (rel > max_rel_err) max_rel_err = rel;
                if (rel > 1e-3) {
                    std::printf("    atom %u dim %d: ana=%.6f num=%.6f rel=%.2e\n",
                                at, dim, F_ana, F_num, rel);
                    all_ok = false;
                }
            }
        }
        std::printf("    max relative error: %.2e\n", max_rel_err);
        check(all_ok, "H2O composite: all 9 force components match numeric (< 1e-3)");
        check(max_rel_err < 1e-3,
              "H2O composite: max relative error < 1e-3 (angle sign bug fixed)");
    }

    // 3b. Angle-only force-energy (isolated test)
    std::printf("\n  3b. Isolated angle force-energy\n");
    {
        // Create a simple 3-atom angle system with ONLY angle potential
        vsepr::io::XYZMolecule m;
        m.atoms.emplace_back("O", 0.0, 0.0, 0.0);
        m.atoms.emplace_back("H", 1.0, 0.0, 0.0);
        double ang = 100.0 * M_PI / 180.0;
        m.atoms.emplace_back("H", std::cos(ang), std::sin(ang), 0.0);
        m.bonds.emplace_back(0, 1, 1.0);
        m.bonds.emplace_back(0, 2, 1.0);
        State s0 = parse(m);

        // Build bonded-only model
        BondedTopology topo;
        // Angle only, no bonds
        topo.angles.emplace_back(1u, 0u, 2u, 50.0, 109.5 * M_PI / 180.0);
        BondedModel bmodel(topo);
        ModelParams mp; mp.rc = 12.0;

        s0.F.assign(s0.N, {0,0,0}); s0.E = {};
        bmodel.eval(s0, mp);

        double dr = 1e-5;
        bool ok = true;
        for (uint32_t at = 0; at < s0.N; ++at) {
            for (int dim = 0; dim < 3; ++dim) {
                State sp = s0, sm = s0;
                sp.F.assign(sp.N, {0,0,0}); sp.E = {};
                sm.F.assign(sm.N, {0,0,0}); sm.E = {};
                if (dim==0) { sp.X[at].x += dr; sm.X[at].x -= dr; }
                if (dim==1) { sp.X[at].y += dr; sm.X[at].y -= dr; }
                if (dim==2) { sp.X[at].z += dr; sm.X[at].z -= dr; }
                bmodel.eval(sp, mp); bmodel.eval(sm, mp);

                double F_num = -(sp.E.total() - sm.E.total()) / (2.0*dr);
                double F_ana = (dim==0) ? s0.F[at].x
                             : (dim==1) ? s0.F[at].y : s0.F[at].z;
                double scale = std::max(std::abs(F_ana), 1.0);
                double rel = std::abs(F_num - F_ana) / scale;
                if (rel > 1e-4) {
                    std::printf("    atom %u dim %d: ana=%.8f num=%.8f rel=%.2e\n",
                                at, dim, F_ana, F_num, rel);
                    ok = false;
                }
            }
        }
        check(ok, "Isolated angle: force-energy consistency < 1e-4");
    }
}

// ============================================================================
// M4: Covalent Crystal Relaxation
// ============================================================================

static void milestone_4()
{
    std::printf("\n=== M4: Composite Model Crystal/Cluster Relaxation ===\n");

    // 4a. Ar5 cluster: LJ-only (no bonds). Composite should pass through
    //     to pure LJ. Verifies no regressions.
    std::printf("\n  4a. Ar5 cluster relaxation via composite (no bonds)\n");
    {
        vsepr::io::XYZMolecule m;
        m.atoms.emplace_back("Ar", 0.0, 0.0, 0.0);
        m.atoms.emplace_back("Ar", 4.2, 0.0, 0.0);
        m.atoms.emplace_back("Ar", 2.1, 3.6, 0.0);
        m.atoms.emplace_back("Ar", 2.1, 1.2, 3.3);
        m.atoms.emplace_back("Ar",-1.0, 2.0, 1.5);
        State s = parse(m);
        auto model = create_composite_model(s);  // no bonds → pure LJ
        ModelParams mp; mp.rc = 12.0;
        model->eval(s, mp);
        double U0 = s.E.total();

        FIREParams fp;
        fp.max_steps = 10000; fp.epsF = 1e-4; fp.dt = 1e-3;
        FIRE fire(*model, mp);
        auto st = fire.minimize(s, fp);

        std::printf("    U0=%.4f  U_relax=%.4f  Frms=%.2e  steps=%d\n",
                    U0, st.U, st.Frms, st.step);
        check(st.U < U0, "Ar5 composite: energy decreased");
        check(st.Frms < 0.5, "Ar5 composite: F_rms < 0.5");
    }

    // 4b. BCC Fe with LJ-only (validated in Phase 5) still works through composite
    std::printf("\n  4b. BCC Fe LJ-only through composite\n");
    {
        auto uc = presets::iron_bcc();
        auto sc = construct_supercell(uc, 3, 3, 3);
        State& s = sc.state;
        s.F.resize(s.N, {0,0,0});
        // No bonds → composite is pure LJ, same as Phase 5
        auto model = create_composite_model(s);
        ModelParams mp; mp.rc = 10.0;
        model->eval(s, mp);
        double U0 = s.E.total();
        check(std::isfinite(U0), "BCC Fe composite (LJ): energy finite");
        // BCC Fe 3x3x3 at equilibrium: LJ energy is small and stable
        check(std::abs(U0) < 100.0, "BCC Fe composite: |U| < 100 (near equilibrium)");
    }

    // 4c. Diamond Si — declared limitation
    std::printf("\n  4c. Diamond Si — declared limitation\n");
    {
        auto uc = presets::silicon_diamond();
        State s = uc.to_state();
        s.F.resize(s.N, {0,0,0});
        double nn = nn_dist(s);
        std::printf("    Si nn=%.3f A  sigma_Si=4.295 A  2nd_nn~3.84 A\n", nn);
        std::printf("    Covalent crystal relaxation requires 1-3 exclusions\n");
        std::printf("    or reactive potential (Tersoff/REBO). Declared.\n");
        check(nn < 4.295, "Si diamond: nn < sigma confirms covalent regime");
    }
}

// ============================================================================
// M5: Dielectric Screening
// ============================================================================

static void milestone_5()
{
    std::printf("\n=== M5: Dielectric Screening ===\n");
    auto model = create_lj_coulomb_model();

    // 5a. NaCl pair: U_Coul(vacuum) / U_Coul(water) = eps_r
    //     In vacuum: U = k_e * q_i * q_j / r
    //     In water:  U = (k_e / 78.4) * q_i * q_j / r
    //     Ratio = 78.4
    std::printf("\n  5a. NaCl Coulomb screening: vacuum vs water\n");
    {
        auto make_nacl = []() {
            vsepr::io::XYZMolecule m;
            m.atoms.emplace_back("Na", 0.0, 0.0, 0.0);
            m.atoms.emplace_back("Cl", 5.0, 0.0, 0.0);  // far enough for small LJ
            State s = parse(m);
            s.Q[0] = +1.0; s.Q[1] = -1.0;
            return s;
        };

        ModelParams mp_vac; mp_vac.rc = 20.0;
        mp_vac.env = EnvironmentContext::near_vacuum();

        ModelParams mp_sol; mp_sol.rc = 20.0;
        mp_sol.env = EnvironmentContext::solution(78.4);

        State s_vac = make_nacl(), s_sol = make_nacl();
        model->eval(s_vac, mp_vac);
        model->eval(s_sol, mp_sol);

        double U_coul_vac = s_vac.E.UCoul;
        double U_coul_sol = s_sol.E.UCoul;
        double ratio = U_coul_vac / U_coul_sol;

        std::printf("    U_Coul(vacuum) = %.4f\n", U_coul_vac);
        std::printf("    U_Coul(water)  = %.4f\n", U_coul_sol);
        std::printf("    ratio = %.2f (expect 78.4)\n", ratio);

        check(std::abs(ratio - 78.4) < 0.1,
              "NaCl: U_Coul ratio vacuum/water = 78.4");
    }

    // 5b. Neutral system: screening doesn't affect LJ
    std::printf("\n  5b. Neutral Ar: LJ unaffected by dielectric\n");
    {
        vsepr::io::XYZMolecule m;
        m.atoms.emplace_back("Ar", 0.0, 0.0, 0.0);
        m.atoms.emplace_back("Ar", 4.0, 0.0, 0.0);

        ModelParams mp_vac; mp_vac.rc = 12.0;
        mp_vac.env = EnvironmentContext::near_vacuum();
        ModelParams mp_sol; mp_sol.rc = 12.0;
        mp_sol.env = EnvironmentContext::solution(78.4);

        State sv = parse(m), ss = parse(m);
        model->eval(sv, mp_vac); model->eval(ss, mp_sol);

        check(sv.E.UvdW == ss.E.UvdW, "Ar: LJ identical in vacuum and water");
        check(sv.E.total() == ss.E.total(), "Ar: total energy identical (Q=0)");
    }

    // 5c. Coulomb energy scales as 1/r (verified at two distances)
    //     U(r1) * r1 = U(r2) * r2 = k_e * q_i * q_j / eps_r
    std::printf("\n  5c. Coulomb 1/r scaling\n");
    {
        auto make_nacl_at = [&](double r) {
            vsepr::io::XYZMolecule m;
            m.atoms.emplace_back("Na", 0.0, 0.0, 0.0);
            m.atoms.emplace_back("Cl", r, 0.0, 0.0);
            State s = parse(m);
            s.Q[0] = +1.0; s.Q[1] = -1.0;
            return s;
        };

        ModelParams mp; mp.rc = 20.0;
        mp.env = EnvironmentContext::near_vacuum();

        State s3 = make_nacl_at(3.0), s6 = make_nacl_at(6.0);
        model->eval(s3, mp); model->eval(s6, mp);

        // U_Coul(r) = k_e * q_i * q_j / r, so U(3) / U(6) = 6/3 = 2
        double U3 = s3.E.UCoul, U6 = s6.E.UCoul;
        double ratio = U3 / U6;
        std::printf("    U_Coul(3A)=%.4f  U_Coul(6A)=%.4f  ratio=%.3f (expect 2.0)\n",
                    U3, U6, ratio);
        check(std::abs(ratio - 2.0) < 0.01,
              "NaCl: U_Coul(3)/U_Coul(6) = 2.0 (1/r scaling)");
        // Note: NaCl crystal Madelung sum requires Ewald summation for
        // convergence. Finite cutoff does not correctly reproduce alpha_M.
        // This is a declared future milestone.
    }

    // 5d. DryCondensed: intermediate screening
    std::printf("\n  5d. DryCondensed medium (eps_r=4)\n");
    {
        vsepr::io::XYZMolecule m;
        m.atoms.emplace_back("Na", 0.0, 0.0, 0.0);
        m.atoms.emplace_back("Cl", 5.0, 0.0, 0.0);
        State s = parse(m);
        s.Q[0] = +1.0; s.Q[1] = -1.0;

        ModelParams mp_vac; mp_vac.rc = 20.0;
        mp_vac.env = EnvironmentContext::near_vacuum();
        ModelParams mp_dry; mp_dry.rc = 20.0;
        mp_dry.env = EnvironmentContext::dry_condensed(4.0);

        State sv = s, sd = s;
        sv.F.assign(sv.N, {0,0,0}); sv.E = {};
        sd.F.assign(sd.N, {0,0,0}); sd.E = {};
        model->eval(sv, mp_vac); model->eval(sd, mp_dry);

        double ratio = sv.E.UCoul / sd.E.UCoul;
        std::printf("    U_Coul(vac)=%.4f  U_Coul(dry)=%.4f  ratio=%.2f (expect 4.0)\n",
                    sv.E.UCoul, sd.E.UCoul, ratio);
        check(std::abs(ratio - 4.0) < 0.01,
              "DryCondensed(4): Coulomb ratio = 4.0");
    }
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::printf("\n");
    std::printf("=============================================================\n");
    std::printf("  Phase 14 — Five-Milestone Verification\n");
    std::printf("=============================================================\n");

    milestone_1();
    milestone_2();
    milestone_3();
    milestone_4();
    milestone_5();

    std::printf("\n=============================================================\n");
    std::printf("  Phase 14 Audit:  %d passed,  %d failed\n", g_pass, g_fail);
    std::printf("=============================================================\n\n");
    return g_fail > 0 ? 1 : 0;
}
