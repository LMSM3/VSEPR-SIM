/**
 * test_hash_invariance.cpp — Hash Invariance & Provenance Audit Test Suite
 * =========================================================================
 *
 * Tests the 5 invariance properties that a real provenance layer requires:
 *
 *   Test 1: Topology invariance under coordinate perturbation
 *   Test 2: Geometry invariance under rigid transforms (translate/rotate)
 *   Test 3: Atom-order permutation invariance
 *   Test 4: Near-degenerate geometry sensitivity
 *   Test 5: Scale test (collision detection over many molecules)
 *   Test 6: 3-Tier provenance record correctness
 *
 * Build:
 *   cmake --build build --target test-hash-invariance
 *
 * This suite answers: is the provenance chain useful but brittle,
 * or is it actually robust?
 */

#include "sim/molecule.hpp"
#include "identity/canonical_identity.hpp"
#include "identity/provenance_record.hpp"
#include "core/types.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <random>
#include <functional>
#include <sstream>
#include <algorithm>

// ============================================================================
// Test infrastructure
// ============================================================================

static int g_pass = 0, g_fail = 0, g_total = 0;

static void check(bool ok, const std::string& name) {
    ++g_total;
    if (ok) {
        std::printf("  \033[92m[PASS]\033[0m %s\n", name.c_str());
        ++g_pass;
    } else {
        std::printf("  \033[91m[FAIL]\033[0m %s\n", name.c_str());
        ++g_fail;
    }
}

static const std::map<uint8_t, std::string> Z_MAP = {
    {1, "H"}, {6, "C"}, {7, "N"}, {8, "O"}, {9, "F"},
    {14, "Si"}, {15, "P"}, {16, "S"}, {17, "Cl"}, {18, "Ar"},
    {26, "Fe"}, {29, "Cu"}, {35, "Br"}, {53, "I"}, {92, "U"}
};

static std::string z_sym(uint8_t Z) {
    auto it = Z_MAP.find(Z);
    return (it != Z_MAP.end()) ? it->second : "?";
}

// ============================================================================
// Molecule builders
// ============================================================================

static vsepr::Molecule make_h2o() {
    vsepr::Molecule mol;
    mol.add_atom(8, 0.0, 0.0, 0.0);
    mol.add_atom(1, 0.7572, 0.5865, 0.0);
    mol.add_atom(1, -0.7572, 0.5865, 0.0);
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    return mol;
}

static vsepr::Molecule make_ch4() {
    vsepr::Molecule mol;
    mol.add_atom(6, 0.0, 0.0, 0.0);
    mol.add_atom(1,  0.6297,  0.6297,  0.6297);
    mol.add_atom(1, -0.6297, -0.6297,  0.6297);
    mol.add_atom(1, -0.6297,  0.6297, -0.6297);
    mol.add_atom(1,  0.6297, -0.6297, -0.6297);
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    mol.add_bond(0, 4, 1);
    return mol;
}

static vsepr::Molecule make_nh3() {
    vsepr::Molecule mol;
    mol.add_atom(7, 0.0, 0.0, 0.0);
    mol.add_atom(1,  0.9377, -0.3816, 0.0);
    mol.add_atom(1, -0.4689, -0.3816,  0.8121);
    mol.add_atom(1, -0.4689, -0.3816, -0.8121);
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    return mol;
}

static vsepr::Molecule make_sf6() {
    vsepr::Molecule mol;
    double d = 1.56;
    mol.add_atom(16, 0.0, 0.0, 0.0);
    mol.add_atom(9,  d,   0.0, 0.0);
    mol.add_atom(9, -d,   0.0, 0.0);
    mol.add_atom(9,  0.0,  d,  0.0);
    mol.add_atom(9,  0.0, -d,  0.0);
    mol.add_atom(9,  0.0, 0.0,  d);
    mol.add_atom(9,  0.0, 0.0, -d);
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    mol.add_bond(0, 4, 1);
    mol.add_bond(0, 5, 1);
    mol.add_bond(0, 6, 1);
    return mol;
}

// ============================================================================
// Utility: perturb coordinates by small random displacement
// ============================================================================

static vsepr::Molecule perturb_coords(const vsepr::Molecule& mol, double magnitude, uint32_t seed) {
    vsepr::Molecule result = mol;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-magnitude, magnitude);
    for (size_t i = 0; i < result.coords.size(); ++i) {
        result.coords[i] += dist(rng);
    }
    return result;
}

// ============================================================================
// Utility: translate all coordinates
// ============================================================================

static vsepr::Molecule translate(const vsepr::Molecule& mol, double tx, double ty, double tz) {
    vsepr::Molecule result = mol;
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        result.coords[3 * i]     += tx;
        result.coords[3 * i + 1] += ty;
        result.coords[3 * i + 2] += tz;
    }
    return result;
}

// ============================================================================
// Utility: rotate around Z axis by angle (radians)
// ============================================================================

static vsepr::Molecule rotate_z(const vsepr::Molecule& mol, double angle) {
    vsepr::Molecule result = mol;
    double c = std::cos(angle);
    double s = std::sin(angle);
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        double x = mol.coords[3 * i];
        double y = mol.coords[3 * i + 1];
        result.coords[3 * i]     = c * x - s * y;
        result.coords[3 * i + 1] = s * x + c * y;
    }
    return result;
}

// ============================================================================
// Utility: rotate around arbitrary axis (Rodrigues)
// ============================================================================

static vsepr::Molecule rotate_axis(const vsepr::Molecule& mol,
                                    double kx, double ky, double kz,
                                    double angle) {
    double nm = std::sqrt(kx * kx + ky * ky + kz * kz);
    kx /= nm; ky /= nm; kz /= nm;
    double c = std::cos(angle);
    double s = std::sin(angle);

    vsepr::Molecule result = mol;
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        double vx = mol.coords[3 * i];
        double vy = mol.coords[3 * i + 1];
        double vz = mol.coords[3 * i + 2];

        double dot = kx * vx + ky * vy + kz * vz;
        // Full Rodrigues:
        result.coords[3 * i]     = vx * c + (ky * vz - kz * vy) * s + kx * dot * (1 - c);
        result.coords[3 * i + 1] = vy * c + (kz * vx - kx * vz) * s + ky * dot * (1 - c);
        result.coords[3 * i + 2] = vz * c + (kx * vy - ky * vx) * s + kz * dot * (1 - c);
    }
    return result;
}

// ============================================================================
// Utility: reflect through XY plane (z → -z)
// ============================================================================

static vsepr::Molecule reflect_z(const vsepr::Molecule& mol) {
    vsepr::Molecule result = mol;
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        result.coords[3 * i + 2] = -mol.coords[3 * i + 2];
    }
    return result;
}

// ============================================================================
// Utility: permute atom ordering
// ============================================================================

static vsepr::Molecule permute_atoms(const vsepr::Molecule& mol,
                                      const std::vector<uint32_t>& perm) {
    vsepr::Molecule result;

    // Inverse permutation for bond remapping
    std::vector<uint32_t> inv(perm.size());
    for (size_t i = 0; i < perm.size(); ++i) {
        inv[perm[i]] = static_cast<uint32_t>(i);
    }

    // Add atoms in permuted order
    for (size_t i = 0; i < perm.size(); ++i) {
        uint32_t old_idx = perm[i];
        result.add_atom(mol.atoms[old_idx].Z,
                        mol.coords[3 * old_idx],
                        mol.coords[3 * old_idx + 1],
                        mol.coords[3 * old_idx + 2]);
    }

    // Remap bonds
    for (const auto& b : mol.bonds) {
        result.add_bond(inv[b.i], inv[b.j], b.order);
    }

    return result;
}

// ============================================================================
// TEST 1: Topology invariance under coordinate perturbation
// ============================================================================

static void test_topo_invariance_under_perturbation() {
    std::cout << "\n\033[1m  ═══ Test 1: Topology Invariance Under Coordinate Perturbation ═══\033[0m\n\n";

    struct Case {
        std::string name;
        std::function<vsepr::Molecule()> build;
    };

    std::vector<Case> cases = {
        {"H2O",  make_h2o},
        {"CH4",  make_ch4},
        {"NH3",  make_nh3},
        {"SF6",  make_sf6},
    };

    double perturbations[] = {0.001, 0.01, 0.1, 0.5};

    for (const auto& c : cases) {
        auto mol = c.build();
        auto id_orig = vsepr::identity::compute_identity(mol, z_sym);

        for (double pert : perturbations) {
            auto mol_pert = perturb_coords(mol, pert, 42);
            auto id_pert = vsepr::identity::compute_identity(mol_pert, z_sym);

            // Topo MUST remain unchanged
            check(id_orig.topology_hash == id_pert.topology_hash,
                  c.name + " topo unchanged after " + std::to_string(pert) + " A perturbation");

            // Geom SHOULD change if perturbation is above tolerance (0.01 A)
            if (pert > 0.02) {
                check(id_orig.geometry_hash != id_pert.geometry_hash,
                      c.name + " geom changes after " + std::to_string(pert) + " A perturbation");
            }
        }
    }
}

// ============================================================================
// TEST 2: Geometry invariance under rigid transforms
// ============================================================================

static void test_geom_invariance_under_rigid_transforms() {
    std::cout << "\n\033[1m  ═══ Test 2: Geometry Invariance Under Rigid Transforms ═══\033[0m\n\n";

    struct Case {
        std::string name;
        std::function<vsepr::Molecule()> build;
    };

    std::vector<Case> cases = {
        {"H2O",  make_h2o},
        {"CH4",  make_ch4},
        {"NH3",  make_nh3},
        {"SF6",  make_sf6},
    };

    for (const auto& c : cases) {
        auto mol = c.build();
        auto id_orig = vsepr::identity::compute_identity(mol, z_sym);

        // Translation
        auto mol_t = translate(mol, 100.0, -50.0, 37.5);
        auto id_t = vsepr::identity::compute_identity(mol_t, z_sym);
        check(id_orig.geometry_hash == id_t.geometry_hash,
              c.name + " geom invariant under translation (100, -50, 37.5)");
        check(id_orig.topology_hash == id_t.topology_hash,
              c.name + " topo invariant under translation");

        // Rotation around Z (45°)
        auto mol_rz = rotate_z(mol, 0.785398);
        auto id_rz = vsepr::identity::compute_identity(mol_rz, z_sym);
        check(id_orig.geometry_hash == id_rz.geometry_hash,
              c.name + " geom invariant under Z-rotation (45 deg)");

        // Rotation around arbitrary axis (73° around [1,1,1])
        auto mol_ra = rotate_axis(mol, 1.0, 1.0, 1.0, 1.274);
        auto id_ra = vsepr::identity::compute_identity(mol_ra, z_sym);
        check(id_orig.geometry_hash == id_ra.geometry_hash,
              c.name + " geom invariant under arbitrary rotation (73 deg)");

        // Translation + rotation composed
        auto mol_tr = translate(rotate_z(mol, 2.1), -999.0, 42.0, 0.001);
        auto id_tr = vsepr::identity::compute_identity(mol_tr, z_sym);
        check(id_orig.geometry_hash == id_tr.geometry_hash,
              c.name + " geom invariant under composed translate+rotate");

        // Reflection through XY plane
        auto mol_ref = reflect_z(mol);
        auto id_ref = vsepr::identity::compute_identity(mol_ref, z_sym);
        check(id_orig.geometry_hash == id_ref.geometry_hash,
              c.name + " geom invariant under Z-reflection");
    }
}

// ============================================================================
// TEST 3: Atom-order permutation invariance
// ============================================================================

static void test_permutation_invariance() {
    std::cout << "\n\033[1m  ═══ Test 3: Atom-Order Permutation Invariance ═══\033[0m\n\n";

    // H2O: swap the two hydrogens
    {
        auto mol = make_h2o();
        auto id_orig = vsepr::identity::compute_identity(mol, z_sym);

        auto mol_perm = permute_atoms(mol, {0, 2, 1});  // swap H1 and H2
        auto id_perm = vsepr::identity::compute_identity(mol_perm, z_sym);

        check(id_orig.topology_hash == id_perm.topology_hash,
              "H2O topo invariant under H-swap");
        check(id_orig.geometry_hash == id_perm.geometry_hash,
              "H2O geom invariant under H-swap");
    }

    // CH4: several permutations of the 4 hydrogens
    {
        auto mol = make_ch4();
        auto id_orig = vsepr::identity::compute_identity(mol, z_sym);

        // Swap H1 and H3 (indices 1 and 3, C stays at 0)
        auto mol_p1 = permute_atoms(mol, {0, 3, 2, 1, 4});
        auto id_p1 = vsepr::identity::compute_identity(mol_p1, z_sym);
        check(id_orig.topology_hash == id_p1.topology_hash,
              "CH4 topo invariant under H1<->H3 swap");
        check(id_orig.geometry_hash == id_p1.geometry_hash,
              "CH4 geom invariant under H1<->H3 swap");

        // Reverse all hydrogens
        auto mol_p2 = permute_atoms(mol, {0, 4, 3, 2, 1});
        auto id_p2 = vsepr::identity::compute_identity(mol_p2, z_sym);
        check(id_orig.topology_hash == id_p2.topology_hash,
              "CH4 topo invariant under H-reversal");
        check(id_orig.geometry_hash == id_p2.geometry_hash,
              "CH4 geom invariant under H-reversal");

        // Move C to end: H1, H2, H3, H4, C
        auto mol_p3 = permute_atoms(mol, {1, 2, 3, 4, 0});
        auto id_p3 = vsepr::identity::compute_identity(mol_p3, z_sym);
        check(id_orig.topology_hash == id_p3.topology_hash,
              "CH4 topo invariant when C moved to end");
        check(id_orig.geometry_hash == id_p3.geometry_hash,
              "CH4 geom invariant when C moved to end");
    }

    // SF6: full cyclic permutation of 6 F atoms
    {
        auto mol = make_sf6();
        auto id_orig = vsepr::identity::compute_identity(mol, z_sym);

        // Cyclic shift: S stays, F1→F6, F2→F1, ... F6→F5
        auto mol_cyc = permute_atoms(mol, {0, 6, 1, 2, 3, 4, 5});
        auto id_cyc = vsepr::identity::compute_identity(mol_cyc, z_sym);
        check(id_orig.topology_hash == id_cyc.topology_hash,
              "SF6 topo invariant under F cyclic permutation");
        check(id_orig.geometry_hash == id_cyc.geometry_hash,
              "SF6 geom invariant under F cyclic permutation");

        // Random shuffle of F atoms
        auto mol_shuf = permute_atoms(mol, {0, 4, 6, 2, 5, 1, 3});
        auto id_shuf = vsepr::identity::compute_identity(mol_shuf, z_sym);
        check(id_orig.topology_hash == id_shuf.topology_hash,
              "SF6 topo invariant under F random shuffle");
        check(id_orig.geometry_hash == id_shuf.geometry_hash,
              "SF6 geom invariant under F random shuffle");
    }
}

// ============================================================================
// TEST 4: Near-degenerate geometry sensitivity
// ============================================================================

static void test_near_degenerate_sensitivity() {
    std::cout << "\n\033[1m  ═══ Test 4: Near-Degenerate Geometry Sensitivity ═══\033[0m\n\n";

    // 4a: Linear vs almost-linear (CO2-like)
    {
        vsepr::Molecule mol_linear;
        mol_linear.add_atom(6, 0.0, 0.0, 0.0);
        mol_linear.add_atom(8, 0.0, 0.0,  1.16);
        mol_linear.add_atom(8, 0.0, 0.0, -1.16);
        mol_linear.add_bond(0, 1, 2);
        mol_linear.add_bond(0, 2, 2);

        vsepr::Molecule mol_bent;
        mol_bent.add_atom(6, 0.0, 0.0, 0.0);
        mol_bent.add_atom(8, 0.0, 0.30,  1.16);   // significant bend (~15°)
        mol_bent.add_atom(8, 0.0, -0.30, -1.16);
        mol_bent.add_bond(0, 1, 2);
        mol_bent.add_bond(0, 2, 2);

        auto id_lin = vsepr::identity::compute_identity(mol_linear, z_sym);
        auto id_bent = vsepr::identity::compute_identity(mol_bent, z_sym);

        check(id_lin.topology_hash == id_bent.topology_hash,
              "CO2 topo unchanged: linear vs slightly bent");
        check(id_lin.geometry_hash != id_bent.geometry_hash,
              "CO2 geom differs: linear vs bent (0.30 A displacement)");
    }

    // 4b: Perfect tetrahedral vs Jahn-Teller-like distortion
    {
        auto mol_tet = make_ch4();

        // Distort: stretch one C-H bond by 0.15 A
        vsepr::Molecule mol_dist = mol_tet;
        double scale = 1.15;
        mol_dist.coords[3] *= scale;  // H1.x
        mol_dist.coords[4] *= scale;  // H1.y
        mol_dist.coords[5] *= scale;  // H1.z

        auto id_tet = vsepr::identity::compute_identity(mol_tet, z_sym);
        auto id_dist = vsepr::identity::compute_identity(mol_dist, z_sym);

        check(id_tet.topology_hash == id_dist.topology_hash,
              "CH4 topo unchanged under JT distortion");
        check(id_tet.geometry_hash != id_dist.geometry_hash,
              "CH4 geom differs under JT distortion (15% stretch)");
    }

    // 4c: Octahedral vs slightly compressed octahedral
    {
        auto mol_oct = make_sf6();

        // Compress axial F atoms by 0.1 A
        vsepr::Molecule mol_comp = mol_oct;
        mol_comp.coords[3 * 5 + 2] -= 0.1;  // F +z
        mol_comp.coords[3 * 6 + 2] += 0.1;  // F -z

        auto id_oct = vsepr::identity::compute_identity(mol_oct, z_sym);
        auto id_comp = vsepr::identity::compute_identity(mol_comp, z_sym);

        check(id_oct.topology_hash == id_comp.topology_hash,
              "SF6 topo unchanged under axial compression");
        check(id_oct.geometry_hash != id_comp.geometry_hash,
              "SF6 geom differs under axial compression (0.1 A)");
    }

    // 4d: Very small perturbation — below tolerance
    {
        auto mol = make_h2o();
        auto id_orig = vsepr::identity::compute_identity(mol, z_sym);

        auto mol_tiny = perturb_coords(mol, 0.001, 99);
        auto id_tiny = vsepr::identity::compute_identity(mol_tiny, z_sym);

        check(id_orig.topology_hash == id_tiny.topology_hash,
              "H2O topo unchanged under sub-tolerance perturbation (0.001 A)");
        // Geom may or may not change depending on tolerance grid — document behavior
        std::cout << "    \033[90m(geom " << (id_orig.geometry_hash == id_tiny.geometry_hash ? "unchanged" : "changed")
                  << " — tolerance-dependent, both behaviors acceptable)\033[0m\n";
    }
}

// ============================================================================
// TEST 5: Scale test — collision detection
// ============================================================================

static void test_scale_collision() {
    std::cout << "\n\033[1m  ═══ Test 5: Scale Test — Collision Detection ═══\033[0m\n\n";

    // Generate many molecules by varying bond lengths and angles
    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> dist_len(0.8, 2.5);
    std::uniform_real_distribution<double> dist_ang(0.3, 3.0);  // radians
    std::uniform_int_distribution<int> dist_Z(1, 35);

    int N_molecules = 500;
    std::set<uint64_t> topo_set;
    std::set<uint64_t> geom_set;
    std::set<std::pair<uint64_t, uint64_t>> combined_set;

    int topo_collisions = 0;
    int geom_collisions = 0;
    int combined_collisions = 0;

    for (int n = 0; n < N_molecules; ++n) {
        vsepr::Molecule mol;

        // Central atom
        int Zc = dist_Z(rng);
        mol.add_atom(static_cast<uint8_t>(Zc), 0.0, 0.0, 0.0);

        // 2-6 ligands
        int n_ligands = 2 + (n % 5);
        for (int j = 0; j < n_ligands; ++j) {
            double r = dist_len(rng);
            double theta = dist_ang(rng);
            double phi = dist_ang(rng);
            double x = r * std::sin(theta) * std::cos(phi);
            double y = r * std::sin(theta) * std::sin(phi);
            double z = r * std::cos(theta);
            int Zl = 1 + (dist_Z(rng) % 9);  // light elements
            mol.add_atom(static_cast<uint8_t>(Zl), x, y, z);
            mol.add_bond(0, static_cast<uint32_t>(j + 1), 1);
        }

        auto id = vsepr::identity::compute_identity(mol, z_sym);

        if (topo_set.count(id.topology_hash)) topo_collisions++;
        if (geom_set.count(id.geometry_hash)) geom_collisions++;
        auto combined = std::make_pair(id.topology_hash, id.geometry_hash);
        if (combined_set.count(combined)) combined_collisions++;

        topo_set.insert(id.topology_hash);
        geom_set.insert(id.geometry_hash);
        combined_set.insert(combined);
    }

    std::cout << "    Molecules generated: " << N_molecules << "\n";
    std::cout << "    Unique topo hashes:  " << topo_set.size() << "\n";
    std::cout << "    Unique geom hashes:  " << geom_set.size() << "\n";
    std::cout << "    Topo collisions:     " << topo_collisions << "\n";
    std::cout << "    Geom collisions:     " << geom_collisions << "\n";
    std::cout << "    Combined collisions: " << combined_collisions << "\n\n";

    // Topo collisions are expected (same element set + same connectivity pattern)
    // Combined collisions should be very rare
    check(combined_collisions < N_molecules / 10,
          "Combined (topo+geom) collision rate < 10% over " + std::to_string(N_molecules) + " molecules");

    // Unique topo count should be substantial
    check(topo_set.size() > static_cast<size_t>(N_molecules / 5),
          "Topo namespace not trivially collapsed (" + std::to_string(topo_set.size()) + " unique)");

    // Unique geom count should be very high (random coords)
    check(geom_set.size() > static_cast<size_t>(N_molecules * 9 / 10),
          "Geom namespace has high uniqueness (" + std::to_string(geom_set.size()) + " unique)");
}

// ============================================================================
// TEST 6: 3-Tier Provenance Record
// ============================================================================

static void test_provenance_record() {
    std::cout << "\n\033[1m  ═══ Test 6: 3-Tier Provenance Record ═══\033[0m\n\n";

    struct Case {
        std::string name;
        std::function<vsepr::Molecule()> build;
        std::string method;
        vsepr::provenance::GeometryClass expected_geom;
        vsepr::provenance::SymmetryClass expected_sym;
        std::string expected_formula;
    };

    using GC = vsepr::provenance::GeometryClass;
    using SC = vsepr::provenance::SymmetryClass;

    std::vector<Case> cases = {
        {"H2O", make_h2o, "experimental geometry", GC::Bent,              SC::C2v, "H2O"},
        {"CH4", make_ch4, "tetrahedral vertices",  GC::Tetrahedral,       SC::Td,  "CH4"},
        {"NH3", make_nh3, "pyramidal placement",   GC::TrigonalPyramidal, SC::C3v, "H3N"},
        {"SF6", make_sf6, "octahedral vertices",   GC::Octahedral,        SC::Oh,  "F6S"},
    };

    for (const auto& c : cases) {
        auto mol = c.build();
        auto rec = vsepr::provenance::build_provenance(
            mol, z_sym, c.method, true,
            [&]() { return c.build(); });

        // Print the full audit record
        std::cout << "    --- " << c.name << " ---\n";
        std::string audit = rec.to_audit_string();
        // Indent each line
        std::istringstream stream(audit);
        std::string line;
        while (std::getline(stream, line)) {
            std::cout << "    " << line << "\n";
        }
        std::cout << "\n";

        // Verify fields
        check(rec.species == c.expected_formula,
              c.name + " formula = " + c.expected_formula);
        check(rec.geom_class == c.expected_geom,
              c.name + " geometry_class = " +
              vsepr::provenance::geometry_class_name(c.expected_geom));
        check(rec.sym_class == c.expected_sym,
              c.name + " symmetry_class = " +
              vsepr::provenance::symmetry_class_name(c.expected_sym));
        check(rec.pose_normalized,
              c.name + " pose_normalized = yes");
        check(rec.deterministic,
              c.name + " verified = deterministic");
        check(rec.hash_version == vsepr::provenance::HASH_VERSION,
              c.name + " hash_version = v" + std::to_string(vsepr::provenance::HASH_VERSION));
        check(!rec.bond_sig.empty(),
              c.name + " bond_signature populated");
        check(!rec.canonical_order.empty(),
              c.name + " canonical_order populated");

        // Tier matching: record should match itself at all tiers
        check(rec.matches_tier1(rec), c.name + " tier-1 self-match");
        check(rec.matches_tier2(rec), c.name + " tier-2 self-match");
        check(rec.matches_tier3(rec), c.name + " tier-3 self-match");
    }

    // Cross-molecule: records should NOT match at tier-2
    {
        auto mol_h2o = make_h2o();
        auto mol_ch4 = make_ch4();
        auto rec_h2o = vsepr::provenance::build_provenance(mol_h2o, z_sym, "test");
        auto rec_ch4 = vsepr::provenance::build_provenance(mol_ch4, z_sym, "test");
        check(!rec_h2o.matches_tier2(rec_ch4), "H2O vs CH4 do not match at tier-2");
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "\n\033[1m";
    std::cout << "  ╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "  ║     Hash Invariance & Provenance Audit Test Suite               ║\n";
    std::cout << "  ║     3-Tier Identity · Topology · Geometry                      ║\n";
    std::cout << "  ╠══════════════════════════════════════════════════════════════════╣\n";
    std::cout << "  ║  Test 1: Topo invariance under coordinate perturbation         ║\n";
    std::cout << "  ║  Test 2: Geom invariance under rigid transforms                ║\n";
    std::cout << "  ║  Test 3: Atom-order permutation invariance                     ║\n";
    std::cout << "  ║  Test 4: Near-degenerate geometry sensitivity                  ║\n";
    std::cout << "  ║  Test 5: Scale test (500 molecules, collision detection)       ║\n";
    std::cout << "  ║  Test 6: 3-Tier provenance record correctness                 ║\n";
    std::cout << "  ╚══════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\033[0m\n";

    test_topo_invariance_under_perturbation();
    test_geom_invariance_under_rigid_transforms();
    test_permutation_invariance();
    test_near_degenerate_sensitivity();
    test_scale_collision();
    test_provenance_record();

    // Summary
    std::cout << "\n\033[1m";
    std::cout << "  ════════════════════════════════════════════════════════════════\n";
    std::printf("   Results: %d/%d passed", g_pass, g_total);
    if (g_fail > 0) {
        std::printf("  (\033[91m%d FAILED\033[0m\033[1m)", g_fail);
    } else {
        std::printf("  (\033[92mALL PASS\033[0m\033[1m)");
    }
    std::cout << "\n  ════════════════════════════════════════════════════════════════\n";
    std::cout << "\033[0m\n";

    return g_fail > 0 ? 1 : 0;
}
