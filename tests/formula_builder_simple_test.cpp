/**
 * formula_builder_simple_test.cpp
 * --------------------------------
 * Simple standalone test to verify formula parsing and molecule building
 * without dependencies on optimizer or energy models.
 */

#include "build/formula_builder.hpp"
#include "build/builder_options.hpp"
#include "pot/periodic_db.hpp"
#include "sim/molecule.hpp"
#include <iostream>
#include <cassert>

using namespace vsepr;

int main() {
    std::cout << "=== Formula Builder Simple Test ===\n\n";
    
    // Load periodic table (V3 separated format)
    std::cout << "Loading periodic table...\n";
    auto pt = PeriodicTable::load_separated("data/elements.physics.json");
    
    // Run self-test
    try {
        pt.self_test();
        std::cout << "  Self-test: PASS\n";
    } catch (const std::exception& e) {
        std::cerr << "  Self-test: FAIL - " << e.what() << "\n";
        return 1;
    }
    
    // Print info
    pt.print_info();
    std::cout << "\n";
    
    // Test 1: Parse H2O
    std::cout << "Test 1: Parse H2O\n";
    auto h2o_comp = parse_formula("H2O", pt);
    assert(h2o_comp.size() == 2);
    assert(h2o_comp[1] == 2);  // 2 H
    assert(h2o_comp[8] == 1);  // 1 O
    std::cout << "  PASS: H2O composition = 2H + 1O\n\n";
    
    // Test 2: Build H2O
    std::cout << "Test 2: Build H2O molecule\n";
    MoleculeBuilderOptions opts = MoleculeBuilderOptions::quality();
    Molecule h2o = build_from_formula("H2O", pt, opts);
    std::cout << "  Atoms: " << h2o.num_atoms() << "\n";
    std::cout << "  Bonds: " << h2o.bonds.size() << "\n";
    std::cout << "  Central atom Z: " << (int)h2o.atoms[0].Z << " (should be 8=O)\n";
    assert(h2o.num_atoms() == 3);
    assert(h2o.bonds.size() == 2);
    assert(h2o.atoms[0].Z == 8);  // O is central
    std::cout << "  PASS\n\n";
    
    // Test 3: Build CH4
    std::cout << "Test 3: Build CH4 molecule\n";
    Molecule ch4 = build_from_formula("CH4", pt, opts);
    std::cout << "  Atoms: " << ch4.num_atoms() << "\n";
    std::cout << "  Bonds: " << ch4.bonds.size() << "\n";
    std::cout << "  Central atom Z: " << (int)ch4.atoms[0].Z << " (should be 6=C)\n";
    assert(ch4.num_atoms() == 5);
    assert(ch4.bonds.size() == 4);
    assert(ch4.atoms[0].Z == 6);  // C is central
    std::cout << "  PASS\n\n";
    
    // Test 4: Build NH3
    std::cout << "Test 4: Build NH3 molecule\n";
    Molecule nh3 = build_from_formula("NH3", pt, opts);
    std::cout << "  Atoms: " << nh3.num_atoms() << "\n";
    std::cout << "  Bonds: " << nh3.bonds.size() << "\n";
    std::cout << "  Central atom Z: " << (int)nh3.atoms[0].Z << " (should be 7=N)\n";
    std::cout << "  Lone pairs: " << (int)nh3.atoms[0].lone_pairs << " (should be 1)\n";
    assert(nh3.num_atoms() == 4);
    assert(nh3.bonds.size() == 3);
    assert(nh3.atoms[0].Z == 7);  // N is central
    assert(nh3.atoms[0].lone_pairs == 1);
    std::cout << "  PASS\n\n";
    
    // Test 5: Different geometry styles
    std::cout << "Test 5: Geometry styles\n";
    MoleculeBuilderOptions circular_opts;
    circular_opts.geometry_style = GeometryGuessStyle::CIRCULAR_2D;
    Molecule h2o_circular = build_from_formula("H2O", pt, circular_opts);
    
    MoleculeBuilderOptions spherical_opts;
    spherical_opts.geometry_style = GeometryGuessStyle::SPHERICAL_3D;
    Molecule h2o_spherical = build_from_formula("H2O", pt, spherical_opts);
    
    std::cout << "  CIRCULAR_2D: " << h2o_circular.num_atoms() << " atoms\n";
    std::cout << "  SPHERICAL_3D: " << h2o_spherical.num_atoms() << " atoms\n";
    assert(h2o_circular.num_atoms() == 3);
    assert(h2o_spherical.num_atoms() == 3);
    std::cout << "  PASS\n\n";
    
    // Test 6: Central atom policies
    std::cout << "Test 6: Central atom policies\n";
    MoleculeBuilderOptions explicit_opts;
    explicit_opts.central_policy = CentralAtomPolicy::EXPLICIT_Z;
    explicit_opts.central_atom_Z = 8;  // Force O as center
    Molecule h2o_explicit = build_from_formula("H2O", pt, explicit_opts);
    assert(h2o_explicit.atoms[0].Z == 8);
    std::cout << "  EXPLICIT_Z=8: central is " << (int)h2o_explicit.atoms[0].Z << " (O)\n";
    std::cout << "  PASS\n\n";
    
    // Test 7: Invalid formula
    std::cout << "Test 7: Invalid formula handling\n";
    try {
        parse_formula("Xyz123", pt);
        std::cout << "  FAIL: Should have thrown on invalid element\n";
        return 1;
    } catch (const std::exception& e) {
        std::cout << "  PASS: Caught exception: " << e.what() << "\n\n";
    }
    
    std::cout << "===========================================\n";
    std::cout << "All tests PASSED!\n";
    std::cout << "===========================================\n";
    
    return 0;
}
