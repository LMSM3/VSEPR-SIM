// test_glycine.cpp — Unit tests for glycine residue validation
// Day 48A: Organic / Peptide Formation Core
//
// Tests:
//   1. Glycine atom creation with correct Z and mass
//   2. Glycine backbone validation (valid)
//   3. Missing backbone atom fails cleanly
//   4. Single-residue pipeline run (no peptide bonds needed)
//   5. Overbonded carbon detection
//   6. Negative atomic number rejection

#include "../peptide/peptide_formation.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>

static int total   = 0;
static int passed  = 0;
static int failed  = 0;

#define TEST(name) \
    do { ++total; std::printf("  [%02d] %-52s", total, name); } while(0)

#define PASS() \
    do { ++passed; std::puts(" PASS"); } while(0)

#define FAIL(msg) \
    do { ++failed; std::printf(" FAIL : %s\n", msg); } while(0)

#define CHECK(cond, msg) \
    do { if (!(cond)) { FAIL(msg); return; } } while(0)

using namespace vsepr::chem;

// --- Helper: create glycine atoms (N, CA, C, O, 2H on CA, H on N) ---
static std::vector<Atom> make_glycine_atoms(int base_id = 1) {
    std::vector<Atom> atoms;
    int id = base_id;

    // N (backbone nitrogen)
    atoms.push_back(Atom{
        .atom_id = id++, .atomic_number = 7, .isotope = 14,
        .atom_name = "N", .element_symbol = "N",
        .position = {0.0, 0.0, 0.0},
        .chem_role = VSEPR_PEPTIDE_ROLE_BACKBONE_N,
        .covalent_radius_pm = 71.0, .vdw_radius_pm = 155.0, .mass_u = 14.007
    });
    // CA (alpha carbon)
    atoms.push_back(Atom{
        .atom_id = id++, .atomic_number = 6, .isotope = 12,
        .atom_name = "CA", .element_symbol = "C",
        .position = {1.47, 0.0, 0.0},
        .chem_role = VSEPR_PEPTIDE_ROLE_ALPHA_C,
        .covalent_radius_pm = 76.0, .vdw_radius_pm = 170.0, .mass_u = 12.011
    });
    // C (carbonyl carbon)
    atoms.push_back(Atom{
        .atom_id = id++, .atomic_number = 6, .isotope = 12,
        .atom_name = "C", .element_symbol = "C",
        .position = {2.99, 0.0, 0.0},
        .chem_role = VSEPR_PEPTIDE_ROLE_CARBONYL_C,
        .covalent_radius_pm = 76.0, .vdw_radius_pm = 170.0, .mass_u = 12.011
    });
    // O (carbonyl oxygen)
    atoms.push_back(Atom{
        .atom_id = id++, .atomic_number = 8, .isotope = 16,
        .atom_name = "O", .element_symbol = "O",
        .position = {2.99, 1.23, 0.0},
        .chem_role = VSEPR_PEPTIDE_ROLE_CARBONYL_O,
        .covalent_radius_pm = 66.0, .vdw_radius_pm = 152.0, .mass_u = 15.999
    });

    return atoms;
}

static Residue make_glycine_residue(int residue_id, int base_atom_id) {
    return Residue{
        .residue_id = residue_id,
        .residue_name = "GLY",
        .atom_ids = {base_atom_id, base_atom_id+1, base_atom_id+2, base_atom_id+3},
        .backbone_N  = base_atom_id,
        .backbone_CA = base_atom_id + 1,
        .backbone_C  = base_atom_id + 2,
        .backbone_O  = base_atom_id + 3,
    };
}

// --- Test 1: Glycine atoms have correct properties ---
static void test_glycine_atom_creation() {
    TEST("Glycine atom creation (Z, mass)");
    auto atoms = make_glycine_atoms();
    CHECK(atoms.size() == 4, "Expected 4 atoms");
    CHECK(atoms[0].atomic_number == 7, "N should be Z=7");
    CHECK(atoms[1].atomic_number == 6, "CA should be Z=6");
    CHECK(atoms[2].atomic_number == 6, "C should be Z=6");
    CHECK(atoms[3].atomic_number == 8, "O should be Z=8");
    CHECK(std::abs(atoms[0].mass_u - 14.007) < 0.01, "N mass wrong");
    CHECK(std::abs(atoms[3].mass_u - 15.999) < 0.01, "O mass wrong");
    PASS();
}

// --- Test 2: Glycine backbone is valid ---
static void test_glycine_backbone_valid() {
    TEST("Glycine backbone validation (valid)");
    auto res = make_glycine_residue(1, 1);
    CHECK(res.has_valid_backbone(), "Glycine backbone should be valid");
    PASS();
}

// --- Test 3: Missing backbone fails cleanly ---
static void test_missing_backbone_fails() {
    TEST("Missing backbone atom fails cleanly");
    Residue bad_res {
        .residue_id = 99,
        .residue_name = "BAD",
        .backbone_N = 1,
        .backbone_CA = 2,
        .backbone_C = -1,   // missing
        .backbone_O = 4,
    };
    CHECK(!bad_res.has_valid_backbone(), "Should detect missing backbone_C");

    OrganicRuleEngine rules;
    MolecularGraph graph;
    auto result = rules.validate_residue(graph, bad_res);
    CHECK(!result.has_value(), "Should return error for incomplete backbone");
    CHECK(result.error().code == ErrorCode::invalid_backbone, "Error code should be invalid_backbone");
    PASS();
}

// --- Test 4: Single-residue pipeline (no peptide bonds) ---
static void test_single_residue_pipeline() {
    TEST("Single glycine pipeline (no peptide bonds)");
    auto atoms = make_glycine_atoms(1);
    auto residue = make_glycine_residue(1, 1);

    std::vector<Residue> residues = {residue};

    PeptideFormationPipeline pipeline{VSEPR_ENV_VACUUM};
    auto graph_result = pipeline.build_from_residues(residues, atoms);
    CHECK(graph_result.has_value(), "build_from_residues should succeed");
    CHECK(graph_result->atom_count() == 4, "Should have 4 atoms");
    CHECK(graph_result->residue_count() == 1, "Should have 1 residue");

    auto solve_result = pipeline.solve(std::move(*graph_result));
    CHECK(solve_result.has_value(), "solve should succeed");
    CHECK(solve_result->formation_class == VSEPR_FORM_CHAIN, "Formation class should be chain");
    CHECK(solve_result->atom_count == 4, "Summary atom count should be 4");
    CHECK(solve_result->residue_count == 1, "Summary residue count should be 1");
    CHECK(solve_result->bond_count == 0, "No bonds for single residue");
    PASS();
}

// --- Test 5: Overbonded carbon detection ---
static void test_overbonded_carbon() {
    TEST("Overbonded carbon detection");
    Atom carbon {
        .atom_id = 1, .atomic_number = 6,
        .atom_name = "C_BAD", .element_symbol = "C",
    };
    // Simulate 5 single bonds (exceeds max valence of 4)
    carbon.bonded_atom_ids = {2, 3, 4, 5, 6};
    carbon.bond_orders = {1, 1, 1, 1, 1};

    OrganicRuleEngine rules;
    auto result = rules.check_valence(carbon);
    CHECK(!result.has_value(), "Should detect overbonded carbon");
    CHECK(result.error().code == ErrorCode::overbonded_atom, "Error code should be overbonded_atom");
    PASS();
}

// --- Test 6: Negative atomic number rejection ---
static void test_negative_atomic_number() {
    TEST("Negative atomic number rejected");
    Atom bad_atom {
        .atom_id = 1, .atomic_number = -3,
        .atom_name = "???", .element_symbol = "??",
    };

    OrganicRuleEngine rules;
    auto result = rules.validate_atom(bad_atom);
    CHECK(!result.has_value(), "Should reject negative Z");
    CHECK(result.error().code == ErrorCode::invalid_valence, "Error code should be invalid_valence");
    PASS();
}

// --- Test 7: Omega defaults to 180 ---
static void test_omega_default() {
    TEST("Omega angle defaults to 180 (trans)");
    Residue res {
        .residue_id = 1,
        .residue_name = "GLY",
        .backbone_N = 1, .backbone_CA = 2, .backbone_C = 3, .backbone_O = 4,
    };
    CHECK(std::abs(res.omega_deg - 180.0) < 1e-10, "Default omega should be 180");
    PASS();
}

// --- Test 8: Valence table lookup ---
static void test_valence_table() {
    TEST("Valence table lookup (C, N, O, H)");
    auto* c = lookup_valence(6);
    CHECK(c != nullptr, "Carbon should exist in table");
    CHECK(c->max_valence == 4, "Carbon max valence should be 4");

    auto* n = lookup_valence(7);
    CHECK(n != nullptr, "Nitrogen should exist in table");
    CHECK(n->max_valence == 4, "Nitrogen max valence should be 4");

    auto* o = lookup_valence(8);
    CHECK(o != nullptr, "Oxygen should exist in table");
    CHECK(o->max_valence == 2, "Oxygen max valence should be 2");

    auto* h = lookup_valence(1);
    CHECK(h != nullptr, "Hydrogen should exist in table");
    CHECK(h->max_valence == 1, "Hydrogen max valence should be 1");

    auto* unknown = lookup_valence(200);
    CHECK(unknown == nullptr, "Z=200 should not exist in table");

    PASS();
}

int main() {
    std::puts("=============================================================");
    std::puts("  VSEPR-SIM  Day 48A  |  Glycine Residue Validation Tests");
    std::puts("=============================================================");

    test_glycine_atom_creation();
    test_glycine_backbone_valid();
    test_missing_backbone_fails();
    test_single_residue_pipeline();
    test_overbonded_carbon();
    test_negative_atomic_number();
    test_omega_default();
    test_valence_table();

    std::puts("-------------------------------------------------------------");
    std::printf("  Total: %d   Passed: %d   Failed: %d\n", total, passed, failed);
    std::puts("=============================================================");

    return failed > 0 ? 1 : 0;
}
