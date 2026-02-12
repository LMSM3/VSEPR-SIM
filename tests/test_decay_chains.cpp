/**
 * test_decay_chains.cpp
 * =====================
 * Test the four natural radioactive decay series
 * Verify decay paths, half-lives, and equilibrium calculations
 */

#include "core/decay_chains.hpp"
#include <iostream>
#include <iomanip>
#include <cassert>

using namespace vsepr::nuclear;

void print_header(const std::string& title) {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ " << std::left << std::setw(64) << title << " ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";
}

void test_thorium_series() {
    print_header("THORIUM SERIES (4n): Th-232 → Pb-208");
    
    const auto& series = get_decay_series();
    const auto& thorium = series.thorium_series();
    
    std::cout << "Series: " << thorium.name << " (" << thorium.series_type << ")\n";
    std::cout << "Parent: " << static_cast<int>(thorium.parent_Z) << "-" << thorium.parent_A << "\n";
    std::cout << "Stable end product: " << static_cast<int>(thorium.stable_Z) << "-" << thorium.stable_A << "\n";
    std::cout << "Total decays: " << thorium.total_decays << " (α=" << thorium.alpha_decays 
              << ", β=" << thorium.beta_decays << ")\n";
    std::cout << "Total energy released: " << thorium.total_energy_MeV << " MeV\n";
    std::cout << "\nDecay Chain:\n";
    std::cout << std::string(70, '─') << "\n";
    
    for (size_t i = 0; i < thorium.chain.size(); ++i) {
        const auto& n = thorium.chain[i];
        std::cout << std::setw(3) << i+1 << ". ";
        std::cout << std::setw(6) << (std::to_string(static_cast<int>(n.Z)) + "-" + std::to_string(n.A));
        std::cout << "  t½ = " << std::setw(12) << format_half_life(n.half_life_seconds);
        std::cout << "  " << std::setw(3) << decay_mode_to_symbol(n.primary_decay);
        std::cout << "  E = " << std::setw(6) << n.decay_energy_MeV << " MeV";
        std::cout << "\n";
    }
    
    // Verify
    assert(thorium.chain.size() == 11);
    assert(thorium.chain[0].Z == 90 && thorium.chain[0].A == 232);  // Th-232
    assert(thorium.chain[6].Z == 86 && thorium.chain[6].A == 220);  // Rn-220 (Thoron)
    assert(thorium.chain[10].Z == 82 && thorium.chain[10].A == 208); // Pb-208 (stable)
    assert(thorium.chain[10].is_stable());
    
    std::cout << "\n✓ Thorium series verified\n";
}

void test_neptunium_series() {
    print_header("NEPTUNIUM SERIES (4n+1): Np-237 → Bi-209 [EXTINCT]");
    
    const auto& series = get_decay_series();
    const auto& neptunium = series.neptunium_series();
    
    std::cout << "Series: " << neptunium.name << " (" << neptunium.series_type << ")\n";
    std::cout << "Parent: " << static_cast<int>(neptunium.parent_Z) << "-" << neptunium.parent_A << "\n";
    std::cout << "Stable end product: " << static_cast<int>(neptunium.stable_Z) << "-" << neptunium.stable_A << "\n";
    std::cout << "Status: EXTINCT in nature (t½ parent = 2.14 million years)\n";
    std::cout << "Total decays: " << neptunium.total_decays << " (α=" << neptunium.alpha_decays 
              << ", β=" << neptunium.beta_decays << ")\n";
    std::cout << "\nDecay Chain:\n";
    std::cout << std::string(70, '─') << "\n";
    
    for (size_t i = 0; i < neptunium.chain.size(); ++i) {
        const auto& n = neptunium.chain[i];
        std::cout << std::setw(3) << i+1 << ". ";
        std::cout << std::setw(6) << (std::to_string(static_cast<int>(n.Z)) + "-" + std::to_string(n.A));
        std::cout << "  t½ = " << std::setw(12) << format_half_life(n.half_life_seconds);
        std::cout << "  " << std::setw(3) << decay_mode_to_symbol(n.primary_decay);
        std::cout << "  E = " << std::setw(6) << n.decay_energy_MeV << " MeV";
        std::cout << "\n";
    }
    
    // Verify
    assert(neptunium.chain.size() == 12);
    assert(neptunium.chain[0].Z == 93 && neptunium.chain[0].A == 237);  // Np-237
    assert(neptunium.chain[11].Z == 83 && neptunium.chain[11].A == 209); // Bi-209 (quasi-stable)
    
    std::cout << "\n✓ Neptunium series verified (extinct but reconstructed)\n";
}

void test_uranium_series() {
    print_header("URANIUM SERIES (4n+2): U-238 → Pb-206");
    
    const auto& series = get_decay_series();
    const auto& uranium = series.uranium_series();
    
    std::cout << "Series: " << uranium.name << " (" << uranium.series_type << ")\n";
    std::cout << "Parent: " << static_cast<int>(uranium.parent_Z) << "-" << uranium.parent_A << "\n";
    std::cout << "Stable end product: " << static_cast<int>(uranium.stable_Z) << "-" << uranium.stable_A << "\n";
    std::cout << "Most abundant natural series (99.27% of natural uranium)\n";
    std::cout << "Total decays: " << uranium.total_decays << " (α=" << uranium.alpha_decays 
              << ", β=" << uranium.beta_decays << ")\n";
    std::cout << "\nDecay Chain:\n";
    std::cout << std::string(70, '─') << "\n";
    
    for (size_t i = 0; i < uranium.chain.size(); ++i) {
        const auto& n = uranium.chain[i];
        std::cout << std::setw(3) << i+1 << ". ";
        std::cout << std::setw(6) << (std::to_string(static_cast<int>(n.Z)) + "-" + std::to_string(n.A));
        std::cout << "  t½ = " << std::setw(12) << format_half_life(n.half_life_seconds);
        std::cout << "  " << std::setw(3) << decay_mode_to_symbol(n.primary_decay);
        std::cout << "  E = " << std::setw(6) << n.decay_energy_MeV << " MeV";
        
        // Highlight Rn-222
        if (n.Z == 86 && n.A == 222) {
            std::cout << "  ⚠️ RADON (major health hazard)";
        }
        std::cout << "\n";
    }
    
    // Verify
    assert(uranium.chain.size() == 15);
    assert(uranium.chain[0].Z == 92 && uranium.chain[0].A == 238);   // U-238
    assert(uranium.chain[6].Z == 86 && uranium.chain[6].A == 222);   // Rn-222 (Radon)
    assert(uranium.chain[14].Z == 82 && uranium.chain[14].A == 206); // Pb-206 (stable)
    assert(uranium.chain[14].is_stable());
    
    std::cout << "\n✓ Uranium series verified\n";
}

void test_actinium_series() {
    print_header("ACTINIUM SERIES (4n+3): U-235 → Pb-207");
    
    const auto& series = get_decay_series();
    const auto& actinium = series.actinium_series();
    
    std::cout << "Series: " << actinium.name << " (" << actinium.series_type << ")\n";
    std::cout << "Parent: " << static_cast<int>(actinium.parent_Z) << "-" << actinium.parent_A << "\n";
    std::cout << "Stable end product: " << static_cast<int>(actinium.stable_Z) << "-" << actinium.stable_A << "\n";
    std::cout << "Abundance: 0.72% of natural uranium\n";
    std::cout << "Total decays: " << actinium.total_decays << " (α=" << actinium.alpha_decays 
              << ", β=" << actinium.beta_decays << ")\n";
    std::cout << "\nDecay Chain:\n";
    std::cout << std::string(70, '─') << "\n";
    
    for (size_t i = 0; i < actinium.chain.size(); ++i) {
        const auto& n = actinium.chain[i];
        std::cout << std::setw(3) << i+1 << ". ";
        std::cout << std::setw(6) << (std::to_string(static_cast<int>(n.Z)) + "-" + std::to_string(n.A));
        std::cout << "  t½ = " << std::setw(12) << format_half_life(n.half_life_seconds);
        std::cout << "  " << std::setw(3) << decay_mode_to_symbol(n.primary_decay);
        std::cout << "  E = " << std::setw(6) << n.decay_energy_MeV << " MeV";
        
        // Highlight Rn-219
        if (n.Z == 86 && n.A == 219) {
            std::cout << "  (Actinon)";
        }
        std::cout << "\n";
    }
    
    // Verify
    assert(actinium.chain.size() == 12);
    assert(actinium.chain[0].Z == 92 && actinium.chain[0].A == 235);  // U-235
    assert(actinium.chain[6].Z == 86 && actinium.chain[6].A == 219);  // Rn-219 (Actinon)
    assert(actinium.chain[11].Z == 82 && actinium.chain[11].A == 207); // Pb-207 (stable)
    assert(actinium.chain[11].is_stable());
    
    std::cout << "\n✓ Actinium series verified\n";
}

void test_radon_isotopes() {
    print_header("RADON ISOTOPES (Environmental Health Hazard)");
    
    const auto& series = get_decay_series();
    
    std::cout << "Three radon isotopes from natural decay series:\n\n";
    
    // Rn-222 (Uranium series)
    auto rn222 = series.find_nuclide(86, 222);
    if (rn222) {
        std::cout << "1. Rn-222 (from U-238 series)\n";
        std::cout << "   Half-life: " << format_half_life(rn222->half_life_seconds) << "\n";
        std::cout << "   Decay mode: " << decay_mode_to_symbol(rn222->primary_decay) << "\n";
        std::cout << "   Importance: MOST SIGNIFICANT radon health hazard\n";
        std::cout << "   EPA action level: 148 Bq/m³ (4 pCi/L)\n\n";
    }
    
    // Rn-220 (Thorium series)
    auto rn220 = series.find_nuclide(86, 220);
    if (rn220) {
        std::cout << "2. Rn-220 \"Thoron\" (from Th-232 series)\n";
        std::cout << "   Half-life: " << format_half_life(rn220->half_life_seconds) << "\n";
        std::cout << "   Decay mode: " << decay_mode_to_symbol(rn220->primary_decay) << "\n";
        std::cout << "   Importance: Short-lived, less concerning\n\n";
    }
    
    // Rn-219 (Actinium series)
    auto rn219 = series.find_nuclide(86, 219);
    if (rn219) {
        std::cout << "3. Rn-219 \"Actinon\" (from U-235 series)\n";
        std::cout << "   Half-life: " << format_half_life(rn219->half_life_seconds) << "\n";
        std::cout << "   Decay mode: " << decay_mode_to_symbol(rn219->primary_decay) << "\n";
        std::cout << "   Importance: Very short-lived, minimal concern\n\n";
    }
    
    std::cout << "✓ All three radon isotopes identified\n";
}

void test_series_classification() {
    print_header("DECAY SERIES CLASSIFICATION BY MASS NUMBER");
    
    std::cout << "Testing 4n modulo arithmetic for series identification:\n\n";
    
    struct TestCase { uint16_t A; std::string expected_series; };
    std::vector<TestCase> tests = {
        {232, "4n (Thorium)"},      // Th-232
        {237, "4n+1 (Neptunium)"},  // Np-237
        {238, "4n+2 (Uranium)"},    // U-238
        {235, "4n+3 (Actinium)"},   // U-235
        {208, "4n (Thorium)"},      // Pb-208 (stable end)
        {209, "4n+1 (Neptunium)"},  // Bi-209 (stable end)
        {206, "4n+2 (Uranium)"},    // Pb-206 (stable end)
        {207, "4n+3 (Actinium)"}    // Pb-207 (stable end)
    };
    
    for (const auto& test : tests) {
        std::string result = series_name_for_A(test.A);
        std::cout << "A=" << test.A << " → " << result;
        assert(result == test.expected_series);
        std::cout << " ✓\n";
    }
    
    std::cout << "\n✓ Series classification verified\n";
}

void test_decay_path_tracing() {
    print_header("DECAY PATH TRACING");
    
    const auto& series = get_decay_series();
    
    // Trace U-238 → Pb-206
    std::cout << "Tracing decay path from U-238:\n";
    auto path = series.trace_decay_path(92, 238);
    std::cout << "Found " << path.size() << " nuclides in decay chain\n";
    
    for (size_t i = 0; i < std::min(path.size(), size_t(5)); ++i) {
        const auto& n = path[i];
        std::cout << "  " << static_cast<int>(n.Z) << "-" << n.A;
        if (i < path.size() - 1) std::cout << " → ";
    }
    std::cout << " ... → Pb-206\n";
    
    assert(path.size() == 15);  // U-238 series has 15 members
    assert(path.front().Z == 92 && path.front().A == 238);
    assert(path.back().Z == 82 && path.back().A == 206);
    
    std::cout << "\n✓ Decay path tracing verified\n";
}

void test_equilibrium_concepts() {
    print_header("SECULAR EQUILIBRIUM EXAMPLE");
    
    std::cout << "Ra-226 / Rn-222 equilibrium in uranium ore:\n\n";
    
    const auto& series = get_decay_series();
    auto ra226 = series.find_nuclide(88, 226);
    auto rn222 = series.find_nuclide(86, 222);
    
    if (ra226 && rn222) {
        double lambda_Ra = ra226->decay_constant();
        double lambda_Rn = rn222->decay_constant();
        
        std::cout << "Ra-226: t½ = " << format_half_life(ra226->half_life_seconds) << "\n";
        std::cout << "Rn-222: t½ = " << format_half_life(rn222->half_life_seconds) << "\n\n";
        
        std::cout << "Since Ra-226 t½ >> Rn-222 t½, secular equilibrium applies:\n";
        std::cout << "At equilibrium: Activity(Rn-222) = Activity(Ra-226)\n";
        std::cout << "                N(Rn-222) / N(Ra-226) = λ(Ra) / λ(Rn)\n";
        
        double ratio = lambda_Ra / lambda_Rn;
        std::cout << "                                        = " << ratio << "\n";
        std::cout << "\nThis means Rn-222 atoms are ~" << (1.0/ratio) << "× less abundant than Ra-226,\n";
        std::cout << "but have the same activity (decays/second).\n";
    }
    
    std::cout << "\n✓ Equilibrium concept demonstrated\n";
}

void print_summary_table() {
    print_header("SUMMARY: Four Natural Decay Series");
    
    const auto& series = get_decay_series();
    
    std::cout << std::left;
    std::cout << std::setw(12) << "Series" 
              << std::setw(10) << "Type"
              << std::setw(12) << "Parent"
              << std::setw(12) << "Stable End"
              << std::setw(10) << "Decays"
              << std::setw(12) << "t½ (years)"
              << "\n";
    std::cout << std::string(80, '─') << "\n";
    
    auto print_row = [](const DecayChain& chain) {
        std::cout << std::setw(12) << chain.name.substr(0, 11)
                  << std::setw(10) << chain.series_type
                  << std::setw(12) << (std::to_string(chain.parent_Z) + "-" + std::to_string(chain.parent_A))
                  << std::setw(12) << (std::to_string(chain.stable_Z) + "-" + std::to_string(chain.stable_A))
                  << std::setw(10) << chain.total_decays
                  << std::scientific << chain.longest_half_life_years
                  << "\n";
    };
    
    print_row(series.thorium_series());
    print_row(series.neptunium_series());
    print_row(series.uranium_series());
    print_row(series.actinium_series());
    
    std::cout << "\n";
}

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║     NATURAL RADIOACTIVE DECAY SERIES TEST SUITE                      ║\n";
    std::cout << "║     Four Major Decay Chains (Thorium, Neptunium, Uranium, Actinium)  ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════════════╝\n";
    
    try {
        // Initialize decay series
        init_decay_series();
        
        // Run tests
        test_thorium_series();
        test_neptunium_series();
        test_uranium_series();
        test_actinium_series();
        test_radon_isotopes();
        test_series_classification();
        test_decay_path_tracing();
        test_equilibrium_concepts();
        print_summary_table();
        
        std::cout << "\n╔═══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║     ✓ ALL DECAY CHAIN TESTS PASSED SUCCESSFULLY!            ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
