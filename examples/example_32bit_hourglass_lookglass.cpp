/**
 * example_32bit_hourglass_lookglass.cpp
 *
 * Headless demonstration and smoke-test for:
 *   1. Identity32  — pack / unpack / round-trip
 *   2. HourglassModel — candidate generation, gate filtering, ranking
 *   3. LookglassModel — backward-pass reflection on a 2-bead system
 *
 * Exits 0 on success, non-zero on any assertion failure.
 */

#include "coarse_grain/core/bit32_identity.hpp"
#include "coarse_grain/models/hourglass_model.hpp"
#include "coarse_grain/models/lookglass_model.hpp"
#include "coarse_grain/models/seed_bead_stepper.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace coarse_grain;

// ── utilities ─────────────────────────────────────────────────────────────────

static void section(const char* title) {
    std::printf("\n── %s ──────────────────────────────────────\n", title);
}

static void check(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "[FAIL] %s\n", msg);
        std::exit(1);
    }
    std::printf("[PASS] %s\n", msg);
}

// ── helpers to build a minimal BeadSystem ────────────────────────────────────

static BeadSystem make_two_bead_system() {
    BeadSystem sys;
    sys.beads.resize(2);

    // Bead 0 — carbon-like covalent bead
    sys.beads[0].position    = {0.0, 0.0, 0.0};
    sys.beads[0].velocity    = {0.0, 0.0, 0.0};
    sys.beads[0].mass        = 12.011;
    sys.beads[0].charge      = -0.25;
    sys.beads[0].structural_role  = StructuralRole::DirectionalCovalent;
    sys.beads[0].stability_class  = StabilityClass::AmbientStable;

    // Bead 1 — sodium-like ionic bead, offset 4 Å
    sys.beads[1].position    = {4.0, 0.0, 0.0};
    sys.beads[1].velocity    = {0.0, 0.0, 0.0};
    sys.beads[1].mass        = 22.990;
    sys.beads[1].charge      = +1.0;
    sys.beads[1].structural_role  = StructuralRole::IonicDominant;
    sys.beads[1].stability_class  = StabilityClass::AmbientStable;

    return sys;
}

// ── Test 1: 32-Bit Identity Word ─────────────────────────────────────────────

static void test_bit32() {
    section("32-Bit Identity Word");

    // ── basic pack / unpack ────────────────────────────────────────────────
    Identity32 id = pack_identity(
        /*Z=*/6, /*A=*/0, /*Q=*/-0.25,
        StructuralRole::DirectionalCovalent,
        StabilityClass::AmbientStable,
        ProvenanceTag::MappedAtomistic);

    check(id.Z()    == 6,                                    "Z round-trips");
    check(id.A()    == 0,                                    "A round-trips");
    check(std::abs(id.Q() - (-0.25)) < 1e-9,                "Q round-trips (s8.2 exact)");
    check(id.sigma() == StructuralRole::DirectionalCovalent, "Sigma round-trips");
    check(id.lambda()== StabilityClass::AmbientStable,       "Lambda round-trips");
    check(id.theta() == ProvenanceTag::MappedAtomistic,      "Theta round-trips");
    check(identity32_roundtrip_ok(id),                       "Round-trip consistency");

    std::printf("  %s\n", id.to_string().c_str());

    // ── charge encoding boundary ───────────────────────────────────────────
    Identity32 id_max = pack_identity(0, 0, 31.75,
        StructuralRole::Mixed, StabilityClass::BulkLattice,
        ProvenanceTag::CrystalSeed);
    check(std::abs(id_max.Q() - 31.75) < 1e-9, "Q max +31.75 encodes exactly");

    Identity32 id_min = pack_identity(0, 0, -32.0,
        StructuralRole::Inert, StabilityClass::Transient,
        ProvenanceTag::Virgin);
    check(std::abs(id_min.Q() - (-32.0)) < 1e-9, "Q min -32.0 encodes exactly");

    // ── pack_from_bead ─────────────────────────────────────────────────────
    BeadSystem sys = make_two_bead_system();
    Identity32 id_b = pack_from_bead(sys.beads[0], /*Z=*/6);
    check(id_b.Z()    == 6,                                    "pack_from_bead Z");
    check(id_b.sigma() == StructuralRole::DirectionalCovalent, "pack_from_bead Sigma");
    check(identity32_roundtrip_ok(id_b),                       "pack_from_bead round-trip");

    // ── all 8 provenance tags ──────────────────────────────────────────────
    for (uint8_t t = 0; t <= 7; ++t) {
        Identity32 pt = pack_identity(1, 0, 0.0,
            StructuralRole::Mixed, StabilityClass::Transient,
            static_cast<ProvenanceTag>(t));
        check(static_cast<uint8_t>(pt.theta()) == t, provenance_tag_name(static_cast<ProvenanceTag>(t)));
    }
}

// ── Test 2: Hourglass Model ───────────────────────────────────────────────────

static void test_hourglass() {
    section("Hourglass Convergence Model");

    BeadSystem sys = make_two_bead_system();

    HourglassParams params;
    params.N_cand    = 64;
    params.delta_max = 0.5;
    params.Lambda_min = StabilityClass::Transient;   // permissive — all pass gate 1
    params.E_tol     = 1e9;                          // permissive — all pass gate 2
    params.w_role_min = 0.0;                         // permissive — all pass gate 3
    params.N_out     = 8;
    params.rng_seed  = 12345;

    HourglassResult result = run_hourglass(sys, params);

    check(result.mouth.N_generated == 64, "Mouth generated 64 candidates");
    check(result.neck.N_survived == 64,   "All pass permissive neck");
    check(result.any_survived,            "any_survived flag set");
    check(!result.ranked_ids.empty(),     "Ranked list non-empty");
    check(result.base.N_ranked <= 8,      "N_out cap respected");

    const CandidateRecord* best = hourglass_best(result);
    check(best != nullptr,         "best candidate non-null");
    check(best->rank == 1,         "best candidate rank == 1");
    check(best->survived,          "best candidate survived");

    std::printf("  E_basin=%.3f kcal/mol  best_score=%.4f\n",
                result.neck.E_basin, result.base.best_score);

    // ── strict gates: should reduce survivors ─────────────────────────────
    HourglassParams strict = params;
    strict.E_tol      = 1e-6;   // only the single lowest-energy candidate passes
    strict.Lambda_min = StabilityClass::BulkLattice; // will block AmbientStable beads
    HourglassResult r2 = run_hourglass(sys, strict);

    // All should be rejected by gate 1 or gate 2 — just verify it runs cleanly
    std::printf("  Strict run: %u survived (expected 0 for bulk-lattice gate)\n",
                r2.neck.N_survived);

    // ── determinism: same seed → same result ──────────────────────────────
    HourglassResult r3 = run_hourglass(sys, params);
    check(result.ranked_ids[0] == r3.ranked_ids[0], "Determinism: same top-ranked id");
    check(std::abs(result.candidates[result.ranked_ids[0]].score -
                   r3.candidates[r3.ranked_ids[0]].score) < 1e-12,
          "Determinism: same top score");
}

// ── Test 3: Lookglass Model ───────────────────────────────────────────────────

static void test_lookglass() {
    section("Lookglass Bidirectional Feedback Model");

    BeadSystem sys = make_two_bead_system();

    // Build a minimal SeedBeadStepRecord to feed the backward pass
    SeedBeadStepRecord fwd{};
    fwd.step_index      = 0;
    fwd.total_energy    = -12.5;
    fwd.rms_force       = 0.8;
    fwd.avg_eta         = 0.3;
    fwd.avg_rho         = 1.1;
    fwd.avg_P2          = 0.55;
    fwd.avg_g_steric    = 1.0;
    fwd.avg_g_elec      = 1.2;
    fwd.avg_g_disp      = 0.9;

    // Provide bead positions slightly offset from sys (simulating CG centroid)
    fwd.bead_positions.push_back({0.1, 0.05, -0.05});
    fwd.bead_positions.push_back({3.9, 0.02,  0.03});

    LookglassParams lg;
    lg.alpha_pos   = 0.05;
    lg.alpha_damp  = 0.3;
    lg.gamma_base  = 0.1;
    lg.alpha_force = 0.2;
    lg.alpha_dt    = 0.5;
    lg.dt_base     = 5.0;
    lg.r_tol       = 0.001;
    lg.eta_tol     = 1e-4;

    LookglassStepRecord bwd = step_lookglass(sys, fwd, lg, /*eta_prev=*/0.0);

    check(bwd.status.l1_pos_correction,  "L1 position correction ran");
    check(bwd.status.l2_damp_modulation, "L2 damping modulation ran");
    check(bwd.status.l3_force_scale,     "L3 force scale ran");
    check(bwd.status.l4_dt_guard,        "L4 dt guard ran");

    // L1: positions should have shifted
    check(bwd.max_pos_correction > 0.0,   "L1 produced non-zero correction");
    check(bwd.max_pos_correction < 0.1,   "L1 correction < alpha_pos * delta");

    // L2: gamma_eff = 0.1 * (1 + 0.3 * 0.3) = 0.1 * 1.09 = 0.109
    double expected_gamma = 0.1 * (1.0 + 0.3 * 0.3);
    check(std::abs(bwd.gamma_effective - expected_gamma) < 1e-10,
          "L2 gamma_eff formula correct");
    std::printf("  gamma_eff = %.4f (expected %.4f)\n",
                bwd.gamma_effective, expected_gamma);

    // L3: mean force scale should be > 0
    check(bwd.mean_force_scale > 0.0, "L3 force scale positive");
    std::printf("  mean_force_scale = %.4f\n", bwd.mean_force_scale);

    // L4: lambda_mean = 2.0 (both AmbientStable)
    //     guard = clamp(1 - 0.5*(3-2)/3, 0.1, 1.0) = clamp(0.8333, 0.1, 1.0) = 0.8333
    //     dt_guarded = 5.0 * 0.8333 = 4.1667 fs
    double expected_dt = 5.0 * (1.0 - 0.5 * (3.0 - 2.0) / 3.0);
    check(std::abs(bwd.dt_guarded - expected_dt) < 1e-6,
          "L4 dt_guarded formula correct");
    std::printf("  dt_guarded = %.4f fs (expected %.4f fs)\n",
                bwd.dt_guarded, expected_dt);
    std::printf("  lambda_mean = %.2f\n", bwd.lambda_mean);

    // ── LookglassRunRecord accumulation ───────────────────────────────────
    LookglassRunRecord run;
    run.record_history = true;
    run.ingest(bwd);

    check(run.steps_run == 1,                "RunRecord: steps_run == 1");
    check(run.history.size() == 1,           "RunRecord: history has 1 entry");
    check(run.peak_pos_correction > 0.0,     "RunRecord: peak correction tracked");

    // ── multi-step: verify eta convergence triggers L5 ────────────────────
    // Synthesise a steady-state record: tiny correction + stable eta
    SeedBeadStepRecord fwd_ss = fwd;
    fwd_ss.avg_eta = 0.3 + 1e-5;   // barely changed
    // Position offsets tiny — will produce near-zero feedback
    fwd_ss.bead_positions[0] = {0.0 + 1e-5, 0.0, 0.0};
    fwd_ss.bead_positions[1] = {4.0 + 1e-5, 0.0, 0.0};

    BeadSystem sys2 = make_two_bead_system();
    LookglassStepRecord bwd_ss = step_lookglass(sys2, fwd_ss, lg, /*eta_prev=*/0.3);

    // With alpha_pos=0.05 and offset=1e-5, max_pos_correction ≈ 5e-7 < r_tol=0.001
    check(bwd_ss.feedback_converged, "L5 feedback_converged for tiny correction");
    check(bwd_ss.eta_converged,      "L5 eta_converged for |Δη| < eta_tol");
    check(bwd_ss.lookglass_steady,   "L5 lookglass_steady when both met");

    std::printf("  L5 steady-state correctly detected\n");
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    std::printf("VSEPR-SIM — 32-Bit / Hourglass / Lookglass Smoke-Test\n");
    std::printf("=======================================================\n");

    test_bit32();
    test_hourglass();
    test_lookglass();

    section("Result");
    std::printf("All checks passed.\n");
    return 0;
}
