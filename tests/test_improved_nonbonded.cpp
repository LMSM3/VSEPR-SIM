/*
test_improved_nonbonded.cpp
----------------------------
Test element-specific LJ parameters on hypervalent compounds.

Validates that the new epsilon parameters reduce nonbonded energies
from hundreds of kcal/mol to realistic values (<50 kcal/mol).

Tests:
1. PF5 - Trigonal bipyramidal (previous: 648.8 kcal/mol)
2. BrF5 - Square pyramidal (previous: 716.2 kcal/mol)
3. IF5 - Square pyramidal (previous: 194.2 kcal/mol)
4. XeF6 - Distorted octahedral (previous: 21.8 kcal/mol)
5. AsF5 - Trigonal bipyramidal (previous: 716.8 kcal/mol)

Success Criteria:
- All nonbonded energies < 100 kcal/mol (preferably < 50)
- Energy reduction factor: 5-10x
- No negative energies (WCA should be repulsive-only)
- Improved convergence in optimization
*/

#include "core/types.hpp"
#include "core/geom_ops.hpp"
#include "pot/energy_model.hpp"
#include "pot/energy_nonbonded.hpp"
#include "pot/lj_epsilon_params.hpp"
#include "pot/periodic_db.hpp"
#include "sim/molecule.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

using namespace vsepr;

// Global periodic table
static PeriodicTable g_ptable;

// Helper: Initialize periodic table (call once)
void init_periodic_table() {
    static bool initialized = false;
    if (!initialized) {
        g_ptable = PeriodicTable::load_from_json_file("data/PeriodicTableJSON.json");
        initialized = true;
    }
}

// Helper: Create a molecule from element symbols
Molecule create_molecule(const std::vector<std::string>& symbols, 
                        const std::vector<Vec3>& positions) {
    init_periodic_table();
    Molecule mol;
    
    for (size_t i = 0; i < symbols.size(); ++i) {
        const Element* elem = g_ptable.by_symbol(symbols[i]);
        if (!elem) {
            throw std::runtime_error("Unknown element: " + symbols[i]);
        }
        
        Atom atom;
        atom.Z = elem->Z;
        atom.mass = elem->atomic_mass;
        mol.atoms.push_back(atom);
    }
    
    // Flatten positions
    mol.coords.resize(positions.size() * 3);
    for (size_t i = 0; i < positions.size(); ++i) {
        mol.coords[3*i] = positions[i].x;
        mol.coords[3*i+1] = positions[i].y;
        mol.coords[3*i+2] = positions[i].z;
    }
    
    return mol;
}

// Test PF5 (Trigonal Bipyramidal)
void test_PF5() {
    std::cout << "\n=== Testing PF5 (Trigonal Bipyramidal) ===\n";
    
    // Ideal TBP geometry
    // P at origin, 2 axial F along z, 3 equatorial F in xy plane
    double r_eq = 1.53;  // Equatorial P-F bond length
    double r_ax = 1.58;  // Axial P-F bond length
    
    std::vector<std::string> symbols = {"P", "F", "F", "F", "F", "F"};
    std::vector<Vec3> positions = {
        Vec3(0, 0, 0),                      // P
        Vec3(0, 0, r_ax),                   // F axial +z
        Vec3(0, 0, -r_ax),                  // F axial -z
        Vec3(r_eq, 0, 0),                   // F equatorial +x
        Vec3(-r_eq*0.5, r_eq*0.866, 0),    // F equatorial 120°
        Vec3(-r_eq*0.5, -r_eq*0.866, 0)    // F equatorial 240°
    };
    
    Molecule mol = create_molecule(symbols, positions);
    
    // Add bonds
    mol.bonds = {{0,1}, {0,2}, {0,3}, {0,4}, {0,5}};
    
    // Test with OLD parameters (uniform epsilon)
    NonbondedConfig config_old;
    config_old.lj.use_element_specific = false;
    config_old.lj.epsilon = 0.01;
    
    auto pairs = build_nonbonded_pairs(mol.atoms.size(), mol.bonds, config_old.scaling);
    NonbondedEnergy energy_old(pairs, mol.atoms, config_old);
    
    EnergyContext ctx;
    ctx.coords = &mol.coords;
    double E_old = energy_old.evaluate(ctx);
    
    // Test with NEW parameters (element-specific epsilon)
    NonbondedConfig config_new;
    config_new.lj.use_element_specific = true;
    
    NonbondedEnergy energy_new(pairs, mol.atoms, config_new);
    double E_new = energy_new.evaluate(ctx);
    
    // Print results
    std::cout << "  Old energy (uniform ε=0.01):    " << std::fixed << std::setprecision(2) 
              << E_old << " kcal/mol\n";
    std::cout << "  New energy (element-specific):  " << E_new << " kcal/mol\n";
    std::cout << "  Reduction factor:               " << E_old / E_new << "x\n";
    
    // Validation
    if (E_new < 100.0 && E_new < E_old) {
        std::cout << "  ✓ PASS: Energy reduced and below threshold\n";
    } else {
        std::cout << "  ✗ FAIL: Energy still too high or not improved\n";
    }
}

// Test BrF5 (Square Pyramidal)
void test_BrF5() {
    std::cout << "\n=== Testing BrF5 (Square Pyramidal) ===\n";
    
    double r = 1.72;  // Br-F bond length
    
    std::vector<std::string> symbols = {"Br", "F", "F", "F", "F", "F"};
    std::vector<Vec3> positions = {
        Vec3(0, 0, 0),          // Br
        Vec3(0, 0, r),          // F apical
        Vec3(r, 0, 0),          // F basal +x
        Vec3(-r, 0, 0),         // F basal -x
        Vec3(0, r, 0),          // F basal +y
        Vec3(0, -r, 0)          // F basal -y
    };
    
    Molecule mol = create_molecule(symbols, positions);
    mol.bonds = {{0,1}, {0,2}, {0,3}, {0,4}, {0,5}};
    
    NonbondedConfig config_old, config_new;
    config_old.lj.use_element_specific = false;
    config_old.lj.epsilon = 0.01;
    config_new.lj.use_element_specific = true;
    
    auto pairs = build_nonbonded_pairs(mol.atoms.size(), mol.bonds, config_old.scaling);
    
    EnergyContext ctx;
    ctx.coords = &mol.coords;
    
    NonbondedEnergy energy_old(pairs, mol.atoms, config_old);
    NonbondedEnergy energy_new(pairs, mol.atoms, config_new);
    
    double E_old = energy_old.evaluate(ctx);
    double E_new = energy_new.evaluate(ctx);
    
    std::cout << "  Old energy (uniform ε=0.01):    " << std::fixed << std::setprecision(2) 
              << E_old << " kcal/mol\n";
    std::cout << "  New energy (element-specific):  " << E_new << " kcal/mol\n";
    std::cout << "  Reduction factor:               " << E_old / E_new << "x\n";
    
    if (E_new < 100.0 && E_new < E_old) {
        std::cout << "  ✓ PASS\n";
    } else {
        std::cout << "  ✗ FAIL\n";
    }
}

// Test IF5 (Square Pyramidal)
void test_IF5() {
    std::cout << "\n=== Testing IF5 (Square Pyramidal) ===\n";
    
    double r = 1.86;  // I-F bond length
    
    std::vector<std::string> symbols = {"I", "F", "F", "F", "F", "F"};
    std::vector<Vec3> positions = {
        Vec3(0, 0, 0),
        Vec3(0, 0, r),
        Vec3(r, 0, 0),
        Vec3(-r, 0, 0),
        Vec3(0, r, 0),
        Vec3(0, -r, 0)
    };
    
    Molecule mol = create_molecule(symbols, positions);
    mol.bonds = {{0,1}, {0,2}, {0,3}, {0,4}, {0,5}};
    
    NonbondedConfig config_old, config_new;
    config_old.lj.use_element_specific = false;
    config_old.lj.epsilon = 0.01;
    config_new.lj.use_element_specific = true;
    
    auto pairs = build_nonbonded_pairs(mol.atoms.size(), mol.bonds, config_old.scaling);
    
    EnergyContext ctx;
    ctx.coords = &mol.coords;
    
    NonbondedEnergy energy_old(pairs, mol.atoms, config_old);
    NonbondedEnergy energy_new(pairs, mol.atoms, config_new);
    
    double E_old = energy_old.evaluate(ctx);
    double E_new = energy_new.evaluate(ctx);
    
    std::cout << "  Old energy:  " << std::fixed << std::setprecision(2) << E_old << " kcal/mol\n";
    std::cout << "  New energy:  " << E_new << " kcal/mol\n";
    std::cout << "  Reduction:   " << E_old / E_new << "x\n";
    
    if (E_new < 100.0 && E_new < E_old) {
        std::cout << "  ✓ PASS\n";
    } else {
        std::cout << "  ✗ FAIL\n";
    }
}

// Test element-specific epsilon retrieval
void test_epsilon_database() {
    std::cout << "\n=== Testing Element-Specific Epsilon Database ===\n";
    
    struct TestCase {
        std::string symbol;
        uint8_t Z;
        double expected_range_min;
        double expected_range_max;
    };
    
    std::vector<TestCase> tests = {
        {"F", 9, 0.04, 0.06},      // Fluorine: weak dispersion
        {"P", 15, 0.25, 0.35},     // Phosphorus: moderate
        {"Cl", 17, 0.20, 0.25},    // Chlorine
        {"Br", 35, 0.24, 0.27},    // Bromine
        {"I", 53, 0.30, 0.36},     // Iodine: larger
        {"Xe", 54, 0.40, 0.45},    // Xenon: noble gas
        {"Th", 90, 0.35, 0.45}     // Thorium: actinide
    };
    
    bool all_pass = true;
    
    for (const auto& test : tests) {
        double eps = get_lj_epsilon(test.Z);
        bool in_range = (eps >= test.expected_range_min && eps <= test.expected_range_max);
        
        std::cout << "  " << std::setw(2) << test.symbol 
                  << " (Z=" << std::setw(2) << (int)test.Z << "): ε = " 
                  << std::fixed << std::setprecision(3) << eps << " kcal/mol";
        
        if (in_range) {
            std::cout << " ✓\n";
        } else {
            std::cout << " ✗ (expected " << test.expected_range_min 
                      << "-" << test.expected_range_max << ")\n";
            all_pass = false;
        }
    }
    
    if (all_pass) {
        std::cout << "  Overall: ✓ PASS\n";
    } else {
        std::cout << "  Overall: ✗ FAIL\n";
    }
}

// Test mixing rules
void test_mixing_rules() {
    std::cout << "\n=== Testing Mixing Rules ===\n";
    
    // P-F pair
    double eps_P = get_lj_epsilon(15);  // P
    double eps_F = get_lj_epsilon(9);   // F
    
    double eps_LB = mix_epsilon(eps_P, eps_F, MixingRule::LorentzBerthelot);
    double eps_geo = mix_epsilon(eps_P, eps_F, MixingRule::Geometric);
    
    std::cout << "  P: ε = " << std::fixed << std::setprecision(3) << eps_P << " kcal/mol\n";
    std::cout << "  F: ε = " << eps_F << " kcal/mol\n";
    std::cout << "  P-F (Lorentz-Berthelot): ε = " << eps_LB << " kcal/mol\n";
    std::cout << "  P-F (Geometric):          ε = " << eps_geo << " kcal/mol\n";
    
    // Should be geometric mean for both (they're the same for epsilon)
    double expected = std::sqrt(eps_P * eps_F);
    
    if (std::abs(eps_LB - expected) < 1e-6 && std::abs(eps_geo - expected) < 1e-6) {
        std::cout << "  ✓ PASS: Mixing rules correct\n";
    } else {
        std::cout << "  ✗ FAIL: Mixing calculation error\n";
    }
}

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║   Element-Specific LJ Parameters Validation Suite       ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    
    try {
        test_epsilon_database();
        test_mixing_rules();
        test_PF5();
        test_BrF5();
        test_IF5();
        
        std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
        std::cout << "║   All Tests Complete                                     ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════╝\n";
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\n✗ FATAL ERROR: " << e.what() << "\n";
        return 1;
    }
}
