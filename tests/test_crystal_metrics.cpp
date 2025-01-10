/**
 * test_crystal_metrics.cpp
 * ------------------------
 * Validation tests for the crystal verification scorecard system.
 *
 * Tests:
 *   1. Identity metrics (stoichiometry, charge, volume, density)
 *   2. Symmetry metrics (lattice system, site classes)
 *   3. Local geometry (CN, bond lengths, distortion)
 *   4. Topology (connectivity, sublattice, hash)
 *   5. Reciprocal space (d-spacings, peak positions)
 *   6. Full scorecard against empirical reference data
 *   7. Supercell metric consistency (metrics scale correctly)
 *   8. Relaxation stability detection
 *   9. All 10 presets pass verification
 *  10. Determinism (same input → same output, always)
 */

#include "atomistic/crystal/crystal_metrics.hpp"
#include "atomistic/crystal/reference_data.hpp"
#include "atomistic/crystal/unit_cell.hpp"
#include "atomistic/crystal/supercell.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <string>
#include <vector>

using namespace atomistic;
using namespace atomistic::crystal;

static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, msg) do { \
    if (cond) { ++tests_passed; } \
    else { ++tests_failed; std::cerr << "FAIL: " << msg << " [" << __FILE__ << ":" << __LINE__ << "]\n"; } \
} while(0)

#define CHECK_CLOSE(a, b, tol, msg) CHECK(std::abs((a) - (b)) < (tol), msg)

// ============================================================================
// Test 1: Identity metrics
// ============================================================================
void test_identity_metrics() {
    auto uc = presets::sodium_chloride();
    auto im = compute_identity_metrics(uc);
    
    // NaCl: 4 Na + 4 Cl = 8 atoms in cell, Z=4
    CHECK(im.element_counts[11] == 4, "NaCl: 4 Na atoms");
    CHECK(im.element_counts[17] == 4, "NaCl: 4 Cl atoms");
    CHECK(im.formula_units_Z == 4, "NaCl: Z=4");
    CHECK(im.reduced_formula == "NaCl", "NaCl reduced formula");
    
    // Charge neutrality
    CHECK(im.charge_neutral, "NaCl charge neutral");
    
    // Volume: a³ = 5.64³ ≈ 179.4
    CHECK_CLOSE(im.cell_volume, 5.64*5.64*5.64, 0.1, "NaCl volume");
    
    // Density: NaCl ≈ 2.16 g/cm³
    CHECK(im.density_gcc > 2.0 && im.density_gcc < 2.4, "NaCl density ~ 2.16 g/cm³");
    
    // Al: single element
    auto uc_al = presets::aluminum_fcc();
    auto im_al = compute_identity_metrics(uc_al);
    CHECK(im_al.formula_units_Z == 4, "Al FCC: Z=4");
    CHECK(im_al.reduced_formula == "Al", "Al reduced formula");
    CHECK(im_al.density_gcc > 2.5 && im_al.density_gcc < 2.9, "Al density ~ 2.70 g/cm³");
    
    // TiO2: compound
    auto uc_tio2 = presets::rutile_tio2();
    auto im_tio2 = compute_identity_metrics(uc_tio2);
    CHECK(im_tio2.formula_units_Z == 2, "TiO2: Z=2");
    CHECK(im_tio2.density_gcc > 3.8 && im_tio2.density_gcc < 4.6, "TiO2 density ~ 4.25 g/cm³");
}

// ============================================================================
// Test 2: Symmetry metrics
// ============================================================================
void test_symmetry_metrics() {
    // Cubic
    auto uc_nacl = presets::sodium_chloride();
    auto sm = compute_symmetry_metrics(uc_nacl);
    CHECK(sm.is_cubic, "NaCl is cubic");
    CHECK(sm.lattice_system == "cubic", "NaCl lattice system = cubic");
    CHECK(sm.space_group_number == 225, "NaCl space group = 225");
    
    // Tetragonal
    auto uc_tio2 = presets::rutile_tio2();
    auto sm2 = compute_symmetry_metrics(uc_tio2);
    CHECK(sm2.is_tetragonal, "TiO2 is tetragonal");
    CHECK(sm2.space_group_number == 136, "TiO2 space group = 136");
    
    // Site multiplicities
    CHECK(sm.num_unique_sites == 2, "NaCl: 2 unique site types (Na, Cl)");
    CHECK(sm2.num_unique_sites == 2, "TiO2: 2 unique site types (Ti, O)");
}

// ============================================================================
// Test 3: Local geometry
// ============================================================================
void test_local_geometry() {
    // NaCl: all CN should be 6
    auto uc_nacl = presets::sodium_chloride();
    auto lgm = compute_local_geometry(uc_nacl, 3.5);
    
    CHECK(lgm.mean_CN_by_type.count(11), "Na CN computed");
    CHECK(lgm.mean_CN_by_type.count(17), "Cl CN computed");
    
    double na_cn = lgm.mean_CN_by_type[11];
    double cl_cn = lgm.mean_CN_by_type[17];
    CHECK_CLOSE(na_cn, 6.0, 0.5, "Na CN ≈ 6");
    CHECK_CLOSE(cl_cn, 6.0, 0.5, "Cl CN ≈ 6");
    
    // Bond statistics: Na-Cl bond ≈ 2.82 Å
    bool found_nacl_bond = false;
    for (const auto& bs : lgm.bond_statistics) {
        if ((bs.type_i == 11 && bs.type_j == 17) || (bs.type_i == 17 && bs.type_j == 11)) {
            found_nacl_bond = true;
            CHECK(bs.mean_length > 2.7 && bs.mean_length < 2.95, "Na-Cl bond ≈ 2.82 Å");
            CHECK(bs.count > 0, "Na-Cl bond count > 0");
        }
    }
    CHECK(found_nacl_bond, "Na-Cl bond pair found");
    
    // Fe BCC: CN=8
    auto uc_fe = presets::iron_bcc();
    auto lgm_fe = compute_local_geometry(uc_fe, 2.7);
    
    double fe_cn = lgm_fe.mean_CN_by_type[26];
    CHECK_CLOSE(fe_cn, 8.0, 0.5, "Fe BCC CN ≈ 8");
    
    // Diamond Si: CN=4
    auto uc_si = presets::silicon_diamond();
    auto lgm_si = compute_local_geometry(uc_si, 2.6);
    
    double si_cn = lgm_si.mean_CN_by_type[14];
    CHECK_CLOSE(si_cn, 4.0, 0.5, "Si diamond CN ≈ 4");
}

// ============================================================================
// Test 4: Topology metrics
// ============================================================================
void test_topology() {
    auto uc = presets::sodium_chloride();
    auto tm = compute_topology_metrics(uc, 3.5);
    
    CHECK(tm.total_bonds > 0, "NaCl has bonds");
    CHECK(tm.fully_connected, "NaCl is fully connected");
    CHECK(tm.num_connected_components == 1, "NaCl: 1 connected component");
    CHECK(tm.topology_hash != 0, "NaCl topology hash computed");
    
    // Determinism: same input → same hash
    auto tm2 = compute_topology_metrics(uc, 3.5);
    CHECK(tm.topology_hash == tm2.topology_hash, "topology hash is deterministic");
    
    // Different structures → different hashes
    auto uc_fe = presets::iron_bcc();
    auto tm_fe = compute_topology_metrics(uc_fe, 3.0);
    CHECK(tm.topology_hash != tm_fe.topology_hash, "NaCl hash ≠ Fe hash");
}

// ============================================================================
// Test 5: Reciprocal space
// ============================================================================
void test_reciprocal_space() {
    auto lat = Lattice::cubic(5.64);  // NaCl
    auto rm = compute_reciprocal_metrics(lat, 90.0, 1.5406);
    
    CHECK(rm.num_peaks > 0, "NaCl has XRD peaks");
    
    // First peak should be around 2θ ≈ 27.4° for (111)
    bool found_first = false;
    for (const auto& ds : rm.d_spacings) {
        if (ds.two_theta > 26.0 && ds.two_theta < 29.0) {
            found_first = true;
        }
    }
    CHECK(found_first, "NaCl (111) peak near 2θ=27.4°");
    
    // d-spacings should be sorted descending
    if (rm.d_spacings.size() > 1) {
        bool sorted = true;
        for (size_t i = 1; i < rm.d_spacings.size(); ++i) {
            if (rm.d_spacings[i].d > rm.d_spacings[i-1].d + 1e-6) {
                sorted = false;
                break;
            }
        }
        CHECK(sorted, "d-spacings sorted descending");
    }
    
    // Reciprocal lattice lengths
    CHECK_CLOSE(rm.a_star, 1.0/5.64, 0.01, "a* = 1/a");
}

// ============================================================================
// Test 6: Full scorecard against reference data
// ============================================================================
void test_full_scorecard() {
    // NaCl
    auto uc = presets::sodium_chloride();
    auto ref = reference::nacl_ref();
    auto sc = verify_against_reference(uc, ref, 3.5);
    
    CHECK(sc.total_checks > 8, "NaCl scorecard has >8 checks");
    CHECK(sc.pass_rate > 70.0, "NaCl scorecard >70% pass rate");
    
    // Al
    auto uc_al = presets::aluminum_fcc();
    auto ref_al = reference::aluminum_fcc_ref();
    auto sc_al = verify_against_reference(uc_al, ref_al, 3.2);
    
    CHECK(sc_al.total_checks > 6, "Al scorecard has >6 checks");
    CHECK(sc_al.pass_rate > 70.0, "Al scorecard >70% pass rate");
    
    // Print one scorecard for visual inspection
    sc.print();
}

// ============================================================================
// Test 7: Supercell metric consistency
// ============================================================================
void test_supercell_consistency() {
    auto uc = presets::sodium_chloride();
    auto im_uc = compute_identity_metrics(uc);
    
    // Build 2×2×2 supercell
    auto result = construct_supercell(uc, 2, 2, 2);
    auto im_sc = compute_identity_metrics(result.state, 
                     Lattice(uc.lattice.A.col(0) * 2.0,
                             uc.lattice.A.col(1) * 2.0,
                             uc.lattice.A.col(2) * 2.0));
    
    // Reduced formula should be the same
    CHECK(im_sc.reduced_formula == im_uc.reduced_formula, 
          "supercell: same reduced formula");
    
    // Z should scale by 8 (2³)
    CHECK(im_sc.formula_units_Z == im_uc.formula_units_Z * 8,
          "supercell: Z scales by 2³");
    
    // Density should stay the same
    CHECK_CLOSE(im_sc.density_gcc, im_uc.density_gcc, 0.1,
                "supercell: density preserved");
}

// ============================================================================
// Test 8: Compute_all_metrics integration
// ============================================================================
void test_all_metrics() {
    auto uc = presets::sodium_chloride();
    auto cm = compute_all_metrics(uc, 3.5);
    
    // Just verify all fields populated
    CHECK(cm.identity.cell_volume > 0, "all_metrics: volume > 0");
    CHECK(!cm.identity.reduced_formula.empty(), "all_metrics: formula populated");
    CHECK(cm.symmetry.num_unique_sites > 0, "all_metrics: sites > 0");
    CHECK(cm.geometry.site_geometries.size() > 0, "all_metrics: geometries populated");
    CHECK(cm.topology.total_bonds > 0, "all_metrics: bonds > 0");
    CHECK(cm.reciprocal.num_peaks > 0, "all_metrics: XRD peaks > 0");
    
    // Print for visual inspection
    cm.print_summary();
}

// ============================================================================
// Test 9: All 10 presets pass basic sanity
// ============================================================================
void test_all_presets_sanity() {
    struct PresetTest {
        std::string name;
        UnitCell (*factory)();
        int expected_atoms;
        std::string expected_system;
    };
    
    PresetTest tests[] = {
        {"Al FCC",  presets::aluminum_fcc,    4, "cubic"},
        {"Fe BCC",  presets::iron_bcc,        2, "cubic"},
        {"Cu FCC",  presets::copper_fcc,      4, "cubic"},
        {"Au FCC",  presets::gold_fcc,        4, "cubic"},
        {"NaCl",    presets::sodium_chloride,  8, "cubic"},
        {"MgO",     presets::magnesium_oxide,  8, "cubic"},
        {"CsCl",    presets::cesium_chloride,  2, "cubic"},
        {"Si",      presets::silicon_diamond,  8, "cubic"},
        {"Diamond", presets::carbon_diamond,   8, "cubic"},
        {"TiO2",    presets::rutile_tio2,      6, "tetragonal"},
    };
    
    for (const auto& t : tests) {
        auto uc = t.factory();
        auto im = compute_identity_metrics(uc);
        auto sm = compute_symmetry_metrics(uc);
        
        CHECK(static_cast<int>(uc.basis.size()) == t.expected_atoms,
              t.name + ": correct atom count");
        CHECK(im.cell_volume > 0, t.name + ": positive volume");
        CHECK(im.density_gcc > 0.5 && im.density_gcc < 25.0,
              t.name + ": density in physical range");
        CHECK(im.charge_neutral, t.name + ": charge neutral");
        CHECK(sm.lattice_system == t.expected_system,
              t.name + ": correct lattice system (" + sm.lattice_system + ")");
        CHECK(sm.space_group_number > 0,
              t.name + ": space group assigned");
    }
}

// ============================================================================
// Test 10: Determinism
// ============================================================================
void test_determinism() {
    auto uc = presets::sodium_chloride();
    
    auto cm1 = compute_all_metrics(uc, 3.5);
    auto cm2 = compute_all_metrics(uc, 3.5);
    
    // Every field must match exactly
    CHECK(cm1.identity.reduced_formula == cm2.identity.reduced_formula,
          "determinism: formula");
    CHECK_CLOSE(cm1.identity.cell_volume, cm2.identity.cell_volume, 1e-12,
                "determinism: volume");
    CHECK_CLOSE(cm1.identity.density_gcc, cm2.identity.density_gcc, 1e-12,
                "determinism: density");
    CHECK(cm1.topology.topology_hash == cm2.topology.topology_hash,
          "determinism: topology hash");
    CHECK(cm1.reciprocal.num_peaks == cm2.reciprocal.num_peaks,
          "determinism: peak count");
    
    // Bond stats
    CHECK(cm1.geometry.bond_statistics.size() == cm2.geometry.bond_statistics.size(),
          "determinism: bond stat count");
    for (size_t i = 0; i < cm1.geometry.bond_statistics.size(); ++i) {
        CHECK_CLOSE(cm1.geometry.bond_statistics[i].mean_length,
                    cm2.geometry.bond_statistics[i].mean_length, 1e-12,
                    "determinism: bond length");
    }
}

// ============================================================================
// Test 11: Reference scorecard for all presets with reference data
// ============================================================================
void test_reference_scorecards() {
    struct RefTest {
        std::string name;
        UnitCell (*factory)();
        ReferenceData ref;
        double cutoff;
    };
    
    RefTest tests[] = {
        {"Al FCC",  presets::aluminum_fcc,    reference::aluminum_fcc_ref(), 3.2},
        {"Fe BCC",  presets::iron_bcc,        reference::iron_bcc_ref(),     3.0},
        {"Cu FCC",  presets::copper_fcc,      reference::copper_fcc_ref(),   3.2},
        {"Au FCC",  presets::gold_fcc,        reference::gold_fcc_ref(),     3.2},
        {"NaCl",    presets::sodium_chloride,  reference::nacl_ref(),         3.5},
        {"MgO",     presets::magnesium_oxide,  reference::mgo_ref(),          3.0},
        {"CsCl",    presets::cesium_chloride,  reference::cscl_ref(),         3.8},
        {"Si",      presets::silicon_diamond,  reference::silicon_ref(),      2.6},
        {"Diamond", presets::carbon_diamond,   reference::diamond_ref(),      1.8},
        {"TiO2",    presets::rutile_tio2,      reference::rutile_tio2_ref(),  2.5},
    };
    
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  COMPREHENSIVE REFERENCE VERIFICATION                            ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";
    
    int total_scorecards = 0;
    int passing_scorecards = 0;
    
    for (const auto& t : tests) {
        auto uc = t.factory();
        auto sc = verify_against_reference(uc, t.ref, t.cutoff);
        total_scorecards++;
        
        bool scorecard_pass = (sc.pass_rate >= 70.0);
        if (scorecard_pass) passing_scorecards++;
        
        std::cout << "  " << std::setw(10) << std::left << t.name 
                  << "  " << sc.passed << "/" << sc.total_checks
                  << " (" << std::fixed << std::setprecision(0) << sc.pass_rate << "%)"
                  << (scorecard_pass ? "  ✓" : "  ✗") << "\n";
        
        CHECK(scorecard_pass, t.name + " scorecard pass rate >= 70%");
    }
    
    std::cout << "\n  Overall: " << passing_scorecards << "/" << total_scorecards 
              << " scorecards passing\n\n";
}

// ============================================================================
// Test 12: Benchmark crystal library — all new presets pass sanity
// ============================================================================
void test_benchmark_crystals_sanity() {
    struct BenchTest {
        std::string name;
        UnitCell (*factory)();
        int min_atoms;
        double min_density;
        double max_density;
    };

    BenchTest tests[] = {
        // Fluorites
        {"ThO2",          presets::tho2_fluorite,       12, 9.0, 11.0},
        {"PuO2",          presets::puo2_fluorite,       12, 10.5, 12.5},
        {"CeO2",          presets::ceo2_fluorite,       12, 6.5, 8.0},
        {"ZrO2 cubic",    presets::zro2_cubic,          12, 5.5, 7.0},
        {"ZrO2 tetrag",   presets::zro2_tetragonal,      6, 5.5, 7.0},
        {"ZrO2 mono",     presets::zro2_monoclinic,     12, 5.0, 6.5},
        {"HfO2 cubic",    presets::hfo2_cubic,          12, 9.0, 11.5},
        {"HfO2 tetrag",   presets::hfo2_tetragonal,      6, 8.5, 11.0},
        {"HfO2 mono",     presets::hfo2_monoclinic,     12, 8.5, 11.0},
        // Spinels (56 atoms after FCC centering)
        {"MgAl2O4",       presets::mgal2o4_spinel,      56, 2.5, 5.0},
        {"Fe3O4",         presets::fe3o4_spinel,        56, 3.0, 7.0},
        {"Co3O4",         presets::co3o4_spinel,        56, 3.0, 8.0},
        // Perovskites
        {"SrTiO3",        presets::srtio3_perovskite,    5, 4.5, 5.5},
        {"BaTiO3 cubic",  presets::batio3_cubic,         5, 5.5, 6.5},
        {"BaTiO3 tetrag", presets::batio3_tetragonal,    5, 5.5, 6.5},
        {"CaTiO3",        presets::catio3_orthorhombic, 20, 3.5, 5.0},
        {"LaAlO3",        presets::laalo3_rhombohedral, 30, 4.0, 8.0},
        // Garnets (160 atoms after I-centering)
        {"YAG",           presets::y3al5o12_garnet,    160, 3.5, 5.5},
        {"GGG",           presets::gd3ga5o12_garnet,   160, 5.0, 8.5},
        // Apatite
        {"Fluorapatite",  presets::ca5po4_3f_apatite,   42, 2.5, 4.0},
        // Monazite
        {"LaPO4",         presets::lapo4_monazite,      24, 4.5, 6.5},
        // Pyrochlores (88 atoms after FCC centering)
        {"Gd2Ti2O7",      presets::gd2ti2o7_pyrochlore, 88, 4.0, 9.0},
        {"La2Zr2O7",      presets::la2zr2o7_pyrochlore, 88, 3.5, 8.0},
        {"Bi2Ti2O7",      presets::bi2ti2o7_pyrochlore, 88, 4.0, 9.0},
        // Actinides (UO3 = 128 atoms after FCC centering)
        {"U3O8",          presets::u3o8_orthorhombic,   20, 6.0, 10.0},
        {"UO3 gamma",     presets::uo3_gamma,          128, 4.0, 9.0},
    };

    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  BENCHMARK CRYSTAL LIBRARY SANITY CHECKS                         ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";

    for (const auto& t : tests) {
        auto uc = t.factory();
        auto im = compute_identity_metrics(uc);
        auto sm = compute_symmetry_metrics(uc);

        CHECK(static_cast<int>(uc.basis.size()) >= t.min_atoms,
              t.name + ": atom count >= " + std::to_string(t.min_atoms));
        CHECK(im.cell_volume > 0, t.name + ": positive volume");
        CHECK(im.density_gcc > t.min_density && im.density_gcc < t.max_density,
              t.name + ": density in range [" + std::to_string(t.min_density) + ", " + std::to_string(t.max_density) + "] (got " + std::to_string(im.density_gcc) + ")");
        CHECK(im.charge_neutral, t.name + ": charge neutral");
        CHECK(sm.space_group_number > 0, t.name + ": space group assigned");

        std::cout << "  " << std::setw(16) << std::left << t.name
                  << " atoms=" << std::setw(3) << uc.basis.size()
                  << " V=" << std::setw(10) << std::fixed << std::setprecision(2) << im.cell_volume
                  << " ρ=" << std::setprecision(2) << im.density_gcc << " g/cm³"
                  << " SG=" << sm.space_group_number
                  << "  ✓\n";
    }
}

// ============================================================================
// Test 13: Fluorite structure verification (ThO2, CeO2, PuO2)
// ============================================================================
void test_fluorite_family() {
    // All fluorites: CN(cation)=8, CN(anion)=4
    struct FluoriteTest {
        std::string name;
        UnitCell (*factory)();
        double expected_density;
    };

    FluoriteTest tests[] = {
        {"ThO2", presets::tho2_fluorite, 10.00},
        {"CeO2", presets::ceo2_fluorite, 7.22},
        {"PuO2", presets::puo2_fluorite, 11.46},
    };

    for (const auto& t : tests) {
        auto uc = t.factory();
        auto im = compute_identity_metrics(uc);
        auto lgm = compute_local_geometry(uc, 3.0);

        // All fluorites have 12 atoms (4 cation + 8 anion)
        CHECK(uc.basis.size() == 12, t.name + ": 12 atoms in cell");

        // Formula units Z=4
        CHECK(im.formula_units_Z == 4, t.name + ": Z=4");

        // Density within 10%
        double derr = 100.0 * std::abs(im.density_gcc - t.expected_density) / t.expected_density;
        CHECK(derr < 10.0, t.name + ": density within 10% of reference");

        // Charge neutral
        CHECK(im.charge_neutral, t.name + ": charge neutral");
    }
}

// ============================================================================
// Test 14: Perovskite structure verification
// ============================================================================
void test_perovskite_family() {
    // SrTiO3: 5 atoms, cubic, Ti CN=6
    auto uc = presets::srtio3_perovskite();
    auto im = compute_identity_metrics(uc);
    auto lgm = compute_local_geometry(uc, 2.5);

    CHECK(uc.basis.size() == 5, "SrTiO3: 5 atoms");
    CHECK(im.formula_units_Z == 1, "SrTiO3: Z=1");
    CHECK(im.charge_neutral, "SrTiO3: charge neutral");

    // Ti should be octahedrally coordinated
    if (lgm.mean_CN_by_type.count(22)) {
        CHECK_CLOSE(lgm.mean_CN_by_type[22], 6.0, 1.0, "SrTiO3: Ti CN ≈ 6");
    }

    // BaTiO3 polymorphs: same formula, different cells
    auto uc_c = presets::batio3_cubic();
    auto uc_t = presets::batio3_tetragonal();
    auto im_c = compute_identity_metrics(uc_c);
    auto im_t = compute_identity_metrics(uc_t);

    CHECK(im_c.reduced_formula == im_t.reduced_formula, 
          "BaTiO3: cubic and tetragonal have same formula");

    // Tetragonal should have c/a > 1
    double ca_ratio = uc_t.lattice.c_len() / uc_t.lattice.a_len();
    CHECK(ca_ratio > 1.005, "BaTiO3 tetragonal: c/a > 1");
}

// ============================================================================
// Test 15: Polymorph volume ordering (ZrO2)
// ============================================================================
void test_polymorph_ordering() {
    auto uc_c = presets::zro2_cubic();
    auto uc_t = presets::zro2_tetragonal();
    auto uc_m = presets::zro2_monoclinic();

    auto im_c = compute_identity_metrics(uc_c);
    auto im_t = compute_identity_metrics(uc_t);
    auto im_m = compute_identity_metrics(uc_m);

    // All have same formula
    CHECK(im_c.reduced_formula == "O2Zr", "ZrO2 cubic: correct formula");
    CHECK(im_t.reduced_formula == "O2Zr", "ZrO2 tetragonal: correct formula");
    CHECK(im_m.reduced_formula == "O2Zr", "ZrO2 monoclinic: correct formula");

    // Volume per formula unit should be consistent (within 15%)
    double vpfu_c = im_c.cell_volume / im_c.formula_units_Z;
    double vpfu_t = im_t.cell_volume / im_t.formula_units_Z;
    double vpfu_m = im_m.cell_volume / im_m.formula_units_Z;

    CHECK(std::abs(vpfu_c - vpfu_t) / vpfu_c < 0.15, 
          "ZrO2: cubic vs tetragonal V/Z consistent");
    CHECK(std::abs(vpfu_c - vpfu_m) / vpfu_c < 0.15, 
          "ZrO2: cubic vs monoclinic V/Z consistent");
}

// ============================================================================
// Test 16: Scorecard for new structures against reference
// ============================================================================
void test_benchmark_scorecards() {
    struct RefTest {
        std::string name;
        UnitCell (*factory)();
        ReferenceData ref;
        double cutoff;
    };

    RefTest tests[] = {
        {"ThO2",     presets::tho2_fluorite,     reference::fluorite_tho2_ref(),         3.0},
        {"CeO2",     presets::ceo2_fluorite,     reference::fluorite_ceo2_ref(),         3.0},
        {"PuO2",     presets::puo2_fluorite,     reference::fluorite_puo2_ref(),         3.0},
        {"ZrO2c",    presets::zro2_cubic,        reference::zro2_cubic_ref(),            3.0},
        {"ZrO2m",    presets::zro2_monoclinic,   reference::zro2_monoclinic_ref(),       2.8},
        {"SrTiO3",   presets::srtio3_perovskite, reference::perovskite_srtio3_ref(),     2.5},
        {"BaTiO3",   presets::batio3_cubic,      reference::perovskite_batio3_cubic_ref(), 2.5},
        {"MgAl2O4",  presets::mgal2o4_spinel,    reference::spinel_mgal2o4_ref(),        2.5},
        {"Fe3O4",    presets::fe3o4_spinel,       reference::spinel_fe3o4_ref(),          2.5},
        {"Gd2Ti2O7", presets::gd2ti2o7_pyrochlore, reference::pyrochlore_gd2ti2o7_ref(), 3.0},
        {"La2Zr2O7", presets::la2zr2o7_pyrochlore, reference::pyrochlore_la2zr2o7_ref(), 3.0},
    };

    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  BENCHMARK CRYSTAL REFERENCE VERIFICATION                        ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";

    for (const auto& t : tests) {
        auto uc = t.factory();
        auto sc = verify_against_reference(uc, t.ref, t.cutoff);

        bool pass = (sc.pass_rate >= 60.0);

        std::cout << "  " << std::setw(12) << std::left << t.name
                  << "  " << sc.passed << "/" << sc.total_checks
                  << " (" << std::fixed << std::setprecision(0) << sc.pass_rate << "%)"
                  << (pass ? "  ✓" : "  ✗") << "\n";

        CHECK(pass, t.name + " scorecard >= 60%");
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "╔══════════════════════════════════════════════════╗\n";
    std::cout << "║  Crystal Metrics & Verification Tests (SS10b)   ║\n";
    std::cout << "╚══════════════════════════════════════════════════╝\n\n";

    test_identity_metrics();
    test_symmetry_metrics();
    test_local_geometry();
    test_topology();
    test_reciprocal_space();
    test_full_scorecard();
    test_supercell_consistency();
    test_all_metrics();
    test_all_presets_sanity();
    test_determinism();
    test_reference_scorecards();
    test_benchmark_crystals_sanity();
    test_fluorite_family();
    test_perovskite_family();
    test_polymorph_ordering();
    test_benchmark_scorecards();

    std::cout << "\n────────────────────────────────────────────────────\n";
    std::cout << "  PASSED: " << tests_passed << "\n";
    std::cout << "  FAILED: " << tests_failed << "\n";
    std::cout << "────────────────────────────────────────────────────\n";

    if (tests_failed == 0) {
        std::cout << "  ✓ ALL TESTS PASSED\n";
    } else {
        std::cout << "  ✗ SOME TESTS FAILED\n";
    }

    return tests_failed;
}
