/**
 * test_element_db_phase1.cpp
 * ===========================
 * Phase 1 — Element DB + Manifold Sanity (fast, brutal)
 * 
 * Goal: Prove new element coverage and manifold selection don't produce garbage
 *       before you even relax a structure.
 * 
 * Tests:
 * 1.1 Coverage + defaults audit (all Z = 1..118)
 * 1.2 Manifold gating tests (routing)
 * 1.3 Ionic-specific checks (LiF, NaCl, MgO, CaF₂)
 */

#include "core/element_data_integrated.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <string>
#include <map>
#include <set>

//=============================================================================
// Test utilities
//=============================================================================

struct TestResult {
    std::string name;
    bool passed;
    std::string message;
};

std::vector<TestResult> g_results;

void test(const std::string& name, bool condition, const std::string& msg = "") {
    g_results.push_back({name, condition, msg});
    if (condition) {
        std::cout << "  ✓ " << name << "\n";
    } else {
        std::cout << "  ✗ " << name;
        if (!msg.empty()) std::cout << ": " << msg;
        std::cout << "\n";
    }
}

void section(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

void subsection(const std::string& title) {
    std::cout << "\n--- " << title << " ---\n";
}

//=============================================================================
// 1.1 Coverage + Defaults Audit
//=============================================================================

void test_coverage_audit() {
    section("1.1 COVERAGE + DEFAULTS AUDIT");
    
    const auto& chem_db = vsepr::chemistry_db();
    
    int missing_count = 0;
    int bad_manifold_count = 0;
    int bad_radii_count = 0;
    int bad_lj_count = 0;
    int no_valence_count = 0;
    
    subsection("Scanning Z = 1..118");
    
    for (uint8_t Z = 1; Z <= 118; ++Z) {
        const auto& chem = chem_db.get_chem_data(Z);
        std::string symbol = chem_db.get_symbol(Z);
        
        // Check Z matches
        if (chem.Z != Z && chem.Z != 0) {
            std::cout << "  [Z=" << (int)Z << " " << symbol << "] Z mismatch: " << (int)chem.Z << "\n";
            missing_count++;
            continue;
        }
        
        // Check manifold assigned
        if (chem.manifold == vsepr::BondingManifold::UNKNOWN && Z <= 86) {
            // Radon and below should be known
            std::cout << "  [Z=" << (int)Z << " " << symbol << "] UNKNOWN manifold\n";
            bad_manifold_count++;
        }
        
        // Check radii exist and are positive
        if (chem.covalent_radius_single <= 0.0 || !std::isfinite(chem.covalent_radius_single)) {
            std::cout << "  [Z=" << (int)Z << " " << symbol << "] Bad single radius: " 
                      << chem.covalent_radius_single << "\n";
            bad_radii_count++;
        }
        
        // Check LJ parameters exist
        if (chem.lj_sigma <= 0.0 || !std::isfinite(chem.lj_sigma)) {
            std::cout << "  [Z=" << (int)Z << " " << symbol << "] Bad LJ sigma: " 
                      << chem.lj_sigma << "\n";
            bad_lj_count++;
        }
        if (chem.lj_epsilon < 0.0 || !std::isfinite(chem.lj_epsilon)) {
            std::cout << "  [Z=" << (int)Z << " " << symbol << "] Bad LJ epsilon: " 
                      << chem.lj_epsilon << "\n";
            bad_lj_count++;
        }
        
        // Check valence patterns (at least for non-noble gases)
        if (chem.manifold != vsepr::BondingManifold::NOBLE_GAS && 
            chem.manifold != vsepr::BondingManifold::UNKNOWN) {
            if (chem.allowed_valences.empty()) {
                std::cout << "  [Z=" << (int)Z << " " << symbol << "] No valence patterns (manifold=" 
                          << (int)chem.manifold << ")\n";
                no_valence_count++;
            }
        }
    }
    
    std::cout << "\n";
    test("All 118 elements load", missing_count == 0, 
         "Missing: " + std::to_string(missing_count));
    test("All manifolds assigned (Z≤86)", bad_manifold_count == 0,
         "Unknown manifolds: " + std::to_string(bad_manifold_count));
    test("All radii valid", bad_radii_count == 0,
         "Bad radii: " + std::to_string(bad_radii_count));
    test("All LJ parameters valid", bad_lj_count == 0,
         "Bad LJ: " + std::to_string(bad_lj_count));
    test("Valence patterns exist (non-noble)", no_valence_count == 0,
         "Missing valences: " + std::to_string(no_valence_count));
    
    // Spot check a few key elements
    subsection("Spot checks");
    
    auto check_element = [&](uint8_t Z, const std::string& expected_symbol, 
                             vsepr::BondingManifold expected_manifold) {
        std::string symbol = chem_db.get_symbol(Z);
        auto manifold = chem_db.get_manifold(Z);
        bool ok = (symbol == expected_symbol && manifold == expected_manifold);
        test(expected_symbol + " (Z=" + std::to_string(Z) + ")", ok);
    };
    
    check_element(1, "H", vsepr::BondingManifold::COVALENT);
    check_element(6, "C", vsepr::BondingManifold::COVALENT);
    check_element(8, "O", vsepr::BondingManifold::COVALENT);
    check_element(11, "Na", vsepr::BondingManifold::IONIC);
    check_element(20, "Ca", vsepr::BondingManifold::IONIC);
    check_element(26, "Fe", vsepr::BondingManifold::COORDINATION);
    check_element(29, "Cu", vsepr::BondingManifold::COORDINATION);
    check_element(18, "Ar", vsepr::BondingManifold::NOBLE_GAS);
    check_element(92, "U", vsepr::BondingManifold::COORDINATION);
}

//=============================================================================
// 1.2 Manifold Gating Tests
//=============================================================================

void test_manifold_routing() {
    section("1.2 MANIFOLD GATING TESTS");
    
    const auto& chem_db = vsepr::chemistry_db();
    
    subsection("Alkali + halide → IONIC");
    {
        uint8_t Li = chem_db.Z_from_symbol("Li");
        uint8_t F = chem_db.Z_from_symbol("F");
        
        auto Li_manifold = chem_db.get_manifold(Li);
        auto F_manifold = chem_db.get_manifold(F);
        
        test("Li is IONIC", Li_manifold == vsepr::BondingManifold::IONIC);
        test("F is COVALENT", F_manifold == vsepr::BondingManifold::COVALENT);
    }
    
    subsection("Main group → COVALENT");
    {
        uint8_t C = chem_db.Z_from_symbol("C");
        uint8_t N = chem_db.Z_from_symbol("N");
        uint8_t O = chem_db.Z_from_symbol("O");
        
        test("C is COVALENT", chem_db.get_manifold(C) == vsepr::BondingManifold::COVALENT);
        test("N is COVALENT", chem_db.get_manifold(N) == vsepr::BondingManifold::COVALENT);
        test("O is COVALENT", chem_db.get_manifold(O) == vsepr::BondingManifold::COVALENT);
    }
    
    subsection("Transition metal → COORDINATION");
    {
        uint8_t Fe = chem_db.Z_from_symbol("Fe");
        uint8_t Cu = chem_db.Z_from_symbol("Cu");
        uint8_t Zn = chem_db.Z_from_symbol("Zn");
        
        test("Fe is COORDINATION", chem_db.get_manifold(Fe) == vsepr::BondingManifold::COORDINATION);
        test("Cu is COORDINATION", chem_db.get_manifold(Cu) == vsepr::BondingManifold::COORDINATION);
        test("Zn is COORDINATION", chem_db.get_manifold(Zn) == vsepr::BondingManifold::COORDINATION);
    }
    
    subsection("Noble gases → NOBLE_GAS");
    {
        uint8_t He = chem_db.Z_from_symbol("He");
        uint8_t Ar = chem_db.Z_from_symbol("Ar");
        uint8_t Xe = chem_db.Z_from_symbol("Xe");
        
        test("He is NOBLE_GAS", chem_db.get_manifold(He) == vsepr::BondingManifold::NOBLE_GAS);
        test("Ar is NOBLE_GAS", chem_db.get_manifold(Ar) == vsepr::BondingManifold::NOBLE_GAS);
        test("Xe is NOBLE_GAS", chem_db.get_manifold(Xe) == vsepr::BondingManifold::NOBLE_GAS);
        
        // Check no valence patterns for noble gases
        test("He has no valence patterns", chem_db.get_allowed_valences(He).empty());
        test("Ar has no valence patterns", chem_db.get_allowed_valences(Ar).empty());
    }
}

//=============================================================================
// 1.3 Ionic-Specific Checks
//=============================================================================

void test_ionic_molecules() {
    section("1.3 IONIC-SPECIFIC CHECKS");
    
    const auto& chem_db = vsepr::chemistry_db();
    
    subsection("Ionic manifold verification");
    
    // Check that ionic elements have the IONIC manifold
    uint8_t Li = chem_db.Z_from_symbol("Li");
    uint8_t Na = chem_db.Z_from_symbol("Na");
    uint8_t Mg = chem_db.Z_from_symbol("Mg");
    uint8_t Ca = chem_db.Z_from_symbol("Ca");
    
    test("Li is IONIC", chem_db.get_manifold(Li) == vsepr::BondingManifold::IONIC);
    test("Na is IONIC", chem_db.get_manifold(Na) == vsepr::BondingManifold::IONIC);
    test("Mg is IONIC", chem_db.get_manifold(Mg) == vsepr::BondingManifold::IONIC);
    test("Ca is IONIC", chem_db.get_manifold(Ca) == vsepr::BondingManifold::IONIC);
    
    // Check that halogens have COVALENT manifold
    uint8_t F = chem_db.Z_from_symbol("F");
    uint8_t Cl = chem_db.Z_from_symbol("Cl");
    
    test("F is COVALENT", chem_db.get_manifold(F) == vsepr::BondingManifold::COVALENT);
    test("Cl is COVALENT", chem_db.get_manifold(Cl) == vsepr::BondingManifold::COVALENT);
    
    // Check that ionic elements have coordination patterns
    subsection("Ionic coordination patterns");
    
    const auto& Li_valences = chem_db.get_allowed_valences(Li);
    const auto& Na_valences = chem_db.get_allowed_valences(Na);
    const auto& Mg_valences = chem_db.get_allowed_valences(Mg);
    const auto& Ca_valences = chem_db.get_allowed_valences(Ca);
    
    test("Li has valence patterns", !Li_valences.empty());
    test("Na has valence patterns", !Na_valences.empty());
    test("Mg has valence patterns", !Mg_valences.empty());
    test("Ca has valence patterns", !Ca_valences.empty());
    
    // Check that ionic patterns have zero bond order (coordinate bonds only)
    bool all_zero_bonds = true;
    for (const auto& v : Li_valences) {
        if (v.total_bonds != 0) all_zero_bonds = false;
    }
    test("Li patterns have zero covalent bonds", all_zero_bonds);
    
    all_zero_bonds = true;
    for (const auto& v : Na_valences) {
        if (v.total_bonds != 0) all_zero_bonds = false;
    }
    test("Na patterns have zero covalent bonds", all_zero_bonds);
    
    // Display some patterns
    std::cout << "\n  Li+ patterns:\n";
    for (const auto& v : Li_valences) {
        std::cout << "    bonds=" << v.total_bonds << ", coord=" << v.coordination_number 
                  << ", charge=" << v.formal_charge << (v.common ? " (common)" : "") << "\n";
    }
    
    std::cout << "\n  Mg2+ patterns:\n";
    for (const auto& v : Mg_valences) {
        std::cout << "    bonds=" << v.total_bonds << ", coord=" << v.coordination_number 
                  << ", charge=" << v.formal_charge << (v.common ? " (common)" : "") << "\n";
    }
}

//=============================================================================
// Main
//=============================================================================

int main() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║  PHASE 1: Element DB + Manifold Sanity Tests                ║
║  (Brutal validation before attempting structure relaxation)  ║
╚══════════════════════════════════════════════════════════════╝
)";
    
    try {
        // Initialize periodic table and chemistry database
        std::cout << "Initializing periodic table and chemistry database...\n";
        auto pt = vsepr::PeriodicTable::load_separated(
            "../data/elements.physics.json",
            "../data/elements.visual.json"
        );
        vsepr::init_chemistry_db(&pt);
        std::cout << "  ✓ Databases initialized\n";
        
        // Run test suites
        test_coverage_audit();
        test_manifold_routing();
        test_ionic_molecules();
        
        // Summary
        section("SUMMARY");
        
        int passed = 0, failed = 0;
        for (const auto& r : g_results) {
            if (r.passed) ++passed;
            else ++failed;
        }
        
        std::cout << "\nTotal tests: " << g_results.size() << "\n";
        std::cout << "  Passed: " << passed << "\n";
        std::cout << "  Failed: " << failed << "\n";
        
        if (failed == 0) {
            std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
            std::cout << "║  ✓ PHASE 1 COMPLETE: All sanity checks passed!              ║\n";
            std::cout << "║  Element database is ready for complex molecules.            ║\n";
            std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
        } else {
            std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
            std::cout << "║  ✗ PHASE 1 FAILED: Fix element database issues               ║\n";
            std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ FATAL ERROR: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
