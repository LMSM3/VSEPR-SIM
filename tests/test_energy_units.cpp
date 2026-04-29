/**
 * test_energy_units.cpp  —  Energy unit system verification tests
 * ================================================================
 * VSEPR-SIM 3.0.1
 *
 * Tests:
 *   T1 : Hartree→eV conversion (CODATA)
 *   T2 : Hartree→kcal/mol conversion
 *   T3 : Hartree→kJ/mol conversion
 *   T4 : Round-trip: Ha→eV→Ha identity
 *   T5 : Round-trip: Ha→kcal→Ha identity
 *   T6 : Energy::from_ev construction
 *   T7 : Energy arithmetic (+, -, *, negation)
 *   T8 : Thermal accessibility at 298 K
 *   T9 : kbt() helper at room temperature
 *   T10: Energy::format() string output
 *   T11: Energy::format_all() multi-unit string
 *   T12: convert_energy() general converter
 *   T13: ReportedQuantity from_composition (water: 2H + 1O)
 *   T14: ReportedQuantity from_z_count (Ar4 cluster)
 *   T15: ReportedQuantity format_z_count
 *   T16: ReportedQuantity format_report
 *   T17: compute_moles helper
 *   T18: atoms_to_grams helper
 *   T19: standard_atomic_masses_amu table size + spot checks
 *   T20: element_symbol table spot checks
 *   T21: Energy comparison operators
 *   T22: Multi-unit energy views on ReportedQuantity
 */

#include "core/energy_units.hpp"
#include "core/reported_quantity.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

using namespace vsepr;

// ============================================================================
// Harness
// ============================================================================

static int g_passed = 0;
static int g_failed = 0;

#define TEST(name) \
    std::cout << "  [TEST] " << #name << "... "; \
    try { test_##name(); ++g_passed; std::cout << "PASS\n"; } \
    catch (const std::exception& ex) { ++g_failed; std::cout << "FAIL: " << ex.what() << "\n"; }

#define ASSERT(expr) \
    do { if (!(expr)) throw std::runtime_error("Assertion failed: " #expr); } while(0)

#define ASSERT_NEAR(a, b, tol) \
    do { double _a = (a), _b = (b); if (std::abs(_a - _b) > (tol)) { \
        std::ostringstream _m; \
        _m << #a << " = " << _a << " not within " << (tol) << " of " << #b << " = " << _b; \
        throw std::runtime_error(_m.str()); } } while(0)

#define ASSERT_STR_CONTAINS(s, needle) \
    do { if ((s).find(needle) == std::string::npos) { \
        throw std::runtime_error(std::string("Expected \"") + (needle) + "\" in: " + (s)); } } while(0)

// ============================================================================
// T1: Hartree → eV
// ============================================================================

static void test_hartree_to_ev() {
    Energy e = Energy::from_hartree(1.0);
    ASSERT_NEAR(e.as_ev(), 27.211386245988, 1e-8);
}

// ============================================================================
// T2: Hartree → kcal/mol
// ============================================================================

static void test_hartree_to_kcalmol() {
    Energy e = Energy::from_hartree(1.0);
    ASSERT_NEAR(e.as_kcalmol(), 627.509474, 1e-4);
}

// ============================================================================
// T3: Hartree → kJ/mol
// ============================================================================

static void test_hartree_to_kjmol() {
    Energy e = Energy::from_hartree(1.0);
    ASSERT_NEAR(e.as_kjmol(), 2625.49962, 1e-3);
}

// ============================================================================
// T4: Round-trip Ha → eV → Ha
// ============================================================================

static void test_roundtrip_ev() {
    double original = 0.123456789;
    double ev_val   = from_hartree(original, EnergyUnit::EV);
    double back     = to_hartree(ev_val, EnergyUnit::EV);
    ASSERT_NEAR(back, original, 1e-14);
}

// ============================================================================
// T5: Round-trip Ha → kcal → Ha
// ============================================================================

static void test_roundtrip_kcal() {
    double original = -0.987654321;
    double kcal_val = from_hartree(original, EnergyUnit::KcalMol);
    double back     = to_hartree(kcal_val, EnergyUnit::KcalMol);
    ASSERT_NEAR(back, original, 1e-12);
}

// ============================================================================
// T6: Energy::from_ev
// ============================================================================

static void test_from_ev() {
    Energy e = Energy::from_ev(27.211386245988);
    ASSERT_NEAR(e.as_hartree(), 1.0, 1e-10);
}

// ============================================================================
// T7: Energy arithmetic
// ============================================================================

static void test_energy_arithmetic() {
    Energy a = Energy::from_hartree(1.0);
    Energy b = Energy::from_hartree(0.5);

    Energy sum  = a + b;
    Energy diff = a - b;
    Energy scaled = a * 3.0;
    Energy neg  = -a;

    ASSERT_NEAR(sum.as_hartree(),  1.5, 1e-15);
    ASSERT_NEAR(diff.as_hartree(), 0.5, 1e-15);
    ASSERT_NEAR(scaled.as_hartree(), 3.0, 1e-15);
    ASSERT_NEAR(neg.as_hartree(), -1.0, 1e-15);

    Energy c = Energy::from_hartree(0.0);
    c += a;
    c += b;
    ASSERT_NEAR(c.as_hartree(), 1.5, 1e-15);
}

// ============================================================================
// T8: Thermal accessibility
// ============================================================================

static void test_thermal_accessibility() {
    // kBT at 298 K ≈ 0.000950 Ha
    Energy small_e = Energy::from_hartree(0.0005);  // less than kBT
    Energy big_e   = Energy::from_hartree(1.0);     // way above kBT

    ASSERT(small_e.thermally_accessible(298.15, 1.0));
    ASSERT(!big_e.thermally_accessible(298.15, 1.0));

    double ratio = big_e.thermal_ratio(298.15);
    ASSERT(ratio > 1000.0);
}

// ============================================================================
// T9: kbt() helper
// ============================================================================

static void test_kbt_helper() {
    Energy kt = kbt(298.15);
    ASSERT_NEAR(kt.as_hartree(), energy_const::KB_HARTREE_PER_K * 298.15, 1e-15);
    ASSERT_NEAR(kt.as_ev(), 0.02569, 1e-3);
}

// ============================================================================
// T10: format()
// ============================================================================

static void test_format() {
    Energy e = Energy::from_hartree(1.0);
    std::string s = e.format(EnergyUnit::EV, 4);
    ASSERT_STR_CONTAINS(s, "27.21");
    ASSERT_STR_CONTAINS(s, "eV");
}

// ============================================================================
// T11: format_all()
// ============================================================================

static void test_format_all() {
    Energy e = Energy::from_hartree(0.5);
    std::string s = e.format_all(4);
    ASSERT_STR_CONTAINS(s, "Ha");
    ASSERT_STR_CONTAINS(s, "eV");
    ASSERT_STR_CONTAINS(s, "kcal/mol");
    ASSERT_STR_CONTAINS(s, "kJ/mol");
}

// ============================================================================
// T12: convert_energy() general
// ============================================================================

static void test_convert_energy() {
    double val_ev = 1.0;
    double val_kcal = convert_energy(val_ev, EnergyUnit::EV, EnergyUnit::KcalMol);
    // 1 eV = 23.0605 kcal/mol  (= 627.509 / 27.211)
    ASSERT_NEAR(val_kcal, 23.0605, 0.01);

    // Identity
    double same = convert_energy(42.0, EnergyUnit::KJMol, EnergyUnit::KJMol);
    ASSERT_NEAR(same, 42.0, 1e-15);
}

// ============================================================================
// T13: ReportedQuantity from_composition (water)
// ============================================================================

static void test_rq_water() {
    const auto& masses = standard_atomic_masses_amu();
    // Water: H, H, O
    std::vector<int> atoms = {1, 1, 8};
    Energy e = Energy::from_kcalmol(-57.8);  // approximate formation enthalpy

    ReportedQuantity rq = ReportedQuantity::from_composition(atoms, masses, e);

    ASSERT(rq.z_count.size() == 2);       // H and O
    ASSERT(rq.z_count.at(1) == 2);        // 2 H
    ASSERT(rq.z_count.at(8) == 1);        // 1 O
    ASSERT(rq.total_atoms() == 3);

    // Molar mass of water: ~18.015 g/mol
    ASSERT_NEAR(rq.molar_mass_g_per_mol, 18.015, 0.02);

    // Mass should be approximately 18.015 * AMU_TO_GRAMS * 3 atoms...
    // Actually: mass_g = total_mass_amu * AMU_TO_GRAMS
    // total_mass_amu = 1.008 + 1.008 + 15.999 = 18.015
    // mass_g ≈ 18.015 * 1.66054e-24 ≈ 2.992e-23
    ASSERT(rq.mass_g > 0.0);
    ASSERT(rq.mass_g < 1e-20);

    // Energy should be stored in Hartree
    ASSERT_NEAR(rq.energy.as_kcalmol(), -57.8, 0.01);

    ASSERT(rq.is_valid());
}

// ============================================================================
// T14: ReportedQuantity from_z_count (Ar4)
// ============================================================================

static void test_rq_ar4() {
    const auto& masses = standard_atomic_masses_amu();
    ZCountMap comp = {{18, 4}};  // 4 Argon atoms
    Energy e = Energy::from_kcalmol(-1.375);

    ReportedQuantity rq = ReportedQuantity::from_z_count(comp, masses, e);

    ASSERT(rq.z_count.at(18) == 4);
    ASSERT(rq.total_atoms() == 4);
    ASSERT(rq.element_count() == 1);

    // Molar mass: 4 * 39.948 = 159.792 g/mol
    ASSERT_NEAR(rq.molar_mass_g_per_mol, 159.792, 0.01);

    ASSERT(rq.mass_g > 0.0);
    ASSERT(rq.is_valid());
}

// ============================================================================
// T15: format_z_count
// ============================================================================

static void test_format_z_count() {
    ReportedQuantity rq;
    rq.z_count = {{6, 6}, {1, 12}, {8, 6}};

    std::string s = rq.format_z_count();
    ASSERT_STR_CONTAINS(s, "H");
    ASSERT_STR_CONTAINS(s, "C");
    ASSERT_STR_CONTAINS(s, "O");
}

// ============================================================================
// T16: format_report
// ============================================================================

static void test_format_report() {
    const auto& masses = standard_atomic_masses_amu();
    ZCountMap comp = {{6, 1}, {8, 2}};  // CO2
    Energy e = Energy::from_kcalmol(-94.05);

    ReportedQuantity rq = ReportedQuantity::from_z_count(comp, masses, e);
    rq.label = "carbon dioxide";
    rq.formula = "CO2";

    std::string report = rq.format_report();

    ASSERT_STR_CONTAINS(report, "carbon dioxide");
    ASSERT_STR_CONTAINS(report, "CO2");
    ASSERT_STR_CONTAINS(report, "Hartree");
    ASSERT_STR_CONTAINS(report, "eV");
    ASSERT_STR_CONTAINS(report, "kcal/mol");
    ASSERT_STR_CONTAINS(report, "kJ/mol");
    ASSERT_STR_CONTAINS(report, "g");
    ASSERT_STR_CONTAINS(report, "mol");
}

// ============================================================================
// T17: compute_moles helper
// ============================================================================

static void test_compute_moles() {
    double moles = compute_moles(18.015, 18.015);
    ASSERT_NEAR(moles, 1.0, 1e-10);

    double half = compute_moles(9.0075, 18.015);
    ASSERT_NEAR(half, 0.5, 1e-5);

    double zero = compute_moles(10.0, 0.0);
    ASSERT_NEAR(zero, 0.0, 1e-15);
}

// ============================================================================
// T18: atoms_to_grams helper
// ============================================================================

static void test_atoms_to_grams() {
    // 1 atom of C-12: 12.011 amu * 1.66054e-24 g/amu
    double m = atoms_to_grams(1, 12.011);
    ASSERT_NEAR(m, 12.011 * energy_const::AMU_TO_GRAMS, 1e-30);

    // 1000 atoms of C-12
    double m1k = atoms_to_grams(1000, 12.011);
    ASSERT_NEAR(m1k, 1000.0 * 12.011 * energy_const::AMU_TO_GRAMS, 1e-25);

    // Verify scale: 1 mole of C-12 should be ~12.011 g
    // atoms_to_moles(N) * molar_mass should recover grams
    std::size_t N = 1000000;
    double grams = atoms_to_grams(N, 12.011);
    double moles = atoms_to_moles(N);
    double recovered = moles * 12.011;
    ASSERT_NEAR(grams, recovered, 1e-15);
}

// ============================================================================
// T19: standard_atomic_masses table
// ============================================================================

static void test_mass_table() {
    const auto& m = standard_atomic_masses_amu();
    ASSERT(m.size() == 118);

    // Spot checks
    ASSERT_NEAR(m[0],  1.008,  0.001);    // H
    ASSERT_NEAR(m[5],  12.011, 0.001);    // C
    ASSERT_NEAR(m[7],  15.999, 0.001);    // O
    ASSERT_NEAR(m[25], 55.845, 0.001);    // Fe
    ASSERT_NEAR(m[78], 196.967, 0.01);    // Au
    ASSERT_NEAR(m[91], 238.029, 0.01);    // U
}

// ============================================================================
// T20: element_symbol table
// ============================================================================

static void test_element_symbol() {
    ASSERT(std::string(element_symbol(1))   == "H");
    ASSERT(std::string(element_symbol(6))   == "C");
    ASSERT(std::string(element_symbol(8))   == "O");
    ASSERT(std::string(element_symbol(26))  == "Fe");
    ASSERT(std::string(element_symbol(79))  == "Au");
    ASSERT(std::string(element_symbol(118)) == "Og");
    ASSERT(std::string(element_symbol(0))   == "??");
    ASSERT(std::string(element_symbol(999)) == "??");
}

// ============================================================================
// T21: Energy comparison
// ============================================================================

static void test_energy_comparison() {
    Energy a = Energy::from_hartree(-1.0);
    Energy b = Energy::from_hartree(-0.5);
    Energy c = Energy::from_hartree(0.0);

    ASSERT(a < b);
    ASSERT(b > a);
    ASSERT(a <= b);
    ASSERT(b >= a);
    ASSERT(a.is_negative());
    ASSERT(a.is_bound());
    ASSERT(c.is_zero());
    ASSERT(!c.is_negative());
}

// ============================================================================
// T22: Multi-unit views on ReportedQuantity
// ============================================================================

static void test_rq_multi_unit() {
    const auto& masses = standard_atomic_masses_amu();
    ZCountMap comp = {{1, 2}, {8, 1}};
    Energy e = Energy::from_hartree(-0.5);

    ReportedQuantity rq = ReportedQuantity::from_z_count(comp, masses, e);

    ASSERT_NEAR(rq.energy_as(EnergyUnit::Hartree), -0.5, 1e-15);
    ASSERT_NEAR(rq.energy_as(EnergyUnit::EV), -0.5 * 27.211386245988, 1e-6);
    ASSERT_NEAR(rq.energy_as(EnergyUnit::KcalMol), -0.5 * 627.509474, 1e-3);
    ASSERT_NEAR(rq.energy_as(EnergyUnit::KJMol), -0.5 * 2625.49962, 1e-2);

    std::string all = rq.format_energy_all();
    ASSERT_STR_CONTAINS(all, "Ha");
    ASSERT_STR_CONTAINS(all, "eV");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n";
    std::cout << "  \xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x97\n";
    std::cout << "  \xe2\x95\x91  Energy Units + Reported Quantity  \xe2\x80\x94  Tests  \xe2\x95\x91\n";
    std::cout << "  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d\n\n";

    TEST(hartree_to_ev)
    TEST(hartree_to_kcalmol)
    TEST(hartree_to_kjmol)
    TEST(roundtrip_ev)
    TEST(roundtrip_kcal)
    TEST(from_ev)
    TEST(energy_arithmetic)
    TEST(thermal_accessibility)
    TEST(kbt_helper)
    TEST(format)
    TEST(format_all)
    TEST(convert_energy)
    TEST(rq_water)
    TEST(rq_ar4)
    TEST(format_z_count)
    TEST(format_report)
    TEST(compute_moles)
    TEST(atoms_to_grams)
    TEST(mass_table)
    TEST(element_symbol)
    TEST(energy_comparison)
    TEST(rq_multi_unit)

    std::cout << "\n  ──────────────────────────────────────────────\n";
    std::cout << "  Results: " << g_passed << " passed, " << g_failed << " failed\n";

    if (g_failed == 0) {
        std::cout << "  Status:  ALL PASS\n\n";
        return 0;
    }
    std::cout << "  Status:  FAILURES DETECTED\n\n";
    return 1;
}
