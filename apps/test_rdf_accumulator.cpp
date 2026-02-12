/**
 * RDF Accumulator Test
 * 
 * Validates RDF accumulation with known systems.
 */

#include "cli/rdf_accumulator.hpp"
#include "atomistic/core/state.hpp"
#include <iostream>
#include <iomanip>
#include <random>
#include <algorithm>

using namespace vsepr::cli;
using namespace atomistic;

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  RDF Accumulator Test                                     ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    // Test 1: Simple cubic lattice (FCC-like)
    std::cout << "Test 1: FCC lattice (4 atoms)\n";
    
    State state;
    state.N = 4;
    state.X.resize(4);
    
    // FCC positions in 10 Å box
    double L = 10.0;
    state.X[0] = {0.0, 0.0, 0.0};
    state.X[1] = {L/2, L/2, 0.0};
    state.X[2] = {L/2, 0.0, L/2};
    state.X[3] = {0.0, L/2, L/2};
    
    // Setup RDF accumulator
    double r_max = L / 2.0;  // 5 Å
    double dr = 0.1;         // 0.1 Å bins
    
    RDFAccumulator rdf(r_max, dr);
    
    std::vector<double> box = {L, L, L};
    
    // Accumulate (single snapshot)
    rdf.accumulate(state, box);
    
    // Compute g(r)
    double V = L * L * L;
    rdf.compute_gr(state.N, V);
    
    auto r_bins = rdf.get_r_bins();
    auto g_r = rdf.get_gr();
    
    std::cout << "  N atoms: " << state.N << "\n";
    std::cout << "  Box: " << L << " Å\n";
    std::cout << "  r_max: " << r_max << " Å\n";
    std::cout << "  N bins: " << r_bins.size() << "\n";
    std::cout << "  N samples: " << rdf.get_n_samples() << "\n\n";
    
    std::cout << "  First few bins:\n";
    std::cout << "  r (Å)    g(r)\n";
    std::cout << "  ──────────────\n";
    for (size_t i = 0; i < std::min(size_t(10), r_bins.size()); ++i) {
        std::cout << "  " << std::fixed << std::setprecision(2) << std::setw(6) << r_bins[i]
                  << "  " << std::setprecision(3) << g_r[i] << "\n";
    }
    
    std::cout << "\n";
    
    // Test 2: Uniform random (should approach g(r) ≈ 1 for large N)
    std::cout << "Test 2: Random gas (64 atoms, multiple snapshots)\n";
    
    State state2;
    state2.N = 64;
    state2.X.resize(64);
    
    RDFAccumulator rdf2(r_max, dr);
    
    // Accumulate 100 random snapshots
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(0.0, L);
    
    for (int snap = 0; snap < 100; ++snap) {
        // Random positions
        for (uint32_t i = 0; i < state2.N; ++i) {
            state2.X[i] = {dist(rng), dist(rng), dist(rng)};
        }
        
        rdf2.accumulate(state2, box);
    }
    
    rdf2.compute_gr(state2.N, V);
    
    auto r_bins2 = rdf2.get_r_bins();
    auto g_r2 = rdf2.get_gr();
    
    std::cout << "  N atoms: " << state2.N << "\n";
    std::cout << "  N samples: " << rdf2.get_n_samples() << "\n";
    std::cout << "  Expected: g(r) ≈ 1.0 (ideal gas)\n\n";
    
    std::cout << "  Sample g(r) values:\n";
    std::cout << "  r (Å)    g(r)    Deviation\n";
    std::cout << "  ────────────────────────────\n";
    
    double mean_deviation = 0.0;
    int count = 0;
    
    for (size_t i = 5; i < std::min(size_t(20), r_bins2.size()); ++i) {
        double dev = std::abs(g_r2[i] - 1.0);
        mean_deviation += dev;
        count++;
        
        std::cout << "  " << std::fixed << std::setprecision(2) << std::setw(6) << r_bins2[i]
                  << "  " << std::setprecision(3) << g_r2[i]
                  << "  " << std::showpos << dev << std::noshowpos << "\n";
    }
    
    mean_deviation /= count;
    
    std::cout << "\n";
    std::cout << "  Mean deviation from 1.0: " << std::fixed << std::setprecision(3) << mean_deviation << "\n";
    
    // Validation
    bool pass = (mean_deviation < 0.2);  // Within 20% is reasonable for N=64
    
    std::cout << "\n";
    
    if (pass) {
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
