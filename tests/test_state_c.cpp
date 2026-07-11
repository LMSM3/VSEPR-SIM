/**
 * test_state_c.cpp
 *
 * Tests for the Dev Day ~42 revised notation: StateC, OutcomeState,
 * EnergyBalance, evolve(), Composition, ModelLevel, and the full
 * transformation pipeline.
 *
 * Build: linked against atomistic (INTERFACE or STATIC library)
 */

#include "atomistic/core/state_c.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

static constexpr double TOL = 1e-8;

static bool approx(double a, double b, double rel = 1e-6) {
    if (std::abs(b) < 1e-30) return std::abs(a) < 1e-10;
    return std::abs(a - b) / std::abs(b) < rel;
}

// ============================================================================
// Composition enum
// ============================================================================

static void test_composition_names() {
    using namespace atomistic;
    assert(std::string(composition_name(Composition::Atomistic)) == "atomistic");
    assert(std::string(composition_name(Composition::Bead))      == "bead");
    assert(std::string(composition_name(Composition::Hybrid))    == "hybrid");
    assert(std::string(composition_name(Composition::Macro))     == "macro");
    std::cout << "  [PASS] composition_names\n";
}

// ============================================================================
// ModelLevel enum
// ============================================================================

static void test_model_level_names() {
    using namespace atomistic;
    assert(std::string(model_level_name(ModelLevel::C_Empirical))   == "C-Empirical");
    assert(std::string(model_level_name(ModelLevel::C_Reactive))    == "C-Reactive");
    assert(std::string(model_level_name(ModelLevel::C_Polarizable)) == "C-Polarizable");
    assert(std::string(model_level_name(ModelLevel::CG_Reduced))    == "CG-Reduced");
    assert(std::string(model_level_name(ModelLevel::CG_Enriched))   == "CG-Enriched");
    assert(std::string(model_level_name(ModelLevel::Hybrid_AdRes))  == "Hybrid-AdRes");
    std::cout << "  [PASS] model_level_names\n";
}

// ============================================================================
// Environment struct
// ============================================================================

static void test_environment_defaults() {
    using namespace atomistic;
    Environment env;
    assert(env.temperature == 0.0);
    assert(env.pressure == 0.0);
    assert(!env.has_fields());
    assert(!env.has_flow());
    std::cout << "  [PASS] environment_defaults\n";
}

static void test_environment_fields() {
    using namespace atomistic;
    Environment env;
    env.E_field[0] = 1.0;
    assert(env.has_fields());
    assert(!env.has_flow());

    env.shear_rate = 0.5;
    assert(env.has_flow());
    std::cout << "  [PASS] environment_fields\n";
}

// ============================================================================
// Constraints struct
// ============================================================================

static void test_constraints_defaults() {
    using namespace atomistic;
    Constraints k;
    assert(!k.is_fully_constrained());
    assert(!k.is_reactive());
    assert(k.max_coordination == 0);
    assert(k.frozen_atoms.empty());
    std::cout << "  [PASS] constraints_defaults\n";
}

static void test_constraints_reactive() {
    using namespace atomistic;
    Constraints k;
    k.allow_bond_breaking = true;
    assert(k.is_reactive());
    assert(!k.is_fully_constrained());

    k.fix_positions = true;
    assert(k.is_fully_constrained());
    std::cout << "  [PASS] constraints_reactive\n";
}

// ============================================================================
// SourceTag struct
// ============================================================================

static void test_source_tag() {
    using namespace atomistic;
    SourceTag tag;
    tag.origin = "file:water.xyz";
    tag.engine_version = "VSEPR-SIM 2.6.0";
    tag.hash = "abc123";
    tag.creation_step = 42;
    tag.parent_id = 0;
    tag.notes = "test";

    assert(tag.origin == "file:water.xyz");
    assert(tag.creation_step == 42);
    std::cout << "  [PASS] source_tag\n";
}

// ============================================================================
// StateC construction and from_state factory
// ============================================================================

static void test_state_c_default() {
    using namespace atomistic;
    StateC sc;
    assert(sc.mass == 0.0);
    assert(sc.energy == 0.0);
    assert(sc.comp == Composition::Atomistic);
    assert(sc.detail == nullptr);
    std::cout << "  [PASS] state_c_default\n";
}

static void test_state_c_from_state() {
    using namespace atomistic;

    // Build a simple 3-particle state
    State s;
    s.N = 3;
    s.X = {{0,0,0}, {1,0,0}, {0,1,0}};
    s.V = {{0.1, 0, 0}, {-0.1, 0, 0}, {0, 0.1, 0}};
    s.M = {16.0, 1.0, 1.0};  // O, H, H
    s.Q = {-0.8, 0.4, 0.4};
    s.type = {8, 1, 1};

    s.E.Ubond = -50.0;
    s.E.UvdW  = 2.0;

    SourceTag tag;
    tag.origin = "test:water";

    StateC sc = StateC::from_state(s, tag);

    // Check mass
    assert(approx(sc.mass, 18.0));

    // Check energy
    assert(approx(sc.energy, -48.0));

    // Check COM position: (16*0 + 1*1 + 1*0) / 18, (16*0 + 1*0 + 1*1) / 18, 0
    assert(approx(sc.position.x, 1.0/18.0));
    assert(approx(sc.position.y, 1.0/18.0));
    assert(approx(sc.position.z, 0.0));

    // Check COM velocity: (16*0.1 + 1*(-0.1) + 1*0) / 18
    assert(approx(sc.velocity.x, 1.5/18.0));

    // Check composition
    assert(sc.comp == Composition::Atomistic);

    // Check provenance
    assert(sc.provenance.origin == "test:water");

    // Check detail pointer
    assert(sc.detail == &s);

    std::cout << "  [PASS] state_c_from_state\n";
}

// ============================================================================
// EnergyBalance
// ============================================================================

static void test_energy_balance_sane() {
    using namespace atomistic;
    EnergyBalance eb;
    eb.E_0 = 100.0;
    eb.E_in = 5.0;
    eb.E_out = 3.0;
    eb.E_f = 101.0;
    eb.E_loss = 1.0;
    // (100 + 5 - 3) = 102 vs (101 + 1) = 102 => drift = 0
    assert(eb.energy_sane());
    assert(approx(eb.energy_drift(), 0.0));
    std::cout << "  [PASS] energy_balance_sane\n";
}

static void test_energy_balance_drift() {
    using namespace atomistic;
    EnergyBalance eb;
    eb.E_0 = 100.0;
    eb.E_in = 5.0;
    eb.E_out = 3.0;
    eb.E_f = 110.0;  // Wrong: should be ~101
    eb.E_loss = 1.0;
    // (100 + 5 - 3) = 102 vs (110 + 1) = 111 => drift = 9
    assert(!eb.energy_sane());
    assert(approx(eb.energy_drift(), 9.0));
    std::cout << "  [PASS] energy_balance_drift\n";
}

static void test_mass_balance() {
    using namespace atomistic;
    EnergyBalance eb;
    eb.m_0 = 18.0;
    eb.m_in = 0.0;
    eb.m_out = 0.0;
    eb.m_f = 18.0;
    assert(eb.mass_sane());
    assert(approx(eb.mass_drift(), 0.0));

    eb.m_f = 17.5;
    assert(!eb.mass_sane());
    std::cout << "  [PASS] mass_balance\n";
}

static void test_energy_balance_check() {
    using namespace atomistic;
    EnergyBalance eb;
    eb.E_0 = 100.0; eb.E_in = 0; eb.E_out = 0;
    eb.E_f = 100.0; eb.E_loss = 0;
    eb.m_0 = 18.0; eb.m_in = 0; eb.m_out = 0; eb.m_f = 18.0;

    QualityFlags q = eb.check();
    assert(q.energy_conserved);
    assert(q.mass_conserved);
    assert(q.is_trustworthy());
    assert(q.energy_drift < 1e-10);
    std::cout << "  [PASS] energy_balance_check\n";
}

// ============================================================================
// EventLog
// ============================================================================

static void test_event_log() {
    using namespace atomistic;
    EventLog log;
    log.record(10, "bond_break", "O-H bond broken", -50.0);
    log.record(20, "bond_form", "O-C bond formed", -30.0);
    log.record(30, "warning", "energy drift", 0.1);

    assert(log.events.size() == 3);
    assert(log.count_by_type("bond_break") == 1);
    assert(log.count_by_type("warning") == 1);
    assert(approx(log.total_energy_delta(), -79.9));
    std::cout << "  [PASS] event_log\n";
}

// ============================================================================
// QualityFlags
// ============================================================================

static void test_quality_flags() {
    using namespace atomistic;
    QualityFlags q;
    assert(q.is_trustworthy());

    q.energy_conserved = false;
    assert(!q.is_trustworthy());

    q.energy_conserved = true;
    q.mass_conserved = false;
    assert(!q.is_trustworthy());
    std::cout << "  [PASS] quality_flags\n";
}

// ============================================================================
// OutcomeState
// ============================================================================

static void test_outcome_state_default() {
    using namespace atomistic;
    OutcomeState sf;
    assert(sf.mass == 0.0);
    assert(sf.energy == 0.0);
    assert(sf.structure.geometry_class.empty());
    assert(sf.stability.is_converged == false);
    assert(sf.events.events.empty());
    assert(sf.quality.is_trustworthy());
    assert(sf.detail == nullptr);
    std::cout << "  [PASS] outcome_state_default\n";
}

// ============================================================================
// evolve() — full TransformConfig version
// ============================================================================

static void test_evolve_invalid_mass() {
    using namespace atomistic;
    StateC s0;
    s0.mass = 0.0;  // Invalid
    s0.energy = 10.0;

    TransformConfig config;
    OutcomeState sf = evolve(s0, config);

    assert(!sf.quality.energy_conserved);
    assert(sf.quality.warning_count > 0);
    assert(sf.events.events.size() == 1);
    assert(sf.events.events[0].type == "failure");
    std::cout << "  [PASS] evolve_invalid_mass\n";
}

static void test_evolve_invalid_energy() {
    using namespace atomistic;
    StateC s0;
    s0.mass = 18.0;
    s0.energy = std::numeric_limits<double>::infinity();

    TransformConfig config;
    OutcomeState sf = evolve(s0, config);

    assert(!sf.quality.energy_conserved);
    assert(sf.events.events[0].type == "failure");
    std::cout << "  [PASS] evolve_invalid_energy\n";
}

static void test_evolve_summary_level() {
    using namespace atomistic;
    StateC s0;
    s0.mass = 18.0;
    s0.energy = -48.0;
    s0.provenance.origin = "test:evolve";

    TransformConfig config;
    config.n_steps = 100;
    config.level = ModelLevel::C_Empirical;

    OutcomeState sf = evolve(s0, config);

    // Without a detail State, evolve runs summary-level
    assert(approx(sf.mass, 18.0));
    assert(approx(sf.energy, -48.0));
    assert(sf.quality.energy_conserved);
    assert(sf.quality.mass_conserved);
    assert(sf.origin.origin == "test:evolve");
    std::cout << "  [PASS] evolve_summary_level\n";
}

static void test_evolve_with_state() {
    using namespace atomistic;

    // Build a sane State
    State s;
    s.N = 2;
    s.X = {{0,0,0}, {1,0,0}};
    s.V = {{0,0,0}, {0,0,0}};
    s.M = {12.0, 16.0};
    s.Q = {0.0, 0.0};
    s.type = {6, 8};

    s.E.Ubond = -25.0;

    StateC s0 = StateC::from_state(s);

    TransformConfig config;
    config.n_steps = 10;
    config.level = ModelLevel::C_Empirical;

    OutcomeState sf = evolve(s0, config);

    assert(approx(sf.mass, 28.0));
    assert(sf.quality.energy_conserved);
    assert(sf.stability.energy_per_particle != 0.0);
    std::cout << "  [PASS] evolve_with_state\n";
}

// ============================================================================
// evolve() — simplified (dt, level) signature
// ============================================================================

static void test_evolve_simplified() {
    using namespace atomistic;

    StateC s0;
    s0.mass = 18.0;
    s0.energy = -48.0;
    s0.position = {1.0, 2.0, 3.0};
    s0.velocity = {0.1, 0.0, 0.0};
    s0.comp = Composition::Atomistic;
    s0.provenance.origin = "test:simplified";

    StateC s1 = evolve(s0, 0.001, ModelLevel::C_Empirical);

    // Mass and energy preserved (no integrator running)
    assert(approx(s1.mass, 18.0));
    assert(approx(s1.energy, -48.0));

    // Composition preserved
    assert(s1.comp == Composition::Atomistic);

    // Provenance chain extended
    assert(s1.provenance.origin == "test:simplified");
    assert(s1.provenance.notes.find("evolve") != std::string::npos);
    assert(s1.provenance.notes.find("C-Empirical") != std::string::npos);
    assert(s1.provenance.creation_step == 1);

    std::cout << "  [PASS] evolve_simplified\n";
}

static void test_evolve_chain() {
    using namespace atomistic;

    StateC s0;
    s0.mass = 18.0;
    s0.energy = -48.0;
    s0.provenance.origin = "test:chain";

    // Chain: T(T(T(S_0)))
    StateC s1 = evolve(s0, 0.1, ModelLevel::C_Empirical);
    StateC s2 = evolve(s1, 0.001, ModelLevel::C_Polarizable);
    StateC s3 = evolve(s2, 1.0, ModelLevel::CG_Reduced);

    assert(s3.provenance.creation_step == 3);
    assert(s3.provenance.notes.find("C-Empirical") != std::string::npos);
    assert(s3.provenance.notes.find("C-Polarizable") != std::string::npos);
    assert(s3.provenance.notes.find("CG-Reduced") != std::string::npos);
    std::cout << "  [PASS] evolve_chain\n";
}

// ============================================================================
// StructuralState / StabilityMetric / PerformanceProperties
// ============================================================================

static void test_structural_state() {
    using namespace atomistic;
    StructuralState phi;
    phi.geometry_class = "tetrahedral";
    phi.topology = "branched";
    phi.symmetry_score = 0.95;
    phi.coordination = 4;

    assert(phi.geometry_class == "tetrahedral");
    assert(phi.coordination == 4);
    std::cout << "  [PASS] structural_state\n";
}

static void test_stability_metric() {
    using namespace atomistic;
    StabilityMetric stab;
    stab.energy_per_particle = -5.0;
    stab.rms_force = 0.001;
    stab.is_local_minimum = true;
    stab.is_converged = true;

    assert(stab.is_converged);
    assert(stab.is_local_minimum);
    std::cout << "  [PASS] stability_metric\n";
}

static void test_performance_properties() {
    using namespace atomistic;
    PerformanceProperties pi;
    pi.diffusion_coeff = 2.3e-5;
    pi.density = 1.0;
    pi.bulk_modulus = 2.2;

    assert(pi.diffusion_coeff > 0.0);
    assert(pi.density > 0.0);
    std::cout << "  [PASS] performance_properties\n";
}

// ============================================================================
// TransformConfig
// ============================================================================

static void test_transform_config_defaults() {
    using namespace atomistic;
    TransformConfig cfg;
    assert(approx(cfg.dt, 0.001));
    assert(cfg.n_steps == 1000);
    assert(cfg.level == ModelLevel::C_Empirical);
    assert(cfg.track_events);
    assert(!cfg.compute_properties);
    std::cout << "  [PASS] transform_config_defaults\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== Dev Day ~42: StateC Revised Notation Tests ===\n\n";

    std::cout << "--- Composition ---\n";
    test_composition_names();

    std::cout << "\n--- ModelLevel ---\n";
    test_model_level_names();

    std::cout << "\n--- Environment ---\n";
    test_environment_defaults();
    test_environment_fields();

    std::cout << "\n--- Constraints ---\n";
    test_constraints_defaults();
    test_constraints_reactive();

    std::cout << "\n--- SourceTag ---\n";
    test_source_tag();

    std::cout << "\n--- StateC ---\n";
    test_state_c_default();
    test_state_c_from_state();

    std::cout << "\n--- EnergyBalance ---\n";
    test_energy_balance_sane();
    test_energy_balance_drift();
    test_mass_balance();
    test_energy_balance_check();

    std::cout << "\n--- EventLog ---\n";
    test_event_log();

    std::cout << "\n--- QualityFlags ---\n";
    test_quality_flags();

    std::cout << "\n--- OutcomeState ---\n";
    test_outcome_state_default();

    std::cout << "\n--- evolve() [full] ---\n";
    test_evolve_invalid_mass();
    test_evolve_invalid_energy();
    test_evolve_summary_level();
    test_evolve_with_state();

    std::cout << "\n--- evolve() [simplified] ---\n";
    test_evolve_simplified();
    test_evolve_chain();

    std::cout << "\n--- Data Structures ---\n";
    test_structural_state();
    test_stability_metric();
    test_performance_properties();
    test_transform_config_defaults();

    std::cout << "\n=== ALL 27 TESTS PASSED ===\n";
    return 0;
}
