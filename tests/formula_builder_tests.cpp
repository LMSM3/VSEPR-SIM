/**
 * formula_builder_tests.cpp
 * -------------------------
 * Unit tests for formula parsing and molecule building.
 * 
 * Tests:
 * - Formula parsing (valid/invalid)
 * - Topology generation (atoms, bonds, angles, torsions)
 * - Central atom selection policies
 * - Geometry guess styles
 * - Integration with optimizer
 */

#include "../src/build/builder_core.hpp"
#include "../src/build/formula_builder.hpp"
#include "../src/build/builder_options.hpp"
#include "pot/periodic_db.hpp"
#include "../src/sim/molecule.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <string>

using namespace vsepr;

//=============================================================================
// Test Utilities
//=============================================================================

void assert_true(bool condition, const std::string& msg) {
    if (!condition) {
        std::cerr << "FAIL: " << msg << "\n";
        std::exit(1);
    }
    std::cout << "PASS: " << msg << "\n";
}

void assert_eq(int actual, int expected, const std::string& msg) {
    if (actual != expected) {
        std::cerr << "FAIL: " << msg << " (expected " << expected << ", got " << actual << ")\n";
        std::exit(1);
    }
    std::cout << "PASS: " << msg << "\n";
}

double distance(const Molecule& mol, uint32_t i, uint32_t j) {
    double dx = mol.coords[3*i] - mol.coords[3*j];
    double dy = mol.coords[3*i+1] - mol.coords[3*j+1];
    double dz = mol.coords[3*i+2] - mol.coords[3*j+2];
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

//=============================================================================
// Formula Parsing Tests
//=============================================================================

void test_parse_simple() {
    std::cout << "\n=== test_parse_simple ===\n";
    
    PeriodicTable pt;
    pt.load_separated("data/elements.physics.json");
    
    // H2O
    auto h2o = parse_formula("H2O", pt);
    assert_eq(h2o.size(), 2, "H2O has 2 element types");
    assert_eq(h2o[1], 2, "H2O has 2 H atoms");
    assert_eq(h2o[8], 1, "H2O has 1 O atom");
    
    // CH4
    auto ch4 = parse_formula("CH4", pt);
    assert_eq(ch4.size(), 2, "CH4 has 2 element types");
    assert_eq(ch4[6], 1, "CH4 has 1 C atom");
    assert_eq(ch4[1], 4, "CH4 has 4 H atoms");
    
    // NH3
    auto nh3 = parse_formula("NH3", pt);
    assert_eq(nh3.size(), 2, "NH3 has 2 element types");
    assert_eq(nh3[7], 1, "NH3 has 1 N atom");
    assert_eq(nh3[1], 3, "NH3 has 3 H atoms");
}

void test_parse_large() {
    std::cout << "\n=== test_parse_large ===\n";
    
    PeriodicTable pt;
    pt.load_separated("data/elements.physics.json");
    
    // C10H22 (decane)
    auto decane = parse_formula("C10H22", pt);
    assert_eq(decane.size(), 2, "C10H22 has 2 element types");
    assert_eq(decane[6], 10, "C10H22 has 10 C atoms");
    assert_eq(decane[1], 22, "C10H22 has 22 H atoms");
    
    // C6H12O6 (glucose)
    auto glucose = parse_formula("C6H12O6", pt);
    assert_eq(glucose.size(), 3, "C6H12O6 has 3 element types");
    assert_eq(glucose[6], 6, "C6H12O6 has 6 C atoms");
    assert_eq(glucose[1], 12, "C6H12O6 has 12 H atoms");
    assert_eq(glucose[8], 6, "C6H12O6 has 6 O atoms");
}

void test_parse_invalid() {
    std::cout << "\n=== test_parse_invalid ===\n";
    
    PeriodicTable pt;
    pt.load_separated("data/elements.physics.json");
    
    // Unknown element
    try {
        parse_formula("Zz99", pt);
        assert_true(false, "Should throw on unknown element");
    } catch (const std::exception& e) {
        std::cout << "PASS: Unknown element throws: " << e.what() << "\n";
    }
    
    // Invalid syntax
    try {
        parse_formula("123H", pt);
        assert_true(false, "Should throw on invalid syntax");
    } catch (const std::exception& e) {
        std::cout << "PASS: Invalid syntax throws: " << e.what() << "\n";
    }
    
    // Empty formula
    try {
        parse_formula("", pt);
        assert_true(false, "Should throw on empty formula");
    } catch (const std::exception& e) {
        std::cout << "PASS: Empty formula throws: " << e.what() << "\n";
    }
}

//=============================================================================
// Topology Generation Tests
//=============================================================================

void test_build_h2o() {
    std::cout << "\n=== test_build_h2o ===\n";
    
    PeriodicTable pt;
    pt.load_separated("data/elements.physics.json");
    
    Molecule mol = build_from_formula("H2O", pt);
    
    assert_eq(mol.num_atoms(), 3, "H2O has 3 atoms");
    assert_eq(mol.bonds.size(), 2, "H2O has 2 bonds");
    
    // Check central atom is O (Z=8)
    assert_eq(mol.atoms[0].Z, 8, "Central atom is O");
    assert_eq(mol.atoms[1].Z, 1, "Ligand 1 is H");
    assert_eq(mol.atoms[2].Z, 1, "Ligand 2 is H");
    
    // Check bonds
    bool has_bond_0_1 = false;
    bool has_bond_0_2 = false;
    for (const auto& bond : mol.bonds) {
        if ((bond.i == 0 && bond.j == 1) || (bond.i == 1 && bond.j == 0)) has_bond_0_1 = true;
        if ((bond.i == 0 && bond.j == 2) || (bond.i == 2 && bond.j == 0)) has_bond_0_2 = true;
    }
    assert_true(has_bond_0_1, "Bond O-H1 exists");
    assert_true(has_bond_0_2, "Bond O-H2 exists");
}

void test_build_ch4() {
    std::cout << "\n=== test_build_ch4 ===\n";
    
    PeriodicTable pt;
    pt.load_separated("data/elements.physics.json");
    
    Molecule mol = build_from_formula("CH4", pt);
    
    assert_eq(mol.num_atoms(), 5, "CH4 has 5 atoms");
    assert_eq(mol.bonds.size(), 4, "CH4 has 4 bonds");
    
    // Check central atom is C (Z=6)
    assert_eq(mol.atoms[0].Z, 6, "Central atom is C");
    
    // Check all ligands are H
    for (uint32_t i = 1; i < mol.num_atoms(); ++i) {
        assert_eq(mol.atoms[i].Z, 1, "Ligand is H");
    }
}

void test_build_nh3() {
    std::cout << "\n=== test_build_nh3 ===\n";
    
    PeriodicTable pt;
    pt.load_separated("data/elements.physics.json");
    
    Molecule mol = build_from_formula("NH3", pt);
    
    assert_eq(mol.num_atoms(), 4, "NH3 has 4 atoms");
    assert_eq(mol.bonds.size(), 3, "NH3 has 3 bonds");
    
    // Check central atom is N (Z=7)
    assert_eq(mol.atoms[0].Z, 7, "Central atom is N");
    
    // Check lone pairs (N has 1 lone pair)
    assert_eq(mol.atoms[0].lone_pairs, 1, "N has 1 lone pair");
}

//=============================================================================
// Central Atom Policy Tests
//=============================================================================

void test_central_policy_highest_valence() {
    std::cout << "\n=== test_central_policy_highest_valence ===\n";
    
    PeriodicTable pt;
    pt.load_separated("data/elements.physics.json");
    
    MoleculeBuilderOptions opts;
    opts.central_policy = CentralAtomPolicy::HIGHEST_VALENCE;
    
    // H2O: O has higher valence than H
    Molecule h2o = build_from_formula("H2O", pt, opts);
    assert_eq(h2o.atoms[0].Z, 8, "H2O central is O");
    
    // CO2: C and O both have 4 valence, but C has lower count
    Molecule co2 = build_from_formula("CO2", pt, opts);
    assert_eq(co2.atoms[0].Z, 6, "CO2 central is C");
}

void test_central_policy_lowest_z() {
    std::cout << "\n=== test_central_policy_lowest_z ===\n";
    
    PeriodicTable pt;
    pt.load_separated("data/elements.physics.json");
    
    MoleculeBuilderOptions opts;
    opts.central_policy = CentralAtomPolicy::LOWEST_Z;
    
    // H2O: O has lowest Z (excluding H)
    Molecule h2o = build_from_formula("H2O", pt, opts);
    assert_eq(h2o.atoms[0].Z, 8, "H2O central is O (lowest non-H)");
    
    // CH4: C has lower Z than H
    Molecule ch4 = build_from_formula("CH4", pt, opts);
    assert_eq(ch4.atoms[0].Z, 6, "CH4 central is C");
}

void test_central_policy_explicit() {
    std::cout << "\n=== test_central_policy_explicit ===\n";
    
    PeriodicTable pt;
    pt.load_separated("data/elements.physics.json");
    
    MoleculeBuilderOptions opts;
    opts.central_policy = CentralAtomPolicy::EXPLICIT_Z;
    opts.central_atom_Z = 8;  // Force O as center
    
    // H2O with explicit O center
    Molecule h2o = build_from_formula("H2O", pt, opts);
    assert_eq(h2o.atoms[0].Z, 8, "H2O central is O (explicit)");
}

//=============================================================================
// Geometry Style Tests
//=============================================================================

void test_geometry_circular_2d() {
    std::cout << "\n=== test_geometry_circular_2d ===\n";
    
    PeriodicTable pt;
    pt.load_separated("data/elements.physics.json");
    
    MoleculeBuilderOptions opts;
    opts.geometry_style = GeometryGuessStyle::CIRCULAR_2D;
    
    Molecule mol = build_from_formula("CH4", pt, opts);
    
    // Check all ligands are ~same distance from center
    double d01 = distance(mol, 0, 1);
    double d02 = distance(mol, 0, 2);
    double d03 = distance(mol, 0, 3);
    double d04 = distance(mol, 0, 4);
    
    assert_true(std::abs(d01 - d02) < 0.5, "Ligands roughly equidistant (circular)");
    assert_true(std::abs(d02 - d03) < 0.5, "Ligands roughly equidistant (circular)");
    assert_true(std::abs(d03 - d04) < 0.5, "Ligands roughly equidistant (circular)");
}

void test_geometry_spherical_3d() {
    std::cout << "\n=== test_geometry_spherical_3d ===\n";
    
    PeriodicTable pt;
    pt.load_separated("data/elements.physics.json");
    
    MoleculeBuilderOptions opts;
    opts.geometry_style = GeometryGuessStyle::SPHERICAL_3D;
    
    Molecule mol = build_from_formula("CH4", pt, opts);
    
    // Check all ligands are ~same distance from center
    double d01 = distance(mol, 0, 1);
    double d02 = distance(mol, 0, 2);
    double d03 = distance(mol, 0, 3);
    double d04 = distance(mol, 0, 4);
    
    assert_true(std::abs(d01 - d02) < 0.5, "Ligands roughly equidistant (sphere)");
    assert_true(std::abs(d02 - d03) < 0.5, "Ligands roughly equidistant (sphere)");
    assert_true(std::abs(d03 - d04) < 0.5, "Ligands roughly equidistant (sphere)");
}

//=============================================================================
// Integration Tests (with optimizer)
//=============================================================================

void test_optimize_h2o() {
    std::cout << "\n=== test_optimize_h2o ===\n";
    
    MoleculeBuildSettings settings = MoleculeBuildSettings::production();
    settings.max_iterations = 500;
    settings.force_tolerance = 1e-3;
    
    Molecule mol = build_and_optimize_from_formula("H2O", settings);
    
    assert_eq(mol.num_atoms(), 3, "Optimized H2O has 3 atoms");
    assert_eq(mol.bonds.size(), 2, "Optimized H2O has 2 bonds");
    
    // After optimization, O-H bonds should be ~0.96 Å (experimental)
    double d01 = distance(mol, 0, 1);
    double d02 = distance(mol, 0, 2);
    
    std::cout << "  O-H1 distance: " << d01 << " Å\n";
    std::cout << "  O-H2 distance: " << d02 << " Å\n";
    
    // Rough sanity check (VSEPR won't match experimental exactly)
    assert_true(d01 > 0.5 && d01 < 1.5, "O-H1 bond length reasonable");
    assert_true(d02 > 0.5 && d02 < 1.5, "O-H2 bond length reasonable");
}

void test_optimize_ch4() {
    std::cout << "\n=== test_optimize_ch4 ===\n";
    
    MoleculeBuildSettings settings = MoleculeBuildSettings::production();
    settings.max_iterations = 500;
    
    Molecule mol = build_and_optimize_from_formula("CH4", settings);
    
    assert_eq(mol.num_atoms(), 5, "Optimized CH4 has 5 atoms");
    assert_eq(mol.bonds.size(), 4, "Optimized CH4 has 4 bonds");
    
    // Check tetrahedral symmetry (all C-H bonds ~same length)
    double d01 = distance(mol, 0, 1);
    double d02 = distance(mol, 0, 2);
    double d03 = distance(mol, 0, 3);
    double d04 = distance(mol, 0, 4);
    
    std::cout << "  C-H distances: " << d01 << ", " << d02 << ", " << d03 << ", " << d04 << " Å\n";
    
    assert_true(std::abs(d01 - d02) < 0.2, "CH4 tetrahedral (d01 ≈ d02)");
    assert_true(std::abs(d02 - d03) < 0.2, "CH4 tetrahedral (d02 ≈ d03)");
    assert_true(std::abs(d03 - d04) < 0.2, "CH4 tetrahedral (d03 ≈ d04)");
}

//=============================================================================
// Validation Tests
//=============================================================================

void test_validate_formula() {
    std::cout << "\n=== test_validate_formula ===\n";
    
    PeriodicTable pt;
    pt.load_separated("data/elements.physics.json");
    
    // Valid formulas
    std::string err1 = validate_formula("H2O", pt);
    assert_true(err1.empty(), "H2O is valid");
    
    std::string err2 = validate_formula("CH4", pt);
    assert_true(err2.empty(), "CH4 is valid");
    
    // Invalid formulas
    std::string err3 = validate_formula("Zz99", pt);
    assert_true(!err3.empty(), "Zz99 is invalid");
    
    std::string err4 = validate_formula("", pt);
    assert_true(!err4.empty(), "Empty formula is invalid");
}

void test_get_composition() {
    std::cout << "\n=== test_get_composition ===\n";
    
    PeriodicTable pt;
    pt.load_separated("data/elements.physics.json");
    
    auto comp = get_composition("H2O", pt);
    assert_eq(comp.size(), 2, "H2O composition has 2 elements");
    assert_eq(comp[1], 2, "H2O has 2 H");
    assert_eq(comp[8], 1, "H2O has 1 O");
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "  Formula Builder Tests\n";
    std::cout << "========================================\n";
    
    // Formula parsing
    test_parse_simple();
    test_parse_large();
    test_parse_invalid();
    
    // Topology generation
    test_build_h2o();
    test_build_ch4();
    test_build_nh3();
    
    // Central atom policies
    test_central_policy_highest_valence();
    test_central_policy_lowest_z();
    test_central_policy_explicit();
    
    // Geometry styles
    test_geometry_circular_2d();
    test_geometry_spherical_3d();
    
    // Integration with optimizer
    test_optimize_h2o();
    test_optimize_ch4();
    
    // Validation helpers
    test_validate_formula();
    test_get_composition();
    
    std::cout << "\n========================================\n";
    std::cout << "  All tests passed!\n";
    std::cout << "========================================\n";
    
    return 0;
}
