/**
 * example_layer_stack.cpp
 *
 * End-to-end demonstration of the five-layer vertical integration stack.
 *
 * Scenario: Iron ore processing simulation.
 *   Step 1 — L1 catalog: Fe2O3 (hematite), H2SO4, Fe (target product)
 *   Step 2 — L2 states:  Fe³⁺ in acid leach environment
 *   Step 3 — L2↔L4 boundary evaluation on a small bead system
 *   Step 4 — L5 macro body: a steel pipe under corrosive conditions
 *
 * Exits 0 on success.
 */

#include "include/layer_stack.hpp"
#include "coarse_grain/models/layer_boundary.hpp"
#include "coarse_grain/core/bead_system.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace layer_stack;
using namespace coarse_grain;

// ── utilities ─────────────────────────────────────────────────────────────────

static void section(const char* title) {
    std::printf("\n── %s ─────────────────────────────────────\n", title);
}

static void check(bool cond, const char* msg) {
    if (!cond) { std::fprintf(stderr, "[FAIL] %s\n", msg); std::exit(1); }
    std::printf("[PASS] %s\n", msg);
}

// ── Step 1: L1 paper identity catalog ─────────────────────────────────────────

static void demo_l1() {
    section("L1 — Paper Identity Catalog");

    // Hematite
    L1_PaperIdentity hematite;
    hematite.formula      = "Fe2O3";
    hematite.common_name  = "hematite";
    hematite.class_tag    = "oxide";
    hematite.cas_number   = "1317-60-8";
    hematite.Z_set        = {26, 8};
    hematite.stoich       = {2, 3};
    check(hematite.is_valid(),            "hematite L1 valid");
    check(!hematite.is_single_element(),  "hematite is not single-element");

    // Sulphuric acid (leach reagent)
    L1_PaperIdentity h2so4;
    h2so4.formula      = "H2SO4";
    h2so4.common_name  = "sulphuric acid";
    h2so4.class_tag    = "acid";
    h2so4.Z_set        = {1, 8, 16};
    h2so4.stoich       = {2, 4, 1};
    check(h2so4.is_valid(), "H2SO4 L1 valid");

    // Iron (target product)
    L1_PaperIdentity iron;
    iron.formula     = "Fe";
    iron.class_tag   = "element";
    iron.Z_set       = {26};
    iron.stoich      = {1};
    check(iron.is_valid(),           "Fe L1 valid");
    check(iron.is_single_element(),  "Fe is single-element");

    // L1 gate screening
    auto r_hematite = l1_gate(hematite);
    auto r_h2so4    = l1_gate(h2so4);
    auto r_iron     = l1_gate(iron, /*excluded_classes=*/{"radioactive"});

    check(bool(r_hematite), "hematite passes L1 gate");
    check(bool(r_h2so4),    "H2SO4 passes L1 gate");
    check(bool(r_iron),     "Fe passes L1 gate");

    // Invalid candidate (bad Z)
    L1_PaperIdentity bad;
    bad.formula = "Xx";
    bad.Z_set   = {200};  // Z > 118
    bad.stoich  = {1};
    auto r_bad = l1_gate(bad);
    check(!bool(r_bad), "invalid Z=200 rejected at L1");

    std::printf("  L1 gate scores: hematite=%.3f  H2SO4=%.3f  Fe=%.3f\n",
                r_hematite.score, r_h2so4.score, r_iron.score);
}

// ── Step 2: L2 atomistic state — Fe³⁺ in acid leach ──────────────────────────

static void demo_l2() {
    section("L2 — Atomistic State (Fe³⁺ in acid leach)");

    // Fe in ore body: metallic, neutral
    L2_AtomisticState fe_ore;
    fe_ore.Z              = 26;
    fe_ore.A              = 56;
    fe_ore.Q              = 0.0;
    fe_ore.epsilon        = 0.13;   // kcal/mol — metallic Fe (TraPPE-derived)
    fe_ore.sigma          = 2.60;   // Å
    fe_ore.structural_role  = StructuralRole::Metallic;
    fe_ore.stability_class  = StabilityClass::BulkLattice;
    fe_ore.phase          = chemistry::Phase::SOLID;
    fe_ore.source_formula = "Fe";
    fe_ore.pack_identity();
    check(fe_ore.is_valid(), "Fe ore L2 valid");

    // Fe³⁺ in acid leach: ionic, +3 charge, higher epsilon
    L2_AtomisticState fe_aq;
    fe_aq.Z              = 26;  // same Z — invariant holds
    fe_aq.A              = 56;  // same A — invariant holds
    fe_aq.Q              = 3.0; // +3 charge state
    fe_aq.epsilon        = 0.55; // kcal/mol — ionic Fe in solution
    fe_aq.sigma          = 2.10; // Å — smaller hydrated radius
    fe_aq.structural_role  = StructuralRole::IonicDominant; // role changed
    fe_aq.stability_class  = StabilityClass::AmbientStable;
    fe_aq.phase          = chemistry::Phase::AQUEOUS;
    fe_aq.source_formula = "Fe2O3";
    fe_aq.pack_identity();
    check(fe_aq.is_valid(), "Fe³⁺ aqueous L2 valid");

    // Verify propagation invariant: Z and A unchanged
    check(fe_ore.Z == fe_aq.Z, "Z invariant: Fe ore == Fe³⁺ aq (both Z=26)");
    check(fe_ore.A == fe_aq.A, "A invariant: Fe ore == Fe³⁺ aq (both A=56)");

    // L2 gate
    auto r_ore = l2_gate(fe_ore);
    auto r_aq  = l2_gate(fe_aq);
    check(bool(r_ore), "Fe ore passes L2 gate");
    check(bool(r_aq),  "Fe³⁺ aq passes L2 gate");

    // An unphysically high charge should fail
    L2_AtomisticState fe_bad = fe_aq;
    fe_bad.Q = 50.0;
    auto r_bad = l2_gate(fe_bad);
    check(!bool(r_bad), "Q=50 rejected at L2 gate");

    std::printf("  Fe ore:  Q=%.1f  ε=%.3f kcal/mol  role=%s\n",
                fe_ore.Q, fe_ore.epsilon,
                structural_role_name(fe_ore.structural_role));
    std::printf("  Fe³⁺ aq: Q=%.1f  ε=%.3f kcal/mol  role=%s\n",
                fe_aq.Q, fe_aq.epsilon,
                structural_role_name(fe_aq.structural_role));
}

// ── Step 3: L2↔L4 boundary evaluation ────────────────────────────────────────

static void demo_boundary() {
    section("L2↔L4 Boundary Evaluation");

    // Small 2-bead system: Fe bead + O bead (oxide fragment)
    BeadSystem sys;
    sys.beads.resize(2);

    sys.beads[0].position       = {0.0, 0.0, 0.0};
    sys.beads[0].mass           = 55.845;
    sys.beads[0].charge         = 3.0;    // Fe³⁺ — ionic
    sys.beads[0].structural_role  = StructuralRole::Metallic;  // pre-transition (stale)
    sys.beads[0].stability_class  = StabilityClass::AmbientStable;

    sys.beads[1].position       = {2.8, 0.0, 0.0};
    sys.beads[1].mass           = 15.999;
    sys.beads[1].charge         = -2.0;   // O²⁻
    sys.beads[1].structural_role  = StructuralRole::IonicDominant;
    sys.beads[1].stability_class  = StabilityClass::AmbientStable;

    // L2 states for each bead (dominant atom)
    std::vector<L2_AtomisticState> l2_states(2);

    l2_states[0].Z       = 26; l2_states[0].A = 56;
    l2_states[0].Q       = 3.0;
    l2_states[0].epsilon = 0.55; l2_states[0].sigma = 2.10;
    l2_states[0].structural_role  = StructuralRole::IonicDominant;
    l2_states[0].stability_class  = StabilityClass::AmbientStable;

    l2_states[1].Z       = 8;  l2_states[1].A = 16;
    l2_states[1].Q       = -2.0;
    l2_states[1].epsilon = 0.21; l2_states[1].sigma = 3.12;
    l2_states[1].structural_role  = StructuralRole::IonicDominant;
    l2_states[1].stability_class  = StabilityClass::AmbientStable;

    // Phase vectors: both beads are solid-phase only
    std::vector<std::vector<chemistry::Phase>> phases = {
        {chemistry::Phase::SOLID},
        {chemistry::Phase::SOLID}
    };

    BoundaryParams params;
    params.recompute_sigma = true;     // B4: re-evaluate Σ at boundary
    params.energy_tol_kcal = 100.0;   // permissive for this demo

    BoundaryReport report = evaluate_boundary(sys, l2_states, phases, params);

    check(report.n_beads_checked == 2, "boundary checked 2 beads");
    check(!report.bead_records.empty(), "bead records populated");

    // B4: Fe bead had stale Σ=Metallic with Q=3.0 — should be updated to Ionic
    const auto& fe_rec = report.bead_records[0];
    check(fe_rec.sigma_changed,
          "B4: Fe structural role updated (Metallic→Ionic for Q=3.0)");
    check(sys.beads[0].structural_role == StructuralRole::IonicDominant,
          "B4: Fe bead Σ is now IonicDominant in-place");

    // B2: charge should be conserved (we set matching charges)
    check(fe_rec.charge_ok, "B2: Fe charge conservation OK");

    // B3: no phase mixing
    check(!fe_rec.phase_mixed, "B3: no phase mixing");

    std::printf("  %s\n", report.summary.c_str());
    std::printf("  Fe boundary record: energy_gap=%.3f  charge_err=%.4f  residual=%.3f\n",
                fe_rec.energy_gap, fe_rec.charge_error, fe_rec.mapping_residual);
}

// ── Step 4: L5 macro body — steel pipe under corrosive conditions ─────────────

static void demo_l5() {
    section("L5 — Macro Geometry (Steel Pipe)");

    L5_MacroGeometry pipe;
    pipe.body_id          = "pipe-001";
    pipe.body_type        = MacroBodyType::Pipe;
    pipe.material_formula = "Fe";          // Bulk material is iron

    // Physical dimensions (SI: metres)
    pipe.length_m           = 12.0;
    pipe.outer_diameter_m   = 0.2;
    pipe.wall_thickness_m   = 0.012;
    pipe.volume_m3          = 3.14159 * (0.1*0.1 - 0.088*0.088) * 12.0;
    pipe.surface_area_m2    = 3.14159 * 0.2 * 12.0;  // outer surface

    // Operating conditions: acid leach service
    pipe.conditions.temperature_K   = 333.15;   // 60°C
    pipe.conditions.pressure_bar    = 2.5;
    pipe.conditions.flow_velocity_ms = 1.2;
    pipe.conditions.fluid_formula   = "H2SO4_aq";

    // Initial surface condition
    pipe.surface.corrosion_depth_m  = 0.0;
    pipe.surface.oxide_layer_m      = 5e-9;     // 5 nm native oxide
    pipe.surface.roughness_Ra_m     = 1.6e-6;   // Ra 1.6 μm (machined)
    pipe.surface.dominant_species   = "Fe2O3";  // passivation layer
    pipe.structural_integrity       = 1.0;

    check(pipe.is_valid(), "Pipe L5 valid");

    // Simulate one corrosion increment (driven by L4 chemistry in a real run)
    double corrosion_rate_m_per_s = 2e-12;  // ~0.06 mm/year — typical for mild steel in H2SO4
    double dt_s = 3600.0 * 24 * 365;        // one year
    pipe.surface.corrosion_depth_m += corrosion_rate_m_per_s * dt_s;

    // Update structural integrity
    pipe.structural_integrity = 1.0
        - pipe.surface.corrosion_depth_m / pipe.wall_thickness_m;
    if (pipe.structural_integrity < 0.0) pipe.structural_integrity = 0.0;

    // Update surface species: Fe is now Fe2O3 at the corroded surface
    pipe.surface.dominant_species = "Fe2O3";

    check(pipe.structural_integrity < 1.0,  "Integrity reduced after corrosion");
    check(pipe.structural_integrity > 0.0,  "Pipe not failed after 1 year");

    std::printf("  After 1 year:\n");
    std::printf("    corrosion depth  : %.4f mm\n",
                pipe.surface.corrosion_depth_m * 1e3);
    std::printf("    wall thickness   : %.1f mm\n",
                pipe.wall_thickness_m * 1e3);
    std::printf("    integrity        : %.4f\n",
                pipe.structural_integrity);
    std::printf("    surface species  : %s\n",
                pipe.surface.dominant_species.c_str());
    std::printf("    body type        : %s\n",
                macro_body_type_name(pipe.body_type));

    // Verify propagation invariant at L5: material_formula back-references Z=26
    check(pipe.material_formula == "Fe",
          "L5 material_formula preserves L1 back-reference to Fe (Z=26)");
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    std::printf("VSEPR-SIM — Five-Layer Vertical Integration Stack Demo\n");
    std::printf("========================================================\n");
    std::printf("Scenario: Iron ore processing — cradle to product\n");

    demo_l1();
    demo_l2();
    demo_boundary();
    demo_l5();

    section("Result");
    std::printf("All checks passed.\n");
    std::printf("\nLayer stack summary:\n");
    std::printf("  L1  Paper Identity   : hematite, H2SO4, Fe — catalog valid\n");
    std::printf("  L2  Atomistic        : Fe³⁺ (Q=+3, ε=0.55) — propagation invariant holds\n");
    std::printf("  L2↔L4 Boundary       : B4 role update fired (Metallic→Ionic)\n");
    std::printf("  L5  Macro            : 12 m pipe, 1-year corrosion modelled\n");
    return 0;
}
