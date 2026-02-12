/**
 * @file formula_fuzz_tester.cpp
 * @brief Intensive fuzz testing for formula parser
 * 
 * This tool performs extensive automated testing with:
 * - Random formula generation
 * - Mutation-based fuzzing
 * - Edge case discovery
 * - Crash detection
 * - Performance profiling
 * 
 * Usage:
 *   ./formula_fuzz_tester [--iterations N] [--seed SEED] [--verbose]
 */

#include "vsepr/formula_parser.hpp"
#include "vsepr/formula_generator.hpp"
#include "pot/periodic_db.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <fstream>
#include <cstring>

using namespace vsepr;
using namespace vsepr::formula;

struct FuzzStats {
    int total_tests = 0;
    int successful_parses = 0;
    int expected_errors = 0;
    int unexpected_errors = 0;
    int crashes = 0;
    
    std::chrono::microseconds total_time{0};
    std::chrono::microseconds min_time{std::chrono::microseconds::max()};
    std::chrono::microseconds max_time{0};
    
    std::vector<std::string> failed_formulas;
    std::vector<std::string> slowest_formulas;
    
    void record_parse(const std::string& formula, std::chrono::microseconds duration, bool success) {
        ++total_tests;
        
        if (success) {
            ++successful_parses;
        } else {
            ++expected_errors;
        }
        
        total_time += duration;
        
        if (duration < min_time) min_time = duration;
        if (duration > max_time) {
            max_time = duration;
            if (slowest_formulas.empty() || duration.count() > 1000) {
                slowest_formulas.push_back(formula + " (" + std::to_string(duration.count()) + "μs)");
            }
        }
    }
    
    void print() const {
        std::cout << "\n==============================================\n";
        std::cout << "  Fuzz Testing Results\n";
        std::cout << "==============================================\n\n";
        
        std::cout << "Tests run:          " << total_tests << "\n";
        std::cout << "Successful parses:  " << successful_parses << " (" 
                  << (100.0 * successful_parses / total_tests) << "%)\n";
        std::cout << "Expected errors:    " << expected_errors << "\n";
        std::cout << "Unexpected errors:  " << unexpected_errors << "\n";
        std::cout << "Crashes:            " << crashes << "\n\n";
        
        std::cout << "Performance:\n";
        std::cout << "  Total time:   " << total_time.count() << " μs\n";
        std::cout << "  Average time: " << (total_time.count() / total_tests) << " μs\n";
        std::cout << "  Min time:     " << min_time.count() << " μs\n";
        std::cout << "  Max time:     " << max_time.count() << " μs\n\n";
        
        if (!slowest_formulas.empty()) {
            std::cout << "Slowest formulas:\n";
            for (size_t i = 0; i < std::min(size_t(5), slowest_formulas.size()); ++i) {
                std::cout << "  " << slowest_formulas[i] << "\n";
            }
            std::cout << "\n";
        }
        
        if (!failed_formulas.empty()) {
            std::cout << "Failed formulas (unexpected):\n";
            for (const auto& f : failed_formulas) {
                std::cout << "  " << f << "\n";
            }
        }
    }
};

/**
 * Test a single formula with timing
 */
void test_formula(const std::string& formula, const PeriodicTable& pt, 
                  FuzzStats& stats, bool verbose) {
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        auto comp = parse(formula, pt);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        stats.record_parse(formula, duration, true);
        
        if (verbose) {
            std::cout << "✓ " << formula << " → " << to_formula(comp, pt) 
                      << " (" << duration.count() << "μs)\n";
        }
    } catch (const ParseError& e) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        stats.record_parse(formula, duration, false);
        
        if (verbose) {
            std::cout << "✗ " << formula << " - " << e.what() << "\n";
        }
    } catch (const std::exception& e) {
        ++stats.unexpected_errors;
        stats.failed_formulas.push_back(formula + " [" + e.what() + "]");
        
        if (verbose) {
            std::cout << "! " << formula << " - UNEXPECTED: " << e.what() << "\n";
        }
    }
}

/**
 * Mutate a formula string (for mutation-based fuzzing)
 */
std::string mutate_formula(const std::string& formula, std::mt19937& rng) {
    if (formula.empty()) return "H";
    
    std::uniform_int_distribution<int> mutation_type(0, 4);
    std::string mutated = formula;
    
    switch (mutation_type(rng)) {
        case 0: // Insert random character
            if (!mutated.empty()) {
                std::uniform_int_distribution<size_t> pos(0, mutated.size());
                std::uniform_int_distribution<char> ch('A', 'Z');
                mutated.insert(pos(rng), 1, ch(rng));
            }
            break;
            
        case 1: // Delete character
            if (mutated.size() > 1) {
                std::uniform_int_distribution<size_t> pos(0, mutated.size() - 1);
                mutated.erase(pos(rng), 1);
            }
            break;
            
        case 2: // Change character
            if (!mutated.empty()) {
                std::uniform_int_distribution<size_t> pos(0, mutated.size() - 1);
                std::uniform_int_distribution<char> ch('0', 'z');
                mutated[pos(rng)] = ch(rng);
            }
            break;
            
        case 3: // Duplicate substring
            if (mutated.size() >= 2) {
                std::uniform_int_distribution<size_t> start(0, mutated.size() - 2);
                std::uniform_int_distribution<size_t> len(1, mutated.size() - start(rng));
                size_t s = start(rng);
                mutated += mutated.substr(s, len(rng));
            }
            break;
            
        case 4: // Add number
            std::uniform_int_distribution<int> num(2, 99);
            mutated += std::to_string(num(rng));
            break;
    }
    
    return mutated;
}

void run_random_fuzz(const PeriodicTable& pt, int iterations, unsigned seed, bool verbose) {
    std::cout << "\n=== Random Formula Generation Fuzz Test ===\n";
    std::cout << "Iterations: " << iterations << "\n";
    std::cout << "Seed: " << seed << "\n\n";
    
    FuzzStats stats;
    FormulaGenerator gen(pt, seed);
    
    for (int i = 0; i < iterations; ++i) {
        std::string formula = gen.generate();
        test_formula(formula, pt, stats, verbose);
        
        if (!verbose && (i + 1) % 100 == 0) {
            std::cout << "." << std::flush;
        }
    }
    
    if (!verbose) std::cout << "\n";
    stats.print();
}

void run_mutation_fuzz(const PeriodicTable& pt, int iterations, unsigned seed, bool verbose) {
    std::cout << "\n=== Mutation-Based Fuzz Test ===\n";
    std::cout << "Iterations: " << iterations << "\n";
    std::cout << "Seed: " << seed << "\n\n";
    
    FuzzStats stats;
    std::mt19937 rng(seed);
    
    // Start with valid formulas
    std::vector<std::string> base_formulas = {
        "H2O", "CO2", "CH4", "NH3", "C6H12O6",
        "NaCl", "CaCO3", "H2SO4", "Fe2O3"
    };
    
    std::uniform_int_distribution<size_t> base_dist(0, base_formulas.size() - 1);
    
    for (int i = 0; i < iterations; ++i) {
        std::string base = base_formulas[base_dist(rng)];
        std::string mutated = mutate_formula(base, rng);
        
        test_formula(mutated, pt, stats, verbose);
        
        if (!verbose && (i + 1) % 100 == 0) {
            std::cout << "." << std::flush;
        }
    }
    
    if (!verbose) std::cout << "\n";
    stats.print();
}

void run_edge_case_fuzz(const PeriodicTable& pt, bool verbose) {
    std::cout << "\n=== Edge Case Fuzz Test ===\n\n";
    
    FuzzStats stats;
    
    // Generate edge cases
    std::vector<std::string> edge_cases = {
        "",              // Empty
        " ",             // Whitespace only
        "H",             // Single atom
        "H1",            // Explicit count of 1
        "H0",            // Zero count (should error)
        "1H",            // Number first
        "h",             // Lowercase
        "HH",            // Repeated element
        "H2O2",          // Valid peroxide
        "C999",          // Large count
        "C1000",         // Very large count
        "Ca(OH)2",       // Parentheses
        "((H))",         // Nested parentheses
        "Ca(OH",         // Unclosed parentheses
        "Ca)OH",         // Mismatched parentheses
        "H-O-H",         // Hyphens
        "H₂O",           // Unicode subscripts
        "H2O ",          // Trailing space
        " H2O",          // Leading space
        "H 2 O",         // Internal spaces
        "Xyz",           // Invalid element
        "H2O3N4C5",      // Many elements
        "ABCDEFGH",      // All invalid
        "123456",        // All numbers
    };
    
    for (const auto& formula : edge_cases) {
        test_formula(formula, pt, stats, verbose);
    }
    
    stats.print();
}

void run_category_fuzz(const PeriodicTable& pt, bool verbose) {
    std::cout << "\n=== Category Validation Fuzz Test ===\n\n";
    
    FuzzStats stats;
    
    auto test_category = [&](const std::string& name, const std::vector<std::string>& formulas) {
        std::cout << "\nTesting " << name << " (" << formulas.size() << " formulas)...\n";
        for (const auto& formula : formulas) {
            test_formula(formula, pt, stats, verbose);
        }
    };
    
    test_category("Simple molecules", categories::simple_molecules());
    test_category("Organic molecules", categories::organic_molecules());
    test_category("Inorganic salts", categories::inorganic_salts());
    test_category("Complex molecules", categories::complex_molecules());
    test_category("Stress test formulas", categories::stress_test_formulas());
    
    stats.print();
}

int main(int argc, char* argv[]) {
    // Parse arguments
    int iterations = 1000;
    unsigned seed = std::random_device{}();
    bool verbose = false;
    
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            iterations = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = std::stoul(argv[++i]);
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            std::cout << "Formula Parser Fuzz Tester\n\n";
            std::cout << "Usage: " << argv[0] << " [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --iterations N    Number of iterations (default: 1000)\n";
            std::cout << "  --seed SEED       Random seed (default: random)\n";
            std::cout << "  --verbose, -v     Verbose output\n";
            std::cout << "  --help, -h        Show this help\n";
            return 0;
        }
    }
    
    std::cout << "==============================================\n";
    std::cout << "  VSEPR Formula Parser Fuzz Tester\n";
    std::cout << "  Intensive Automated Testing\n";
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
    
    // Run fuzz tests
    run_edge_case_fuzz(pt, verbose);
    run_category_fuzz(pt, verbose);
    run_random_fuzz(pt, iterations, seed, verbose);
    run_mutation_fuzz(pt, iterations, seed + 1, verbose);
    
    std::cout << "\n==============================================\n";
    std::cout << "  ✓ FUZZ TESTING COMPLETE\n";
    std::cout << "==============================================\n";
    
    return 0;
}
