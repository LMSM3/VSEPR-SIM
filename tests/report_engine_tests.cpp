/**
 * report_engine_tests.cpp
 * -----------------------
 * Tests for the Autonomous Report-Generation Engine.
 * Work Order: WO-TMS-CRG-001
 *
 * Verifies:
 *   1. Material property engine (element lookup, synthetic, alloy mixing)
 *   2. Case generator (all 5 complexity levels)
 *   3. Experiment runner (all experiment types)
 *   4. Report writer (Markdown, CSV)
 *   5. Autonomous engine (batch generation)
 *   6. Complexity escalation
 *   7. Deterministic reproducibility
 *   8. 1000+ unique report generation
 */

#include "core/report_engine.hpp"
#include <iostream>
#include <cassert>
#include <set>
#include <string>
#include <filesystem>
#include <fstream>

using namespace vsepr::report;

static int passed = 0;
static int failed = 0;

#define TEST(name) \
    { \
        std::string test_name = name; \
        bool test_ok = true; \
        try {

#define EXPECT(cond) \
    if (!(cond)) { \
        std::cerr << "  FAIL: " #cond " (line " << __LINE__ << ")\n"; \
        test_ok = false; \
    }

#define END_TEST \
        } catch (const std::exception& e) { \
            std::cerr << "  EXCEPTION: " << e.what() << "\n"; \
            test_ok = false; \
        } \
        if (test_ok) { \
            std::cout << "  PASS: " << test_name << "\n"; \
            ++passed; \
        } else { \
            std::cerr << "  FAIL: " << test_name << "\n"; \
            ++failed; \
        } \
    }

int main() {
    std::cout << "=== Report Engine Test Suite ===\n";
    std::cout << "=== Work Order: WO-TMS-CRG-001 ===\n\n";

    // ---- Test 1: Element properties ----
    TEST("element_properties_iron")
        MaterialPropertyEngine engine;
        auto fe = engine.element_properties(26);
        EXPECT(fe.name == "Iron")
        EXPECT(fe.formula == "Fe")
        EXPECT(fe.density_kg_m3 > 7000)
        EXPECT(fe.elastic_modulus_GPa > 200)
        EXPECT(fe.thermal_conductivity_W_mK > 50)
        EXPECT(fe.melting_point_K > 1800)
        EXPECT(fe.confidence_score > 0.9)
    END_TEST

    TEST("element_properties_copper")
        MaterialPropertyEngine engine;
        auto cu = engine.element_properties(29);
        EXPECT(cu.name == "Copper")
        EXPECT(cu.thermal_conductivity_W_mK > 300)
        EXPECT(cu.melting_point_K > 1300)
    END_TEST

    TEST("element_properties_tungsten")
        MaterialPropertyEngine engine;
        auto w = engine.element_properties(74);
        EXPECT(w.name == "Tungsten")
        EXPECT(w.density_kg_m3 > 19000)
        EXPECT(w.elastic_modulus_GPa > 400)
        EXPECT(w.melting_point_K > 3600)
    END_TEST

    TEST("element_properties_synthetic_fallback")
        MaterialPropertyEngine engine;
        auto syn = engine.element_properties(99);  // Einsteinium - not in table
        EXPECT(syn.is_synthetic)
        EXPECT(syn.confidence_score < 0.5)
        EXPECT(syn.density_kg_m3 > 0)
    END_TEST

    // ---- Test 2: Synthetic properties ----
    TEST("synthetic_properties_generation")
        MaterialPropertyEngine engine;
        std::mt19937_64 rng(42);
        auto syn = engine.synthetic_properties(rng, 0.5);
        EXPECT(syn.is_synthetic)
        EXPECT(syn.category == "synthetic")
        EXPECT(syn.density_kg_m3 > 0)
        EXPECT(syn.elastic_modulus_GPa > 0)
        EXPECT(!syn.name.empty())
        EXPECT(!syn.formula.empty())
    END_TEST

    // ---- Test 3: Alloy mixing ----
    TEST("alloy_mixing_binary")
        MaterialPropertyEngine engine;
        std::mt19937_64 rng(42);
        auto fe = engine.element_properties(26);
        auto cu = engine.element_properties(29);
        auto alloy = engine.mix_alloy({fe, cu}, {0.7, 0.3}, rng);
        EXPECT(alloy.category == "alloy")
        EXPECT(alloy.density_kg_m3 > 0)
        // Alloy density should be between Fe and Cu
        double min_d = std::min(fe.density_kg_m3, cu.density_kg_m3) * 0.7;
        double max_d = std::max(fe.density_kg_m3, cu.density_kg_m3) * 1.3;
        EXPECT(alloy.density_kg_m3 > min_d && alloy.density_kg_m3 < max_d)
    END_TEST

    // ---- Test 4: Case generation L1 ----
    TEST("case_generation_l1")
        CaseGenerator gen(42);
        auto mc = gen.generate_at_level(ComplexityLevel::L1_SIMPLE);
        EXPECT(mc.level == ComplexityLevel::L1_SIMPLE)
        EXPECT(mc.components.size() == 1)
        EXPECT(mc.fractions.size() == 1)
        EXPECT(mc.boundaries.size() == 2)
        EXPECT(!mc.case_name.empty())
    END_TEST

    // ---- Test 5: Case generation L2 ----
    TEST("case_generation_l2")
        CaseGenerator gen(42);
        auto mc = gen.generate_at_level(ComplexityLevel::L2_BINARY);
        EXPECT(mc.components.size() >= 2)
        EXPECT(mc.fractions.size() == mc.components.size())
        double frac_sum = 0;
        for (auto f : mc.fractions) frac_sum += f;
        EXPECT(std::abs(frac_sum - 1.0) < 0.05)
    END_TEST

    // ---- Test 6: Case generation L3 ----
    TEST("case_generation_l3")
        CaseGenerator gen(42);
        auto mc = gen.generate_at_level(ComplexityLevel::L3_ANISOTROPIC);
        EXPECT(!mc.layers.empty())
        EXPECT(mc.effective.anisotropy_factor > 1.0)
    END_TEST

    // ---- Test 7: Case generation L4 ----
    TEST("case_generation_l4")
        CaseGenerator gen(42);
        auto mc = gen.generate_at_level(ComplexityLevel::L4_TRANSIENT);
        EXPECT(mc.thermal_load.is_transient)
        EXPECT(mc.thermal_load.num_cycles > 1)
        EXPECT(mc.thermal_load.heating_rate_K_s > 0)
    END_TEST

    // ---- Test 8: Case generation L5 ----
    TEST("case_generation_l5")
        CaseGenerator gen(42);
        auto mc = gen.generate_at_level(ComplexityLevel::L5_EXOTIC);
        EXPECT(mc.thermal_load.is_transient)
        EXPECT(!mc.defects.empty())
        EXPECT(mc.rarity_score > 0.5)
    END_TEST

    // ---- Test 9: Experiment runner ----
    TEST("experiment_runner_l1")
        CaseGenerator gen(42);
        auto mc = gen.generate_at_level(ComplexityLevel::L1_SIMPLE);
        ExperimentRunner runner;
        auto results = runner.run_all(mc);
        EXPECT(results.size() >= 3)  // steady-state, expansion, stress at minimum
        for (const auto& r : results) {
            EXPECT(!r.experiment_name.empty())
            EXPECT(r.numerical_stability >= 0.0)
            EXPECT(r.physical_plausibility >= 0.0)
        }
    END_TEST

    TEST("experiment_runner_l4")
        CaseGenerator gen(42);
        auto mc = gen.generate_at_level(ComplexityLevel::L4_TRANSIENT);
        ExperimentRunner runner;
        auto results = runner.run_all(mc);
        EXPECT(results.size() >= 5)  // L4 gets transient + fatigue + sensitivity + proxies
    END_TEST

    // ---- Test 10: Report writer ----
    TEST("report_writer_markdown")
        EngineConfig config;
        config.target_reports = 1;
        config.write_individual = false;
        config.write_csv_log = false;
        config.print_progress = false;
        AutonomousEngine engine(config);
        auto report = engine.generate_one();
        std::string md = ReportWriter::to_markdown(report);
        EXPECT(md.size() > 500)
        EXPECT(md.find("# TMS-") != std::string::npos)
        EXPECT(md.find("## Material System") != std::string::npos)
        EXPECT(md.find("## Experiment Results") != std::string::npos)
        EXPECT(md.find("## Analysis") != std::string::npos)
        EXPECT(md.find("## Conclusion") != std::string::npos)
    END_TEST

    TEST("report_writer_csv")
        EngineConfig config;
        config.target_reports = 1;
        config.write_individual = false;
        config.write_csv_log = false;
        config.print_progress = false;
        AutonomousEngine engine(config);
        auto report = engine.generate_one();
        std::string csv = ReportWriter::to_csv_line(report);
        EXPECT(!csv.empty())
        // Should have commas
        int commas = 0;
        for (char c : csv) if (c == ',') commas++;
        EXPECT(commas >= 10)
    END_TEST

    // ---- Test 11: Complexity escalation ----
    TEST("complexity_escalation")
        CaseGenerator gen(42);
        gen.set_escalation_thresholds(5, 10, 15, 20);

        // Level check happens at start of generate_next(), so we need
        // to generate one past the threshold to see the escalation
        EXPECT(gen.current_level() == ComplexityLevel::L1_SIMPLE)
        for (int i = 0; i < 6; ++i) gen.generate_next();  // 6th triggers L2 check
        EXPECT(gen.current_level() == ComplexityLevel::L2_BINARY)
        for (int i = 0; i < 5; ++i) gen.generate_next();  // 11th triggers L3
        EXPECT(gen.current_level() == ComplexityLevel::L3_ANISOTROPIC)
        for (int i = 0; i < 5; ++i) gen.generate_next();  // 16th triggers L4
        EXPECT(gen.current_level() == ComplexityLevel::L4_TRANSIENT)
        for (int i = 0; i < 5; ++i) gen.generate_next();  // 21st triggers L5
        EXPECT(gen.current_level() == ComplexityLevel::L5_EXOTIC)
    END_TEST

    // ---- Test 12: Deterministic reproducibility ----
    TEST("deterministic_reproducibility")
        EngineConfig cfg;
        cfg.target_reports = 5;
        cfg.write_individual = false;
        cfg.write_csv_log = false;
        cfg.print_progress = false;
        cfg.base_seed = 12345;

        AutonomousEngine e1(cfg);
        AutonomousEngine e2(cfg);

        for (int i = 0; i < 5; ++i) {
            auto r1 = e1.generate_one();
            auto r2 = e2.generate_one();
            EXPECT(r1.material_case.case_name == r2.material_case.case_name)
            EXPECT(r1.experiments.size() == r2.experiments.size())
            EXPECT(r1.material_case.components.size() == r2.material_case.components.size())
        }
    END_TEST

    // ---- Test 13: Mass generation (1000+ unique reports) ----
    TEST("mass_generation_1000_unique")
        EngineConfig cfg;
        cfg.target_reports = 1000;
        cfg.write_individual = false;
        cfg.write_csv_log = false;
        cfg.print_progress = false;
        cfg.base_seed = 42;

        AutonomousEngine engine(cfg);
        std::set<std::string> unique_names;
        std::set<uint64_t> unique_ids;
        int l1_count = 0, l2_count = 0, l3_count = 0, l4_count = 0, l5_count = 0;
        int total_experiments = 0;
        int total_warnings = 0;

        for (int i = 0; i < 1000; ++i) {
            auto report = engine.generate_one();
            unique_names.insert(report.material_case.case_name);
            unique_ids.insert(report.report_id);
            total_experiments += static_cast<int>(report.experiments.size());
            total_warnings += static_cast<int>(report.warnings.size());

            switch (report.current_level) {
                case ComplexityLevel::L1_SIMPLE:      l1_count++; break;
                case ComplexityLevel::L2_BINARY:      l2_count++; break;
                case ComplexityLevel::L3_ANISOTROPIC: l3_count++; break;
                case ComplexityLevel::L4_TRANSIENT:   l4_count++; break;
                case ComplexityLevel::L5_EXOTIC:      l5_count++; break;
            }
        }

        EXPECT(unique_ids.size() == 1000)
        EXPECT(unique_names.size() >= 500)  // High uniqueness
        EXPECT(l1_count > 0)
        EXPECT(l2_count > 0)
        EXPECT(l3_count > 0)
        EXPECT(l4_count > 0)
        EXPECT(l5_count > 0)
        EXPECT(total_experiments > 3000)  // At least 3 per report

        std::cout << "    [1000-report stats]\n";
        std::cout << "    Unique names: " << unique_names.size() << "/1000\n";
        std::cout << "    L1=" << l1_count << " L2=" << l2_count
                  << " L3=" << l3_count << " L4=" << l4_count
                  << " L5=" << l5_count << "\n";
        std::cout << "    Total experiments: " << total_experiments << "\n";
        std::cout << "    Total warnings: " << total_warnings << "\n";
    END_TEST

    // ---- Test 14: Property table output ----
    TEST("property_table_format")
        MaterialPropertyEngine engine;
        auto fe = engine.element_properties(26);
        std::string table = ReportWriter::property_table(fe);
        EXPECT(table.find("Density") != std::string::npos)
        EXPECT(table.find("Thermal Conductivity") != std::string::npos)
        EXPECT(table.find("Melting Point") != std::string::npos)
    END_TEST

    // ---- Test 15: Experiment data series ----
    TEST("experiment_data_series")
        CaseGenerator gen(42);
        auto mc = gen.generate_at_level(ComplexityLevel::L1_SIMPLE);
        ExperimentRunner runner;
        auto result = runner.steady_state_conduction(mc);
        EXPECT(!result.series.empty())
        EXPECT(result.series.size() >= 10)
        EXPECT(!result.series_x_label.empty())
        EXPECT(!result.series_y_label.empty())
    END_TEST

    // ---- Test 16: Gamma escalation ----
    TEST("gamma_escalation")
        CaseGenerator gen(42);
        double g0 = gen.current_gamma();
        for (int i = 0; i < 100; ++i) gen.generate_next();
        double g100 = gen.current_gamma();
        EXPECT(g100 > g0)
    END_TEST

    // ---- Summary ----
    std::cout << "\n=== Results: " << passed << "/" << (passed + failed)
              << " passed ===\n";

    return failed > 0 ? 1 : 0;
}
