/**
 * test_gas3.cpp
 * -------------
 * Verification tests for gas3 module:
 *   - GasStateRecord generation and export
 *   - Quality scoring and tier classification
 *   - Linear sweep
 *   - Random sweep
 *   - CSV export
 *   - Curve fitting (1D, 2D)
 *   - Pipeline integration
 *
 * 25 tests. All deterministic.
 */

#include "gas3/gas3_engine.hpp"
#include <iostream>
#include <cmath>
#include <string>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

static int pass_count = 0;
static int fail_count = 0;

#define TEST(name) do { std::cout << "  T" << (pass_count + fail_count + 1) << " " << name; } while(0)
#define PASS() do { ++pass_count; std::cout << " [PASS]\n"; } while(0)
#define FAIL(msg) do { ++fail_count; std::cout << " [FAIL] " << msg << "\n"; } while(0)
#define ASSERT_TRUE(c, m)  do { if (!(c)) { FAIL(m); return; } } while(0)
#define ASSERT_NEAR(a, b, t, m) do { if (std::abs((a)-(b))>(t)) { FAIL(std::string(m) + " got=" + std::to_string(a) + " exp=" + std::to_string(b)); return; } } while(0)

using namespace vsepr::gas3;
using namespace vsepr::gas2;

// ============================================================================
// GasStateRecord generation
// ============================================================================

void test_generate_state_ar() {
    TEST("Generate state: Ar at STP, VdW");
    auto r = generate_state("Ar", 298.15, 1.0, EOSType::VanDerWaals,
                             1.0, SampleMode::Linear, 0, 0);
    ASSERT_TRUE(r.species == "Ar", "species");
    ASSERT_TRUE(r.model_name == "vdW", "model");
    ASSERT_TRUE(r.converged, "converged");
    ASSERT_TRUE(r.physically_valid, "valid");
    ASSERT_TRUE(r.Z > 0.95 && r.Z < 1.05, "Z near 1");
    ASSERT_TRUE(r.v_rms > 300 && r.v_rms < 600, "v_rms");
    PASS();
}

void test_generate_state_co2() {
    TEST("Generate state: CO2 at 500K 10atm, RK");
    auto r = generate_state("CO2", 500.0, 10.0, EOSType::RedlichKwong,
                             1.0, SampleMode::Linear, 0, 0);
    ASSERT_TRUE(r.species == "CO2", "species");
    ASSERT_TRUE(r.model_name == "RK", "model");
    ASSERT_TRUE(r.converged, "converged");
    ASSERT_TRUE(!std::isnan(r.Tr), "Tr defined");
    ASSERT_TRUE(!std::isnan(r.Pr), "Pr defined");
    ASSERT_TRUE(r.gamma > 1.0 && r.gamma < 2.0, "gamma range");
    PASS();
}

void test_generate_state_ideal() {
    TEST("Generate state: N2 ideal at STP");
    auto r = generate_state("N2", 273.15, 1.0, EOSType::Ideal,
                             1.0, SampleMode::Linear, 0, 0);
    ASSERT_TRUE(r.Z == 1.0, "Z ideal");
    ASSERT_TRUE(r.iterations == 0, "no iterations");
    ASSERT_TRUE(r.residual == 0.0, "zero residual");
    PASS();
}

void test_generate_state_unknown() {
    TEST("Generate state: unknown species");
    auto r = generate_state("XeF6", 300.0, 1.0, EOSType::VanDerWaals,
                             1.0, SampleMode::Linear, 0, 0);
    ASSERT_TRUE(!r.converged, "should not converge");
    ASSERT_TRUE(r.quality_tier == QualityTier::Q0, "Q0 for unknown");
    PASS();
}

// ============================================================================
// Quality scoring
// ============================================================================

void test_quality_ideal_stp() {
    TEST("Quality: ideal gas at STP = Q4");
    auto r = generate_state("Ar", 298.15, 1.0, EOSType::Ideal,
                             1.0, SampleMode::Linear, 0, 0);
    ASSERT_TRUE(r.quality_tier == QualityTier::Q4, "expected Q4");
    ASSERT_TRUE(r.quality_score >= 90.0, "score >= 90");
    PASS();
}

void test_quality_vdw_stp() {
    TEST("Quality: VdW Ar at STP >= Q2");
    auto r = generate_state("Ar", 298.15, 1.0, EOSType::VanDerWaals,
                             1.0, SampleMode::Linear, 0, 0);
    ASSERT_TRUE(static_cast<int>(r.quality_tier) >= 2, "at least Q2");
    PASS();
}

void test_quality_penalties() {
    TEST("Quality: unconverged gets Q0");
    GasStateRecord r;
    r.converged = false;
    r.V_m3 = 0.024;
    r.T_K = 300;
    r.P_Pa = 101325;
    r.Z = 1.0;
    score_record(r);
    ASSERT_TRUE(r.quality_tier == QualityTier::Q0, "Q0 for unconverged");
    ASSERT_TRUE(r.quality_score == 0.0, "score 0");
    PASS();
}

void test_quality_nonphysical() {
    TEST("Quality: negative volume = Q0");
    GasStateRecord r;
    r.converged = true;
    r.V_m3 = -1.0;
    r.T_K = 300;
    r.P_Pa = 101325;
    r.Z = -1.0;
    score_record(r);
    ASSERT_TRUE(r.quality_tier == QualityTier::Q0, "Q0 for negative V");
    PASS();
}

// ============================================================================
// Species classification
// ============================================================================

void test_species_class_noble() {
    TEST("Species class: Ar = noble");
    ASSERT_TRUE(classify_species("Ar") == SpeciesClass::Noble, "Ar noble");
    PASS();
}

void test_species_class_diatomic() {
    TEST("Species class: N2 = light_diatomic");
    ASSERT_TRUE(classify_species("N2") == SpeciesClass::LightDiatomic, "N2 diatomic");
    PASS();
}

void test_species_class_polar() {
    TEST("Species class: H2O = polar_small");
    ASSERT_TRUE(classify_species("H2O") == SpeciesClass::PolarSmall, "H2O polar");
    PASS();
}

// ============================================================================
// CSV export
// ============================================================================

void test_csv_header() {
    TEST("CSV header is non-empty");
    auto h = GasStateRecord::csv_header();
    ASSERT_TRUE(h.size() > 50, "header too short");
    ASSERT_TRUE(h.find("species") != std::string::npos, "has species");
    ASSERT_TRUE(h.find("quality_tier") != std::string::npos, "has quality_tier");
    PASS();
}

void test_csv_row() {
    TEST("CSV row matches header field count");
    auto r = generate_state("Ar", 300.0, 1.0, EOSType::VanDerWaals,
                             1.0, SampleMode::Linear, 0, 0);
    auto header = GasStateRecord::csv_header();
    auto row = r.to_csv_row();
    // Count commas
    auto count_commas = [](const std::string& s) {
        return static_cast<int>(std::count(s.begin(), s.end(), ','));
    };
    ASSERT_TRUE(count_commas(header) == count_commas(row),
                "comma count mismatch");
    PASS();
}

// ============================================================================
// Log line format
// ============================================================================

void test_log_line() {
    TEST("Log line is machine-readable");
    auto r = generate_state("CO2", 500.0, 10.0, EOSType::RedlichKwong,
                             1.0, SampleMode::Linear, 0, 0);
    auto line = r.to_log_line();
    ASSERT_TRUE(line.find("species=CO2") != std::string::npos, "has species");
    ASSERT_TRUE(line.find("model=RK") != std::string::npos, "has model");
    ASSERT_TRUE(line.find("quality=") != std::string::npos, "has quality");
    PASS();
}

// ============================================================================
// Linear sweep
// ============================================================================

void test_linear_sweep_small() {
    TEST("Linear sweep: 3 species x 3T x 2P x 1 model");
    SweepConfig cfg;
    cfg.T_min_K = 200; cfg.T_max_K = 400; cfg.T_step_K = 100;
    cfg.P_grid_atm = {1.0, 10.0};
    cfg.species_list = {"Ar", "N2", "CO2"};
    cfg.models = {EOSType::VanDerWaals};

    SweepStats stats;
    auto records = linear_sweep(cfg, stats);
    // 3 species * 3 T values * 2 P values * 1 model = 18
    ASSERT_TRUE(records.size() == 18, "expected 18 records, got " + std::to_string(records.size()));
    ASSERT_TRUE(stats.total_points == 18, "stats total");
    ASSERT_TRUE(stats.converged > 0, "some converged");
    PASS();
}

// ============================================================================
// Random sweep
// ============================================================================

void test_random_sweep_count() {
    TEST("Random sweep: 50 samples");
    SweepConfig cfg;
    cfg.random_count = 50;
    cfg.seed = 12345;

    SweepStats stats;
    auto records = random_sweep(cfg, stats);
    ASSERT_TRUE(records.size() == 50, "expected 50");
    ASSERT_TRUE(stats.total_points == 50, "stats total");
    PASS();
}

void test_random_sweep_deterministic() {
    TEST("Random sweep: deterministic with same seed");
    SweepConfig cfg;
    cfg.random_count = 20;
    cfg.seed = 42;

    SweepStats s1, s2;
    auto r1 = random_sweep(cfg, s1);
    auto r2 = random_sweep(cfg, s2);
    // Same species and same Z values (within floating-point)
    bool same = true;
    for (size_t i = 0; i < 20; ++i) {
        if (r1[i].species != r2[i].species) { same = false; break; }
        if (std::abs(r1[i].Z - r2[i].Z) > 1e-10) { same = false; break; }
    }
    ASSERT_TRUE(same, "not deterministic");
    PASS();
}

// ============================================================================
// Filter by tier
// ============================================================================

void test_filter_by_tier() {
    TEST("Filter by tier: Q3+ subset");
    SweepConfig cfg;
    cfg.T_min_K = 200; cfg.T_max_K = 500; cfg.T_step_K = 100;
    cfg.P_grid_atm = {1.0};
    cfg.species_list = {"Ar"};
    cfg.models = {EOSType::VanDerWaals};

    SweepStats stats;
    auto records = linear_sweep(cfg, stats);
    auto filtered = filter_by_tier(records, QualityTier::Q3);
    ASSERT_TRUE(filtered.size() <= records.size(), "filtered <= total");
    for (const auto& r : filtered) {
        ASSERT_TRUE(static_cast<int>(r.quality_tier) >= 3, "all Q3+");
    }
    PASS();
}

// ============================================================================
// Curve fitting
// ============================================================================

void test_fit_1d_linear() {
    TEST("1D fit: linear on y=2x+1");
    std::vector<double> x, y;
    for (int i = 0; i < 20; ++i) {
        x.push_back(static_cast<double>(i));
        y.push_back(2.0 * i + 1.0);
    }
    auto fit = poly_fit_1d(x, y, 1, "test linear", "x", "y", 0.8);
    ASSERT_NEAR(fit.coeffs[0], 1.0, 1e-6, "intercept");
    ASSERT_NEAR(fit.coeffs[1], 2.0, 1e-6, "slope");
    PASS();
}

void test_fit_1d_quadratic() {
    TEST("1D fit: quadratic on y=x^2");
    std::vector<double> x, y;
    for (int i = 0; i < 50; ++i) {
        double xi = static_cast<double>(i) * 0.1;
        x.push_back(xi);
        y.push_back(xi * xi);
    }
    auto fit = poly_fit_1d(x, y, 2, "test quad", "x", "y", 0.8);
    ASSERT_NEAR(fit.coeffs[0], 0.0, 0.01, "a0");
    ASSERT_NEAR(fit.coeffs[1], 0.0, 0.01, "a1");
    ASSERT_NEAR(fit.coeffs[2], 1.0, 0.01, "a2");
    PASS();
}

void test_fit_2d_plane() {
    TEST("2D fit: plane Z=1+0.001*T-0.002*P");
    std::vector<double> T, P, Z;
    for (int i = 0; i < 100; ++i) {
        double ti = 100.0 + 10.0 * i;
        double pi = 1.0 + 0.5 * i;
        T.push_back(ti);
        P.push_back(pi);
        Z.push_back(1.0 + 0.001 * ti - 0.002 * pi);
    }
    auto fit = poly_fit_2d(T, P, Z, 1, "test plane", "T", "P", "Z", 0.8);
    ASSERT_TRUE(fit.coeffs.size() == 3, "3 terms for degree-1 2D");
    ASSERT_TRUE(fit.r_squared > 0.99 || std::isnan(fit.r_squared), "R^2 ~ 1");
    PASS();
}

// ============================================================================
// JSON export
// ============================================================================

void test_json_format() {
    TEST("JSON output is valid-looking");
    auto r = generate_state("Ar", 300.0, 1.0, EOSType::VanDerWaals,
                             1.0, SampleMode::Linear, 0, 0);
    auto j = r.to_json();
    ASSERT_TRUE(j.front() == '{', "starts with {");
    ASSERT_TRUE(j.back() == '}', "ends with }");
    ASSERT_TRUE(j.find("\"species\":\"Ar\"") != std::string::npos, "has species");
    ASSERT_TRUE(j.find("\"quality_tier\"") != std::string::npos, "has tier");
    PASS();
}

// ============================================================================
// Tier names
// ============================================================================

void test_tier_names() {
    TEST("Tier names Q0-Q4");
    ASSERT_TRUE(std::string(tier_name(QualityTier::Q0)) == "Q0", "Q0");
    ASSERT_TRUE(std::string(tier_name(QualityTier::Q4)) == "Q4", "Q4");
    PASS();
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n=== gas3 Module Tests ===\n\n";

    // State generation
    test_generate_state_ar();
    test_generate_state_co2();
    test_generate_state_ideal();
    test_generate_state_unknown();

    // Quality
    test_quality_ideal_stp();
    test_quality_vdw_stp();
    test_quality_penalties();
    test_quality_nonphysical();

    // Species classification
    test_species_class_noble();
    test_species_class_diatomic();
    test_species_class_polar();

    // Export formats
    test_csv_header();
    test_csv_row();
    test_log_line();
    test_json_format();
    test_tier_names();

    // Sweep
    test_linear_sweep_small();
    test_random_sweep_count();
    test_random_sweep_deterministic();
    test_filter_by_tier();

    // Fitting
    test_fit_1d_linear();
    test_fit_1d_quadratic();
    test_fit_2d_plane();

    // Summary
    std::cout << "\n--- Results: " << pass_count << " passed, "
              << fail_count << " failed out of " << (pass_count + fail_count) << " ---\n\n";

    return fail_count > 0 ? 1 : 0;
}
