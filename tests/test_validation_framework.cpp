/**
 * test_validation_framework.cpp
 * ==============================
 * Demonstrates the comprehensive validation framework
 * Tests all debugging guidelines:
 * 1. Single-element debugging
 * 2. Multi-element debugging
 */

#include "validation/molecule_validation.hpp"
#include "core/periodic_table.hpp"
#include <iostream>
#include <iomanip>

using namespace vsepr;
using namespace vsepr::validation;

void print_report(const ValidationReport& report, const std::string& test_name) {
    std::cout << "\n╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << std::left << std::setw(61) << test_name << "║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
    
    std::cout << report.summary() << "\n\n";
    
    for (const auto& result : report.results) {
        if (!result.passed) {
            std::string level_str;
            switch (result.level) {
                case ValidationLevel::CRITICAL: level_str = "❌ CRITICAL"; break;
                case ValidationLevel::WARNING:  level_str = "⚠️  WARNING"; break;
                case ValidationLevel::INFO:     level_str = "ℹ️  INFO"; break;
            }
            
            std::cout << level_str << " [" << result.reason_code << "]\n";
            std::cout << "  " << result.message << "\n\n";
        }
    }
    
    if (report.passed()) {
        std::cout << "✅ All checks passed!\n";
    }
}

// Test 1A: Symbol case validation
void test_symbol_case() {
    std::cout << "\n=== TEST 1A: Symbol Canonicalization ===\n";
    
    ValidationReport report;
    
    // Valid symbols
    report.add(validate_symbol_case("H"));
    report.add(validate_symbol_case("He"));
    report.add(validate_symbol_case("As"));
    
    // Invalid symbols
    report.add(validate_symbol_case("AS"));   // Should fail
    report.add(validate_symbol_case("as"));   // Should fail
    report.add(validate_symbol_case(""));     // Should fail
    
    print_report(report, "Symbol Case Validation");
}

// Test 1B: Valence envelope
void test_valence_envelope() {
    std::cout << "\n=== TEST 1B: Valence Envelope ===\n";
    
    ValidationReport report;
    
    // Carbon - normal coordination
    report.add(validate_coordination(6, 4));  // CH4 - OK
    
    // Carbon - exceeded
    report.add(validate_coordination(6, 5));  // Should fail
    
    // Sulfur - hypervalent but allowed
    report.add(validate_coordination(16, 6)); // SF6 - OK with warning
    
    // Fluorine - exceeded
    report.add(validate_coordination(9, 2));  // Should fail
    
    print_report(report, "Valence Envelope Validation");
}

// Test 1C: Geometry sanity
void test_geometry_sanity() {
    std::cout << "\n=== TEST 1C: Geometry Sanity ===\n";
    
    PeriodicTable ptable;
    
    // Create molecule with atoms too close
    Molecule mol_bad;
    mol_bad.add_atom(6, 0.0, 0.0, 0.0);  // C
    mol_bad.add_atom(1, 0.3, 0.0, 0.0);  // H too close! (should be ~1.1 Å)
    
    ValidationReport report1;
    report1.add(validate_minimum_distances(mol_bad, ptable));
    print_report(report1, "Geometry: Atoms Too Close");
    
    // Create molecule with good geometry
    Molecule mol_good;
    mol_good.add_atom(6, 0.0, 0.0, 0.0);  // C
    mol_good.add_atom(1, 1.1, 0.0, 0.0);  // H at proper distance
    
    ValidationReport report2;
    report2.add(validate_minimum_distances(mol_good, ptable));
    print_report(report2, "Geometry: Proper Distances");
}

// Test 2A: Bond plausibility
void test_bond_plausibility() {
    std::cout << "\n=== TEST 2A: Bond Plausibility Matrix ===\n";
    
    ValidationReport report;
    
    // C-H single bond at 1.1 Å - typical
    Bond ch_bond{0, 1, 1};
    report.add(validate_bond_plausibility(ch_bond, 6, 1, 1.1));
    
    // C-H triple bond - implausible
    Bond ch_triple{0, 1, 3};
    report.add(validate_bond_plausibility(ch_triple, 6, 1, 1.1));
    
    // Xe-F single bond at 2.0 Å - rare but valid
    Bond xef_bond{0, 1, 1};
    report.add(validate_bond_plausibility(xef_bond, 54, 9, 2.0));
    
    // C-O double bond at 0.5 Å - distance implausible
    Bond co_bond{0, 1, 2};
    report.add(validate_bond_plausibility(co_bond, 6, 8, 0.5));
    
    print_report(report, "Bond Plausibility Validation");
}

// Test 2B: Electron accounting
void test_electron_accounting() {
    std::cout << "\n=== TEST 2B: Electron Accounting ===\n";
    
    PeriodicTable ptable;
    ValidationReport report;
    
    // Methane: CH4 = 4 + 4 = 8 electrons (even, OK)
    std::vector<Atom> ch4_atoms = {
        {6, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}
    };
    int ch4_electrons = calculate_valence_electrons(ch4_atoms, ptable, 0);
    report.add(validate_electron_parity(ch4_electrons, false));
    
    // Methyl radical: CH3 = 4 + 3 = 7 electrons (odd, radical)
    std::vector<Atom> ch3_atoms = {
        {6, 0}, {1, 0}, {1, 0}, {1, 0}
    };
    int ch3_electrons = calculate_valence_electrons(ch3_atoms, ptable, 0);
    report.add(validate_electron_parity(ch3_electrons, true));  // With radical flag
    report.add(validate_electron_parity(ch3_electrons, false)); // Without flag - should warn
    
    // Formal charge validation
    std::vector<int> charges = {+1, 0, 0, -1};  // Should sum to 0
    report.add(validate_formal_charges(charges, 0));
    
    std::vector<int> bad_charges = {+1, 0, 0, 0};  // Sums to +1 but expect 0
    report.add(validate_formal_charges(bad_charges, 0));
    
    print_report(report, "Electron Accounting Validation");
}

// Test 2C: Noble gas gating
void test_noble_gas_gating() {
    std::cout << "\n=== TEST 2C: Noble Gas Gating ===\n";
    
    ValidationReport report;
    
    // Xenon hexafluoride: XeF6 - allowed
    std::vector<uint8_t> xef6_partners = {9, 9, 9, 9, 9, 9};  // 6 fluorines
    report.add(validate_noble_gas_compound(54, xef6_partners, 0.005, 2.0));
    
    // Krypton difluoride: KrF2 - allowed
    std::vector<uint8_t> krf2_partners = {9, 9};  // 2 fluorines
    report.add(validate_noble_gas_compound(36, krf2_partners, 0.008, 3.0));
    
    // Xenon-hydrogen: XeH (fictional) - not allowed
    std::vector<uint8_t> xeh_partners = {1};  // Hydrogen
    report.add(validate_noble_gas_compound(54, xeh_partners, 0.005, 2.0));
    
    // Xenon with poor convergence
    report.add(validate_noble_gas_compound(54, xef6_partners, 0.5, 2.0));  // High force
    
    // Xenon with high strain
    report.add(validate_noble_gas_compound(54, xef6_partners, 0.005, 20.0));  // High strain
    
    print_report(report, "Noble Gas Gating Validation");
}

// Test 2D: Optimization integrity
void test_optimization_integrity() {
    std::cout << "\n=== TEST 2D: Optimization Integrity ===\n";
    
    ValidationReport report;
    
    // Good optimization
    OptimizationQuality good_opt;
    good_opt.energy_history = {-100.0, -105.0, -108.0, -109.5, -110.0};
    good_opt.final_max_force = 0.008;
    good_opt.num_steps = 50;
    good_opt.converged = true;
    report.add(validate_optimization_quality(good_opt));
    
    // Too fast convergence (suspicious)
    OptimizationQuality fast_opt;
    fast_opt.energy_history = {-100.0, -110.0};
    fast_opt.final_max_force = 0.001;
    fast_opt.num_steps = 2;
    fast_opt.converged = true;
    report.add(validate_optimization_quality(fast_opt));
    
    // Non-monotonic energy
    OptimizationQuality bad_opt;
    bad_opt.energy_history = {-100.0, -90.0, -95.0, -105.0, -98.0};
    bad_opt.final_max_force = 0.05;
    bad_opt.num_steps = 50;
    bad_opt.converged = false;
    report.add(validate_optimization_quality(bad_opt));
    
    // Too many steps (stuck)
    OptimizationQuality stuck_opt;
    stuck_opt.energy_history.resize(15000, -100.0);
    stuck_opt.final_max_force = 0.5;
    stuck_opt.num_steps = 15000;
    stuck_opt.converged = false;
    report.add(validate_optimization_quality(stuck_opt));
    
    print_report(report, "Optimization Integrity Validation");
}

// Test full molecule validation
void test_full_molecule_validation() {
    std::cout << "\n=== FULL MOLECULE VALIDATION ===\n";
    
    PeriodicTable ptable;
    
    // Build water molecule (H2O)
    Molecule h2o;
    h2o.add_atom(8, 0.0, 0.0, 0.0);      // O
    h2o.add_atom(1, 0.96, 0.0, 0.0);     // H
    h2o.add_atom(1, -0.24, 0.93, 0.0);   // H
    h2o.add_bond(0, 1, 1);
    h2o.add_bond(0, 2, 1);
    
    BuildMetadata metadata;
    metadata.random_seed = 12345;
    
    OptimizationQuality opt;
    opt.energy_history = {-50.0, -55.0, -58.0, -59.5, -60.0};
    opt.final_max_force = 0.005;
    opt.num_steps = 25;
    opt.converged = true;
    
    ValidationReport report = validate_molecule(
        h2o, ptable, metadata, &opt, 0, false
    );
    
    print_report(report, "Full Validation: Water (H₂O)");
}

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                                               ║\n";
    std::cout << "║     Molecular Validation Framework Test Suite v2.3.1         ║\n";
    std::cout << "║                                                               ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
    
    test_symbol_case();
    test_valence_envelope();
    test_geometry_sanity();
    test_bond_plausibility();
    test_electron_accounting();
    test_noble_gas_gating();
    test_optimization_integrity();
    test_full_molecule_validation();
    
    std::cout << "\n╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  All validation tests complete!                               ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n\n";
    
    return 0;
}
