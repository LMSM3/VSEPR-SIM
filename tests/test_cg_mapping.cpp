/**
 * test_cg_mapping.cpp — Coarse-Grained Mapping Pipeline Test
 *
 * Exercises:
 *   1. Construct a small atomistic State (ethanol: C2H6O)
 *   2. Define a MappingScheme (3 beads: CH3, CH2, OH)
 *   3. Run AtomToBeadMapper with COM and COG projections
 *   4. Verify mass and charge conservation
 *   5. Verify sane() invariants
 *   6. Export a Markdown mapping report
 *   7. Verify bead-bead topology inference
 *
 * Anti-black-box: every assertion is commented with its physical meaning.
 */

#include "coarse_grain/core/bead_system.hpp"
#include "coarse_grain/mapping/atom_to_bead_mapper.hpp"
#include "coarse_grain/mapping/mapping_rule.hpp"
#include "coarse_grain/report/mapping_report.hpp"
#include "atomistic/core/state.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

// ============================================================================
// Test Molecule: Ethanol (C2H6O)
// ============================================================================
//
//  Atom layout:
//    0: C  (methyl carbon)        mass = 12.011
//    1: H                         mass = 1.008
//    2: H                         mass = 1.008
//    3: H                         mass = 1.008
//    4: C  (methylene carbon)     mass = 12.011
//    5: H                         mass = 1.008
//    6: H                         mass = 1.008
//    7: O  (hydroxyl oxygen)      mass = 15.999
//    8: H  (hydroxyl hydrogen)    mass = 1.008
//
//  Bonds: 0-1, 0-2, 0-3, 0-4, 4-5, 4-6, 4-7, 7-8
//
//  Mapping scheme (3 beads):
//    Bead 0: "CH3"  → atoms {0,1,2,3}
//    Bead 1: "CH2"  → atoms {4,5,6}
//    Bead 2: "OH"   → atoms {7,8}
//

static atomistic::State make_ethanol() {
    atomistic::State s;
    s.N = 9;
    s.X.resize(s.N);
    s.V.resize(s.N);
    s.Q.resize(s.N, 0.0);
    s.M.resize(s.N);
    s.type.resize(s.N);
    s.F.resize(s.N);

    // Positions (approximate, in Angstrom)
    s.X[0] = {-1.5, 0.0, 0.0};   // C (methyl)
    s.X[1] = {-2.1, 0.9, 0.0};   // H
    s.X[2] = {-2.1,-0.5, 0.8};   // H
    s.X[3] = {-2.1,-0.5,-0.8};   // H
    s.X[4] = { 0.0, 0.0, 0.0};   // C (methylene)
    s.X[5] = { 0.5, 0.9, 0.0};   // H
    s.X[6] = { 0.5,-0.9, 0.0};   // H
    s.X[7] = { 1.2, 0.0, 0.0};   // O (hydroxyl)
    s.X[8] = { 1.8, 0.8, 0.0};   // H

    // Velocities (zero for static test)
    for (auto& v : s.V) v = {0.0, 0.0, 0.0};

    // Masses (amu)
    s.M[0] = 12.011;  s.type[0] = 6;   // C
    s.M[1] = 1.008;   s.type[1] = 1;   // H
    s.M[2] = 1.008;   s.type[2] = 1;   // H
    s.M[3] = 1.008;   s.type[3] = 1;   // H
    s.M[4] = 12.011;  s.type[4] = 6;   // C
    s.M[5] = 1.008;   s.type[5] = 1;   // H
    s.M[6] = 1.008;   s.type[6] = 1;   // H
    s.M[7] = 15.999;  s.type[7] = 8;   // O
    s.M[8] = 1.008;   s.type[8] = 1;   // H

    // Charges (partial charges from OPLS, approximate)
    s.Q[0] = -0.18;   // C methyl
    s.Q[1] =  0.06;   // H
    s.Q[2] =  0.06;   // H
    s.Q[3] =  0.06;   // H
    s.Q[4] =  0.145;  // C methylene
    s.Q[5] =  0.06;   // H
    s.Q[6] =  0.06;   // H
    s.Q[7] = -0.683;  // O
    s.Q[8] =  0.418;  // H (hydroxyl)

    // Bonds (atomistic connectivity)
    s.B = {{0,1},{0,2},{0,3},{0,4},{4,5},{4,6},{4,7},{7,8}};

    return s;
}

static coarse_grain::MappingScheme make_ethanol_scheme() {
    using namespace coarse_grain;

    MappingScheme scheme;
    scheme.name = "Ethanol 3-bead (CH3/CH2/OH)";

    // Bead 0: CH3 → atoms {0,1,2,3}
    MappingRule r0;
    r0.rule_id = 0;
    r0.label = "CH3";
    r0.bead_type_id = 0;
    r0.selector.mode = SelectorMode::BY_INDICES;
    r0.selector.indices = {0, 1, 2, 3};
    scheme.rules.push_back(r0);

    // Bead 1: CH2 → atoms {4,5,6}
    MappingRule r1;
    r1.rule_id = 1;
    r1.label = "CH2";
    r1.bead_type_id = 1;
    r1.selector.mode = SelectorMode::BY_INDICES;
    r1.selector.indices = {4, 5, 6};
    scheme.rules.push_back(r1);

    // Bead 2: OH → atoms {7,8}
    MappingRule r2;
    r2.rule_id = 2;
    r2.label = "OH";
    r2.bead_type_id = 2;
    r2.selector.mode = SelectorMode::BY_INDICES;
    r2.selector.indices = {7, 8};
    scheme.rules.push_back(r2);

    return scheme;
}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
    using namespace coarse_grain;

    std::cout << "╔══════════════════════════════════════════════════╗\n";
    std::cout << "║    Coarse-Grained Mapping Pipeline Test         ║\n";
    std::cout << "╚══════════════════════════════════════════════════╝\n\n";

    int pass = 0;
    int fail = 0;

    auto check = [&](bool cond, const std::string& name) {
        if (cond) {
            std::cout << "  [PASS] " << name << "\n";
            ++pass;
        } else {
            std::cout << "  [FAIL] " << name << "\n";
            ++fail;
        }
    };

    // --- Build test data ---
    atomistic::State state = make_ethanol();
    MappingScheme scheme = make_ethanol_scheme();

    check(atomistic::sane(state), "Source state is sane");

    // --- Test 1: COM Mapping ---
    std::cout << "\n--- COM Mapping ---\n";
    AtomToBeadMapper mapper;
    auto result = mapper.map(state, scheme, ProjectionMode::CENTER_OF_MASS);

    check(result.ok, "Mapping succeeded");
    check(result.system.num_beads() == 3, "3 beads produced");
    check(result.system.sane(), "BeadSystem passes sane()");

    // Mass conservation (every amu accounted for)
    check(result.conservation.mass_conserved,
          "Mass conserved (error=" + std::to_string(result.conservation.mass_error) + " amu)");

    // Charge conservation (every electron accounted for)
    check(result.conservation.charge_conserved,
          "Charge conserved (error=" + std::to_string(result.conservation.charge_error) + " e)");

    // Check bead masses
    double expected_ch3_mass = 12.011 + 3 * 1.008;  // 15.035
    double expected_ch2_mass = 12.011 + 2 * 1.008;  // 14.027
    double expected_oh_mass  = 15.999 + 1.008;       // 17.007

    check(std::abs(result.system.beads[0].mass - expected_ch3_mass) < 1e-6,
          "CH3 bead mass = " + std::to_string(result.system.beads[0].mass));
    check(std::abs(result.system.beads[1].mass - expected_ch2_mass) < 1e-6,
          "CH2 bead mass = " + std::to_string(result.system.beads[1].mass));
    check(std::abs(result.system.beads[2].mass - expected_oh_mass) < 1e-6,
          "OH  bead mass = " + std::to_string(result.system.beads[2].mass));

    // Check parent atom indices (provenance is traceable)
    check(result.system.beads[0].parent_atom_indices.size() == 4, "CH3 has 4 parent atoms");
    check(result.system.beads[1].parent_atom_indices.size() == 3, "CH2 has 3 parent atoms");
    check(result.system.beads[2].parent_atom_indices.size() == 2, "OH  has 2 parent atoms");

    // Check mapping residual (COM vs COG difference)
    for (uint32_t i = 0; i < result.system.num_beads(); ++i) {
        check(result.system.beads[i].mapping_residual >= 0.0,
              "Bead " + std::to_string(i) + " residual is non-negative ("
              + std::to_string(result.system.beads[i].mapping_residual) + " A)");
    }

    // --- Test 2: Bead-Bead Topology ---
    std::cout << "\n--- Topology Inference ---\n";

    // Expected bead bonds:  CH3-CH2 (bead 0-1), CH2-OH (bead 1-2)
    // Because atom 0 (in bead 0) is bonded to atom 4 (in bead 1)
    // and atom 4 (in bead 1) is bonded to atom 7 (in bead 2)
    check(result.system.bonds.size() == 2, "2 bead-bead bonds inferred");

    if (result.system.bonds.size() >= 2) {
        auto has_bond = [&](uint32_t a, uint32_t b) {
            auto p1 = std::make_pair(a, b);
            auto p2 = std::make_pair(b, a);
            for (const auto& bond : result.system.bonds) {
                if (bond == p1 || bond == p2) return true;
            }
            return false;
        };
        check(has_bond(0, 1), "Bond: CH3 - CH2");
        check(has_bond(1, 2), "Bond: CH2 - OH");
    }

    // --- Test 3: COG Mapping ---
    std::cout << "\n--- COG Mapping ---\n";
    auto cog_result = mapper.map(state, scheme, ProjectionMode::CENTER_OF_GEOMETRY);
    check(cog_result.ok, "COG mapping succeeded");
    check(cog_result.conservation.mass_conserved, "COG mass conserved");
    check(cog_result.conservation.charge_conserved, "COG charge conserved");

    // COM and COG positions should generally differ (unless uniform masses)
    for (uint32_t i = 0; i < result.system.num_beads(); ++i) {
        auto com_pos = result.system.beads[i].com_position;
        auto cog_pos = result.system.beads[i].cog_position;
        auto diff = com_pos - cog_pos;
        double d = atomistic::norm(diff);
        std::cout << "    Bead " << i << ": COM-COG distance = " << d << " A\n";
    }

    // --- Test 4: Diagnostics ---
    std::cout << "\n--- Diagnostics ---\n";
    auto diag = result.system.diagnostics();
    std::cout << "    Mean residual: " << diag.mean_residual << " A\n";
    std::cout << "    Max residual:  " << diag.max_residual << " A\n";
    std::cout << "    Atoms mapped:  " << diag.n_atoms_mapped << "/" << state.N << "\n";
    check(diag.n_atoms_mapped == state.N, "All atoms mapped");

    // --- Test 5: Validation Failure (bad scheme) ---
    std::cout << "\n--- Error Handling ---\n";
    MappingScheme bad_scheme;
    bad_scheme.name = "Bad scheme (missing atoms)";
    MappingRule bad_rule;
    bad_rule.rule_id = 0;
    bad_rule.label = "partial";
    bad_rule.selector.mode = SelectorMode::BY_INDICES;
    bad_rule.selector.indices = {0, 1};  // Only 2 of 9 atoms
    bad_scheme.rules.push_back(bad_rule);

    auto bad_result = mapper.map(state, bad_scheme);
    check(!bad_result.ok, "Incomplete scheme rejected");
    std::cout << "    Error: " << bad_result.error << "\n";

    // --- Test 6: Export Mapping Report ---
    std::cout << "\n--- Report Export ---\n";
    bool wrote = write_mapping_report("cg_mapping_report.md",
                                       state, scheme, result.system, result.conservation);
    check(wrote, "Mapping report written to cg_mapping_report.md");

    // Print report to stdout too
    std::string report = AtomToBeadMapper::mapping_report(state, scheme, result.system, result.conservation);
    std::cout << "\n" << report << "\n";

    // --- Summary ---
    std::cout << "══════════════════════════════════════════════════\n";
    std::cout << "  Results: " << pass << " passed, " << fail << " failed\n";
    std::cout << "══════════════════════════════════════════════════\n";

    return (fail == 0) ? 0 : 1;
}
