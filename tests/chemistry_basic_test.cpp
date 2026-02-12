/**
 * chemistry_basic_test.cpp
 * =========================
 * Basic validation of chemistry typing (no force field dependencies)
 * 
 * Tests:
 * 1. Hybridization detection
 * 2. Ideal angles
 * 3. Force constants
 * 4. Valence checking
 * 5. Temperature configuration
 */

#include "core/chemistry.hpp"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>

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

int main() {
    std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Chemistry Typing & Thermodynamics Validation          ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    
    test_hybridization();
    test_valence();
    test_force_constants();
    test_thermal_config();
    
    std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  ✓ ALL TESTS PASSED                                     ║\n";
    std::cout << "║  Chemistry typing validated successfully!               ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "Summary:\n";
    std::cout << "  • Hybridization detection: sp3/sp2/sp ✓\n";
    std::cout << "  • Valence checking: C≤4, N≤3, O≤2 ✓\n";
    std::cout << "  • Force constants: sp > sp2 > sp3, all strong ✓\n";
    std::cout << "  • Temperature: T=0 and T>0 modes ✓\n";
    std::cout << "  • Ensemble free energy: F = -kT ln(Z) ✓\n";
    std::cout << "\nChemistry improvements validated!\n";
    std::cout << "Next: Integrate into force field and test real molecules.\n";
    
    return 0;
}
