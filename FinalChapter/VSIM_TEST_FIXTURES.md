# VSIM Test Fixture Library
## Canonical C++ fixtures for all module tests

**File:** `tests/test_fixtures.hpp`  
**Version:** v5.1.13.5  
**Purpose:** Single source of truth for all SimState and VsimDocument test objects.  
**Rule:** If a test hardcodes atom positions, it should use a fixture instead.

---

## Header skeleton

```cpp
// tests/test_fixtures.hpp
#pragma once
#include "vsim/sim_state.hpp"
#include "vsim/vsim_document.hpp"
#include <cmath>
#include <vector>

namespace TestFixtures {
```

---

## Fixture: CH4 ground state

```cpp
// Methane in tetrahedral geometry, ground state (0 K)
// C at origin, 4 H at tetrahedral vertices
// C-H bond = 1.09 Å, H-C-H angle = 109.47°
inline SimState build_ch4_ground_state() {
    SimState s;
    s.n_atoms = 5;
    s.species = {"C", "H", "H", "H", "H"};
    s.masses  = {12.011, 1.008, 1.008, 1.008, 1.008};
    s.charges = {-0.83, 0.21, 0.21, 0.21, 0.21};  // approximate partial charges
    s.atomic_number = {6, 1, 1, 1, 1};

    const double bond = 1.09;
    const double a = bond / std::sqrt(3.0);
    s.positions = {
        {0.0,  0.0,  0.0},       // C
        { a,   a,   a},          // H1
        {-a,  -a,   a},          // H2
        {-a,   a,  -a},          // H3
        { a,  -a,  -a}           // H4
    };
    s.velocities.assign(5, Vec3{0,0,0});
    s.forces.assign(5, Vec3{0,0,0});

    s.box_lengths = {20.0, 20.0, 20.0};
    s.periodic[0] = s.periodic[1] = s.periodic[2] = false;
    s.converged   = true;
    s.temperature_K = 0.0;
    return s;
}

inline VsimDocument make_ch4_doc() {
    VsimDocument doc;
    doc.project.name     = "test_ch4";
    doc.project.seed_base = 101;
    doc.material.formula  = "CH4";
    doc.analysis_structure.enabled           = true;
    doc.analysis_structure.neighbor_cutoff_A = 3.0;
    doc.analysis_structure.contact_cutoff_A  = 1.5;
    doc.verify_structure.enabled                      = true;
    doc.verify_structure.expected_coordination        = 4;
    doc.verify_structure.coordination_tolerance       = 0;
    doc.verify_structure.expected_nearest_neighbor_A  = 1.09;
    doc.verify_structure.nearest_neighbor_tolerance_A = 0.06;
    return doc;
}
```

---

## Fixture: NaCl rocksalt 2×2×2

```cpp
// NaCl B1 rocksalt, 2×2×2 supercell = 64 atoms
// a = 5.64 Å, coordination = 6
// Minimal periodic cell for testing Ewald + scale sampling
inline SimState build_nacl_rocksalt_2x2x2() {
    SimState s;
    const double a = 5.64;   // lattice constant Å
    s.box_lengths = {2*a, 2*a, 2*a};
    s.periodic[0] = s.periodic[1] = s.periodic[2] = true;

    // Build B1 rocksalt in 2×2×2
    const std::vector<Vec3> basis = {
        {0.0, 0.0, 0.0},           // Na
        {0.5*a, 0.5*a, 0.0},       // Cl
        {0.5*a, 0.0,   0.5*a},     // Cl
        {0.0,   0.5*a, 0.5*a},     // Cl
        {0.5*a, 0.5*a, 0.5*a},     // Na
        {0.0,   0.0,   0.5*a},     // Na
        {0.0,   0.5*a, 0.0},       // Na
        {0.5*a, 0.0,   0.0},       // Cl
    };
    const std::vector<std::string> basis_species = {
        "Na","Cl","Cl","Cl","Na","Na","Na","Cl"
    };
    const std::vector<double> charges_basis = {
        +1.0,-1.0,-1.0,-1.0,+1.0,+1.0,+1.0,-1.0
    };

    for (int ix = 0; ix < 2; ++ix)
    for (int iy = 0; iy < 2; ++iy)
    for (int iz = 0; iz < 2; ++iz) {
        Vec3 offset = {ix*a, iy*a, iz*a};
        for (int b = 0; b < 8; ++b) {
            s.positions.push_back(basis[b] + offset);
            s.species.push_back(basis_species[b]);
            s.charges.push_back(charges_basis[b]);
            s.masses.push_back(basis_species[b] == "Na" ? 22.99 : 35.45);
            s.atomic_number.push_back(basis_species[b] == "Na" ? 11 : 17);
        }
    }
    s.n_atoms = (int)s.positions.size();
    s.velocities.assign(s.n_atoms, Vec3{0,0,0});
    s.forces.assign(s.n_atoms, Vec3{0,0,0});
    s.converged   = true;
    s.temperature_K = 0.0;
    return s;
}

inline VsimDocument make_nacl_doc() {
    VsimDocument doc;
    doc.project.name      = "test_nacl";
    doc.material.formula  = "NaCl";
    doc.material.prototype = "B1_NaCl";
    doc.environment.periodic    = true;
    doc.pbc.minimum_image       = true;
    doc.simulation.use_ewald    = true;
    doc.simulation.ewald_alpha  = 0.3;
    doc.simulation.ewald_rcut   = 12.0;
    doc.simulation.ewald_kmax   = 5;
    doc.analysis_structure.enabled           = true;
    doc.analysis_structure.neighbor_cutoff_A = 5.6;
    doc.analysis_structure.contact_cutoff_A  = 3.0;
    doc.verify_structure.enabled                      = true;
    doc.verify_structure.expected_coordination        = 6;
    doc.verify_structure.coordination_tolerance       = 0;
    doc.verify_structure.expected_nearest_neighbor_A  = 2.82;
    doc.verify_structure.nearest_neighbor_tolerance_A = 0.15;
    return doc;
}
```

---

## Fixture: Si diamond cubic 2×2×2

```cpp
// Si A4 diamond cubic, 2×2×2 supercell = 64 atoms
// a = 5.43 Å, coordination = 4 (tetrahedral)
inline SimState build_si_diamond_2x2x2() {
    SimState s;
    const double a = 5.43;
    s.box_lengths = {2*a, 2*a, 2*a};
    s.periodic[0] = s.periodic[1] = s.periodic[2] = true;

    // Diamond cubic basis (2 atoms per unit cell)
    const std::vector<Vec3> basis = {
        {0.0,     0.0,     0.0},
        {0.25*a,  0.25*a,  0.25*a}
    };

    // FCC translations
    const std::vector<Vec3> fcc_trans = {
        {0,       0,       0},
        {0.5*a,   0.5*a,   0},
        {0.5*a,   0,       0.5*a},
        {0,       0.5*a,   0.5*a}
    };

    for (int ix = 0; ix < 2; ++ix)
    for (int iy = 0; iy < 2; ++iy)
    for (int iz = 0; iz < 2; ++iz) {
        Vec3 cell_offset = {ix*a, iy*a, iz*a};
        for (const auto& ft : fcc_trans)
        for (const auto& b : basis) {
            s.positions.push_back(b + ft + cell_offset);
            s.species.push_back("Si");
            s.charges.push_back(0.0);
            s.masses.push_back(28.086);
            s.atomic_number.push_back(14);
        }
    }
    s.n_atoms = (int)s.positions.size();
    s.velocities.assign(s.n_atoms, Vec3{0,0,0});
    s.forces.assign(s.n_atoms, Vec3{0,0,0});
    s.converged   = true;
    s.temperature_K = 0.0;
    return s;
}

inline VsimDocument make_si_doc() {
    VsimDocument doc;
    doc.project.name     = "test_si";
    doc.material.formula = "Si";
    doc.material.prototype = "A4_Si";
    doc.environment.periodic = true;
    doc.pbc.minimum_image    = true;
    doc.analysis_structure.enabled           = true;
    doc.analysis_structure.neighbor_cutoff_A = 5.5;
    doc.analysis_structure.contact_cutoff_A  = 2.5;
    doc.verify_structure.enabled                      = true;
    doc.verify_structure.expected_coordination        = 4;
    doc.verify_structure.coordination_tolerance       = 0;
    doc.verify_structure.expected_nearest_neighbor_A  = 2.35;
    doc.verify_structure.nearest_neighbor_tolerance_A = 0.08;
    return doc;
}
```

---

## Fixture: Fe BCC 2×2×2

```cpp
// Fe A2 BCC, 2×2×2 supercell = 16 atoms
// a = 2.87 Å, coordination = 8
inline SimState build_fe_bcc_2x2x2() {
    SimState s;
    const double a = 2.87;
    s.box_lengths = {2*a, 2*a, 2*a};
    s.periodic[0] = s.periodic[1] = s.periodic[2] = true;

    // BCC basis: corner + body centre
    const std::vector<Vec3> basis = {
        {0.0,     0.0,     0.0},
        {0.5*a,   0.5*a,   0.5*a}
    };

    for (int ix = 0; ix < 2; ++ix)
    for (int iy = 0; iy < 2; ++iy)
    for (int iz = 0; iz < 2; ++iz) {
        Vec3 offset = {ix*a, iy*a, iz*a};
        for (const auto& b : basis) {
            s.positions.push_back(b + offset);
            s.species.push_back("Fe");
            s.charges.push_back(0.0);
            s.masses.push_back(55.845);
            s.atomic_number.push_back(26);
        }
    }
    s.n_atoms = (int)s.positions.size();
    s.velocities.assign(s.n_atoms, Vec3{0,0,0});
    s.forces.assign(s.n_atoms, Vec3{0,0,0});
    s.converged   = true;
    s.temperature_K = 0.0;
    return s;
}

inline VsimDocument make_fe_doc() {
    VsimDocument doc;
    doc.project.name      = "test_fe";
    doc.material.formula  = "Fe";
    doc.material.prototype = "A2_Fe";
    doc.environment.periodic = true;
    doc.pbc.minimum_image    = true;
    doc.analysis_structure.enabled           = true;
    doc.analysis_structure.neighbor_cutoff_A = 5.0;
    doc.analysis_structure.contact_cutoff_A  = 3.0;
    doc.verify_structure.enabled                      = true;
    doc.verify_structure.expected_coordination        = 8;
    doc.verify_structure.coordination_tolerance       = 0;
    doc.verify_structure.expected_nearest_neighbor_A  = 2.48;
    doc.verify_structure.nearest_neighbor_tolerance_A = 0.10;
    return doc;
}
```

---

## Fixture: Graphene monolayer 4×4×1

```cpp
// Graphene, single layer, 4×4 in-plane supercell = 32 atoms
// a = 2.46 Å (in-plane), z-open boundary
// coordination = 3 (sp2), bond = 1.42 Å
inline SimState build_graphene_4x4x1() {
    SimState s;
    const double a_cc = 1.42;   // C-C bond
    const double a    = 2.46;   // lattice constant
    const double lz   = 20.0;   // non-periodic z box

    s.box_lengths = {4*a, 4*a * std::sqrt(3.0) / 2.0 * 2.0, lz};
    s.periodic[0] = true;
    s.periodic[1] = true;
    s.periodic[2] = false;

    // 2-atom basis per unit cell, hexagonal
    const Vec3 a1 = {a, 0, 0};
    const Vec3 a2 = {a*0.5, a*std::sqrt(3.0)*0.5, 0};
    const Vec3 b1 = {0, 0, 0};
    const Vec3 b2 = {a*0.5, a*std::sqrt(3.0)/6.0, 0};

    for (int ix = 0; ix < 4; ++ix)
    for (int iy = 0; iy < 4; ++iy) {
        Vec3 origin = a1 * ix + a2 * iy;
        for (const auto& b : {b1, b2}) {
            s.positions.push_back(origin + b + Vec3{0,0,0});
            s.species.push_back("C");
            s.charges.push_back(0.0);
            s.masses.push_back(12.011);
            s.atomic_number.push_back(6);
        }
    }
    s.n_atoms = (int)s.positions.size();
    s.velocities.assign(s.n_atoms, Vec3{0,0,0});
    s.forces.assign(s.n_atoms, Vec3{0,0,0});
    s.converged   = true;
    s.temperature_K = 0.0;
    return s;
}

inline VsimDocument make_graphene_doc() {
    VsimDocument doc;
    doc.project.name     = "test_graphene";
    doc.material.formula = "C";
    doc.analysis_structure.enabled           = true;
    doc.analysis_structure.neighbor_cutoff_A = 3.5;
    doc.analysis_structure.contact_cutoff_A  = 1.8;
    doc.verify_structure.enabled                      = true;
    doc.verify_structure.expected_coordination        = 3;
    doc.verify_structure.coordination_tolerance       = 0;
    doc.verify_structure.expected_nearest_neighbor_A  = 1.42;
    doc.verify_structure.nearest_neighbor_tolerance_A = 0.08;
    return doc;
}
```

---

## Fixture: N2 gas box (8 atoms)

```cpp
// 4 × N2 molecules in a 15 Å box
// Baseline molecular scale fixture — no periodicity
// coordination = 1, N-N bond = 1.10 Å
inline SimState build_n2_gas_box() {
    SimState s;
    s.n_atoms = 8;
    s.box_lengths = {15.0, 15.0, 15.0};
    s.periodic[0] = s.periodic[1] = s.periodic[2] = false;

    // 4 N2 molecules, placed randomly within box (seeded positions)
    const double bond = 1.10;
    const std::vector<Vec3> centers = {
        {3.5, 3.5, 3.5}, {11.5, 3.5, 3.5},
        {3.5, 11.5, 3.5}, {3.5, 3.5, 11.5}
    };
    for (const auto& c : centers) {
        s.positions.push_back({c[0] - bond/2, c[1], c[2]});
        s.positions.push_back({c[0] + bond/2, c[1], c[2]});
        s.species.push_back("N");
        s.species.push_back("N");
        s.charges.push_back(0.0);
        s.charges.push_back(0.0);
        s.masses.push_back(14.007);
        s.masses.push_back(14.007);
        s.atomic_number.push_back(7);
        s.atomic_number.push_back(7);
    }
    s.velocities.assign(8, Vec3{0,0,0});
    s.forces.assign(8, Vec3{0,0,0});
    s.converged   = true;
    s.temperature_K = 0.0;
    return s;
}
```

---

## Fixture: scale sampling document (shared)

```cpp
// Standard scale_sampling config for unit tests
// Uses 4-window layout consistent with all example scripts
inline void apply_standard_scale_sampling(VsimDocument& doc,
    const std::vector<double>& windows_A)
{
    doc.analysis_scale_sampling.enabled                        = true;
    doc.analysis_scale_sampling.compute_field_projection       = true;
    doc.analysis_scale_sampling.compute_rve_sampling           = true;
    doc.analysis_scale_sampling.compute_emergence_metrics      = true;
    doc.analysis_scale_sampling.field_grid                     = {8, 8, 8};
    doc.analysis_scale_sampling.rve_window_lengths_A           = windows_A;
    doc.analysis_scale_sampling.rve_windows_per_level          = 4;
    doc.analysis_scale_sampling.rve_window_placement           = "grid";
    doc.analysis_scale_sampling.min_particles_for_scale_sampling = 16;
    doc.analysis_scale_sampling.spatial_cv_threshold           = 0.35;
    doc.analysis_scale_sampling.temporal_drift_threshold       = 0.20;
    doc.analysis_scale_sampling.scale_drift_threshold          = 0.30;
    doc.analysis_scale_sampling.scale_drift_metric             = "successive_window_difference";
}
```

---

## Usage in tests

```cpp
// Standard test pattern using fixtures:
#include "test_fixtures.hpp"

TEST(BondAngleAnalysis, CH4TetrahedralAngle) {
    auto state = TestFixtures::build_ch4_ground_state();
    auto doc   = TestFixtures::make_ch4_doc();
    auto mod   = AnalysisModuleRegistry::get().create("bond_angles");
    ASSERT_NE(mod, nullptr);
    AnalysisRecord rec;
    mod->configure(doc);
    mod->compute(state, rec);
    EXPECT_NEAR(rec.bond_angle_record.mean(), 109.47, 1.5);
}

TEST(ScaleSampling, NaClMassConserved) {
    auto state = TestFixtures::build_nacl_rocksalt_2x2x2();
    auto doc   = TestFixtures::make_nacl_doc();
    TestFixtures::apply_standard_scale_sampling(doc, {2.82, 5.64, 11.28, 22.56});
    // ... run scale sampling operator, check mass_conserved = true
}
```

---

## Quick reference: fixture → test coverage

| Fixture | Used by | Key assertion |
|---|---|---|
| `build_ch4_ground_state` | bond_angles, spectral_response, formation tests | angle = 109.47°, bond = 1.09 Å |
| `build_nacl_rocksalt_2x2x2` | scale_sampling, verify_rdf, verify_structure, Ewald | coord = 6, peak = 2.82 Å |
| `build_si_diamond_2x2x2` | scale_sampling, excite, spectral_response | coord = 4, peak = 2.35 Å |
| `build_fe_bcc_2x2x2` | scale_sampling, field limit, EAM | coord = 8, peak = 2.48 Å |
| `build_graphene_4x4x1` | post_step, interference, WIC trajectory | coord = 3, bond = 1.42 Å |
| `build_n2_gas_box` | molecular scale, no-PBC tests | coord = 1, bond = 1.10 Å |

```cpp
} // namespace TestFixtures
```
