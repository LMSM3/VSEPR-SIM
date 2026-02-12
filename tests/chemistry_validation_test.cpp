/**
 * chemistry_validation_test.cpp
 * ==============================
 * Validates chemistry-realistic simulation improvements:
 * 
 * 1. Hybridization detection (sp3/sp2/sp)
 * 2. Geometry-aware angle parameters
 * 3. Bond order and valence checking
 * 4. Torsion deduplication
 * 5. Temperature-aware ranking
 */

#include "core/chemistry.hpp"
#include "pot/chemistry_params.hpp"
#include "core/types.hpp"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>
#include <vector>
#include <set>
#include <tuple>

using namespace vsepr;

void test_hybridization() {
    std::cout << "\n=== TEST 1: Hybridization Detection ===\n";
    
    // Methane: C with 4 single bonds → sp3
    {
        std::vector<uint8_t> orders = {1, 1, 1, 1};
        auto hyb = infer_hybridization(6, orders, 0);
        assert(hyb == Hybridization::SP3);
        double angle = ideal_angle_for_hybridization(hyb);
        double angle_deg = angle * 180.0 / M_PI;
        std::cout << "  Methane (CH4): sp3, ideal angle = " << std::fixed << std::setprecision(1) 
                  << angle_deg << "°\n";
        assert(std::abs(angle_deg - 109.5) < 0.1);
    }
    
    // Ethene: C with 1 double + 2 single → sp2
    {
        std::vector<uint8_t> orders = {2, 1, 1};
        auto hyb = infer_hybridization(6, orders, 0);
        assert(hyb == Hybridization::SP2);
        double angle = ideal_angle_for_hybridization(hyb);
        double angle_deg = angle * 180.0 / M_PI;
        std::cout << "  Ethene (C2H4): sp2, ideal angle = " << angle_deg << "°\n";
        assert(std::abs(angle_deg - 120.0) < 0.1);
    }
    
    // Acetylene: C with 1 triple + 1 single → sp
    {
        std::vector<uint8_t> orders = {3, 1};
        auto hyb = infer_hybridization(6, orders, 0);
        assert(hyb == Hybridization::SP);
        double angle = ideal_angle_for_hybridization(hyb);
        double angle_deg = angle * 180.0 / M_PI;
        std::cout << "  Acetylene (C2H2): sp, ideal angle = " << angle_deg << "°\n";
        assert(std::abs(angle_deg - 180.0) < 0.1);
    }
    
    // Water: O with 2 single + 2 lone pairs → sp3 (bent)
    {
        std::vector<uint8_t> orders = {1, 1};
        auto hyb = infer_hybridization(8, orders, 2);
        assert(hyb == Hybridization::SP3);
        std::cout << "  Water (H2O): sp3 with 2 LP (bent geometry)\n";
    }
    
    std::cout << "  ✓ All hybridization tests passed\n";
}

void test_valence() {
    std::cout << "\n=== TEST 2: Valence Checking ===\n";
    
    // Carbon: max 4
    {
        std::vector<uint8_t> valid = {1, 1, 1, 1};  // CH4
        assert(check_valence(6, valid));
        std::cout << "  C with (1,1,1,1): ✓ valid (sum=4)\n";
        
        std::vector<uint8_t> also_valid = {2, 1, 1};  // C=C-H-H
        assert(check_valence(6, also_valid));
        std::cout << "  C with (2,1,1): ✓ valid (sum=4)\n";
        
        std::vector<uint8_t> invalid = {2, 2, 1};  // exceeds 4
        assert(!check_valence(6, invalid));
        std::cout << "  C with (2,2,1): ✗ invalid (sum=5 > 4)\n";
    }
    
    // Nitrogen: max 3
    {
        std::vector<uint8_t> valid = {1, 1, 1};  // NH3
        assert(check_valence(7, valid));
        std::cout << "  N with (1,1,1): ✓ valid (sum=3)\n";
    }
    
    // Oxygen: max 2
    {
        std::vector<uint8_t> valid = {1, 1};  // H2O
        assert(check_valence(8, valid));
        std::cout << "  O with (1,1): ✓ valid (sum=2)\n";
        
        std::vector<uint8_t> also_valid = {2};  // C=O
        assert(check_valence(8, also_valid));
        std::cout << "  O with (2): ✓ valid (sum=2)\n";
    }
    
    std::cout << "  ✓ All valence tests passed\n";
}

void test_force_constants() {
    std::cout << "\n=== TEST 3: Angle Force Constants ===\n";
    
    double k_sp = angle_force_constant_from_hybridization(Hybridization::SP);
    double k_sp2 = angle_force_constant_from_hybridization(Hybridization::SP2);
    double k_sp3 = angle_force_constant_from_hybridization(Hybridization::SP3);
    
    std::cout << "  sp:  k = " << k_sp << " kcal/mol/rad² (most rigid)\n";
    std::cout << "  sp2: k = " << k_sp2 << " kcal/mol/rad²\n";
    std::cout << "  sp3: k = " << k_sp3 << " kcal/mol/rad²\n";
    
    // sp should be strongest (linear is rigid)
    assert(k_sp > k_sp2);
    assert(k_sp2 > k_sp3);
    
    // sp3 should be strong enough to kill stars (>50)
    assert(k_sp3 >= 60.0);
    
    std::cout << "  ✓ Force constant hierarchy correct\n";
    std::cout << "  ✓ sp3 strong enough to enforce tetrahedral (k=" << k_sp3 << ")\n";
}

void test_thermal_config() {
    std::cout << "\n=== TEST 4: Temperature Configuration ===\n";
    
    // T=0 (pure energy mode)
    {
        ThermalConfig pure_energy;
        assert(pure_energy.is_zero_kelvin());
        assert(std::isinf(pure_energy.beta()));
        std::cout << "  T=0 K: pure energy mode, beta=∞\n";
    }
    
    // T=300 K
    {
        ThermalConfig thermal(300.0);
        assert(!thermal.is_zero_kelvin());
        double beta = thermal.beta();
        double expected_beta = 1.0 / (ThermalConfig::kB * 300.0);
        assert(std::abs(beta - expected_beta) < 1e-6);
        std::cout << "  T=300 K: beta = " << std::fixed << std::setprecision(3) 
                  << beta << " mol/kcal\n";
    }
    
    // Ensemble free energy
    {
        ThermalConfig thermal(300.0);
        std::vector<double> energies = {0.0, 0.8, 0.8, 2.0};  // Degenerate gauche
        double F = thermal.free_energy_from_energies(energies);
        
        std::cout << "  Conformer energies: [0.0, 0.8, 0.8, 2.0] kcal/mol\n";
        std::cout << "  Free energy F(300K) = " << F << " kcal/mol\n";
        
        // F should be less than E_min due to entropy
        assert(F < 0.0);
    }
    
    // T=0 free energy should be minimum
    {
        ThermalConfig pure_energy;
        std::vector<double> energies = {0.0, 0.8, 2.0};
        double F = pure_energy.free_energy_from_energies(energies);
        assert(std::abs(F - 0.0) < 1e-6);
        std::cout << "  T=0 K: F = E_min = 0.0 kcal/mol ✓\n";
    }
    
    std::cout << "  ✓ All thermal configuration tests passed\n";
}

void test_torsion_deduplication() {
    std::cout << "\n=== TEST 5: Torsion Deduplication ===\n";
    
    // Butane: C-C-C-C chain with hydrogens
    std::vector<Atom> atoms(14);  // 4 carbons + 10 hydrogens
    std::vector<Bond> bonds;
    
    // Setup carbons
    for (int i = 0; i < 4; ++i) {
        atoms[i].Z = 6;  // Carbon
        atoms[i].id = i;
    }
    // Setup hydrogens
    for (int i = 4; i < 14; ++i) {
        atoms[i].Z = 1;  // Hydrogen
        atoms[i].id = i;
    }
    
    // C-C bonds
    bonds.push_back({0, 1, 1});
    bonds.push_back({1, 2, 1});
    bonds.push_back({2, 3, 1});
    
    // C-H bonds (simplified geometry)
    bonds.push_back({0, 4, 1});
    bonds.push_back({0, 5, 1});
    bonds.push_back({0, 6, 1});
    bonds.push_back({1, 7, 1});
    bonds.push_back({1, 8, 1});
    bonds.push_back({2, 9, 1});
    bonds.push_back({2, 10, 1});
    bonds.push_back({3, 11, 1});
    bonds.push_back({3, 12, 1});
    bonds.push_back({3, 13, 1});
    
    auto torsions = generate_torsions_deduplicated(bonds, atoms.size());
    
    std::cout << "  Butane: " << atoms.size() << " atoms, " 
              << bonds.size() << " bonds\n";
    std::cout << "  Torsions found: " << torsions.size() << "\n";
    
    // Print first few
    for (size_t i = 0; i < std::min(size_t(5), torsions.size()); ++i) {
        const auto& t = torsions[i];
        std::cout << "    " << t.i << "-" << t.j << "-" << t.k << "-" << t.l << "\n";
    }
    
    // Should have no duplicates
    std::set<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> unique;
    for (const auto& t : torsions) {
        auto canonical = std::make_tuple(
            std::min(t.i, t.l),
            std::min(t.j, t.k),
            std::max(t.j, t.k),
            std::max(t.i, t.l)
        );
        assert(unique.insert(canonical).second);  // No duplicates
    }
    
    std::cout << "  ✓ No duplicate torsions detected\n";
}

void test_torsion_parameters() {
    std::cout << "\n=== TEST 6: Chemistry-Based Torsion Parameters ===\n";
    
    // Create simple molecules for testing
    std::vector<Atom> atoms(4);
    std::vector<Bond> bonds;
    
    // sp3-sp3 (ethane-like)
    atoms[1].Z = 6; atoms[1].id = 1;
    atoms[2].Z = 6; atoms[2].id = 2;
    bonds.push_back({1, 2, 1});
    bonds.push_back({1, 0, 1});
    bonds.push_back({2, 3, 1});
    
    auto params_sp3 = get_torsion_params_chemistry(atoms[1], atoms[2], bonds, 1);
    std::cout << "  sp3-sp3: n=" << params_sp3.n << ", V=" << params_sp3.V << " kcal/mol\n";
    assert(params_sp3.n == 3);
    assert(std::abs(params_sp3.V - 1.4) < 0.1);
    
    // sp2-sp2 (biphenyl-like)
    bonds[0].order = 2;  // Make one bond double
    auto params_sp2 = get_torsion_params_chemistry(atoms[1], atoms[2], bonds, 1);
    std::cout << "  sp2-sp2: n=" << params_sp2.n << ", V=" << params_sp2.V << " kcal/mol\n";
    assert(params_sp2.n == 2);
    
    // Double bond (rigid)
    auto params_double = get_torsion_params_chemistry(atoms[1], atoms[2], bonds, 2);
    std::cout << "  C=C: n=" << params_double.n << ", V=" << params_double.V << " kcal/mol (rigid)\n";
    assert(params_double.V > 10.0);  // Very high barrier
    
    std::cout << "  ✓ Torsion parameters chemistry-aware\n";
}

int main() {
    std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Chemistry-Realistic Simulation Validation Tests       ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    
    test_hybridization();
    test_valence();
    test_force_constants();
    test_thermal_config();
    test_torsion_deduplication();
    test_torsion_parameters();
    
    std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  ✓ ALL TESTS PASSED                                     ║\n";
    std::cout << "║  Chemistry improvements validated successfully!         ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "Summary:\n";
    std::cout << "  • Hybridization detection: sp3/sp2/sp ✓\n";
    std::cout << "  • Valence checking: C≤4, N≤3, O≤2 ✓\n";
    std::cout << "  • Force constants: sp > sp2 > sp3, all strong ✓\n";
    std::cout << "  • Temperature: T=0 and T>0 modes ✓\n";
    std::cout << "  • Torsion deduplication: no duplicates ✓\n";
    std::cout << "  • Torsion parameters: chemistry-based ✓\n";
    std::cout << "\nNext steps:\n";
    std::cout << "  1. Integrate into energy model\n";
    std::cout << "  2. Test on real molecules (CH4, C2H4, C2H2)\n";
    std::cout << "  3. Validate no star topologies\n";
    std::cout << "  4. Run conformer search at T=300K\n";
    
    return 0;
}
