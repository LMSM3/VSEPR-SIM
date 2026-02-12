/**
 * test_periodic_table_102.cpp
 * ============================
 * Test complete periodic table with isotope support for Z=1-102
 */

#include "core/periodic_table_complete.hpp"
#include <iostream>
#include <iomanip>
#include <cassert>

using namespace vsepr::periodic;

void test_element_access() {
    std::cout << "\n=== Testing Element Access ===" << std::endl;
    
    const auto& table = get_periodic_table();
    
    // Test by atomic number
    const auto& H = table[1];
    assert(H.symbol == "H");
    assert(H.name == "Hydrogen");
    assert(H.atomic_number == 1);
    std::cout << "✓ Hydrogen (Z=1): " << H.name << ", " << H.standard_atomic_weight << " amu" << std::endl;
    
    const auto& C = table[6];
    assert(C.symbol == "C");
    assert(C.name == "Carbon");
    std::cout << "✓ Carbon (Z=6): " << C.name << ", " << C.standard_atomic_weight << " amu" << std::endl;
    
    const auto& Fe = table[26];
    assert(Fe.symbol == "Fe");
    assert(Fe.name == "Iron");
    std::cout << "✓ Iron (Z=26): " << Fe.name << ", " << Fe.standard_atomic_weight << " amu" << std::endl;
    
    const auto& U = table[92];
    assert(U.symbol == "U");
    assert(U.name == "Uranium");
    std::cout << "✓ Uranium (Z=92): " << U.name << ", " << U.standard_atomic_weight << " amu" << std::endl;
    
    const auto& No = table[102];
    assert(No.symbol == "No");
    assert(No.name == "Nobelium");
    std::cout << "✓ Nobelium (Z=102): " << No.name << ", " << No.standard_atomic_weight << " amu" << std::endl;
    
    // Test by symbol
    uint8_t Z_Au = table.get_atomic_number("Au");
    assert(Z_Au == 79);
    std::cout << "✓ Gold symbol lookup: Z=" << static_cast<int>(Z_Au) << std::endl;
}

void test_isotopes() {
    std::cout << "\n=== Testing Isotope Data ===" << std::endl;
    
    const auto& table = get_periodic_table();
    
    // Hydrogen isotopes
    const auto& H_isotopes = table.get_isotopes(1);
    std::cout << "Hydrogen isotopes: " << H_isotopes.size() << std::endl;
    for (const auto& iso : H_isotopes) {
        std::cout << "  H-" << iso.mass_number << ": " 
                  << iso.atomic_mass << " amu, "
                  << iso.abundance << "% abundance, "
                  << (iso.is_stable ? "stable" : "radioactive") << std::endl;
    }
    
    // Carbon isotopes
    const auto& C_isotopes = table.get_isotopes(6);
    std::cout << "Carbon isotopes: " << C_isotopes.size() << std::endl;
    for (const auto& iso : C_isotopes) {
        std::cout << "  C-" << iso.mass_number << ": " 
                  << iso.atomic_mass << " amu, "
                  << iso.abundance << "% abundance, "
                  << (iso.is_stable ? "stable" : "radioactive");
        if (!iso.is_stable) {
            std::cout << " (t½=" << iso.half_life_years << " years)";
        }
        std::cout << std::endl;
    }
    
    // Most common isotope masses
    double H_mass = table.get_most_common_isotope_mass(1);
    double C_mass = table.get_most_common_isotope_mass(6);
    std::cout << "Most common isotopes:" << std::endl;
    std::cout << "  H-1: " << H_mass << " amu" << std::endl;
    std::cout << "  C-12: " << C_mass << " amu" << std::endl;
}

void test_colors() {
    std::cout << "\n=== Testing Color Data ===" << std::endl;
    
    const auto& table = get_periodic_table();
    
    // Test CPK colors
    float r, g, b;
    table.get_cpk_color(1, r, g, b);  // Hydrogen - white
    std::cout << "Hydrogen CPK: RGB(" << r << ", " << g << ", " << b << ") = " 
              << table.get_cpk_hex(1) << std::endl;
    
    table.get_cpk_color(6, r, g, b);  // Carbon - gray
    std::cout << "Carbon CPK: RGB(" << r << ", " << g << ", " << b << ") = " 
              << table.get_cpk_hex(6) << std::endl;
    
    table.get_cpk_color(8, r, g, b);  // Oxygen - red
    std::cout << "Oxygen CPK: RGB(" << r << ", " << g << ", " << b << ") = " 
              << table.get_cpk_hex(8) << std::endl;
    
    table.get_cpk_color(79, r, g, b);  // Gold
    std::cout << "Gold CPK: RGB(" << r << ", " << g << ", " << b << ") = " 
              << table.get_cpk_hex(79) << std::endl;
}

void test_radii() {
    std::cout << "\n=== Testing Atomic Radii ===" << std::endl;
    
    const auto& table = get_periodic_table();
    
    // Covalent radii with bond orders
    double C_single = table.get_covalent_radius(6, 1);
    double C_double = table.get_covalent_radius(6, 2);
    double C_triple = table.get_covalent_radius(6, 3);
    
    std::cout << "Carbon covalent radii:" << std::endl;
    std::cout << "  Single bond: " << C_single << " Å" << std::endl;
    std::cout << "  Double bond: " << C_double << " Å" << std::endl;
    std::cout << "  Triple bond: " << C_triple << " Å" << std::endl;
    
    // Van der Waals radii
    double H_vdw = table.get_vdw_radius(1);
    double C_vdw = table.get_vdw_radius(6);
    double N_vdw = table.get_vdw_radius(7);
    double O_vdw = table.get_vdw_radius(8);
    
    std::cout << "Van der Waals radii:" << std::endl;
    std::cout << "  H: " << H_vdw << " Å" << std::endl;
    std::cout << "  C: " << C_vdw << " Å" << std::endl;
    std::cout << "  N: " << N_vdw << " Å" << std::endl;
    std::cout << "  O: " << O_vdw << " Å" << std::endl;
}

void test_properties() {
    std::cout << "\n=== Testing Chemical Properties ===" << std::endl;
    
    const auto& table = get_periodic_table();
    
    // Electronegativity
    std::cout << "Electronegativity (Pauling scale):" << std::endl;
    std::cout << "  H:  " << table.get_electronegativity(1) << std::endl;
    std::cout << "  C:  " << table.get_electronegativity(6) << std::endl;
    std::cout << "  N:  " << table.get_electronegativity(7) << std::endl;
    std::cout << "  O:  " << table.get_electronegativity(8) << std::endl;
    std::cout << "  F:  " << table.get_electronegativity(9) << " (most electronegative)" << std::endl;
    
    // Oxidation states
    const auto& C_ox = table.get_oxidation_states(6);
    std::cout << "\nCarbon oxidation states: ";
    for (int ox : C_ox) {
        std::cout << (ox >= 0 ? "+" : "") << ox << " ";
    }
    std::cout << std::endl;
}

void test_categories() {
    std::cout << "\n=== Testing Element Categories ===" << std::endl;
    
    const auto& table = get_periodic_table();
    
    std::cout << "Element categories:" << std::endl;
    std::cout << "  Na (11): " << table.get_category(11) << std::endl;
    std::cout << "  Ca (20): " << table.get_category(20) << std::endl;
    std::cout << "  Fe (26): " << table.get_category(26) << std::endl;
    std::cout << "  C  (6):  " << table.get_category(6) << std::endl;
    std::cout << "  Cl (17): " << table.get_category(17) << std::endl;
    std::cout << "  Ne (10): " << table.get_category(10) << std::endl;
    std::cout << "  La (57): " << table.get_category(57) << std::endl;
    std::cout << "  U  (92): " << table.get_category(92) << std::endl;
    
    // Category checks
    assert(table.is_metal(11) == true);   // Na
    assert(table.is_nonmetal(8) == true); // O
    assert(table.is_transition_metal(26) == true);  // Fe
    assert(table.is_lanthanide(57) == true);        // La
    assert(table.is_actinide(92) == true);          // U
    assert(table.is_halogen(17) == true);           // Cl
    assert(table.is_noble_gas(10) == true);         // Ne
    
    std::cout << "✓ All category checks passed" << std::endl;
}

void test_coverage() {
    std::cout << "\n=== Testing Complete Coverage (Z=1-102) ===" << std::endl;
    
    const auto& table = get_periodic_table();
    
    int valid_count = 0;
    int missing_count = 0;
    
    for (uint8_t Z = 1; Z <= 102; ++Z) {
        const auto& elem = table[Z];
        if (elem.atomic_number == Z && !elem.symbol.empty()) {
            valid_count++;
        } else {
            std::cout << "✗ Missing data for Z=" << static_cast<int>(Z) << std::endl;
            missing_count++;
        }
    }
    
    std::cout << "Valid elements: " << valid_count << "/102" << std::endl;
    std::cout << "Missing elements: " << missing_count << std::endl;
    
    assert(valid_count == 102);
    std::cout << "✓ All 102 elements present!" << std::endl;
}

void print_periodic_table() {
    std::cout << "\n=== Periodic Table Summary ===" << std::endl;
    std::cout << std::left;
    
    const auto& table = get_periodic_table();
    
    std::cout << "\nPeriod 1:" << std::endl;
    for (uint8_t Z = 1; Z <= 2; ++Z) {
        const auto& e = table[Z];
        std::cout << "  " << std::setw(3) << Z << " " << std::setw(2) << e.symbol 
                  << " " << std::setw(15) << e.name 
                  << " " << std::setw(10) << e.category << std::endl;
    }
    
    std::cout << "\nPeriod 2:" << std::endl;
    for (uint8_t Z = 3; Z <= 10; ++Z) {
        const auto& e = table[Z];
        std::cout << "  " << std::setw(3) << Z << " " << std::setw(2) << e.symbol 
                  << " " << std::setw(15) << e.name 
                  << " " << std::setw(10) << e.category << std::endl;
    }
    
    std::cout << "\nSelected actinides (Z=90-102):" << std::endl;
    for (uint8_t Z = 90; Z <= 102; ++Z) {
        const auto& e = table[Z];
        std::cout << "  " << std::setw(3) << static_cast<int>(Z) << " " << std::setw(2) << e.symbol 
                  << " " << std::setw(15) << e.name 
                  << " " << std::setw(10) << e.category << std::endl;
    }
}

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  VSEPR-Sim Periodic Table Test (Z=1-102 with Isotopes)  ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝" << std::endl;
    
    try {
        // Initialize periodic table
        init_periodic_table();
        
        // Run tests
        test_element_access();
        test_isotopes();
        test_colors();
        test_radii();
        test_properties();
        test_categories();
        test_coverage();
        print_periodic_table();
        
        std::cout << "\n╔═══════════════════════════════════════╗" << std::endl;
        std::cout << "║  ✓ ALL TESTS PASSED SUCCESSFULLY!   ║" << std::endl;
        std::cout << "╚═══════════════════════════════════════╝" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}
