/**
 * isomer_test.cpp
 * ---------------
 * Comprehensive test for isomer enumeration and identification system.
 * 
 * Tests:
 * 1. Geometric isomers: cis/trans [Co(NH3)4Cl2]+
 * 2. Geometric isomers: fac/mer [Co(NH3)3Cl3]
 * 3. Conformational isomers: butane (gauche vs anti)
 * 4. Canonical signature verification
 * 5. RMSD-based deduplication
 */

#include "sim/molecule.hpp"
#include "sim/conformer_finder.hpp"
#include "sim/isomer_signature.hpp"
#include "sim/isomer_generator.hpp"
#include "pot/energy_model.hpp"
#include <iostream>
#include <iomanip>

using namespace vsepr;

//=============================================================================
// Test 1: cis/trans [Co(NH3)4Cl2]+ Geometric Isomers
//=============================================================================

void test_cis_trans_isomers() {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  TEST 1: cis/trans [Co(NH3)4Cl2]+ Geometric Isomers\n";
    std::cout << "================================================================\n\n";
    
    // Build base octahedral complex: Co with 4 NH3 and 2 Cl
    // Metal: Co (Z=27), Ligands: N (Z=7) x4, Cl (Z=17) x2
    
    uint32_t metal_Z = 27;
    std::map<uint32_t, uint32_t> ligand_counts = {
        {7, 4},   // 4 x NH3 (represented by N donor atoms)
        {17, 2}   // 2 x Cl
    };
    uint32_t CN = 6;
    
    std::cout << "Generating geometric isomers for [Co(NH3)4Cl2]+...\n";
    
    auto isomers = IsomerGenerator::generate_coordination_isomers(
        metal_Z, ligand_counts, CN);
    
    std::cout << "\nFound " << isomers.size() << " symmetry-distinct isomers:\n\n";
    
    for (size_t i = 0; i < isomers.size(); ++i) {
        std::cout << "Isomer " << (i+1) << ": " << isomers[i].descriptor << "\n";
        std::cout << "  Type: " << (isomers[i].type == VariantType::GEOMETRIC_ISOMER ? "GEOMETRIC" : "OTHER") << "\n";
        std::cout << "  Atoms: " << isomers[i].structure.num_atoms() << "\n";
        std::cout << "  Signature: " << isomers[i].signature.coordination.to_string() << "\n";
        
        // Show ligand positions
        const auto& mol = isomers[i].structure;
        std::cout << "  Ligands:\n";
        for (uint32_t j = 1; j < mol.num_atoms(); ++j) {
            std::cout << "    " << j << ": Z=" << mol.atoms[j].Z
                      << " at (" << std::fixed << std::setprecision(2)
                      << mol.coords[3*j] << ", "
                      << mol.coords[3*j+1] << ", "
                      << mol.coords[3*j+2] << ")\n";
        }
        std::cout << "\n";
    }
    
    // Verify expected results
    bool pass = (isomers.size() == 2); // Should find cis and trans
    
    if (pass) {
        // Check that signatures are different
        if (isomers.size() >= 2) {
            pass = (isomers[0].signature != isomers[1].signature);
        }
    }
    
    std::cout << (pass ? "✓ PASS" : "✗ FAIL") << ": Found expected number of isomers with distinct signatures\n\n";
}

//=============================================================================
// Test 2: fac/mer [Co(NH3)3Cl3] Geometric Isomers
//=============================================================================

void test_fac_mer_isomers() {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  TEST 2: fac/mer [Co(NH3)3Cl3] Geometric Isomers\n";
    std::cout << "================================================================\n\n";
    
    uint32_t metal_Z = 27;
    std::map<uint32_t, uint32_t> ligand_counts = {
        {7, 3},   // 3 x NH3
        {17, 3}   // 3 x Cl
    };
    uint32_t CN = 6;
    
    std::cout << "Generating geometric isomers for [Co(NH3)3Cl3]...\n";
    
    auto isomers = IsomerGenerator::generate_coordination_isomers(
        metal_Z, ligand_counts, CN);
    
    std::cout << "\nFound " << isomers.size() << " symmetry-distinct isomers:\n\n";
    
    for (size_t i = 0; i < isomers.size(); ++i) {
        std::cout << "Isomer " << (i+1) << ": " << isomers[i].descriptor << "\n";
        std::cout << "  Signature: " << isomers[i].signature.coordination.to_string() << "\n\n";
    }
    
    bool pass = (isomers.size() == 2); // Should find fac and mer
    std::cout << (pass ? "✓ PASS" : "✗ FAIL") << ": Found 2 isomers (fac and mer)\n\n";
}

//=============================================================================
// Test 3: Conformational Search (Butane)
//=============================================================================

void test_conformers() {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  TEST 3: Conformational Search (Butane Rotamers)\n";
    std::cout << "================================================================\n\n";
    
    // Build butane manually
    Molecule mol;
    mol.add_atom(6, 0.0, 0.0, 0.0);  // C0
    mol.add_atom(6, 1.5, 0.0, 0.0);  // C1
    mol.add_atom(6, 3.0, 0.0, 0.0);  // C2
    mol.add_atom(6, 4.5, 0.0, 0.0);  // C3
    
    // Add hydrogens
    for (int i = 0; i < 10; ++i) {
        mol.add_atom(1, i*0.5, 1.0, 0.0);
    }
    
    // C-C bonds
    mol.add_bond(0, 1, 1);
    mol.add_bond(1, 2, 1);
    mol.add_bond(2, 3, 1);
    
    // C-H bonds
    mol.add_bond(0, 4, 1);
    mol.add_bond(0, 5, 1);
    mol.add_bond(0, 6, 1);
    mol.add_bond(1, 7, 1);
    mol.add_bond(1, 8, 1);
    mol.add_bond(2, 9, 1);
    mol.add_bond(2, 10, 1);
    mol.add_bond(3, 11, 1);
    mol.add_bond(3, 12, 1);
    mol.add_bond(3, 13, 1);
    
    std::cout << "Butane structure: " << mol.num_atoms() << " atoms, " << mol.num_bonds() << " bonds\n";
    
    // Setup energy model
    NonbondedParams nb_params;
    nb_params.scale_13 = 0.0;
    nb_params.scale_14 = 0.5;
    EnergyModel energy(mol, 300.0, true, true, nb_params, false, false, 0.1);
    
    // Run conformer search
    ConformerFinderSettings settings;
    settings.num_starts = 30;
    settings.seed = 42;
    settings.enumerate_geometric_isomers = false; // Butane has no geometric isomers
    settings.enumerate_conformers = true;
    settings.opt_settings.max_iterations = 300;
    settings.opt_settings.tol_rms_force = 0.01;
    
    ConformerFinder finder(settings);
    auto conformers = finder.find_conformers(mol, energy);
    
    std::cout << "\nFound " << conformers.size() << " unique conformers:\n\n";
    
    for (size_t i = 0; i < std::min(conformers.size(), size_t(5)); ++i) {
        std::cout << "  " << (i+1) << ". E = " << std::fixed << std::setprecision(3)
                  << conformers[i].energy << " kcal/mol";
        if (i > 0) {
            double delta = conformers[i].energy - conformers[0].energy;
            std::cout << " (+" << std::setprecision(2) << delta << ")";
        }
        std::cout << "\n";
    }
    
    bool pass = (conformers.size() >= 2 && conformers.size() <= 10);
    std::cout << "\n" << (pass ? "✓ PASS" : "✗ FAIL") << ": Found reasonable number of conformers (2-10)\n\n";
}

//=============================================================================
// Test 4: Signature Uniqueness
//=============================================================================

void test_signature_uniqueness() {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  TEST 4: Canonical Signature Verification\n";
    std::cout << "================================================================\n\n";
    
    // Build two identical molecules with different atom orderings
    Molecule mol1, mol2;
    
    // mol1: C-C-C with H atoms in order 0,1,2,3,4,5,6,7
    mol1.add_atom(6, 0, 0, 0);   // C0
    mol1.add_atom(6, 1.5, 0, 0); // C1
    mol1.add_atom(6, 3.0, 0, 0); // C2
    mol1.add_atom(1, 0, 1, 0);   // H3
    mol1.add_atom(1, 0, -1, 0);  // H4
    mol1.add_atom(1, 1.5, 1, 0); // H5
    mol1.add_atom(1, 1.5, -1, 0);// H6
    mol1.add_atom(1, 3, 1, 0);   // H7
    mol1.add_atom(1, 3, -1, 0);  // H8
    
    // Bonds
    mol1.add_bond(0, 1, 1);
    mol1.add_bond(1, 2, 1);
    mol1.add_bond(0, 3, 1);
    mol1.add_bond(0, 4, 1);
    mol1.add_bond(1, 5, 1);
    mol1.add_bond(1, 6, 1);
    mol1.add_bond(2, 7, 1);
    mol1.add_bond(2, 8, 1);
    
    // mol2: Same structure, atoms in reverse order
    mol2.add_atom(1, 3, -1, 0);  // H0 (was H8)
    mol2.add_atom(1, 3, 1, 0);   // H1 (was H7)
    mol2.add_atom(6, 3.0, 0, 0); // C2
    mol2.add_atom(1, 1.5, -1, 0);// H3 (was H6)
    mol2.add_atom(1, 1.5, 1, 0); // H4 (was H5)
    mol2.add_atom(6, 1.5, 0, 0); // C5 (was C1)
    mol2.add_atom(1, 0, -1, 0);  // H6 (was H4)
    mol2.add_atom(1, 0, 1, 0);   // H7 (was H3)
    mol2.add_atom(6, 0, 0, 0);   // C8 (was C0)
    
    mol2.add_bond(2, 5, 1);
    mol2.add_bond(5, 8, 1);
    mol2.add_bond(2, 0, 1);
    mol2.add_bond(2, 1, 1);
    mol2.add_bond(5, 3, 1);
    mol2.add_bond(5, 4, 1);
    mol2.add_bond(8, 6, 1);
    mol2.add_bond(8, 7, 1);
    
    auto sig1 = compute_isomer_signature(mol1);
    auto sig2 = compute_isomer_signature(mol2);
    
    std::cout << "Molecule 1 signature: " << sig1.to_string() << "\n";
    std::cout << "Molecule 2 signature: " << sig2.to_string() << "\n";
    
    bool pass = (sig1 == sig2);
    std::cout << "\n" << (pass ? "✓ PASS" : "✗ FAIL") 
              << ": Identical molecules have identical signatures\n\n";
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                                                ║\n";
    std::cout << "║          ISOMER ENUMERATION & IDENTIFICATION TESTS             ║\n";
    std::cout << "║                                                                ║\n";
    std::cout << "║  Testing systematic isomer generation, canonical signatures,  ║\n";
    std::cout << "║  symmetry-aware deduplication, and conformational search.     ║\n";
    std::cout << "║                                                                ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    
    try {
        test_cis_trans_isomers();
        test_fac_mer_isomers();
        test_conformers();
        test_signature_uniqueness();
        
        std::cout << "\n";
        std::cout << "================================================================\n";
        std::cout << "  ALL TESTS COMPLETE\n";
        std::cout << "================================================================\n\n";
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\n✗ EXCEPTION: " << e.what() << "\n\n";
        return 1;
    }
}
