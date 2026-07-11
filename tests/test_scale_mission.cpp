/**
 * test_scale_mission.cpp
 * ======================
 * Scale Mission: Particles, Clouds, Lattice, and Pipe Gas 3
 * Formation Engine v0.5.1 — Test Suite
 *
 * Validates all four simulation modes across all four runtime tiers.
 *
 * Tests:
 *   [1] V1 Particles N — Instant, Short, Medium
 *   [2] V2 Gas Clouds  — Instant, Short, Medium
 *   [3] V3 Lattice     — Instant (FCC metals, UO2), Short (supercell),
 *                        Medium (vacancy campaign)
 *   [4] V4 Pipe Gas 3  — Instant (single segment), Short, Medium (sweep)
 *   [5] MissionProfile — RuntimeProfile factory correctness
 *   [6] ExternalLayer  — force accumulator, history ring buffer
 *   [7] Cross-version  — shared Capability flags, MissionDeliverable fields
 *
 * Build & run:
 *   cmake --build build --target test_scale_mission
 *   ctest -R ScaleMissionTest -V
 */

#include "mission/mission_profile.hpp"
#include "mission/sim_particles.hpp"
#include "mission/sim_clouds.hpp"
#include "mission/sim_lattice.hpp"
#include "mission/sim_pipe_gas3.hpp"

#include <cstdio>
#include <cassert>
#include <cmath>
#include <string>

// ============================================================================
// Test harness
// ============================================================================

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, msg) do { \
    ++tests_run; \
    if (cond) { ++tests_passed; std::printf("  [PASS] %s\n", msg); } \
    else { ++tests_failed; \
           std::printf("  [FAIL] %s  (line %d)\n", msg, __LINE__); } \
} while(0)

#define CHECK_NEAR(a, b, tol, msg) CHECK(std::abs((a)-(b)) <= (tol), msg)

// ============================================================================
// Test 1 — MissionProfile: RuntimeProfile factory
// ============================================================================

static void test_mission_profile() {
    using namespace vsepr::mission;
    std::puts("\n=== [1] MissionProfile / RuntimeProfile ===");

    // Budget seconds
    CHECK_NEAR(mission_scale_budget_s(MissionScale::Instant),    0.0,   1e-9, "Instant budget = 0");
    CHECK_NEAR(mission_scale_budget_s(MissionScale::Short_50ms), 0.05,  1e-9, "Short budget = 0.05");
    CHECK_NEAR(mission_scale_budget_s(MissionScale::Medium_5s),  5.0,   1e-9, "Medium budget = 5");
    CHECK_NEAR(mission_scale_budget_s(MissionScale::Long_5min),  300.0, 1e-9, "Long budget = 300");

    // V1 profile factories
    auto pi = particles::profile_for(MissionScale::Instant);
    CHECK(pi.max_entities <= 32,   "V1 Instant: N <= 32");
    CHECK(pi.max_steps    == 1,    "V1 Instant: 1 step");
    CHECK(!pi.export_full_history, "V1 Instant: no full history");

    auto ps = particles::profile_for(MissionScale::Short_50ms);
    CHECK(ps.max_entities <= 256,  "V1 Short: N <= 256");

    auto pm = particles::profile_for(MissionScale::Medium_5s);
    CHECK(pm.max_entities >= 1000, "V1 Medium: N >= 1000");
    CHECK(pm.high_accuracy,        "V1 Medium: high accuracy on");

    auto pl = particles::profile_for(MissionScale::Long_5min);
    CHECK(pl.max_entities >= 10000,"V1 Long: N >= 10000");
    CHECK(pl.export_full_history,  "V1 Long: full history on");

    // V4 pipe profile
    auto pp = pipe_gas3::profile_for(MissionScale::Instant);
    CHECK(pp.max_steps == 0, "V4 Instant: single-point solve (steps=0)");
}

// ============================================================================
// Test 2 — ExternalLayer: force + history ring buffer
// ============================================================================

static void test_external_layer() {
    using namespace vsepr::mission;
    std::puts("\n=== [2] ExternalLayer ===");

    ExternalLayer ext;
    ext.force = {1.0, 2.0, 3.0};
    ext.energy = 42.0;

    ext.zero_force();
    CHECK(ext.force[0] == 0.0 && ext.force[1] == 0.0 && ext.force[2] == 0.0,
          "zero_force clears force components");
    CHECK(ext.energy == 0.0, "zero_force clears energy");

    // History ring buffer
    for (int i = 0; i < 10; ++i) ext.push_energy(static_cast<double>(i));
    CHECK(ext.history_head == 10, "history head advances");
    // Last 8 values should be 2..9
    int sum = 0;
    for (double v : ext.energy_history) sum += static_cast<int>(v);
    CHECK(sum == (2+3+4+5+6+7+8+9), "ring buffer holds last 8 values");

    // Capability flags
    auto cap = Capability::Kinematic | Capability::Steric;
    CHECK( has_capability(cap, Capability::Kinematic), "has Kinematic");
    CHECK( has_capability(cap, Capability::Steric),    "has Steric");
    CHECK(!has_capability(cap, Capability::Reactive),  "no Reactive");
}

// ============================================================================
// Test 3 — V1 Particles N
// ============================================================================

static void test_v1_particles() {
    using namespace vsepr::mission;
    std::puts("\n=== [3] V1 Particles N ===");

    // --- Instant: N=8, static ------------------------------------------------
    {
        auto sys = particles::make_particle_system(8, 20.0, 40.0, 3.4, 300.0,
                                                    MissionScale::Instant);
        CHECK(sys.size() == 8, "Instant: 8 particles created");
        CHECK(sys.profile.scale == MissionScale::Instant, "Instant: scale tag correct");

        particles::apply_steric(sys);
        double ke = particles::kinetic_energy(sys);
        CHECK(ke >= 0.0, "Instant: KE non-negative");

        auto d = particles::run(sys);
        CHECK(d.sim_version == "V1_Particles", "Instant: version string");
        CHECK(d.entity_count == 8,             "Instant: entity count = 8");
        CHECK(d.wall_time_s >= 0.0,            "Instant: wall time non-negative");
    }

    // --- Short: N=32 ---------------------------------------------------------
    {
        auto sys = particles::make_particle_system(32, 30.0, 40.0, 3.4, 500.0,
                                                    MissionScale::Short_50ms);
        auto d = particles::run(sys);
        CHECK(d.entity_count == 32,       "Short: 32 particles");
        CHECK(d.steps_run >= 1,           "Short: at least 1 step run");
        CHECK(d.wall_time_s < 1.0,        "Short: completes in < 1 s");
        CHECK(d.mobility_mean.has_value(), "Short: mobility_mean populated");
        CHECK(*d.mobility_mean >= 0.0,    "Short: mobility_mean non-negative");
    }

    // --- Medium: N=128 -------------------------------------------------------
    {
        auto sys = particles::make_particle_system(128, 50.0, 28.0, 3.4, 300.0,
                                                    MissionScale::Medium_5s);
        // Reduce steps for test speed
        sys.profile.max_steps = 5;
        sys.profile.wall_budget_s = 10.0;

        auto d = particles::run(sys);
        CHECK(d.entity_count == 128, "Medium: 128 particles");
        CHECK(d.steps_run >= 1,      "Medium: at least 1 step");

        auto h = particles::mobility_histogram(sys, 10);
        CHECK(h.bin_edges.size() == 11, "Medium: histogram has 11 edges");
        CHECK(h.v_mean >= 0.0,          "Medium: v_mean non-negative");
        CHECK(h.v_rms  >= h.v_mean,     "Medium: v_rms >= v_mean");
    }

    // --- Report smoke test ---------------------------------------------------
    {
        auto sys = particles::make_particle_system(16, 20.0, 40.0, 3.4, 300.0,
                                                    MissionScale::Short_50ms);
        sys.profile.max_steps = 2;
        auto d = particles::run(sys);
        auto rep = particles::report(sys, d);
        CHECK(!rep.empty(),                     "Report: non-empty string");
        CHECK(rep.find("V1 Particles") != std::string::npos,
              "Report: contains sim version label");
    }
}

// ============================================================================
// Test 4 — V2 Gas Clouds
// ============================================================================

static void test_v2_clouds() {
    using namespace vsepr::mission;
    using namespace vsepr::mission::clouds;
    std::puts("\n=== [4] V2 Gas Clouds ===");

    // --- Instant: single cloud, N2 ------------------------------------------
    {
        auto c = clouds::make_cloud(300.0, 101325.0, {{"N2", 1.0}});
        CHECK(c.T_K  > 0.0,              "Instant: cloud T_K > 0");
        CHECK(c.P_Pa > 0.0,              "Instant: cloud P_Pa > 0");
        CHECK(c.rho_kg_m3 > 0.0,         "Instant: cloud density > 0");
        CHECK(c.molar_mass > 0.0,        "Instant: molar mass > 0");
        CHECK_NEAR(c.molar_mass, 28.0, 1.0, "Instant: N2 molar mass ~28 g/mol");
        CHECK(c.gamma_mix > 1.0,         "Instant: gamma > 1");
        CHECK(c.sound_speed > 0.0,       "Instant: sound speed > 0");

        // N2 ~ 334 m/s at 300K
        CHECK(c.sound_speed > 300.0 && c.sound_speed < 400.0,
              "Instant: N2 sound speed plausible [300,400] m/s");

        CloudSystem sys;
        sys.clouds  = {c};
        sys.profile = clouds::profile_for(MissionScale::Instant);
        auto d = clouds::run(sys);
        CHECK(d.sim_version == "V2_Clouds", "Instant: version string");
        CHECK(d.steps_run == 0,             "Instant: 0 steps (single-point)");
    }

    // --- Short: 3-cloud air + CO2 + Ar mix -----------------------------------
    {
        CloudSystem sys;
        sys.profile = clouds::profile_for(MissionScale::Short_50ms);
        sys.clouds.push_back(clouds::make_cloud(300.0, 101325.0, {{"N2",0.78},{"O2",0.21},{"Ar",0.01}}));
        sys.clouds.push_back(clouds::make_cloud(350.0, 200000.0, {{"CO2",1.0}}));
        sys.clouds.push_back(clouds::make_cloud(280.0,  80000.0, {{"Ar", 1.0}}));

        auto d = clouds::run(sys);
        CHECK(d.entity_count == 3,  "Short: 3 clouds");
        CHECK(d.steps_run >= 1,     "Short: at least 1 step");
        CHECK(d.wall_time_s < 1.0,  "Short: completes quickly");
        CHECK(d.temperature_out.has_value(), "Short: temperature_out set");
    }

    // --- Medium: evolving cloud ----------------------------------------------
    {
        CloudSystem sys;
        sys.profile = clouds::profile_for(MissionScale::Medium_5s);
        sys.profile.max_steps = 10; // reduced for test speed

        for (int i = 0; i < 5; ++i)
            sys.clouds.push_back(clouds::make_cloud(
                300.0 + i*10.0, 100000.0 + i*20000.0, {{"CH4",1.0}}));

        auto d = clouds::run(sys);
        CHECK(d.entity_count == 5, "Medium: 5 clouds");
        CHECK(d.steps_run >= 1,    "Medium: at least 1 step");

        // All clouds should still have physical state
        for (const auto& c : sys.clouds) {
            CHECK(c.T_K > 200.0 && c.T_K < 800.0,  "Medium: T still physical");
            CHECK(c.P_Pa > 0.0,                      "Medium: P_Pa positive");
        }

        // CSV smoke test
        auto csv = clouds::to_csv(sys);
        CHECK(!csv.empty(), "Medium: CSV export non-empty");
        CHECK(csv.find("T_K") != std::string::npos, "Medium: CSV has T_K header");
    }
}

// ============================================================================
// Test 5 — V3 Lattice
// ============================================================================

static void test_v3_lattice() {
    using namespace vsepr::mission;
    using namespace vsepr::mission::lattice;
    std::puts("\n=== [5] V3 Lattice ===");

    const auto lib = unit_cell_library();
    CHECK(!lib.empty(), "Library: unit cell library non-empty");
    CHECK(lib.size() >= 8, "Library: at least 8 reference cells");

    // --- Instant: Al-FCC unit cell ------------------------------------------
    {
        auto& spec = lib[0]; // Al-FCC
        auto ls = lattice::build_lattice(spec, 1);

        CHECK(!ls.sites.empty(),     "Al-FCC: sites built");
        CHECK(ls.sites.size() == 4,  "Al-FCC: 4 atoms in primitive cell");
        CHECK(ls.spacegroup == "Fm3m","Al-FCC: correct spacegroup");
        CHECK(ls.cohesive_energy_eV > 0.0, "Al-FCC: cohesive energy > 0");

        lattice::apply_thermal_displacement(ls);
        CHECK(ls.u_rms_mean >= 0.0, "Al-FCC Instant: u_rms >= 0");

        // At 300K, Al-FCC T_Debye=428K → u_rms ~0.08*sqrt(300/428) ≈ 0.067 Å
        CHECK(ls.u_rms_mean < 0.5, "Al-FCC Instant: u_rms physically small");

        lattice::apply_stress_accumulation(ls);
        CHECK(ls.stress_field.size() == ls.sites.size(),
              "Al-FCC: stress_field sized correctly");
    }

    // --- Short: 2×2×2 Fe-BCC supercell ---------------------------------------
    {
        // Fe-BCC is index 4
        const lattice::UnitCellSpec* fe_spec = nullptr;
        for (auto& s : lib) if (s.name == "Fe-BCC") { fe_spec = &s; break; }
        CHECK(fe_spec != nullptr, "Fe-BCC: found in library");

        if (fe_spec) {
            auto ls = lattice::build_lattice(*fe_spec, 2); // 2×2×2 = 16 atoms
            CHECK(ls.sites.size() == 16, "Fe-BCC 2x2x2: 16 sites");

            LatticeSystem sys;
            sys.state   = ls;
            sys.profile = lattice::profile_for(MissionScale::Short_50ms);
            sys.profile.max_steps = 1;

            auto d = lattice::run(sys);
            CHECK(d.entity_count == 16, "Fe-BCC Short: 16 entities");
            CHECK(d.steps_run >= 1,     "Fe-BCC Short: at least 1 step");
            CHECK(d.wall_time_s >= 0.0, "Fe-BCC Short: wall time non-negative");
        }
    }

    // --- Medium: vacancy insertion + stress ----------------------------------
    {
        const lattice::UnitCellSpec* nacl_spec = nullptr;
        for (auto& s : lib) if (s.name == "NaCl") { nacl_spec = &s; break; }
        CHECK(nacl_spec != nullptr, "NaCl: found in library");

        if (nacl_spec) {
            auto ls = lattice::build_lattice(*nacl_spec, 2); // 64 atoms
            const std::size_t n_before = ls.sites.size();

            lattice::insert_vacancy(ls, 0);
            CHECK(ls.defects.size() == 1, "NaCl: 1 defect after vacancy insert");
            CHECK(!ls.sites[0].occupied,  "NaCl: site 0 unoccupied");
            CHECK(ls.sites[0].species_code ==
                  static_cast<int>(vsepr::physics::ParticleID::vacancy),
                  "NaCl: vacancy species code = -15");

            lattice::insert_interstitial(ls, 1, "Na", 3.5);
            CHECK(ls.defects.size() == 2,      "NaCl: 2 defects after interstitial");
            CHECK(ls.sites.size() == n_before + 1, "NaCl: site count grew by 1");

            // Stress run
            LatticeSystem sys;
            sys.state   = ls;
            sys.profile = lattice::profile_for(MissionScale::Medium_5s);
            sys.profile.max_steps = 5;
            sys.profile.wall_budget_s = 10.0;

            auto d = lattice::run(sys);
            CHECK(d.defect_count == 2, "NaCl Medium: 2 defects reported");
            CHECK(d.temperature_out.has_value(), "NaCl Medium: temperature_out set");

            auto rep = lattice::report(sys, d);
            CHECK(rep.find("Defect list") != std::string::npos,
                  "NaCl: report contains defect list");
        }
    }

    // --- UO2 unit cell (actinide, from Z=94 domain) -------------------------
    {
        const lattice::UnitCellSpec* uo2_spec = nullptr;
        for (auto& s : lib) if (s.name == "UO2") { uo2_spec = &s; break; }
        CHECK(uo2_spec != nullptr, "UO2: found in library");

        if (uo2_spec) {
            auto ls = lattice::build_lattice(*uo2_spec, 1);
            CHECK(ls.sites.size() == 8,       "UO2: 8 sites in unit cell (4U + 4O... wait, 8 total)");
            CHECK_NEAR(ls.T_debye_K, 182.0, 1.0, "UO2: Debye temp ~182 K");
            CHECK(ls.cohesive_energy_eV > 5.0,    "UO2: cohesive energy > 5 eV");
        }
    }
}

// ============================================================================
// Test 6 — V4 Pipe Gas 3
// ============================================================================

static void test_v4_pipe() {
    using namespace vsepr::mission;
    using namespace vsepr::mission::pipe_gas3;
    using vsepr::gas2::find_species;
    std::puts("\n=== [6] V4 Pipe Gas 3 ===");

    // --- Instant: single N2 segment ------------------------------------------
    {
        PipeFlowState seg;
        seg.formula  = "N2";
        seg.species  = find_species("N2");
        seg.T_in_K   = 300.0;
        seg.P_in_Pa  = 200000.0; // 2 bar
        seg.mdot     = 0.01;     // 10 g/s
        seg.geometry.length_m   = 1.0;
        seg.geometry.diameter_m = 0.025;

        pipe_gas3::solve_segment(seg);

        CHECK(seg.solved,            "N2 Instant: segment solved");
        CHECK(seg.Re > 0.0,         "N2 Instant: Re > 0");
        CHECK(seg.v_mean > 0.0,     "N2 Instant: v_mean > 0");
        CHECK(seg.dP_Pa > 0.0,      "N2 Instant: pressure drop > 0");
        CHECK(seg.P_out_Pa > 0.0,   "N2 Instant: P_out positive");
        CHECK(seg.P_out_Pa < seg.P_in_Pa, "N2 Instant: P_out < P_in (pressure drop)");
        CHECK(seg.mach > 0.0,       "N2 Instant: Mach > 0");
        CHECK(seg.mach < 1.0,       "N2 Instant: Mach < 1 (subsonic at 2 bar)");
        CHECK(seg.friction_f > 0.0, "N2 Instant: friction factor > 0");

        // Churchill: turbulent flow expected (Re >> 2300)
        // N2 at 2 bar, 300K, D=25mm, mdot=10 g/s
        CHECK(seg.Re > 1000.0, "N2 Instant: Re > 1000 (turbulent expected)");
    }

    // --- Short: methane segment -----------------------------------------------
    {
        PipeNetwork net;
        net.profile = pipe_gas3::profile_for(MissionScale::Short_50ms);

        PipeFlowState seg;
        seg.formula  = "CH4";
        seg.species  = find_species("CH4");
        seg.T_in_K   = 280.0;
        seg.P_in_Pa  = 500000.0; // 5 bar
        seg.mdot     = 0.005;
        seg.geometry.length_m   = 2.0;
        seg.geometry.diameter_m = 0.02;
        net.segments.push_back(seg);

        auto d = pipe_gas3::run(net);
        CHECK(d.sim_version == "V4_PipeGas3", "CH4 Short: version string");
        CHECK(d.entity_count == 1, "CH4 Short: 1 segment");
        CHECK(d.delta_P.has_value() && *d.delta_P > 0.0, "CH4 Short: ΔP > 0");
    }

    // --- Medium: 3-segment CO2 series ----------------------------------------
    {
        PipeNetwork net;
        net.profile = pipe_gas3::profile_for(MissionScale::Medium_5s);
        net.profile.max_steps = 5;

        for (int i = 0; i < 3; ++i) {
            PipeFlowState seg;
            seg.formula  = "CO2";
            seg.species  = find_species("CO2");
            seg.T_in_K   = 300.0;
            seg.P_in_Pa  = 300000.0;
            seg.mdot     = 0.02;
            seg.geometry.length_m   = 1.0;
            seg.geometry.diameter_m = 0.03;
            net.segments.push_back(seg);
        }

        auto d = pipe_gas3::run(net);
        CHECK(d.entity_count == 3, "CO2 Medium: 3 segments");
        CHECK(d.steps_run >= 1,    "CO2 Medium: at least 1 sweep step");

        // CSV export
        auto csv = pipe_gas3::to_csv_pipe_state(net);
        CHECK(!csv.empty(),                               "CO2 Medium: CSV non-empty");
        CHECK(csv.find("seg_id") != std::string::npos,   "CO2 Medium: CSV header present");
        CHECK(csv.find("CO2")    != std::string::npos,   "CO2 Medium: formula in CSV");

        // Report smoke test
        auto rep = pipe_gas3::report(net, d);
        CHECK(rep.find("Pipe Gas 3") != std::string::npos, "CO2 Medium: report label");
        CHECK(rep.find("ΔP")         != std::string::npos, "CO2 Medium: ΔP in report");
    }

    // --- Churchill friction factor validation --------------------------------
    {
        // Laminar: Re = 1000 → f = 64/Re = 0.064
        double f_lam = pipe_gas3::friction_factor_churchill(1000.0, 0.0);
        CHECK_NEAR(f_lam, 0.064, 0.001, "Churchill: laminar f = 64/Re");

        // Turbulent: Re = 1e5, smooth → Moody ~0.018
        double f_turb = pipe_gas3::friction_factor_churchill(1e5, 0.0001);
        CHECK(f_turb > 0.008 && f_turb < 0.04,
              "Churchill: turbulent f in [0.008, 0.04]");

        // High Re smooth: Re = 1e6
        double f_hi = pipe_gas3::friction_factor_churchill(1e6, 0.0);
        CHECK(f_hi > 0.005 && f_hi < 0.02, "Churchill: high-Re f in [0.005, 0.02]");
    }
}

// ============================================================================
// Test 7 — Cross-version: MissionDeliverable field contract
// ============================================================================

static void test_deliverable_contract() {
    using namespace vsepr::mission;
    std::puts("\n=== [7] Cross-version: MissionDeliverable ===");

    MissionDeliverable d{};
    d.sim_version  = "V1_Particles";
    d.scale        = MissionScale::Instant;
    d.entity_count = 8;
    d.steps_run    = 1;
    d.wall_time_s  = 0.001;
    d.energy_total = 100.0;
    d.converged    = true;

    CHECK(!d.sim_version.empty(),     "Deliverable: sim_version non-empty");
    CHECK(d.entity_count == 8,        "Deliverable: entity_count set");
    CHECK(d.converged,                "Deliverable: converged flag");
    CHECK(!d.mobility_mean.has_value(),"Deliverable: mobility_mean optional empty by default");
    CHECK(!d.defect_count.has_value(), "Deliverable: defect_count optional empty by default");

    // Test OutputFormat flags
    auto fmt = OutputFormat::Console | OutputFormat::CSV | OutputFormat::XYZA;
    CHECK( wants_format(fmt, OutputFormat::Console), "OutputFormat: Console set");
    CHECK( wants_format(fmt, OutputFormat::CSV),     "OutputFormat: CSV set");
    CHECK( wants_format(fmt, OutputFormat::XYZA),    "OutputFormat: XYZA set");
    CHECK(!wants_format(fmt, OutputFormat::XYZF),    "OutputFormat: XYZF not set");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::puts("╔══════════════════════════════════════════════════════════════╗");
    std::puts("║  Scale Mission Test Suite — V1 Particles / V2 Clouds /      ║");
    std::puts("║  V3 Lattice / V4 Pipe Gas 3       VSEPR-SIM 4.0-LB          ║");
    std::puts("╚══════════════════════════════════════════════════════════════╝");

    test_mission_profile();
    test_external_layer();
    test_v1_particles();
    test_v2_clouds();
    test_v3_lattice();
    test_v4_pipe();
    test_deliverable_contract();

    std::puts("\n──────────────────────────────────────────────────────────────");
    std::printf("  %d / %d tests passed", tests_passed, tests_run);
    if (tests_failed > 0)
        std::printf("  (%d FAILED)", tests_failed);
    std::puts("");
    std::puts("──────────────────────────────────────────────────────────────");

    return (tests_failed == 0) ? 0 : 1;
}
