// test_gly_ala.cpp — Gly-Ala peptide bond formation and scoring tests
// Day 48A: Organic / Peptide Formation Core
//
// Tests:
//   1. Gly-Gly peptide bond formation
//   2. Gly-Ala peptide bond formation
//   3. Bond count after chain formation
//   4. Energy evaluation in vacuum
//   5. Energy evaluation in polar solvent
//   6. Formation report completeness
//   7. Gly-Ala-Ser three-residue chain
//   8. Incompatible residues (missing backbone) rejected

#include "../peptide/peptide_formation.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>

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

// --- Atom factories ---
static std::vector<Atom> make_backbone_atoms(int base_id, const char* res_name,
                                              double x_offset) {
    std::vector<Atom> atoms;
    int id = base_id;
    std::string prefix(res_name);

    atoms.push_back(Atom{
        .atom_id = id++, .atomic_number = 7, .isotope = 14,
        .atom_name = prefix + "_N", .element_symbol = "N",
        .position = {x_offset, 0.0, 0.0},
        .chem_role = VSEPR_ROLE_BACKBONE_N,
        .covalent_radius_pm = 71.0, .vdw_radius_pm = 155.0, .mass_u = 14.007
    });
    atoms.push_back(Atom{
        .atom_id = id++, .atomic_number = 6, .isotope = 12,
        .atom_name = prefix + "_CA", .element_symbol = "C",
        .position = {x_offset + 1.47, 0.0, 0.0},
        .chem_role = VSEPR_ROLE_ALPHA_C,
        .covalent_radius_pm = 76.0, .vdw_radius_pm = 170.0, .mass_u = 12.011
    });
    atoms.push_back(Atom{
        .atom_id = id++, .atomic_number = 6, .isotope = 12,
        .atom_name = prefix + "_C", .element_symbol = "C",
        .position = {x_offset + 2.99, 0.0, 0.0},
        .chem_role = VSEPR_ROLE_CARBONYL_C,
        .covalent_radius_pm = 76.0, .vdw_radius_pm = 170.0, .mass_u = 12.011
    });
    atoms.push_back(Atom{
        .atom_id = id++, .atomic_number = 8, .isotope = 16,
        .atom_name = prefix + "_O", .element_symbol = "O",
        .position = {x_offset + 2.99, 1.23, 0.0},
        .chem_role = VSEPR_ROLE_CARBONYL_O,
        .covalent_radius_pm = 66.0, .vdw_radius_pm = 152.0, .mass_u = 15.999
    });

    return atoms;
}

static Atom make_sidechain_atom(int id, const char* name, double x, double y, double z,
                                 int Z = 6, double mass = 12.011) {
    return Atom{
        .atom_id = id, .atomic_number = Z,
        .atom_name = name, .element_symbol = (Z == 6 ? "C" : (Z == 8 ? "O" : "X")),
        .position = {x, y, z},
        .chem_role = VSEPR_ROLE_SIDECHAIN,
        .covalent_radius_pm = 76.0, .vdw_radius_pm = 170.0, .mass_u = mass
    };
}

static Residue make_residue(int res_id, const char* name, int base_id,
                             int sidechain_root = -1,
                             VSEPR_SidechainClass sc_class = VSEPR_SIDECHAIN_NONE) {
    Residue r {
        .residue_id = res_id,
        .residue_name = name,
        .atom_ids = {base_id, base_id+1, base_id+2, base_id+3},
        .backbone_N  = base_id,
        .backbone_CA = base_id + 1,
        .backbone_C  = base_id + 2,
        .backbone_O  = base_id + 3,
        .sidechain_root = sidechain_root,
        .sidechain_class = sc_class,
    };
    if (sidechain_root >= 0) {
        r.atom_ids.push_back(sidechain_root);
    }
    return r;
}

// --- Test 1: Gly-Gly peptide bond ---
static void test_gly_gly_bond() {
    TEST("Gly-Gly peptide bond formation");
    auto atoms1 = make_backbone_atoms(1, "G1", 0.0);
    auto atoms2 = make_backbone_atoms(5, "G2", 4.32);

    std::vector<Atom> all_atoms;
    all_atoms.insert(all_atoms.end(), atoms1.begin(), atoms1.end());
    all_atoms.insert(all_atoms.end(), atoms2.begin(), atoms2.end());

    std::vector<Residue> residues = {
        make_residue(1, "GLY", 1),
        make_residue(2, "GLY", 5),
    };

    PeptideFormationPipeline pipeline{VSEPR_ENV_VACUUM};
    auto result = pipeline.build_from_residues(residues, all_atoms);
    CHECK(result.has_value(), "build should succeed");

    auto formed = pipeline.form_chain(std::move(*result));
    CHECK(formed.has_value(), "form_chain should succeed");
    CHECK(formed->bond_count() == 1, "Should have 1 peptide bond");

    // The bond should connect residue 1 C to residue 2 N
    auto& bonds = formed->bonds();
    CHECK(bonds[0].a == 3, "Bond should originate from Gly1 C (id=3)");
    CHECK(bonds[0].b == 5, "Bond should terminate at Gly2 N (id=5)");
    PASS();
}

// --- Test 2: Gly-Ala peptide bond ---
static void test_gly_ala_bond() {
    TEST("Gly-Ala peptide bond formation");
    auto gly_atoms = make_backbone_atoms(1, "GLY", 0.0);
    auto ala_atoms = make_backbone_atoms(5, "ALA", 4.32);

    // Ala sidechain: CB methyl
    auto cb = make_sidechain_atom(9, "ALA_CB", 5.79 + 1.54, -1.0, 0.0);

    std::vector<Atom> all_atoms;
    all_atoms.insert(all_atoms.end(), gly_atoms.begin(), gly_atoms.end());
    all_atoms.insert(all_atoms.end(), ala_atoms.begin(), ala_atoms.end());
    all_atoms.push_back(cb);

    std::vector<Residue> residues = {
        make_residue(1, "GLY", 1),
        make_residue(2, "ALA", 5, 9, VSEPR_SIDECHAIN_HYDROPHOBIC),
    };

    PeptideFormationPipeline pipeline{VSEPR_ENV_POLAR_SOLVENT};
    auto result = pipeline.build_from_residues(residues, all_atoms);
    CHECK(result.has_value(), "build should succeed");

    auto summary = pipeline.solve(std::move(*result));
    CHECK(summary.has_value(), "solve should succeed");
    CHECK(summary->bond_count == 1, "Should have 1 peptide bond");
    CHECK(summary->atom_count == 9, "Should have 9 atoms total");
    CHECK(summary->residue_count == 2, "Should have 2 residues");
    PASS();
}

// --- Test 3: Bond count correct after chaining ---
static void test_bond_count_after_chain() {
    TEST("Bond count correct for 3-residue chain");
    std::vector<Atom> all_atoms;
    std::vector<Residue> residues;

    for (int i = 0; i < 3; ++i) {
        int base = 1 + i * 4;
        auto atoms = make_backbone_atoms(base, ("R" + std::to_string(i)).c_str(), i * 4.32);
        all_atoms.insert(all_atoms.end(), atoms.begin(), atoms.end());
        residues.push_back(make_residue(i + 1, "GLY", base));
    }

    PeptideFormationPipeline pipeline{VSEPR_ENV_VACUUM};
    auto result = pipeline.build_from_residues(residues, all_atoms);
    CHECK(result.has_value(), "build should succeed");

    auto formed = pipeline.form_chain(std::move(*result));
    CHECK(formed.has_value(), "form_chain should succeed");
    CHECK(formed->bond_count() == 2, "3 residues should produce 2 peptide bonds");
    PASS();
}

// --- Test 4: Energy in vacuum ---
static void test_energy_vacuum() {
    TEST("Energy evaluation in vacuum");
    auto gly_atoms = make_backbone_atoms(1, "GLY", 0.0);
    auto ala_atoms = make_backbone_atoms(5, "ALA", 4.32);

    std::vector<Atom> all_atoms;
    all_atoms.insert(all_atoms.end(), gly_atoms.begin(), gly_atoms.end());
    all_atoms.insert(all_atoms.end(), ala_atoms.begin(), ala_atoms.end());

    std::vector<Residue> residues = {
        make_residue(1, "GLY", 1),
        make_residue(2, "ALA", 5),
    };

    PeptideFormationPipeline pipeline{VSEPR_ENV_VACUUM};
    auto result = pipeline.build_from_residues(residues, all_atoms);
    CHECK(result.has_value(), "build should succeed");

    auto summary = pipeline.solve(std::move(*result));
    CHECK(summary.has_value(), "solve should succeed");
    // Energy should be finite and non-NaN
    CHECK(std::isfinite(summary->energy.total_kj_mol), "Total energy should be finite");
    CHECK(std::isfinite(summary->energy.bond_kj_mol), "Bond energy should be finite");
    CHECK(std::isfinite(summary->energy.vdw_kj_mol), "VdW energy should be finite");
    PASS();
}

// --- Test 5: Energy in polar solvent ---
static void test_energy_polar_solvent() {
    TEST("Energy evaluation in polar solvent");
    auto gly_atoms = make_backbone_atoms(1, "GLY", 0.0);
    auto ala_atoms = make_backbone_atoms(5, "ALA", 4.32);

    std::vector<Atom> all_atoms;
    all_atoms.insert(all_atoms.end(), gly_atoms.begin(), gly_atoms.end());
    all_atoms.insert(all_atoms.end(), ala_atoms.begin(), ala_atoms.end());

    std::vector<Residue> residues = {
        make_residue(1, "GLY", 1),
        make_residue(2, "ALA", 5),
    };

    PeptideFormationPipeline pipeline_vac{VSEPR_ENV_VACUUM};
    PeptideFormationPipeline pipeline_sol{VSEPR_ENV_POLAR_SOLVENT};

    auto result_v = pipeline_vac.build_from_residues(residues, all_atoms);
    auto result_s = pipeline_sol.build_from_residues(residues, all_atoms);
    CHECK(result_v.has_value() && result_s.has_value(), "builds should succeed");

    auto sum_v = pipeline_vac.solve(std::move(*result_v));
    auto sum_s = pipeline_sol.solve(std::move(*result_s));
    CHECK(sum_v.has_value() && sum_s.has_value(), "solves should succeed");

    // Both should produce finite energies
    CHECK(std::isfinite(sum_v->energy.total_kj_mol), "Vacuum energy finite");
    CHECK(std::isfinite(sum_s->energy.total_kj_mol), "Polar energy finite");
    PASS();
}

// --- Test 6: Formation report completeness ---
static void test_formation_report() {
    TEST("Formation report completeness");
    auto gly_atoms = make_backbone_atoms(1, "GLY", 0.0);
    auto ala_atoms = make_backbone_atoms(5, "ALA", 4.32);

    std::vector<Atom> all_atoms;
    all_atoms.insert(all_atoms.end(), gly_atoms.begin(), gly_atoms.end());
    all_atoms.insert(all_atoms.end(), ala_atoms.begin(), ala_atoms.end());

    std::vector<Residue> residues = {
        make_residue(1, "GLY", 1),
        make_residue(2, "ALA", 5),
    };

    PeptideFormationPipeline pipeline{VSEPR_ENV_POLAR_SOLVENT};
    auto result = pipeline.build_from_residues(residues, all_atoms);
    CHECK(result.has_value(), "build should succeed");

    auto summary = pipeline.solve(std::move(*result));
    CHECK(summary.has_value(), "solve should succeed");

    // Check all scores are in [0, 1] range (or at least finite)
    CHECK(summary->score.steric_score >= 0.0, "Steric score >= 0");
    CHECK(summary->score.planarity_score >= 0.0, "Planarity score >= 0");
    CHECK(summary->score.confidence_score >= 0.0, "Confidence >= 0");
    CHECK(std::isfinite(summary->score.confidence_score), "Confidence finite");

    // Check formation state
    CHECK(summary->formation_state == VSEPR_STATE_LOCAL_FOLDED, "State should be LOCAL_FOLDED");
    CHECK(summary->chemical_validity_pass, "Chemical validity should pass");
    PASS();
}

// --- Test 7: Gly-Ala-Ser three-residue chain ---
static void test_gly_ala_ser() {
    TEST("Gly-Ala-Ser three-residue chain");

    std::vector<Atom> all_atoms;
    std::vector<Residue> residues;
    int atom_id = 1;

    // GLY
    {
        auto a = make_backbone_atoms(atom_id, "GLY", 0.0);
        all_atoms.insert(all_atoms.end(), a.begin(), a.end());
        residues.push_back(make_residue(1, "GLY", atom_id));
        atom_id += 4;
    }
    // ALA (with CB sidechain)
    {
        auto a = make_backbone_atoms(atom_id, "ALA", 4.32);
        all_atoms.insert(all_atoms.end(), a.begin(), a.end());
        int cb_id = atom_id + 4;
        all_atoms.push_back(make_sidechain_atom(cb_id, "ALA_CB", 5.79 + 1.54, -1.0, 0.0));
        residues.push_back(make_residue(2, "ALA", atom_id, cb_id, VSEPR_SIDECHAIN_HYDROPHOBIC));
        atom_id += 5;
    }
    // SER (with OG sidechain)
    {
        auto a = make_backbone_atoms(atom_id, "SER", 8.64);
        all_atoms.insert(all_atoms.end(), a.begin(), a.end());
        int og_id = atom_id + 4;
        all_atoms.push_back(make_sidechain_atom(og_id, "SER_OG", 10.11 + 1.43, -1.0, 0.0, 8, 15.999));
        residues.push_back(make_residue(3, "SER", atom_id, og_id, VSEPR_SIDECHAIN_POLAR));
        atom_id += 5;
    }

    PeptideFormationPipeline pipeline{VSEPR_ENV_POLAR_SOLVENT};
    auto result = pipeline.build_from_residues(residues, all_atoms);
    CHECK(result.has_value(), "build should succeed");

    auto summary = pipeline.solve(std::move(*result));
    CHECK(summary.has_value(), "solve should succeed");
    CHECK(summary->residue_count == 3, "Should have 3 residues");
    CHECK(summary->bond_count == 2, "Should have 2 peptide bonds");
    CHECK(summary->atom_count == static_cast<int>(all_atoms.size()), "All atoms should be present");
    PASS();
}

// --- Test 8: Incompatible residues rejected ---
static void test_incompatible_residues() {
    TEST("Incompatible residues (missing backbone) rejected");

    Residue bad {
        .residue_id = 1,
        .residue_name = "BAD",
        .backbone_N = 1, .backbone_CA = 2, .backbone_C = -1, .backbone_O = 4,
    };
    Residue good {
        .residue_id = 2,
        .residue_name = "GLY",
        .backbone_N = 5, .backbone_CA = 6, .backbone_C = 7, .backbone_O = 8,
    };

    MolecularGraph graph;
    PeptideBondEngine engine;
    auto result = engine.can_form_peptide_bond(graph, bad, good);
    CHECK(!result.has_value(), "Should reject bond with incomplete backbone");
    CHECK(result.error().code == ErrorCode::incompatible_residues, "Code should be incompatible_residues");
    PASS();
}

int main() {
    std::puts("=============================================================");
    std::puts("  VSEPR-SIM  Day 48A  |  Gly-Ala Peptide Bond Tests");
    std::puts("=============================================================");

    test_gly_gly_bond();
    test_gly_ala_bond();
    test_bond_count_after_chain();
    test_energy_vacuum();
    test_energy_polar_solvent();
    test_formation_report();
    test_gly_ala_ser();
    test_incompatible_residues();

    std::puts("-------------------------------------------------------------");
    std::printf("  Total: %d   Passed: %d   Failed: %d\n", total, passed, failed);
    std::puts("=============================================================");

    return failed > 0 ? 1 : 0;
}
