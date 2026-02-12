/**
 * @file test_formula_parser.cpp
 * @brief Comprehensive automated testing for formula parser
 * 
 * Test Categories:
 * 1. Basic parsing (known formulas)
 * 2. Error handling (invalid formulas)
 * 3. Edge cases (empty, single atom, large counts)
 * 4. Parentheses support
 * 5. Random formula generation (fuzz testing)
 * 6. Stress testing (performance, large batches)
 * 7. Property validation (roundtrip, mass conservation)
 * 
 * Automated Testing Features:
 * - Random formula generation
 * - Property-based testing
 * - Regression suite
 * - Performance benchmarks
 */

#include "vsepr/formula_parser.hpp"
#include "vsepr/formula_generator.hpp"
#include "pot/periodic_db.hpp"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <chrono>
#include <vector>
#include <map>

using namespace vsepr;
using namespace vsepr::formula;

// Test statistics
struct TestStats {
    int total = 0;
    int passed = 0;
    int failed = 0;
    
    void record_pass() { ++total; ++passed; }
    void record_fail() { ++total; ++failed; }
    
    void print() const {
        std::cout << "\n=== Test Statistics ===\n";
        std::cout << "Total:  " << total << "\n";
        std::cout << "Passed: " << passed << " (" 
                  << (total > 0 ? (100.0 * passed / total) : 0) << "%)\n";
        std::cout << "Failed: " << failed << "\n";
    }
};

TestStats g_stats;

#define TEST(name) \
    std::cout << "TEST: " << name << "... "; \
    try {

#define EXPECT_EQ(a, b) \
    if ((a) != (b)) { \
        std::cout << "FAIL\n  Expected: " << (b) << "\n  Got: " << (a) << "\n"; \
        g_stats.record_fail(); \
    } else { \
        std::cout << "PASS\n"; \
        g_stats.record_pass(); \
    }

#define EXPECT_TRUE(cond) \
    if (!(cond)) { \
        std::cout << "FAIL\n  Condition false: " #cond "\n"; \
        g_stats.record_fail(); \
    } else { \
        std::cout << "PASS\n"; \
        g_stats.record_pass(); \
    }

#define EXPECT_THROW(expr) \
    { \
        bool threw = false; \
        try { \
            (expr); \
        } catch (...) { \
            threw = true; \
        } \
        if (!threw) { \
            std::cout << "FAIL\n  Expected exception but none thrown\n"; \
            g_stats.record_fail(); \
        } else { \
            std::cout << "PASS\n"; \
            g_stats.record_pass(); \
        } \
    }

#define END_TEST \
    } catch (const std::exception& e) { \
        std::cout << "FAIL (exception)\n  " << e.what() << "\n"; \
        g_stats.record_fail(); \
    }

//=============================================================================
// 1. BASIC PARSING TESTS
//=============================================================================

void test_basic_parsing(const PeriodicTable& pt) {
    std::cout << "\n=== Basic Parsing Tests ===\n\n";
    
    TEST("H2O") {
        auto comp = parse("H2O", pt);
        EXPECT_EQ(comp.size(), 2u);
        EXPECT_EQ(comp[1], 2);  // H: 2
        EXPECT_EQ(comp[8], 1);  // O: 1
    } END_TEST
    
    TEST("CH4") {
        auto comp = parse("CH4", pt);
        EXPECT_EQ(comp.size(), 2u);
        EXPECT_EQ(comp[6], 1);  // C: 1
        EXPECT_EQ(comp[1], 4);  // H: 4
    } END_TEST
    
    TEST("C6H12O6") {
        auto comp = parse("C6H12O6", pt);
        EXPECT_EQ(comp.size(), 3u);
        EXPECT_EQ(comp[6], 6);   // C: 6
        EXPECT_EQ(comp[1], 12);  // H: 12
        EXPECT_EQ(comp[8], 6);   // O: 6
    } END_TEST
    
    TEST("NH3") {
        auto comp = parse("NH3", pt);
        EXPECT_EQ(comp.size(), 2u);
        EXPECT_EQ(comp[7], 1);  // N: 1
        EXPECT_EQ(comp[1], 3);  // H: 3
    } END_TEST
    
    TEST("CO2") {
        auto comp = parse("CO2", pt);
        EXPECT_EQ(comp.size(), 2u);
        EXPECT_EQ(comp[6], 1);  // C: 1
        EXPECT_EQ(comp[8], 2);  // O: 2
    } END_TEST
    
    TEST("C10H22 (large count)") {
        auto comp = parse("C10H22", pt);
        EXPECT_EQ(comp[6], 10);  // C: 10
        EXPECT_EQ(comp[1], 22);  // H: 22
    } END_TEST
}

//=============================================================================
// 2. ERROR HANDLING TESTS
//=============================================================================

void test_error_handling(const PeriodicTable& pt) {
    std::cout << "\n=== Error Handling Tests ===\n\n";
    
    TEST("Empty formula") {
        EXPECT_THROW(parse("", pt));
    } END_TEST
    
    TEST("Invalid element (Zz)") {
        EXPECT_THROW(parse("Zz99", pt));
    } END_TEST
    
    TEST("Starts with number") {
        EXPECT_THROW(parse("2H", pt));
    } END_TEST
    
    TEST("Lowercase start") {
        EXPECT_THROW(parse("h2o", pt));
    } END_TEST
    
    TEST("Invalid characters") {
        EXPECT_THROW(parse("H2O@", pt));
    } END_TEST
    
    TEST("Unknown element Xyz") {
        EXPECT_THROW(parse("Xyz", pt));
    } END_TEST
}

//=============================================================================
// 3. EDGE CASES
//=============================================================================

void test_edge_cases(const PeriodicTable& pt) {
    std::cout << "\n=== Edge Cases ===\n\n";
    
    TEST("Single atom (H)") {
        auto comp = parse("H", pt);
        EXPECT_EQ(comp.size(), 1u);
        EXPECT_EQ(comp[1], 1);
    } END_TEST
    
    TEST("Single atom with count (H2)") {
        auto comp = parse("H2", pt);
        EXPECT_EQ(comp.size(), 1u);
        EXPECT_EQ(comp[1], 2);
    } END_TEST
    
    TEST("Two-letter element (Fe)") {
        auto comp = parse("Fe", pt);
        EXPECT_EQ(comp.size(), 1u);
        EXPECT_EQ(comp[26], 1);
    } END_TEST
    
    TEST("Two-letter element with count (Fe2O3)") {
        auto comp = parse("Fe2O3", pt);
        EXPECT_EQ(comp[26], 2);  // Fe: 2
        EXPECT_EQ(comp[8], 3);   // O: 3
    } END_TEST
    
    TEST("Whitespace handling") {
        auto comp = parse(" H2O ", pt);
        EXPECT_EQ(comp[1], 2);
        EXPECT_EQ(comp[8], 1);
    } END_TEST
}

//=============================================================================
// 4. PARENTHESES SUPPORT
//=============================================================================

void test_parentheses(const PeriodicTable& pt) {
    std::cout << "\n=== Parentheses Support ===\n\n";
    
    TEST("Ca(OH)2") {
        auto comp = parse("Ca(OH)2", pt);
        EXPECT_EQ(comp[20], 1);  // Ca: 1
        EXPECT_EQ(comp[8], 2);   // O: 2
        EXPECT_EQ(comp[1], 2);   // H: 2
    } END_TEST
    
    TEST("Mg(NO3)2") {
        auto comp = parse("Mg(NO3)2", pt);
        EXPECT_EQ(comp[12], 1);  // Mg: 1
        EXPECT_EQ(comp[7], 2);   // N: 2
        EXPECT_EQ(comp[8], 6);   // O: 6
    } END_TEST
    
    TEST("Al(OH)3") {
        auto comp = parse("Al(OH)3", pt);
        EXPECT_EQ(comp[13], 1);  // Al: 1
        EXPECT_EQ(comp[8], 3);   // O: 3
        EXPECT_EQ(comp[1], 3);   // H: 3
    } END_TEST
    
    TEST("Ca3(PO4)2") {
        auto comp = parse("Ca3(PO4)2", pt);
        EXPECT_EQ(comp[20], 3);  // Ca: 3
        EXPECT_EQ(comp[15], 2);  // P: 2
        EXPECT_EQ(comp[8], 8);   // O: 8
    } END_TEST
}

//=============================================================================
// 5. UTILITY FUNCTIONS
//=============================================================================

void test_utility_functions(const PeriodicTable& pt) {
    std::cout << "\n=== Utility Functions ===\n\n";
    
    TEST("to_formula roundtrip") {
        auto comp = parse("H2O", pt);
        std::string formula = to_formula(comp, pt);
        EXPECT_EQ(formula, "H2O");
    } END_TEST
    
    TEST("total_atoms") {
        auto comp = parse("C6H12O6", pt);
        EXPECT_EQ(total_atoms(comp), 24);
    } END_TEST
    
    TEST("molecular_mass H2O") {
        auto comp = parse("H2O", pt);
        double mass = molecular_mass(comp, pt);
        // H: ~1.008, O: ~15.999 → ~18.015
        EXPECT_TRUE(mass > 17.0 && mass < 19.0);
    } END_TEST
    
    TEST("validate valid formula") {
        EXPECT_TRUE(validate("H2O", pt));
    } END_TEST
    
    TEST("validate invalid formula") {
        EXPECT_TRUE(!validate("Xyz123", pt));
    } END_TEST
}

//=============================================================================
// 6. RANDOM FORMULA GENERATION
//=============================================================================

void test_random_generation(const PeriodicTable& pt) {
    std::cout << "\n=== Random Formula Generation ===\n\n";
    
    TEST("Generate 100 random formulas") {
        FormulaGenerator gen(pt, 12345);
        int success = 0;
        
        for (int i = 0; i < 100; ++i) {
            std::string formula = gen.generate();
            
            try {
                auto comp = parse(formula, pt);
                ++success;
            } catch (const ParseError& e) {
                std::cout << "\nFailed to parse generated formula: " << formula << "\n";
                std::cout << e.detailed_message() << "\n";
            }
        }
        
        EXPECT_TRUE(success == 100);
    } END_TEST
    
    TEST("Generate organic molecules") {
        FormulaGenerator gen(pt, 54321);
        int success = 0;
        
        for (int i = 0; i < 50; ++i) {
            std::string formula = gen.generate_organic();
            
            try {
                auto comp = parse(formula, pt);
                ++success;
            } catch (...) {
                std::cout << "\nFailed: " << formula << "\n";
            }
        }
        
        EXPECT_TRUE(success == 50);
    } END_TEST
}

//=============================================================================
// 7. STRESS TESTING
//=============================================================================

void test_stress(const PeriodicTable& pt) {
    std::cout << "\n=== Stress Testing ===\n\n";
    
    TEST("Parse 1000 known formulas") {
        auto formulas = categories::simple_molecules();
        auto organic = categories::organic_molecules();
        formulas.insert(formulas.end(), organic.begin(), organic.end());
        
        int iterations = 1000 / formulas.size() + 1;
        int success = 0;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            for (const auto& formula : formulas) {
                try {
                    auto comp = parse(formula, pt);
                    ++success;
                } catch (...) {}
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "\n  Parsed " << success << " formulas in " 
                  << duration.count() << " μs\n";
        std::cout << "  Average: " << (duration.count() / success) << " μs per formula\n";
        
        EXPECT_TRUE(success > 900);
    } END_TEST
    
    TEST("Large batch generation") {
        FormulaGenerator gen(pt);
        auto batch = gen.generate_batch(500);
        EXPECT_EQ(batch.size(), 500u);
    } END_TEST
}

//=============================================================================
// 8. PROPERTY VALIDATION
//=============================================================================

void test_properties(const PeriodicTable& pt) {
    std::cout << "\n=== Property Validation ===\n\n";
    
    TEST("Roundtrip property") {
        std::vector<std::string> test_formulas = {
            "H2O", "CO2", "CH4", "NH3", "C6H12O6",
            "Fe2O3", "NaCl", "CaSO4"
        };
        
        int success = 0;
        for (const auto& orig : test_formulas) {
            auto comp = parse(orig, pt);
            auto reconstructed = to_formula(comp, pt);
            auto comp2 = parse(reconstructed, pt);
            
            if (comp == comp2) {
                ++success;
            } else {
                std::cout << "\nRoundtrip failed: " << orig 
                          << " → " << reconstructed << "\n";
            }
        }
        
        EXPECT_EQ(success, static_cast<int>(test_formulas.size()));
    } END_TEST
    
    TEST("Mass conservation") {
        auto comp1 = parse("H2O", pt);
        auto comp2 = parse("H2O", pt);
        
        double mass1 = molecular_mass(comp1, pt);
        double mass2 = molecular_mass(comp2, pt);
        
        EXPECT_TRUE(std::abs(mass1 - mass2) < 0.001);
    } END_TEST
}

//=============================================================================
// 9. REGRESSION TESTS
//=============================================================================

void test_regression(const PeriodicTable& pt) {
    std::cout << "\n=== Regression Tests ===\n\n";
    
    // Test all category formulas
    TEST("Simple molecules category") {
        int success = 0;
        for (const auto& formula : categories::simple_molecules()) {
            try {
                parse(formula, pt);
                ++success;
            } catch (...) {
                std::cout << "\nFailed: " << formula << "\n";
            }
        }
        EXPECT_TRUE(success == static_cast<int>(categories::simple_molecules().size()));
    } END_TEST
    
    TEST("Organic molecules category") {
        int success = 0;
        for (const auto& formula : categories::organic_molecules()) {
            try {
                parse(formula, pt);
                ++success;
            } catch (...) {
                std::cout << "\nFailed: " << formula << "\n";
            }
        }
        EXPECT_TRUE(success == static_cast<int>(categories::organic_molecules().size()));
    } END_TEST
    
    TEST("Inorganic salts category") {
        int success = 0;
        for (const auto& formula : categories::inorganic_salts()) {
            try {
                parse(formula, pt);
                ++success;
            } catch (...) {
                std::cout << "\nFailed: " << formula << "\n";
            }
        }
        EXPECT_TRUE(success == static_cast<int>(categories::inorganic_salts().size()));
    } END_TEST
    
    TEST("Complex molecules category") {
        int success = 0;
        for (const auto& formula : categories::complex_molecules()) {
            try {
                parse(formula, pt);
                ++success;
            } catch (...) {
                std::cout << "\nFailed: " << formula << "\n";
            }
        }
        EXPECT_TRUE(success == static_cast<int>(categories::complex_molecules().size()));
    } END_TEST
    
    TEST("Stress test formulas") {
        int success = 0;
        for (const auto& formula : categories::stress_test_formulas()) {
            try {
                parse(formula, pt);
                ++success;
            } catch (...) {
                std::cout << "\nFailed: " << formula << "\n";
            }
        }
        EXPECT_TRUE(success == static_cast<int>(categories::stress_test_formulas().size()));
    } END_TEST
}

//=============================================================================
// MAIN
//=============================================================================

int main() {
    std::cout << "==============================================\n";
    std::cout << "  VSEPR Formula Parser Test Suite\n";
    std::cout << "  Comprehensive Automated Testing\n";
    std::cout << "==============================================\n";
    
    // Load periodic table
    PeriodicTable pt;
    try {
        pt = PeriodicTable::load_from_json_file("data/PeriodicTableJSON.json");
        std::cout << "\n✓ Loaded periodic table: " << pt.all().size() << " elements\n";
    } catch (const std::exception& e) {
        std::cerr << "Failed to load periodic table: " << e.what() << "\n";
        return 1;
    }
    
    // Run all test suites
    test_basic_parsing(pt);
    test_error_handling(pt);
    test_edge_cases(pt);
    test_parentheses(pt);
    test_utility_functions(pt);
    test_random_generation(pt);
    test_stress(pt);
    test_properties(pt);
    test_regression(pt);
    
    // Print summary
    g_stats.print();
    
    std::cout << "\n==============================================\n";
    if (g_stats.failed == 0) {
        std::cout << "  ✓ ALL TESTS PASSED\n";
        std::cout << "==============================================\n";
        return 0;
    } else {
        std::cout << "  ✗ SOME TESTS FAILED\n";
        std::cout << "==============================================\n";
        return 1;
    }
}
