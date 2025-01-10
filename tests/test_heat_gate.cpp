/**
 * test_heat_gate.cpp
 * ------------------
 * Validation tests for the heat-gated reaction control system (SS8b).
 *
 * Tests:
 *   1. Heat normalisation and clamping
 *   2. Gate function behaviour at boundary values
 *   3. Template enable weights across heat range
 *   4. Candidate bond scoring deterministic accept/reject
 *   5. Probabilistic accept sigmoid
 *   6. Active bio-template set correctness
 *   7. Amino acid reference table completeness
 *   8. Spearman correlation implementation
 *   9. Validation campaign aggregation
 *  10. Clash score computation
 */

#include "atomistic/reaction/heat_gate.hpp"
#include "atomistic/reaction/engine.hpp"
#include <iostream>
#include <cmath>
#include <cassert>
#include <string>
#include <vector>
#include <numeric>
#include <algorithm>

using namespace atomistic;
using namespace atomistic::reaction;

static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, msg) do { \
    if (cond) { ++tests_passed; } \
    else { ++tests_failed; std::cerr << "FAIL: " << msg << " [" << __FILE__ << ":" << __LINE__ << "]\n"; } \
} while(0)

#define CHECK_CLOSE(a, b, tol, msg) CHECK(std::abs((a) - (b)) < (tol), msg)

// ============================================================================
// Test 1: Heat normalisation
// ============================================================================
void test_heat_normalisation() {
    HeatConfig h0(0);
    CHECK(h0.heat_3 == 0, "heat_3 == 0");
    CHECK_CLOSE(h0.x_normalized, 0.0, 1e-9, "x_norm(0) == 0");

    HeatConfig h999(999);
    CHECK(h999.heat_3 == 999, "heat_3 == 999");
    CHECK_CLOSE(h999.x_normalized, 1.0, 1e-9, "x_norm(999) == 1.0");

    HeatConfig h500(500);
    CHECK_CLOSE(h500.x_normalized, 500.0 / 999.0, 1e-9, "x_norm(500)");

    // Clamping
    HeatConfig h_over(2000);
    CHECK(h_over.heat_3 == 999, "heat clamped to 999");
    CHECK_CLOSE(h_over.x_normalized, 1.0, 1e-9, "clamped x_norm == 1.0");
}

// ============================================================================
// Test 2: Gate function boundaries
// ============================================================================
void test_gate_function() {
    constexpr double X0 = 250.0 / 999.0;
    constexpr double X1 = 650.0 / 999.0;

    CHECK_CLOSE(gate(0.0, X0, X1), 0.0, 1e-9, "gate(0) == 0");
    CHECK_CLOSE(gate(X0, X0, X1), 0.0, 1e-9, "gate(x0) == 0");
    CHECK_CLOSE(gate(X1, X0, X1), 1.0, 1e-9, "gate(x1) == 1");
    CHECK_CLOSE(gate(1.0, X0, X1), 1.0, 1e-9, "gate(1) == 1");

    // Midpoint
    double mid = (X0 + X1) / 2.0;
    CHECK_CLOSE(gate(mid, X0, X1), 0.5, 1e-3, "gate(mid) ~= 0.5");

    // Monotonicity
    double prev = gate(0.0, X0, X1);
    bool monotone = true;
    for (int i = 1; i <= 999; ++i) {
        double x = i / 999.0;
        double g = gate(x, X0, X1);
        if (g < prev - 1e-15) monotone = false;
        prev = g;
    }
    CHECK(monotone, "gate is non-decreasing");
}

// ============================================================================
// Test 3: Template enable weights
// ============================================================================
void test_enable_weights() {
    // Heat = 0: all bio templates off
    HeatGateController ctrl_cold(0);
    for (int i = 0; i < static_cast<int>(BioTemplateId::COUNT); ++i) {
        auto id = static_cast<BioTemplateId>(i);
        CHECK_CLOSE(ctrl_cold.enable_weight(id), 0.0, 1e-9,
                    std::string("cold: wk(") + bio_template_name(id) + ") == 0");
    }
}

// ============================================================================
// Test 4: Temperature → Heat Conversion (CRITICAL - Item #7)
// ============================================================================
void test_temperature_to_heat_conversion() {
    // Default slope = 1.5

    // Cold regime: T < 167 K → h < 250 (organic mode)
    CHECK(temperature_to_heat(100.0) < 250, "T=100K → h < 250");
    CHECK(temperature_to_heat(166.0) < 250, "T=166K → h < 250");

    // Transitional regime: 167 K < T < 433 K → 250 ≤ h < 650
    uint16_t h_mid = temperature_to_heat(300.0);  // Room temperature
    CHECK(h_mid >= 250 && h_mid < 650, "T=300K → 250 ≤ h < 650 (transitional)");

    // Hot regime: T > 433 K → h ≥ 650 (full bio)
    CHECK(temperature_to_heat(500.0) >= 650, "T=500K → h ≥ 650");
    CHECK(temperature_to_heat(700.0) == 999, "T=700K → h = 999 (saturated)");

    // Monotonicity: higher T → higher h
    for (double T = 0; T < 800; T += 50) {
        uint16_t h1 = temperature_to_heat(T);
        uint16_t h2 = temperature_to_heat(T + 50);
        CHECK(h2 >= h1, "temperature_to_heat is non-decreasing");
    }

    // Inverse mapping
    CHECK_CLOSE(heat_to_temperature(0), 0.0, 0.1, "h=0 → T~0K");
    CHECK_CLOSE(heat_to_temperature(375), 250.0, 1.0, "h=375 → T~250K");
    CHECK_CLOSE(heat_to_temperature(999), 666.0, 1.0, "h=999 → T~666K");

    // Controller integration
    HeatGateController ctrl;
    ctrl.set_heat_from_temperature(300.0);  // Room temperature

    // At 300K, we expect h ~ 450 → transitional mode
    uint16_t h_rt = ctrl.config().heat_3;
    CHECK(h_rt >= 250 && h_rt < 650, "Controller: T=300K → transitional mode");

    // Mode index should be between 0 and 1
    double mode = ctrl.mode_index();
    CHECK(mode > 0.0 && mode < 1.0, "Controller: mode_index ∈ (0,1) at T=300K");

    // Heat = 999: all bio templates at full alpha
    HeatGateController ctrl_hot(999);
    auto params = default_bio_gate_params();
    for (size_t i = 0; i < params.size(); ++i) {
        auto id = static_cast<BioTemplateId>(i);
        CHECK_CLOSE(ctrl_hot.enable_weight(id), params[i].alpha, 1e-6,
                    std::string("hot: wk(") + bio_template_name(id) + ") == alpha");
    }

    // Heat = 450 (transitional): weights are partial
    HeatGateController ctrl_mid(450);
    double m = ctrl_mid.mode_index();
    CHECK(m > 0.0 && m < 1.0, "mode_index(450) is transitional");
    for (size_t i = 0; i < params.size(); ++i) {
        auto id = static_cast<BioTemplateId>(i);
        double w = ctrl_mid.enable_weight(id);
        CHECK(w >= 0.0 && w <= params[i].alpha + 1e-9,
              std::string("mid: wk bounded for ") + bio_template_name(id));
    }
}

// ============================================================================
// Test 4: Candidate bond scoring (deterministic)
// ============================================================================
void test_scoring_deterministic() {
    HeatGateController ctrl(700);  // Above x1 threshold, fully on

    HeatGateController::CandidateEvent event;
    event.template_id = BioTemplateId::PEPTIDE_BOND;
    event.geometry_score = 0.8;
    event.penalty = 0.1;
    event.atoms = {0, 1, 2, 3};

    auto result = ctrl.score_candidate(event);
    CHECK(result.score > 0.0, "score > 0 for good geometry, hot system");
    CHECK(!result.log_line.empty(), "log line not empty");

    // Very high penalty should cause rejection
    HeatGateController::CandidateEvent bad_event;
    bad_event.template_id = BioTemplateId::PEPTIDE_BOND;
    bad_event.geometry_score = 0.1;
    bad_event.penalty = 5.0;
    bad_event.atoms = {4, 5, 6, 7};

    auto bad_result = ctrl.score_candidate(bad_event);
    CHECK(bad_result.score < result.score, "bad event has lower score");

    // Cold system: score should be lower (no heat bias)
    HeatGateController ctrl_cold(0);
    auto cold_result = ctrl_cold.score_candidate(event);
    CHECK(cold_result.score < result.score, "cold score < hot score for same geometry");
}

// ============================================================================
// Test 5: Probabilistic accept sigmoid
// ============================================================================
void test_sigmoid_accept() {
    HeatGateController ctrl(600);

    HeatGateController::CandidateEvent event;
    event.template_id = BioTemplateId::PEPTIDE_BOND;
    event.geometry_score = 0.5;
    event.penalty = 0.0;
    event.atoms = {0, 1, 2, 3};

    // High beta: deterministic-like
    auto sharp = ctrl.score_candidate(event, 100.0);
    if (sharp.accepted) {
        CHECK(sharp.accept_prob > 0.99, "high beta + accepted => prob ~1");
    } else {
        CHECK(sharp.accept_prob < 0.01, "high beta + rejected => prob ~0");
    }

    // Low beta: always ~0.5
    auto soft = ctrl.score_candidate(event, 0.001);
    CHECK_CLOSE(soft.accept_prob, 0.5, 0.05, "low beta => prob ~0.5");
}

// ============================================================================
// Test 6: Active bio-template set
// ============================================================================
void test_active_templates() {
    // Cold: no bio templates active
    HeatGateController ctrl_cold(0);
    auto active_cold = ctrl_cold.active_bio_templates();
    CHECK(active_cold.empty(), "cold: no bio templates active");

    // Hot: all bio templates active
    HeatGateController ctrl_hot(999);
    auto active_hot = ctrl_hot.active_bio_templates();
    CHECK(active_hot.size() == static_cast<size_t>(BioTemplateId::COUNT),
          "hot: all bio templates active");

    // Mid: some templates active depending on alpha
    HeatGateController ctrl_mid(300);
    auto active_mid = ctrl_mid.active_bio_templates();
    CHECK(active_mid.size() > 0, "h=300: some templates active");
    CHECK(active_mid.size() <= static_cast<size_t>(BioTemplateId::COUNT),
          "h=300: not more than total");
}

// ============================================================================
// Test 7: Amino acid reference table
// ============================================================================
void test_amino_acid_table() {
    const auto& table = amino_acid_table();
    CHECK(table.size() == 20, "20 amino acids in table");

    // Verify glycine (smallest)
    bool found_gly = false;
    for (const auto& aa : table) {
        if (aa.one_letter == 'G') {
            found_gly = true;
            CHECK(std::string(aa.name) == "Glycine", "G is Glycine");
            CHECK(aa.C == 2 && aa.H == 5 && aa.N == 1 && aa.O == 2 && aa.S == 0,
                  "Glycine formula C2H5NO2");
        }
    }
    CHECK(found_gly, "Glycine found in table");

    // Verify cysteine (has sulfur)
    bool found_cys = false;
    for (const auto& aa : table) {
        if (aa.one_letter == 'C') {
            found_cys = true;
            CHECK(aa.S == 1, "Cysteine has S=1");
        }
    }
    CHECK(found_cys, "Cysteine found in table");

    // Verify tryptophan (largest)
    bool found_trp = false;
    for (const auto& aa : table) {
        if (aa.one_letter == 'W') {
            found_trp = true;
            CHECK(aa.C == 11, "Tryptophan has C=11");
        }
    }
    CHECK(found_trp, "Tryptophan found in table");

    // All one-letter codes unique
    std::string codes;
    for (const auto& aa : table) codes += aa.one_letter;
    std::sort(codes.begin(), codes.end());
    auto it = std::unique(codes.begin(), codes.end());
    CHECK(it == codes.end(), "All one-letter codes unique");
}

// ============================================================================
// Test 8: Spearman correlation
// ============================================================================
void test_spearman_correlation() {
    // Perfect positive correlation
    std::vector<double> x = {1, 2, 3, 4, 5};
    std::vector<double> y = {10, 20, 30, 40, 50};
    CHECK_CLOSE(spearman_correlation(x, y), 1.0, 1e-6, "perfect positive rho=1");

    // Perfect negative correlation
    std::vector<double> yn = {50, 40, 30, 20, 10};
    CHECK_CLOSE(spearman_correlation(x, yn), -1.0, 1e-6, "perfect negative rho=-1");

    // No correlation (too few points for perfect test, but monotone is ok)
    std::vector<double> y_rand = {3, 1, 5, 2, 4};
    double rho = spearman_correlation(x, y_rand);
    CHECK(rho > -1.0 && rho < 1.0, "random-ish has |rho| < 1");

    // Edge: 2 elements
    std::vector<double> x2 = {1, 2};
    std::vector<double> y2 = {2, 1};
    CHECK_CLOSE(spearman_correlation(x2, y2), -1.0, 1e-6, "2-element negative");
}

// ============================================================================
// Test 9: Validation campaign aggregation
// ============================================================================
void test_campaign_aggregation() {
    // Create synthetic run data: 3 temperatures, 5 seeds each
    std::vector<SingleRunMetrics> runs;
    double temps[] = {100.0, 200.0, 300.0};
    for (double T : temps) {
        for (uint32_t s = 0; s < 5; ++s) {
            SingleRunMetrics m;
            m.seed = s;
            m.temperature = T;
            m.heat_3 = static_cast<uint16_t>(T);
            m.steps = 1000;
            m.E_start = -100.0;
            m.E_end = -100.0 + 0.01 * T;  // Small drift, increases with T
            m.drift = compute_energy_drift(m.E_start, m.E_end);
            m.converged = true;
            m.clash_score = 0.0001;
            m.geom_violation_rate = 0.1;
            m.n_events_logged = 10;
            m.n_events_accepted = 5 + static_cast<uint32_t>(T / 100.0);  // More at higher T
            m.n_events_rejected = m.n_events_logged - m.n_events_accepted;
            m.msd = 0.5 * T / 100.0;  // MSD increases with T
            runs.push_back(m);
        }
    }

    auto buckets = aggregate_by_temperature(runs);
    CHECK(buckets.size() == 3, "3 temperature buckets");
    CHECK(buckets[0].temperature < buckets[1].temperature, "sorted by T");

    auto result = evaluate_campaign(buckets, 15, 3, 5);
    CHECK(result.total_runs == 15, "total_runs == 15");
    CHECK(result.energy_drift_pass, "energy drift passes for small drift");
    CHECK(result.convergence_pass, "convergence passes (all converged)");
    CHECK(result.clash_pass, "clash passes (all < 1e-3)");
    CHECK(result.geom_violation_pass, "geom violation passes");
    CHECK(result.identity_conservation_pass, "identity conservation passes");
}

// ============================================================================
// Test 10: Clash score computation
// ============================================================================
void test_clash_score() {
    // Two atoms well separated
    State s_ok;
    s_ok.N = 2;
    s_ok.X = {{0, 0, 0}, {5, 0, 0}};
    s_ok.V.resize(2);
    s_ok.M = {1.0, 1.0};
    s_ok.Q = {0.0, 0.0};
    s_ok.type = {1, 1};
    double clash_ok = compute_clash_score(s_ok);
    CHECK_CLOSE(clash_ok, 0.0, 1e-9, "well-separated atoms: clash=0");

    // Two atoms overlapping
    State s_bad;
    s_bad.N = 2;
    s_bad.X = {{0, 0, 0}, {0.1, 0, 0}};
    s_bad.V.resize(2);
    s_bad.M = {1.0, 1.0};
    s_bad.Q = {0.0, 0.0};
    s_bad.type = {1, 1};
    double clash_bad = compute_clash_score(s_bad);
    CHECK(clash_bad > 0.0, "overlapping atoms: clash > 0");
}

// ============================================================================
// Test 11: ReactionEngine heat integration
// ============================================================================
void test_engine_heat_integration() {
    ReactionEngine engine;

    // Default: no heat set, bio templates not loaded
    size_t base_count = engine.get_templates().size();
    CHECK(base_count == 5, "5 standard templates loaded by default");

    // Set heat and load bio templates
    engine.set_heat(700);
    engine.load_heat_gated_templates();
    size_t total_count = engine.get_templates().size();
    CHECK(total_count > base_count, "bio templates added at h=700");

    // Verify bio template IDs are active
    auto active = engine.active_bio_templates();
    CHECK(active.size() == static_cast<size_t>(BioTemplateId::COUNT),
          "all bio templates active at h=700");

    // Cold engine: no bio templates
    ReactionEngine cold_engine;
    cold_engine.set_heat(0);
    cold_engine.load_heat_gated_templates();
    CHECK(cold_engine.get_templates().size() == 5,
          "no bio templates added at h=0");
}

// ============================================================================
// Test 12: Bio template factory functions
// ============================================================================
void test_bio_template_factories() {
    auto peptide = peptide_bond_template();
    CHECK(peptide.name == "Peptide Bond Formation", "peptide name");
    CHECK(peptide.mechanism == MechanismType::ADDITION, "peptide mechanism");
    CHECK(peptide.conserve_valence, "peptide conserves valence");

    auto disulf = disulfide_template();
    CHECK(disulf.mechanism == MechanismType::REDOX, "disulfide is redox");
    CHECK(disulf.allow_radicals, "disulfide allows radicals");

    // All bio templates retrievable by ID
    for (int i = 0; i < static_cast<int>(BioTemplateId::COUNT); ++i) {
        auto tmpl = bio_template_by_id(static_cast<BioTemplateId>(i));
        CHECK(!tmpl.name.empty(), std::string("bio_template_by_id(") +
              std::to_string(i) + ") has name");
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "╔══════════════════════════════════════════════════╗\n";
    std::cout << "║  Heat-Gated Reaction Control Tests (SS8b)       ║\n";
    std::cout << "╚══════════════════════════════════════════════════╝\n\n";

    test_heat_normalisation();
    test_gate_function();
    test_enable_weights();
    test_temperature_to_heat_conversion();  // NEW: Critical Item #7
    test_scoring_deterministic();
    test_sigmoid_accept();
    test_active_templates();
    test_amino_acid_table();
    test_spearman_correlation();
    test_campaign_aggregation();
    test_clash_score();
    test_engine_heat_integration();
    test_bio_template_factories();

    std::cout << "\n────────────────────────────────────────────────────\n";
    std::cout << "  PASSED: " << tests_passed << "\n";
    std::cout << "  FAILED: " << tests_failed << "\n";
    std::cout << "────────────────────────────────────────────────────\n";

    if (tests_failed == 0) {
        std::cout << "  ✓ ALL TESTS PASSED\n";
    } else {
        std::cout << "  ✗ SOME TESTS FAILED\n";
    }

    return tests_failed;
}
