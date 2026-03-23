/**
 * test_fragment_bridge.cpp — Tests for Atomistic Preparation Layer & Scale Bridge
 *
 * Validates the atomistic→CG scale boundary:
 *   1. FragmentView construction from State
 *   2. Empty fragment → EmptyFragment status
 *   3. Single atom fragment → Valid, no frame
 *   4. Two-atom fragment → Valid, frame defined
 *   5. Mass and charge aggregation
 *   6. Bond extraction from state edge list
 *   7. Duplicate position detection
 *   8. Out-of-bounds index detection
 *   9. Metal center detection (transition metals)
 *  10. Heteroatom and hydrogen flags
 *  11. Cyclic fragment detection
 *  12. Bridge: FragmentView → Bead
 *  13. Bridge: FragmentView → UnifiedDescriptor structure
 *  14. Bridge: complete fragment_to_bead pipeline
 *  15. Bridge: invalid fragment produces error
 *  16. Benzene canonical case (12 atoms, planar, cyclic)
 *  17. FragmentStatus names are populated
 *
 * Validation sweep groups discussed for unified descriptor development:
 *
 *   Group A — Baseline geometry / boundary cases
 *     - Benzene      : canonical planar aromatic reference
 *     - Water        : small polar boundary case, non-ideal single-bead target
 *     - Naphthalene  : extended planar anisotropy / fused aromatic test
 *
 *   Group B — Chemical asymmetry on aromatic cores
 *     - Pyridine     : heteroatom-substituted aromatic ring
 *     - Phenol       : aromatic + polar OH functionality
 *
 *   Group C — Steric / substituted aromatic complexity
 *     - Cresol       : phenol + methyl substitution
 *     - BHT          : bulky sterically hindered substituted aromatic
 *
 * Current scope in this file:
 *   - Group A and Group B are appropriate for direct fragment/bridge tests.
 *   - Group C should be included as escalation / descriptor-complexity tests
 *     once larger substituted templates are available in fixtures.
 *
 * Anti-black-box: every test prints category and result explicitly.
 */

#include "atomistic/core/fragment_view.hpp"
#include "coarse_grain/mapping/fragment_bridge.hpp"
#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/unified_descriptor.hpp"
#include "atomistic/core/state.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* msg) {
    if (cond) {
        std::printf("  [PASS] %s\n", msg);
        ++g_pass;
    } else {
        std::printf("  [FAIL] %s\n", msg);
        ++g_fail;
    }
}

// ============================================================================
// Validation Groups for descriptor / bridge development
// ----------------------------------------------------------------------------
// Group A:
//   benzene, water, naphthalene
//
// Group B:
//   pyridine, phenol
//
// Group C:
//   cresol, BHT
//
// These are not all implemented as full chemistry fixtures yet, but the
// grouping is preserved here so bridge tests and future descriptor tests stay
// aligned with the modeling roadmap.
// ============================================================================

// ============================================================================
// Helper: create a simple atomistic::State
// ============================================================================
static atomistic::State make_simple_state(int n_atoms) {
    atomistic::State s;
    s.N = n_atoms;
    s.X.resize(n_atoms);
    s.V.resize(n_atoms, {0, 0, 0});
    s.Q.resize(n_atoms, 0.0);
    s.M.resize(n_atoms, 12.0);  // Carbon mass
    s.type.resize(n_atoms, 6);  // Carbon
    s.F.resize(n_atoms, {0, 0, 0});
    return s;
}

// Helper: create a benzene-like State (12 atoms: 6 C + 6 H)
static atomistic::State make_benzene_state() {
    atomistic::State s = make_simple_state(12);
    constexpr double PI = 3.14159265358979323846;

    // Ring carbons in XY plane
    for (int i = 0; i < 6; ++i) {
        double angle = 2.0 * PI * i / 6.0;
        s.X[i] = {1.4 * std::cos(angle), 1.4 * std::sin(angle), 0.0};
        s.M[i] = 12.011;
        s.type[i] = 6;  // Carbon
        s.Q[i] = -0.115;
    }
    // Hydrogen atoms
    for (int i = 0; i < 6; ++i) {
        double angle = 2.0 * PI * i / 6.0;
        s.X[6 + i] = {2.48 * std::cos(angle), 2.48 * std::sin(angle), 0.0};
        s.M[6 + i] = 1.008;
        s.type[6 + i] = 1;  // Hydrogen
        s.Q[6 + i] = 0.115;
    }

    // C-C bonds
    for (int i = 0; i < 6; ++i) {
        s.B.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>((i + 1) % 6)});
    }
    // C-H bonds
    for (int i = 0; i < 6; ++i) {
        s.B.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>(6 + i)});
    }

    return s;
}

// Helper: create a water State (3 atoms: 1 O + 2 H)
static atomistic::State make_water_state() {
    atomistic::State s = make_simple_state(3);

    // Bent geometry, approximate
    s.M[0] = 15.999; s.type[0] = 8; s.Q[0] = -0.84;   // O
    s.M[1] = 1.008;  s.type[1] = 1; s.Q[1] =  0.42;   // H
    s.M[2] = 1.008;  s.type[2] = 1; s.Q[2] =  0.42;   // H

    s.X[0] = {0.0000, 0.0000, 0.0000};
    s.X[1] = {0.9572, 0.0000, 0.0000};
    s.X[2] = {-0.2390, 0.9270, 0.0000};

    s.B.push_back({0, 1});
    s.B.push_back({0, 2});

    return s;
}

// Helper: create a naphthalene-like State (18 atoms: 10 C + 8 H)
static atomistic::State make_naphthalene_state() {
    atomistic::State s = make_simple_state(18);

    // Approximate planar fused-ring layout for bridge testing
    // 10 carbons + 8 hydrogens
    for (int i = 0; i < 10; ++i) {
        s.M[i] = 12.011;
        s.type[i] = 6;
        s.Q[i] = -0.08;
    }
    for (int i = 10; i < 18; ++i) {
        s.M[i] = 1.008;
        s.type[i] = 1;
        s.Q[i] = 0.10;
    }

    // Rough 2-ring fused aromatic skeleton in XY plane (corrected: no duplicate coords)
    s.X[0] = {-2.4,  0.0, 0.0};
    s.X[1] = {-1.2,  0.7, 0.0};
    s.X[2] = { 0.0,  0.7, 0.0};
    s.X[3] = { 1.2,  0.7, 0.0};
    s.X[4] = { 2.4,  0.0, 0.0};
    s.X[5] = { 1.2, -0.7, 0.0};
    s.X[6] = { 0.0, -0.7, 0.0};
    s.X[7] = {-1.2, -0.7, 0.0};
    s.X[8] = {-0.6,  1.8, 0.0};
    s.X[9] = { 0.6,  1.8, 0.0};

    s.X[10] = {-3.3,  0.0, 0.0};
    s.X[11] = {-1.7,  1.6, 0.0};
    s.X[12] = { 1.7,  1.6, 0.0};
    s.X[13] = { 3.3,  0.0, 0.0};
    s.X[14] = { 1.7, -1.6, 0.0};
    s.X[15] = {-1.7, -1.6, 0.0};
    s.X[16] = {-0.6,  2.8, 0.0};
    s.X[17] = { 0.6,  2.8, 0.0};

    // Minimal internal fused-ring bond graph for cyclic detection
    s.B.push_back({0, 1}); s.B.push_back({1, 2}); s.B.push_back({2, 3});
    s.B.push_back({3, 4}); s.B.push_back({4, 5}); s.B.push_back({5, 6});
    s.B.push_back({6, 7}); s.B.push_back({7, 0}); s.B.push_back({2, 9});
    s.B.push_back({9, 8}); s.B.push_back({8, 1});

    return s;
}

// Helper: create a pyridine-like State (benzene with one N substitution)
static atomistic::State make_pyridine_state() {
    atomistic::State s = make_benzene_state();

    // Replace one ring carbon with nitrogen
    s.type[0] = 7;     // N
    s.M[0] = 14.007;
    s.Q[0] = -0.30;

    return s;
}

// Helper: create a phenol-like State (benzene + OH substituent, 13 atoms)
static atomistic::State make_phenol_state() {
    atomistic::State s = make_benzene_state();

    // Extend to 13 atoms: add hydroxyl H proxy
    s.N = 13;
    s.X.push_back({3.2, 0.0, 0.0});   // hydroxyl H proxy
    s.V.push_back({0, 0, 0});
    s.Q.push_back(0.42);
    s.M.push_back(1.008);
    s.type.push_back(1);
    s.F.push_back({0, 0, 0});

    // Turn H at index 6 into O
    s.M[6] = 15.999;
    s.type[6] = 8;
    s.Q[6] = -0.42;
    s.X[6] = {2.4, 0.0, 0.0};

    s.B.push_back({6, 12}); // O-H bond
    return s;
}

// ============================================================================
// 1. FragmentView construction
// ============================================================================
static void test_fragment_construction() {
    std::printf("\n--- 1. FragmentView construction ---\n");

    auto s = make_simple_state(5);
    s.X[0] = {0, 0, 0};
    s.X[1] = {1, 0, 0};
    s.X[2] = {0, 1, 0};
    s.X[3] = {1, 1, 0};
    s.X[4] = {0.5, 0.5, 1};

    std::vector<uint32_t> indices = {0, 1, 2, 3, 4};
    auto frag = atomistic::build_fragment_view(s, indices);

    check(frag.is_valid(), "fragment is valid");
    check(frag.num_atoms() == 5, "5 atoms in fragment");
    check(frag.frame.well_defined, "frame is well-defined");
    check(std::abs(frag.total_mass - 60.0) < 1e-10, "total mass = 60 amu");
}

// ============================================================================
// 2. Empty fragment
// ============================================================================
static void test_empty_fragment() {
    std::printf("\n--- 2. Empty fragment ---\n");

    auto s = make_simple_state(3);
    std::vector<uint32_t> empty;
    auto frag = atomistic::build_fragment_view(s, empty);

    check(!frag.is_valid(), "empty fragment is not valid");
    check(frag.status == atomistic::FragmentStatus::EmptyFragment, "status = EmptyFragment");
}

// ============================================================================
// 3. Single atom fragment
// ============================================================================
static void test_single_atom() {
    std::printf("\n--- 3. Single atom fragment ---\n");

    auto s = make_simple_state(1);
    s.X[0] = {1.5, 2.5, 3.5};

    std::vector<uint32_t> indices = {0};
    auto frag = atomistic::build_fragment_view(s, indices);

    check(frag.is_valid(), "single atom fragment is valid");
    check(frag.num_atoms() == 1, "1 atom");
    check(!frag.frame.well_defined, "frame not well-defined for 1 atom");
    check(std::abs(frag.frame.origin.x - 1.5) < 1e-10, "frame origin at atom position");
}

// ============================================================================
// 4. Two-atom fragment
// ============================================================================
static void test_two_atom() {
    std::printf("\n--- 4. Two-atom fragment ---\n");

    auto s = make_simple_state(2);
    s.X[0] = {0, 0, 0};
    s.X[1] = {2, 0, 0};

    std::vector<uint32_t> indices = {0, 1};
    auto frag = atomistic::build_fragment_view(s, indices);

    check(frag.is_valid(), "two-atom fragment is valid");
    check(frag.frame.well_defined, "frame is well-defined for 2 atoms");
    check(std::abs(frag.frame.origin.x - 1.0) < 1e-10, "COM at midpoint");
}

// ============================================================================
// 5. Mass and charge aggregation
// ============================================================================
static void test_aggregation() {
    std::printf("\n--- 5. Mass and charge aggregation ---\n");

    auto s = make_simple_state(3);
    s.M[0] = 12.0; s.M[1] = 14.0; s.M[2] = 16.0;
    s.Q[0] = 0.5;  s.Q[1] = -0.3; s.Q[2] = 0.1;
    s.X[0] = {0, 0, 0}; s.X[1] = {1, 0, 0}; s.X[2] = {0, 1, 0};

    std::vector<uint32_t> indices = {0, 1, 2};
    auto frag = atomistic::build_fragment_view(s, indices);

    check(std::abs(frag.total_mass - 42.0) < 1e-10, "total mass = 42 amu");
    check(std::abs(frag.total_charge - 0.3) < 1e-10, "total charge = 0.3 e");
}

// ============================================================================
// 6. Bond extraction
// ============================================================================
static void test_bond_extraction() {
    std::printf("\n--- 6. Bond extraction ---\n");

    auto s = make_simple_state(4);
    s.X[0] = {0, 0, 0}; s.X[1] = {1, 0, 0};
    s.X[2] = {2, 0, 0}; s.X[3] = {3, 0, 0};
    s.B.push_back({0, 1});
    s.B.push_back({1, 2});
    s.B.push_back({2, 3});

    // Fragment includes atoms 0, 1, 2 but not 3
    std::vector<uint32_t> indices = {0, 1, 2};
    auto frag = atomistic::build_fragment_view(s, indices);

    check(frag.num_bonds() == 2, "2 internal bonds extracted");
    check(frag.bonds[0].i == 0 && frag.bonds[0].j == 1, "bond 0-1 correct");
    check(frag.bonds[1].i == 1 && frag.bonds[1].j == 2, "bond 1-2 correct");
}

// ============================================================================
// 7. Duplicate position detection
// ============================================================================
static void test_duplicate_positions() {
    std::printf("\n--- 7. Duplicate position detection ---\n");

    auto s = make_simple_state(3);
    s.X[0] = {1.0, 2.0, 3.0};
    s.X[1] = {1.0, 2.0, 3.0};  // Duplicate!
    s.X[2] = {4.0, 5.0, 6.0};

    std::vector<uint32_t> indices = {0, 1, 2};
    auto frag = atomistic::build_fragment_view(s, indices);

    check(!frag.is_valid(), "duplicate positions detected");
    check(frag.status == atomistic::FragmentStatus::DuplicatePositions, "status = DuplicatePositions");
}

// ============================================================================
// 8. Out-of-bounds index
// ============================================================================
static void test_out_of_bounds() {
    std::printf("\n--- 8. Out-of-bounds index ---\n");

    auto s = make_simple_state(3);
    s.X[0] = {0, 0, 0}; s.X[1] = {1, 0, 0}; s.X[2] = {0, 1, 0};

    std::vector<uint32_t> indices = {0, 1, 99};  // 99 is out of bounds
    auto frag = atomistic::build_fragment_view(s, indices);

    check(!frag.is_valid(), "out-of-bounds index detected");
    check(frag.status == atomistic::FragmentStatus::InvalidGeometry, "status = InvalidGeometry");
}

// ============================================================================
// 9. Metal center detection
// ============================================================================
static void test_metal_detection() {
    std::printf("\n--- 9. Metal center detection ---\n");

    auto s = make_simple_state(3);
    s.X[0] = {0, 0, 0}; s.X[1] = {2, 0, 0}; s.X[2] = {0, 2, 0};
    s.type[0] = 6;   // Carbon
    s.type[1] = 26;  // Iron (transition metal)
    s.type[2] = 8;   // Oxygen

    std::vector<uint32_t> indices = {0, 1, 2};
    auto frag = atomistic::build_fragment_view(s, indices);

    check(frag.is_valid(), "fragment with metal is valid");
    check(frag.has_metal_center(), "metal center detected");
    check(frag.organometallic, "organometallic flag set");
    check(frag.atoms[1].is_metal(), "iron atom flagged as metal");
    check(!frag.atoms[0].is_metal(), "carbon not flagged as metal");
}

// ============================================================================
// 10. Heteroatom and hydrogen flags
// ============================================================================
static void test_atom_flags() {
    std::printf("\n--- 10. Heteroatom and hydrogen flags ---\n");

    auto s = make_simple_state(4);
    s.X[0] = {0, 0, 0}; s.X[1] = {1, 0, 0};
    s.X[2] = {0, 1, 0}; s.X[3] = {1, 1, 0};
    s.type[0] = 1;  // H
    s.type[1] = 6;  // C
    s.type[2] = 7;  // N
    s.type[3] = 8;  // O

    std::vector<uint32_t> indices = {0, 1, 2, 3};
    auto frag = atomistic::build_fragment_view(s, indices);

    check(frag.atoms[0].is_hydrogen(), "H flagged as hydrogen");
    check(!frag.atoms[1].is_hydrogen(), "C not flagged as hydrogen");
    check((frag.atoms[2].flags & atomistic::AtomRecord::FLAG_HETEROATOM) != 0,
          "N flagged as heteroatom");
    check((frag.atoms[3].flags & atomistic::AtomRecord::FLAG_HETEROATOM) != 0,
          "O flagged as heteroatom");
}

// ============================================================================
// 11. Cyclic fragment detection
// ============================================================================
static void test_cyclic_detection() {
    std::printf("\n--- 11. Cyclic fragment detection ---\n");

    // Linear chain: 3 atoms, 2 bonds → not cyclic
    auto s1 = make_simple_state(3);
    s1.X[0] = {0, 0, 0}; s1.X[1] = {1, 0, 0}; s1.X[2] = {2, 0, 0};
    s1.B.push_back({0, 1});
    s1.B.push_back({1, 2});

    auto frag1 = atomistic::build_fragment_view(s1, {0, 1, 2});
    check(!frag1.cyclic, "linear chain is not cyclic");

    // Triangle: 3 atoms, 3 bonds → cyclic
    auto s2 = make_simple_state(3);
    s2.X[0] = {0, 0, 0}; s2.X[1] = {1, 0, 0}; s2.X[2] = {0.5, 0.866, 0};
    s2.B.push_back({0, 1});
    s2.B.push_back({1, 2});
    s2.B.push_back({2, 0});

    auto frag2 = atomistic::build_fragment_view(s2, {0, 1, 2});
    check(frag2.cyclic, "triangle is cyclic");
}

// ============================================================================
// 12. Bridge: FragmentView → Bead
// ============================================================================
static void test_bridge_to_bead() {
    std::printf("\n--- 12. Bridge: FragmentView → Bead ---\n");

    auto s = make_simple_state(3);
    s.X[0] = {0, 0, 0}; s.X[1] = {3, 0, 0}; s.X[2] = {0, 3, 0};
    s.M[0] = 12.0; s.M[1] = 12.0; s.M[2] = 12.0;
    s.Q[0] = 0.1; s.Q[1] = 0.2; s.Q[2] = -0.3;

    std::vector<uint32_t> indices = {0, 1, 2};
    auto frag = atomistic::build_fragment_view(s, indices);
    auto bead = coarse_grain::build_bead(frag, 42);

    check(std::abs(bead.mass - 36.0) < 1e-10, "bead mass = 36 amu");
    check(std::abs(bead.charge - 0.0) < 1e-10, "bead charge = 0 e");
    check(bead.type_id == 42, "bead type_id = 42");
    check(bead.parent_atom_indices.size() == 3, "3 parent atom indices");
    check(bead.parent_atom_indices[0] == 0, "parent[0] = 0");
    check(bead.parent_atom_indices[1] == 1, "parent[1] = 1");
    check(bead.parent_atom_indices[2] == 2, "parent[2] = 2");
    check(bead.has_orientation, "orientation computed");
}

// ============================================================================
// 13. Bridge: FragmentView → UnifiedDescriptor structure
// ============================================================================
static void test_bridge_to_descriptor() {
    std::printf("\n--- 13. Bridge: FragmentView → UnifiedDescriptor ---\n");

    // Small fragment: should get low l_max
    auto s = make_simple_state(4);
    s.X[0] = {0, 0, 0}; s.X[1] = {1, 0, 0};
    s.X[2] = {0, 1, 0}; s.X[3] = {1, 1, 0};

    auto frag = atomistic::build_fragment_view(s, {0, 1, 2, 3});
    auto ud = coarse_grain::build_descriptor_structure(frag);

    check(ud.steric.active, "steric channel active");
    check(ud.steric.l_max == 2, "small fragment gets l_max=2");
    check(ud.frame.valid, "frame transferred from fragment");
}

// ============================================================================
// 14. Bridge: complete pipeline
// ============================================================================
static void test_bridge_complete() {
    std::printf("\n--- 14. Bridge: complete pipeline ---\n");

    auto s = make_simple_state(5);
    s.X[0] = {0, 0, 0}; s.X[1] = {1, 0, 0};
    s.X[2] = {0, 1, 0}; s.X[3] = {1, 1, 0};
    s.X[4] = {0.5, 0.5, 1};

    auto frag = atomistic::build_fragment_view(s, {0, 1, 2, 3, 4});
    auto bb = coarse_grain::bridge_fragment_to_bead(frag, 7, 1);

    check(bb.result.ok, "bridge succeeded");
    check(bb.bead.type_id == 7, "bead type_id = 7");
    check(bb.bead.has_unified_data(), "bead has unified descriptor");
    check(bb.bead.unified->steric.active, "steric channel active");
}

// ============================================================================
// 15. Bridge: invalid fragment produces error
// ============================================================================
static void test_bridge_invalid() {
    std::printf("\n--- 15. Bridge: invalid fragment produces error ---\n");

    auto s = make_simple_state(1);
    std::vector<uint32_t> empty;
    auto frag = atomistic::build_fragment_view(s, empty);
    auto bb = coarse_grain::bridge_fragment_to_bead(frag);

    check(!bb.result.ok, "bridge fails for invalid fragment");
    check(bb.result.fragment_status == atomistic::FragmentStatus::EmptyFragment,
          "status = EmptyFragment");
    check(!bb.result.error.empty(), "error message populated");
}

// ============================================================================
// 16. Benzene canonical case
// ============================================================================
static void test_benzene_fragment() {
    std::printf("\n--- 16. Benzene canonical case ---\n");

    auto s = make_benzene_state();
    std::vector<uint32_t> all_indices;
    for (uint32_t i = 0; i < s.N; ++i) all_indices.push_back(i);

    auto frag = atomistic::build_fragment_view(s, all_indices);

    check(frag.is_valid(), "benzene fragment is valid");
    check(frag.num_atoms() == 12, "12 atoms");
    check(frag.cyclic, "benzene is cyclic");
    check(frag.frame.well_defined, "frame well-defined");
    check(!frag.has_metal_center(), "no metal center");

    // COM should be near origin (symmetric structure)
    auto com = frag.center_of_mass();
    check(std::abs(com.x) < 0.1 && std::abs(com.y) < 0.1 && std::abs(com.z) < 0.01,
          "COM near origin for symmetric benzene");

    // Total charge should be ~0 (C and H charges cancel)
    check(std::abs(frag.total_charge) < 0.01, "total charge ~0 for benzene");

    // Bridge to bead
    auto bb = coarse_grain::bridge_fragment_to_bead(frag, 0, 1);
    check(bb.result.ok, "bridge to bead succeeded");
    check(bb.bead.has_unified_data(), "bead has unified descriptor");

    // Benzene (12 atoms, 7-17 range) should get l_max=4
    check(bb.bead.unified->steric.l_max == 4, "benzene gets l_max=4 (moderate)");
}

// ============================================================================
// 17. FragmentStatus names
// ============================================================================
static void test_status_names() {
    std::printf("\n--- 17. FragmentStatus names ---\n");

    check(std::string(atomistic::fragment_status_name(atomistic::FragmentStatus::Valid)) ==
          "Valid", "Valid name");
    check(std::string(atomistic::fragment_status_name(atomistic::FragmentStatus::EmptyFragment)) ==
          "Empty fragment (no atoms provided)", "EmptyFragment name");
    check(std::string(atomistic::fragment_status_name(atomistic::FragmentStatus::DuplicatePositions)) ==
          "Duplicate positions (atoms at same location)", "DuplicatePositions name");
}

// ============================================================================
// Group A. Baseline geometry / boundary cases
// ============================================================================
static void test_group_a_baselines() {
    std::printf("\n--- Group A. Baseline geometry / boundary cases ---\n");

    // A1: Benzene
    {
        auto s = make_benzene_state();
        std::vector<uint32_t> idx;
        for (uint32_t i = 0; i < s.N; ++i) idx.push_back(i);

        auto frag = atomistic::build_fragment_view(s, idx);
        auto bb = coarse_grain::bridge_fragment_to_bead(frag, 100, 1);

        check(frag.is_valid(), "Group A / benzene: fragment valid");
        check(frag.cyclic, "Group A / benzene: cyclic");
        check(!frag.has_metal_center(), "Group A / benzene: no metal");
        check(bb.result.ok, "Group A / benzene: bridge succeeds");
        check(bb.bead.has_unified_data(), "Group A / benzene: unified descriptor attached");
    }

    // A2: Water
    {
        auto s = make_water_state();
        auto frag = atomistic::build_fragment_view(s, {0, 1, 2});
        auto bb = coarse_grain::bridge_fragment_to_bead(frag, 101, 1);

        check(frag.is_valid(), "Group A / water: fragment valid");
        check(!frag.cyclic, "Group A / water: not cyclic");
        check(!frag.has_metal_center(), "Group A / water: no metal");
        check(bb.result.ok, "Group A / water: bridge succeeds");
        check(bb.bead.has_unified_data(), "Group A / water: unified descriptor attached");
    }

    // A3: Naphthalene
    {
        auto s = make_naphthalene_state();
        std::vector<uint32_t> idx;
        for (uint32_t i = 0; i < s.N; ++i) idx.push_back(i);

        auto frag = atomistic::build_fragment_view(s, idx);
        auto bb = coarse_grain::bridge_fragment_to_bead(frag, 102, 1);

        check(frag.is_valid(), "Group A / naphthalene: fragment valid");
        check(frag.num_atoms() == 18, "Group A / naphthalene: 18 atoms");
        check(frag.cyclic, "Group A / naphthalene: cyclic");
        check(bb.result.ok, "Group A / naphthalene: bridge succeeds");
        check(bb.bead.has_unified_data(), "Group A / naphthalene: unified descriptor attached");
        check(bb.bead.unified->steric.l_max >= 4,
              "Group A / naphthalene: descriptor complexity promoted appropriately");
    }
}

// ============================================================================
// Group B. Chemical asymmetry on aromatic cores
// ============================================================================
static void test_group_b_chemical_asymmetry() {
    std::printf("\n--- Group B. Chemical asymmetry on aromatic cores ---\n");

    // B1: Pyridine
    {
        auto s = make_pyridine_state();
        std::vector<uint32_t> idx;
        for (uint32_t i = 0; i < s.N; ++i) idx.push_back(i);

        auto frag = atomistic::build_fragment_view(s, idx);
        auto bb = coarse_grain::bridge_fragment_to_bead(frag, 110, 1);

        check(frag.is_valid(), "Group B / pyridine: fragment valid");
        check(frag.cyclic, "Group B / pyridine: cyclic");
        check(!frag.has_metal_center(), "Group B / pyridine: no metal");
        check(bb.result.ok, "Group B / pyridine: bridge succeeds");
        check(bb.bead.has_unified_data(), "Group B / pyridine: unified descriptor attached");
    }

    // B2: Phenol
    {
        auto s = make_phenol_state();
        std::vector<uint32_t> idx;
        for (uint32_t i = 0; i < s.N; ++i) idx.push_back(i);

        auto frag = atomistic::build_fragment_view(s, idx);
        auto bb = coarse_grain::bridge_fragment_to_bead(frag, 111, 1);

        check(frag.is_valid(), "Group B / phenol: fragment valid");
        check(frag.cyclic, "Group B / phenol: aromatic core remains cyclic");
        check(!frag.has_metal_center(), "Group B / phenol: no metal");
        check(bb.result.ok, "Group B / phenol: bridge succeeds");
        check(bb.bead.has_unified_data(), "Group B / phenol: unified descriptor attached");
    }
}

// ============================================================================
// Group C. Steric / substituted aromatic complexity (placeholder)
// ============================================================================
static void test_group_c_placeholder() {
    std::printf("\n--- Group C. Steric / substituted aromatic complexity ---\n");
    std::printf("  [INFO] Cresol and BHT fixtures not yet implemented in this bridge test file.\n");
    std::printf("  [INFO] Reserved for descriptor escalation / steric complexity cases.\n");
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::printf("=== Atomistic Preparation Layer & Scale Bridge Tests ===\n");

    test_fragment_construction();
    test_empty_fragment();
    test_single_atom();
    test_two_atom();
    test_aggregation();
    test_bond_extraction();
    test_duplicate_positions();
    test_out_of_bounds();
    test_metal_detection();
    test_atom_flags();
    test_cyclic_detection();
    test_bridge_to_bead();
    test_bridge_to_descriptor();
    test_bridge_complete();
    test_bridge_invalid();
    test_benzene_fragment();
    test_status_names();

    test_group_a_baselines();
    test_group_b_chemical_asymmetry();
    test_group_c_placeholder();

    std::printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
