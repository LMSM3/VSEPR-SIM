/**
 * test_seed_bead_stepper.cpp — Tests for the 6+9 Unified Step Function
 *
 * Validates:
 *   1. SeedBeadStepper initialisation
 *   2. Single step execution (all 15 units produce diagnostics)
 *   3. Full run to steady state
 *   4. SnapshotGraphCollector accumulates data
 *   5. Metal/gas study configuration and report generation
 *   6. Determinism (same input → same output)
 */

#include "coarse_grain/models/seed_bead_stepper.hpp"
#include "coarse_grain/report/snapshot_graph.hpp"
#include "coarse_grain/report/metal_gas_study.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace coarse_grain;
using namespace atomistic::validation::reference;

static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "FAIL: " << msg << " (line " << __LINE__ << ")\n"; \
        ++tests_failed; \
    } else { \
        ++tests_passed; \
    } \
} while(0)

// Build a minimal test system
static BeadSystem make_test_system(uint32_t n = 8) {
    BeadSystem sys;
    BeadType bt;
    bt.name = "TEST";
    bt.id = 0;
    bt.sigma = 3.5;
    bt.epsilon = 0.1;
    sys.bead_types.push_back(bt);

    double spacing = 4.0;
    int side = static_cast<int>(std::ceil(std::cbrt(static_cast<double>(n))));
    uint32_t count = 0;
    for (int x = 0; x < side && count < n; ++x) {
        for (int y = 0; y < side && count < n; ++y) {
            for (int z = 0; z < side && count < n; ++z) {
                Bead b;
                b.position = {spacing * x, spacing * y, spacing * z};
                b.mass = 40.0;
                b.type_id = 0;
                sys.beads.push_back(b);
                ++count;
            }
        }
    }
    sys.source_atom_count = n;
    return sys;
}

// Test 1: Initialisation
void test_init() {
    std::cout << "Test 1: Initialisation...\n";
    auto sys = make_test_system(8);
    std::vector<EnvironmentState> env(8);
    SeedBeadParams params;

    SeedBeadStepper::init(sys, env, params);

    CHECK(env.size() == 8, "env_states sized correctly");
    for (auto& e : env) {
        CHECK(e.eta == 0.0, "eta initialised to 0");
    }
}

// Test 2: Single step produces valid diagnostics
void test_single_step() {
    std::cout << "Test 2: Single step diagnostics...\n";
    auto sys = make_test_system(8);
    std::vector<EnvironmentState> env(8);
    std::vector<atomistic::Vec3> vel(8);
    std::vector<atomistic::Vec3> forces(8);
    FIREState fire;
    fire.dt = 1.0;
    fire.alpha = 0.1;
    SeedBeadParams params;
    params.record_positions = true;
    params.snapshot_interval = 1;

    SeedBeadStepper::init(sys, env, params);
    auto record = SeedBeadStepper::step(sys, env, vel, forces, fire, params, 0);

    // SEED unit checks
    CHECK(record.seed_status.s1_vsepr_active, "S1 active");
    CHECK(record.seed_status.s2_verlet_active, "S2 active");
    CHECK(record.seed_status.s3_fire_active, "S3 active");
    CHECK(record.seed_status.s4_forces_evaluated, "S4 active");
    CHECK(record.seed_status.s5_stats_accumulated, "S5 active");

    // BEAD unit checks
    CHECK(record.bead_status.b1_mapping_done, "B1 done");
    CHECK(record.bead_status.b2_density_done, "B2 done");
    CHECK(record.bead_status.b3_coordination_done, "B3 done");
    CHECK(record.bead_status.b4_order_done, "B4 done");
    CHECK(record.bead_status.b5_target_done, "B5 done");
    CHECK(record.bead_status.b6_eta_integrated, "B6 done");
    CHECK(record.bead_status.b7_mod_steric, "B7 done");
    CHECK(record.bead_status.b8_mod_elec, "B8 done");
    CHECK(record.bead_status.b9_mod_disp, "B9 done");

    // Numeric sanity
    CHECK(std::isfinite(record.total_energy), "energy is finite");
    CHECK(std::isfinite(record.rms_force), "rms_force is finite");
    CHECK(record.rms_force >= 0.0, "rms_force >= 0");
    CHECK(record.avg_eta >= 0.0 && record.avg_eta <= 1.0, "eta in [0,1]");
    CHECK(!record.bead_positions.empty(), "snapshot captured");
    CHECK(record.bead_positions.size() == 8, "snapshot has 8 positions");
}

// Test 3: Full run to steady state (or max steps)
void test_full_run() {
    std::cout << "Test 3: Full run...\n";
    auto sys = make_test_system(8);
    SeedBeadParams params;
    params.max_steps = 200;
    params.f_tol = 0.1;
    params.eta_tol = 0.01;
    params.snapshot_interval = 20;

    auto result = SeedBeadStepper::run(sys, params);

    CHECK(result.steps_taken > 0, "at least 1 step taken");
    CHECK(result.steps_taken <= 200, "within max_steps");
    CHECK(!result.history.empty(), "history not empty");
    CHECK(std::isfinite(result.final_energy), "final energy finite");
    CHECK(std::isfinite(result.final_rms_force), "final force finite");
}

// Test 4: SnapshotGraphCollector
void test_collector() {
    std::cout << "Test 4: SnapshotGraphCollector...\n";
    auto sys = make_test_system(8);
    std::vector<EnvironmentState> env(8);
    std::vector<atomistic::Vec3> vel(8);
    std::vector<atomistic::Vec3> forces(8);
    FIREState fire;
    fire.dt = 1.0;
    fire.alpha = 0.1;
    SeedBeadParams params;
    params.record_positions = true;
    params.snapshot_interval = 5;

    SeedBeadStepper::init(sys, env, params);
    SnapshotGraphCollector collector;
    collector.snapshot_interval = 5;

    for (uint64_t s = 0; s < 20; ++s) {
        auto record = SeedBeadStepper::step(sys, env, vel, forces, fire, params, s);
        collector.record(record);
    }

    CHECK(collector.energy_series.points.size() == 20, "20 energy points");
    CHECK(collector.rms_force_series.points.size() == 20, "20 force points");
    CHECK(collector.avg_eta_series.points.size() == 20, "20 eta points");
    CHECK(!collector.snapshots.empty(), "snapshots captured");

    // CSV export (to string streams)
    bool csv_ok = collector.export_timeseries_csv("test_timeseries.csv");
    CHECK(csv_ok, "timeseries CSV exported");
    bool snap_ok = collector.export_snapshots_csv("test_snapshots.csv");
    CHECK(snap_ok, "snapshots CSV exported");

    // Cleanup
    std::remove("test_timeseries.csv");
    std::remove("test_snapshots.csv");
}

// Test 5: Metal/gas study configuration
void test_material_study() {
    std::cout << "Test 5: Material study config...\n";

    auto refs = all_nanoparticle_refs();
    CHECK(refs.size() == 6, "6 reference materials");

    for (auto& ref : refs) {
        auto cfg = build_study_config(ref, 8);
        CHECK(!cfg.material_label.empty(), "label set for " + ref.label);
        CHECK(cfg.system.beads.size() == 8, "8 beads built for " + ref.label);
        CHECK(!cfg.system.bead_types.empty(), "bead type set for " + ref.label);
        CHECK(cfg.system.bead_types[0].sigma > 0, "sigma > 0 for " + ref.label);
        CHECK(cfg.system.bead_types[0].epsilon > 0, "epsilon > 0 for " + ref.label);
    }

    // Classify
    CHECK(classify_material(gold_5nm_fcc()) == MaterialClass::Metal_FCC, "Au is FCC");
    CHECK(classify_material(iron_5nm_bcc()) == MaterialClass::Metal_BCC, "Fe is BCC");
    CHECK(classify_material(argon_5nm_cluster()) == MaterialClass::Noble_Gas, "Ar is gas");
}

// Test 6: Determinism
void test_determinism() {
    std::cout << "Test 6: Determinism...\n";

    auto sys1 = make_test_system(8);
    auto sys2 = make_test_system(8);
    SeedBeadParams params;
    params.max_steps = 50;

    auto r1 = SeedBeadStepper::run(sys1, params);
    auto r2 = SeedBeadStepper::run(sys2, params);

    CHECK(r1.steps_taken == r2.steps_taken, "same step count");
    CHECK(r1.final_energy == r2.final_energy, "same final energy");
    CHECK(r1.final_rms_force == r2.final_rms_force, "same final force");
    CHECK(r1.final_avg_eta == r2.final_avg_eta, "same final eta");
}

// Test 7: Report generation
void test_report() {
    std::cout << "Test 7: Report generation...\n";
    auto sys = make_test_system(8);
    SeedBeadParams params;
    params.max_steps = 50;
    params.snapshot_interval = 10;

    auto result = SeedBeadStepper::run(sys, params);
    SnapshotGraphCollector collector;
    for (auto& rec : result.history) collector.record(rec);

    std::string report = assemble_seed_bead_report(
        "Test Report", params, result, collector);

    CHECK(!report.empty(), "report not empty");
    CHECK(report.find("Outcome") != std::string::npos, "report has outcome section");
    CHECK(report.find("6+9") != std::string::npos, "report references 6+9");
    CHECK(report.find("BEAD") != std::string::npos, "report has BEAD layer");
}

int main() {
    std::cout << "=== Seed & Bead Stepper Test Suite ===\n\n";

    test_init();
    test_single_step();
    test_full_run();
    test_collector();
    test_material_study();
    test_determinism();
    test_report();

    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << tests_passed << "\n";
    std::cout << "Failed: " << tests_failed << "\n";

    return tests_failed > 0 ? 1 : 0;
}
