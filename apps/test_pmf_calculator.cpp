/**
 * PMF Calculator Test
 * 
 * Validates that PMF calculation works correctly for known systems.
 * 
 * Test case: Ar-Ar Lennard-Jones pair
 * - Known equilibrium distance: ~3.8 Å
 * - Known well depth: ~0.24 kcal/mol
 */

#include "atomistic/analysis/pmf.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>

using namespace atomistic;

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  PMF Calculator Test                                      ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    // Test 1: Parse pair type from string
    std::cout << "Test 1: Pair Type Parsing\n";
    
    try {
        auto pair1 = PairType::from_string("Mg:F");
        std::cout << "  ✅ Parsed 'Mg:F' → Z1=" << pair1.type1 << ", Z2=" << pair1.type2 << "\n";
        std::cout << "     Formatted: " << pair1.to_string() << "\n";
        
        auto pair2 = PairType::from_string("Ar:Ar");
        std::cout << "  ✅ Parsed 'Ar:Ar' → Z1=" << pair2.type1 << ", Z2=" << pair2.type2 << "\n";
        std::cout << "     Formatted: " << pair2.to_string() << "\n";
        
    } catch (const std::exception& e) {
        std::cout << "  ❌ FAIL: " << e.what() << "\n";
        return 1;
    }
    
    std::cout << "\n";
    
    // Test 2: Compute PMF from synthetic RDF (Ar-Ar LJ)
    std::cout << "Test 2: PMF from Synthetic RDF (Ar-Ar)\n";
    
    // Create synthetic g(r) for LJ system
    // g(r) should have:
    // - Zero at short range (r < σ)
    // - Peak at r ≈ 1.12 σ ≈ 3.8 Å
    // - Approach 1.0 at large r
    
    std::vector<double> r_bins;
    std::vector<double> g_r;
    
    double sigma = 3.4;  // Ar LJ σ (Å)
    double dr = 0.1;     // Bin width
    
    for (double r = 0.5; r <= 10.0; r += dr) {
        r_bins.push_back(r);
        
        // Synthetic g(r) based on LJ structure
        double x = r / sigma;
        
        if (x < 0.9) {
            // Hard core repulsion
            g_r.push_back(0.0);
        } else if (x < 1.5) {
            // First peak (Gaussian-like)
            double peak_pos = 1.12;
            double width = 0.2;
            double amplitude = 2.5;
            g_r.push_back(amplitude * std::exp(-0.5 * std::pow((x - peak_pos) / width, 2)));
        } else {
            // Approach bulk (g → 1)
            double decay = std::exp(-(x - 1.5) / 2.0);
            g_r.push_back(1.0 - 0.5 * decay);
        }
    }
    
    // Compute PMF
    PMFCalculator calc;
    
    auto pair = PairType::from_string("Ar:Ar");
    double temperature = 300.0;  // K
    
    auto pmf_result = calc.compute_from_rdf(r_bins, g_r, pair, temperature);
    
    std::cout << "  Computed PMF for " << pmf_result.pair.to_string() << " at " << temperature << " K\n";
    std::cout << "  Basin depth:    " << std::fixed << std::setprecision(2) << pmf_result.basin_depth << " kcal/mol (positive = attractive)\n";
    std::cout << "  Basin position: " << pmf_result.basin_position << " Å\n";

    if (pmf_result.has_barrier) {
        std::cout << "  Barrier height: " << pmf_result.barrier_height << " kcal/mol\n";
    } else {
        std::cout << "  Barrier height: none detected\n";
    }

    std::cout << "  PMF shift:      " << pmf_result.pmf_shift << " kcal/mol\n";
    std::cout << "  Floored bins:   " << pmf_result.floored_bins << "\n";

    // Validate results
    bool pass_basin_pos = (pmf_result.basin_position > 3.5 && pmf_result.basin_position < 4.5);
    bool pass_basin_depth = (pmf_result.basin_depth > 0.0 && pmf_result.basin_depth < 2.0);  // Should be positive, < 2 kcal/mol
    
    if (pass_basin_pos) {
        std::cout << "  ✅ Basin position reasonable (~3.8 Å expected)\n";
    } else {
        std::cout << "  ❌ Basin position wrong: " << pmf_result.basin_position << " Å\n";
    }
    
    if (pass_basin_depth) {
        std::cout << "  ✅ Basin depth positive and reasonable\n";
    } else {
        std::cout << "  ❌ Basin depth wrong: " << pmf_result.basin_depth << " kcal/mol\n";
    }
    
    std::cout << "\n";
    
    // Test 3: Save to CSV
    std::cout << "Test 3: Save PMF to CSV\n";
    
    try {
        calc.save_csv(pmf_result, "test_pmf_ar_ar.csv");
        std::cout << "  ✅ Saved to test_pmf_ar_ar.csv\n";
        
        calc.save_metadata_json(pmf_result, "test_pmf_ar_ar.json");
        std::cout << "  ✅ Saved metadata to test_pmf_ar_ar.json\n";
        
    } catch (const std::exception& e) {
        std::cout << "  ❌ FAIL: " << e.what() << "\n";
        return 1;
    }
    
    std::cout << "\n";
    
    // Overall result
    if (pass_basin_pos && pass_basin_depth) {
        std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ✅ TEST PASSED                                           ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        return 0;
    } else {
        std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ❌ TEST FAILED                                           ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        return 1;
    }
}
