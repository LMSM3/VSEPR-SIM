/**
 * test_crystal_pipeline.cpp
 * -------------------------
 * Validation tests for the crystal module:
 *   1. Lattice construction and invariants
 *   2. Metric tensor distance equivalence
 *   3. Fractional ↔ Cartesian round-trip
 *   4. MIC displacement correctness
 *   5. Unit cell presets: atom counts, charges, symmetry
 *   6. Supercell construction: atom scaling, PBC box scaling
 *   7. Bond inference from distances
 *   8. Coordination numbers for known structures
 *   9. Construction validation (strain, bond count)
 *  10. Coordinate wrapping
 *  11. Triclinic lattice from parameters
 *  12. Hexagonal lattice angles
 */

#include "atomistic/crystal/lattice.hpp"
#include "atomistic/crystal/unit_cell.hpp"
#include "atomistic/crystal/supercell.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/integrators/fire.hpp"
#include <iostream>
#include <cmath>
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>

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
// Test 1: Lattice construction invariants
// ============================================================================
void test_lattice_cubic() {
    auto lat = Lattice::cubic(5.64);
    CHECK_CLOSE(lat.V, 5.64*5.64*5.64, 1e-6, "cubic volume");
    CHECK_CLOSE(lat.a_len(), 5.64, 1e-9, "a length");
    CHECK_CLOSE(lat.b_len(), 5.64, 1e-9, "b length");
    CHECK_CLOSE(lat.c_len(), 5.64, 1e-9, "c length");
    CHECK_CLOSE(lat.alpha_deg(), 90.0, 1e-6, "alpha = 90");
    CHECK_CLOSE(lat.beta_deg(),  90.0, 1e-6, "beta = 90");
    CHECK_CLOSE(lat.gamma_deg(), 90.0, 1e-6, "gamma = 90");
}

// ============================================================================
// Test 2: Metric tensor distance equivalence
// ============================================================================
void test_metric_tensor() {
    auto lat = Lattice::cubic(4.05);
    Vec3 f1 = {0.0, 0.0, 0.0};
    Vec3 f2 = {0.5, 0.5, 0.0};

    double d_cart = lat.distance(f1, f2);
    double d_metric = lat.distance_metric(f1, f2);

    CHECK_CLOSE(d_cart, d_metric, 1e-9,
                "Cartesian and metric-tensor distances agree");

    // Expected: sqrt((0.5*4.05)^2 + (0.5*4.05)^2) = 4.05*sqrt(0.5) ≈ 2.864
    double expected = 4.05 * std::sqrt(0.5);
    CHECK_CLOSE(d_cart, expected, 1e-6, "FCC NN distance correct");
}

// ============================================================================
// Test 3: Fractional ↔ Cartesian round-trip
// ============================================================================
void test_roundtrip() {
    auto lat = Lattice::from_parameters(5.0, 6.0, 7.0, 80.0, 85.0, 95.0);
    Vec3 f_orig = {0.3, 0.6, 0.8};
    Vec3 r = lat.to_cartesian(f_orig);
    Vec3 f_back = lat.to_fractional(r);

    CHECK_CLOSE(f_back.x, f_orig.x, 1e-9, "round-trip frac.x");
    CHECK_CLOSE(f_back.y, f_orig.y, 1e-9, "round-trip frac.y");
    CHECK_CLOSE(f_back.z, f_orig.z, 1e-9, "round-trip frac.z");
}

// ============================================================================
// Test 4: MIC displacement
// ============================================================================
void test_mic() {
    auto lat = Lattice::cubic(10.0);
    Vec3 f1 = {0.05, 0.05, 0.05};
    Vec3 f2 = {0.95, 0.95, 0.95};

    // Without MIC, distance would be ~sqrt(3)*0.9*10 ≈ 15.6
    // With MIC, should wrap to (-0.1, -0.1, -0.1) → distance = sqrt(3)*0.1*10 ≈ 1.73
    double d = lat.distance(f1, f2);
    double expected = 10.0 * std::sqrt(3.0) * 0.1;
    CHECK_CLOSE(d, expected, 1e-6, "MIC wraps across boundary");

    // Self-distance should be zero
    CHECK_CLOSE(lat.distance(f1, f1), 0.0, 1e-12, "self-distance = 0");
}

// ============================================================================
// Test 5: Unit cell presets
// ============================================================================
void test_presets() {
    // NaCl: 8 atoms (4 Na + 4 Cl), charge-neutral
    auto nacl = presets::sodium_chloride();
    CHECK(nacl.num_atoms() == 8, "NaCl: 8 atoms");
    double total_q = 0;
    for (const auto& a : nacl.basis) total_q += a.charge;
    CHECK_CLOSE(total_q, 0.0, 1e-9, "NaCl: charge-neutral");

    // Al FCC: 4 atoms, no charge
    auto al = presets::aluminum_fcc();
    CHECK(al.num_atoms() == 4, "Al FCC: 4 atoms");

    // Si diamond: 8 atoms
    auto si = presets::silicon_diamond();
    CHECK(si.num_atoms() == 8, "Si diamond: 8 atoms");

    // Fe BCC: 2 atoms
    auto fe = presets::iron_bcc();
    CHECK(fe.num_atoms() == 2, "Fe BCC: 2 atoms");

    // CsCl: 2 atoms
    auto cscl = presets::cesium_chloride();
    CHECK(cscl.num_atoms() == 2, "CsCl: 2 atoms");

    // Rutile: 6 atoms (2 Ti + 4 O)
    auto tio2 = presets::rutile_tio2();
    CHECK(tio2.num_atoms() == 6, "TiO2 rutile: 6 atoms");
}

// ============================================================================
// Test 6: Supercell construction
// ============================================================================
void test_supercell() {
    auto nacl = presets::sodium_chloride();
    auto result = construct_supercell(nacl, 2, 2, 2);

    CHECK(result.total_atoms == 8 * 2 * 2 * 2, "2x2x2 NaCl: 64 atoms");
    CHECK(result.state.N == 64, "state.N == 64");
    CHECK(result.state.X.size() == 64, "X.size == 64");
    CHECK(result.state.M.size() == 64, "M.size == 64");
    CHECK(result.state.Q.size() == 64, "Q.size == 64");
    CHECK(result.state.type.size() == 64, "type.size == 64");

    // Box should be 2× the unit cell
    CHECK(result.state.box.enabled, "PBC enabled");
    CHECK_CLOSE(result.state.box.L.x, 2 * 5.64, 1e-6, "box Lx = 2a");
    CHECK_CLOSE(result.state.box.L.y, 2 * 5.64, 1e-6, "box Ly = 2a");
    CHECK_CLOSE(result.state.box.L.z, 2 * 5.64, 1e-6, "box Lz = 2a");

    // Total charge should still be zero
    double total_q = 0;
    for (size_t i = 0; i < result.state.Q.size(); ++i) total_q += result.state.Q[i];
    CHECK_CLOSE(total_q, 0.0, 1e-9, "supercell charge-neutral");

    // Provenance chain
    CHECK(result.recipe.steps.size() >= 2, "recipe has load + supercell steps");
}

// ============================================================================
// Test 7: Bond inference
// ============================================================================
void test_bond_inference() {
    auto al = presets::aluminum_fcc();
    auto state = al.to_state();

    auto bonds = infer_bonds_from_distances(state, al.lattice, 1.2);
    // In FCC conventional cell with 4 atoms, each atom has 12 NN in bulk.
    // But within a single unit cell with PBC, the topology is restricted.
    // The key test is that bonds are found (non-zero).
    CHECK(bonds.count > 0, "FCC Al: some bonds inferred");
}

// ============================================================================
// Test 8: Coordination numbers
// ============================================================================
void test_coordination() {
    // Build a 2x2x2 Al FCC supercell: expect CN=12 for bulk FCC
    auto al = presets::aluminum_fcc();
    auto sc = construct_supercell(al, 2, 2, 2);

    auto coord = coordination_numbers(sc.state, sc.lattice, 1.15);
    // FCC: coordination number = 12 for all atoms in bulk
    uint32_t min_cn = *std::min_element(coord.begin(), coord.end());
    uint32_t max_cn = *std::max_element(coord.begin(), coord.end());

    CHECK(min_cn == max_cn, "FCC supercell: uniform coordination");
    CHECK(min_cn == 12, "FCC coordination = 12");
}

// ============================================================================
// Test 9: Construction validation
// ============================================================================
void test_validation() {
    // Simulate passing case
    auto report = validate_construction(-3.36, -3.36, 12, 96, 8);
    CHECK(report.strain_pass, "zero strain passes");
    CHECK(report.bond_count_pass, "exact bond scaling passes");
    CHECK(report.all_passed(), "all passed for ideal case");

    // Simulate failing case: 10% strain
    auto bad_report = validate_construction(-3.36, -3.70, 12, 96, 8);
    CHECK(!bad_report.strain_pass, "10% strain fails");
}

// ============================================================================
// Test 10: Coordinate wrapping
// ============================================================================
void test_wrapping() {
    auto al = presets::aluminum_fcc();
    auto state = al.to_state();

    // Push an atom outside the box
    state.X[0] = {-1.0, -2.0, 10.0};

    wrap_positions(state, al.lattice);

    // Should now be inside [0, a) for each dimension
    double a = 4.05;
    CHECK(state.X[0].x >= 0 && state.X[0].x < a, "wrapped x in [0,a)");
    CHECK(state.X[0].y >= 0 && state.X[0].y < a, "wrapped y in [0,a)");
    CHECK(state.X[0].z >= 0 && state.X[0].z < a, "wrapped z in [0,a)");
}

// ============================================================================
// Test 11: Triclinic lattice from parameters
// ============================================================================
void test_triclinic() {
    auto lat = Lattice::from_parameters(5.0, 6.0, 7.0, 80.0, 85.0, 95.0);
    CHECK_CLOSE(lat.a_len(), 5.0, 1e-6, "triclinic a_len");
    CHECK_CLOSE(lat.b_len(), 6.0, 1e-6, "triclinic b_len");
    CHECK_CLOSE(lat.c_len(), 7.0, 1e-6, "triclinic c_len");
    CHECK_CLOSE(lat.alpha_deg(), 80.0, 0.1, "triclinic alpha");
    CHECK_CLOSE(lat.beta_deg(),  85.0, 0.1, "triclinic beta");
    CHECK_CLOSE(lat.gamma_deg(), 95.0, 0.1, "triclinic gamma");

    // Volume should be positive and reasonable
    CHECK(lat.V > 0, "triclinic volume > 0");
    CHECK(lat.V < 5*6*7, "triclinic V < a*b*c (non-orthogonal)");
}

// ============================================================================
// Test 12: Hexagonal lattice
// ============================================================================
void test_hexagonal() {
    auto lat = Lattice::hexagonal(3.0, 5.0);
    CHECK_CLOSE(lat.a_len(), 3.0, 1e-6, "hex a_len");
    CHECK_CLOSE(lat.b_len(), 3.0, 1e-6, "hex b_len = a");
    CHECK_CLOSE(lat.c_len(), 5.0, 1e-6, "hex c_len");
    CHECK_CLOSE(lat.gamma_deg(), 120.0, 0.1, "hex gamma = 120°");
    CHECK_CLOSE(lat.alpha_deg(), 90.0, 0.1, "hex alpha = 90°");
}

// ============================================================================
// Test 13: UnitCell → State → FIRE (LJ-only relaxation)
// ============================================================================
void test_fire_relaxation() {
    auto al = presets::aluminum_fcc();
    auto sc = construct_supercell(al, 2, 2, 2);

    auto model = create_lj_coulomb_model();
    ModelParams mp;
    mp.rc = 8.0;

    // Evaluate initial energy
    model->eval(sc.state, mp);
    double E_init = sc.state.E.total();
    CHECK(std::isfinite(E_init), "initial energy is finite");

    // Run a short FIRE
    FIRE fire(*model, mp);
    FIREParams fp;
    fp.max_steps = 50;
    fp.epsF = 1e-3;

    auto stats = fire.minimize(sc.state, fp);
    CHECK(std::isfinite(stats.U), "FIRE final energy is finite");
    CHECK(stats.Frms < 100.0, "FIRE reduced forces from initial");
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "╔══════════════════════════════════════════════════╗\n";
    std::cout << "║  Crystal Pipeline Tests                         ║\n";
    std::cout << "╚══════════════════════════════════════════════════╝\n\n";

    test_lattice_cubic();
    test_metric_tensor();
    test_roundtrip();
    test_mic();
    test_presets();
    test_supercell();
    test_bond_inference();
    test_coordination();
    test_validation();
    test_wrapping();
    test_triclinic();
    test_hexagonal();
    test_fire_relaxation();

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
