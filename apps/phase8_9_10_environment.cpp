/**
 * phase8_9_10_environment.cpp
 * Phase 8:  Cleanly Defer Missing Physics
 * Phase 9:  Formalize Layered System Architecture
 * Phase 10: Add Environmental Context
 *
 * Checks:
 *   8.1  Physics registry reports Coulomb as DEFERRED
 *   8.2  Dipole/polarization as DEFERRED or PARTIAL
 *   8.3  Active terms (LJ, FIRE, MD) are marked ACTIVE
 *   9.1  Layer manifest lists all five layers
 *   9.2  No layer is absent (at minimum partial)
 *  10.1  EnvironmentContext constructs with correct defaults
 *  10.2  Factory constructors produce correct medium type and dielectric
 *  10.3  coulomb_scale() is 1/eps_r
 *  10.4  debye_length() is correct at known ionic strength
 *  10.5  Identical physics, different env → different energy (when Coulomb live)
 *  10.6  Near-vacuum eval is deterministic
 *  10.7  ModelParams carries env field
 */

#include "atomistic/core/physics_status.hpp"
#include "atomistic/core/layer_manifest.hpp"
#include "atomistic/core/environment.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/parsers/xyz_parser.hpp"
#include "io/xyz_format.hpp"
#include <cmath>
#include <cstdio>
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

static State ar_dimer(double r)
{
    vsepr::io::XYZMolecule m;
    m.atoms.emplace_back("Ar", 0.0, 0.0, 0.0);
    m.atoms.emplace_back("Ar", r,   0.0, 0.0);
    State s = parsers::from_xyz(m);
    s.F.resize(s.N, {0,0,0});
    return s;
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::printf("\n");
    std::printf("=============================================================\n");
    std::printf("  Phase 8/9/10 — Environment & Architecture Audit\n");
    std::printf("=============================================================\n\n");

    // ------------------------------------------------------------------
    // 8.1-8.3  Physics registry integrity
    // ------------------------------------------------------------------
    std::printf("--- 8.1-8.3 Physics Registry ---\n");
    {
        bool coulomb_active  = false;
        bool dipole_handled   = false;
        bool lj_active        = false;
        bool fire_active      = false;

        for (int i = 0; i < PHYSICS_REGISTRY_SIZE; ++i) {
            const auto& t = PHYSICS_REGISTRY[i];
            std::printf("    %-35s  ", t.name);
            switch (t.state) {
                case PhysicsState::ACTIVE:   std::printf("[ACTIVE]\n");   break;
                case PhysicsState::PARTIAL:  std::printf("[PARTIAL]\n");  break;
                case PhysicsState::DEFERRED: std::printf("[DEFERRED]\n"); break;
                case PhysicsState::ABSENT:   std::printf("[ABSENT]\n");   break;
            }
            std::string n(t.name);
            if (n.find("Coulomb") != std::string::npos)
                coulomb_active = (t.state == PhysicsState::ACTIVE);
            if (n.find("ipole") != std::string::npos || n.find("polariz") != std::string::npos)
                dipole_handled = (t.state != PhysicsState::ACTIVE); // must not be active yet
            if (n.find("LJ") != std::string::npos)
                lj_active = (t.state == PhysicsState::ACTIVE);
            if (n.find("FIRE") != std::string::npos)
                fire_active = (t.state == PhysicsState::ACTIVE);
        }

        std::printf("\n");
        check(coulomb_active,   "Coulomb marked ACTIVE in registry");
        check(dipole_handled,   "Dipole/polarization not marked ACTIVE (correctly deferred/partial)");
        check(lj_active,        "LJ marked ACTIVE");
        check(fire_active,      "FIRE marked ACTIVE");

        int n_active   = count_terms<PhysicsState::ACTIVE>();
        check(n_active   >= 5, "at least 5 active physics terms (LJ, Coulomb, PBC, FIRE, VV, Langevin)");
    }

    // ------------------------------------------------------------------
    // 9.1-9.2  Layer manifest
    // ------------------------------------------------------------------
    std::printf("\n--- 9.1-9.2 Layer Manifest ---\n");
    {
        int n_layers = (int)(sizeof(atomistic::LAYER_MANIFEST) / sizeof(atomistic::LAYER_MANIFEST[0]));
        for (int i = 0; i < n_layers; ++i) {
            const auto& l = atomistic::LAYER_MANIFEST[i];
            std::printf("    Layer %d  %-30s  [%s]\n",
                        (int)l.layer, l.name, l.status);
        }
        std::printf("\n");
        check(n_layers == 5, "all five layers present in manifest");

        bool all_not_absent = true;
        for (int i = 0; i < n_layers; ++i)
            if (std::string(atomistic::LAYER_MANIFEST[i].status) == "absent")
                { all_not_absent = false; break; }
        check(all_not_absent, "no layer is completely absent");
    }

    // ------------------------------------------------------------------
    // 10.1  EnvironmentContext defaults
    // ------------------------------------------------------------------
    std::printf("\n--- 10.1 EnvironmentContext Defaults ---\n");
    {
        EnvironmentContext def;
        check(def.medium == MediumType::NearVacuum, "default medium = NearVacuum");
        check(std::abs(def.dielectric - 1.0) < 1e-12, "default dielectric = 1.0");
        check(std::abs(def.coulomb_scale() - 1.0) < 1e-12, "default coulomb_scale = 1.0");
        check(def.debye_length() > 1e29, "default Debye length = infinity");
    }

    // ------------------------------------------------------------------
    // 10.2  Factory constructors
    // ------------------------------------------------------------------
    std::printf("\n--- 10.2 Factory Constructors ---\n");
    {
        auto vac  = EnvironmentContext::near_vacuum(300.0);
        auto dry  = EnvironmentContext::dry_condensed(3.0, 300.0);
        auto sol  = EnvironmentContext::solution(78.4, 298.15, 0.15, 0.1);

        check(vac.medium == MediumType::NearVacuum,   "near_vacuum: medium type");
        check(std::abs(vac.dielectric - 1.0) < 1e-12, "near_vacuum: dielectric = 1");

        check(dry.medium == MediumType::DryCondensed,  "dry_condensed: medium type");
        check(std::abs(dry.dielectric - 3.0) < 1e-12,  "dry_condensed: dielectric = 3");

        check(sol.medium == MediumType::Solution,       "solution: medium type");
        check(std::abs(sol.dielectric - 78.4) < 1e-12,  "solution: dielectric = 78.4");
        check(std::abs(sol.ionic_strength - 0.15) < 1e-12, "solution: ionic_strength = 0.15");
    }

    // ------------------------------------------------------------------
    // 10.3  Coulomb scale = 1/eps_r
    // ------------------------------------------------------------------
    std::printf("\n--- 10.3 Coulomb Scaling ---\n");
    {
        auto sol = EnvironmentContext::solution(78.4);
        check(std::abs(sol.coulomb_scale() - 1.0/78.4) < 1e-14,
              "solution: coulomb_scale = 1/78.4");

        auto dry = EnvironmentContext::dry_condensed(4.0);
        check(std::abs(dry.coulomb_scale() - 0.25) < 1e-14,
              "dry_condensed(4): coulomb_scale = 0.25");
    }

    // ------------------------------------------------------------------
    // 10.4  Debye length
    // ------------------------------------------------------------------
    std::printf("\n--- 10.4 Debye-Hückel Length ---\n");
    {
        // At I=0.1 mol/L, T=298K, water: lambda_D ≈ 3.04/sqrt(0.1) ≈ 9.6 Å
        auto sol = EnvironmentContext::solution(78.4, 298.15, 0.1);
        double lambda = sol.debye_length();
        std::printf("    I=0.1 mol/L, T=298K: lambda_D = %.3f A (expect ~9.6)\n", lambda);
        check(lambda > 8.0 && lambda < 12.0, "Debye length ~9.6 A at I=0.1 mol/L");

        // Zero ionic strength → infinite Debye length
        auto sol0 = EnvironmentContext::solution(78.4, 298.15, 0.0);
        check(sol0.debye_length() > 1e29, "I=0: Debye length = infinity");
    }

    // ------------------------------------------------------------------
    // 10.5  ModelParams carries env; LJ eval unaffected by env (Coulomb deferred)
    // ------------------------------------------------------------------
    std::printf("\n--- 10.5-10.7 ModelParams with EnvironmentContext ---\n");
    {
        auto model = create_lj_coulomb_model();

        ModelParams mp_vac;
        mp_vac.rc  = 12.0;
        mp_vac.env = EnvironmentContext::near_vacuum();

        ModelParams mp_sol;
        mp_sol.rc  = 12.0;
        mp_sol.env = EnvironmentContext::solution(78.4, 298.15, 0.15);

        State s1 = ar_dimer(4.0);
        State s2 = ar_dimer(4.0);
        model->eval(s1, mp_vac);
        model->eval(s2, mp_sol);

        // LJ is unaffected by dielectric (Coulomb is deferred, charges=0)
        // Both evaluations must be identical for a neutral Ar dimer.
        check(s1.E.total() == s2.E.total(),
              "Ar dimer (neutral): LJ energy unaffected by dielectric (Coulomb deferred)");
        check(std::isfinite(s1.E.total()), "near_vacuum: energy finite");
        check(std::isfinite(s2.E.total()), "solution: energy finite");

        // Determinism under environment
        State s3 = ar_dimer(4.0);
        State s4 = ar_dimer(4.0);
        model->eval(s3, mp_sol);
        model->eval(s4, mp_sol);
        check(s3.E.total() == s4.E.total(),
              "solution env: bitwise deterministic");

        // medium_name() utility
        check(std::string(mp_vac.env.medium_name()) == "near_vacuum", "medium_name near_vacuum");
        check(std::string(mp_sol.env.medium_name()) == "solution",    "medium_name solution");
    }

    // ------------------------------------------------------------------
    // Summary
    // ------------------------------------------------------------------
    std::printf("\n=============================================================\n");
    std::printf("  Phase 8/9/10 Audit:  %d passed,  %d failed\n", g_pass, g_fail);
    std::printf("=============================================================\n\n");
    return g_fail > 0 ? 1 : 0;
}
