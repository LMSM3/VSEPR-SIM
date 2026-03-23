/**
 * test_bead_layers.cpp — Tests for Multi-Layer Bead Visualization Model
 *
 * Validates the Layer 2 (Effective Surface-State) and Layer 3
 * (Internal Structural Reference) data model and builder logic.
 *
 * Phase A: Layer 2 — SurfaceStateGrid correctness
 *   Grid population, vertex count, angular coordinates, SH evaluation,
 *   channel range tracking, channel_value accessor, synthesized channel.
 *
 * Phase B: Layer 3 — InternalRefOptions and SourceFragmentMiniature
 *   Source fragment population from FragmentView, element census,
 *   bond transfer, orientation marker data.
 *
 * Phase C: ViewMode coverage
 *   All 8 view modes produce valid enum values and names.
 *
 * Phase D: Builder integration
 *   build_visual_record() correctly populates Layer 2 grid and
 *   Layer 3 fragment data from a Bead + FragmentView.
 *
 * Architecture position:
 *   Bead + FragmentView + UnifiedDescriptor
 *       → build_visual_record()
 *       → BeadVisualRecord
 *           → Layer 2: SurfaceStateGrid
 *           → Layer 3: SourceFragmentMiniature
 *       → renderer primitives
 *
 * Reference: Multi-Layer Bead Visualization Model specification
 *            (section_anisotropic_beads.tex)
 */

#include "coarse_grain/vis/bead_visual_record.hpp"
#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/unified_descriptor.hpp"
#include "coarse_grain/core/spherical_harmonics.hpp"
#include "atomistic/core/fragment_view.hpp"
#include "atomistic/core/state.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <optional>

using namespace coarse_grain;
using namespace coarse_grain::vis;

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* name) {
    if (cond) {
        std::printf("  [PASS] %s\n", name);
        ++g_pass;
    } else {
        std::printf("  [FAIL] %s\n", name);
        ++g_fail;
    }
}

// ============================================================================
// Helpers: Build minimal test beads with unified descriptors
// ============================================================================

/**
 * Build a minimal bead with a UnifiedDescriptor containing isotropic
 * SH coefficients (l=0 only) for testing Layer 2 grid population.
 */
static Bead make_isotropic_bead(double c_steric, double c_elec, double c_disp) {
    Bead b;
    b.position = {0.0, 0.0, 0.0};
    b.mass = 12.0;
    b.mapping_residual = 0.0;

    UnifiedDescriptor ud;
    ud.steric.active = true;
    ud.steric.l_max = 0;
    ud.steric.coeffs = {c_steric};

    ud.electrostatic.active = true;
    ud.electrostatic.l_max = 0;
    ud.electrostatic.coeffs = {c_elec};

    ud.dispersion.active = true;
    ud.dispersion.l_max = 0;
    ud.dispersion.coeffs = {c_disp};

    b.unified = ud;
    return b;
}

/**
 * Build a bead with anisotropic SH coefficients (l=0 + l=1 terms).
 */
static Bead make_anisotropic_bead() {
    Bead b;
    b.position = {1.0, 2.0, 3.0};
    b.mass = 28.0;
    b.mapping_residual = 0.1;

    UnifiedDescriptor ud;

    // Steric: isotropic + z-dipole
    ud.steric.active = true;
    ud.steric.l_max = 1;
    ud.steric.coeffs = {1.0, 0.0, 0.0, 0.5};  // Y00, Y1-1, Y10, Y11

    // Electrostatic: isotropic + x-dipole
    ud.electrostatic.active = true;
    ud.electrostatic.l_max = 1;
    ud.electrostatic.coeffs = {0.5, 0.0, 0.0, 0.3};

    // Dispersion: isotropic only
    ud.dispersion.active = true;
    ud.dispersion.l_max = 0;
    ud.dispersion.coeffs = {0.8};

    b.unified = ud;
    return b;
}

/**
 * Build a minimal FragmentView for testing Layer 3 population.
 * Simulates a water-like 3-atom fragment.
 */
static atomistic::FragmentView make_water_fragment() {
    atomistic::FragmentView frag;
    frag.status = atomistic::FragmentStatus::Valid;

    // O atom
    atomistic::AtomRecord o;
    o.position = {0.0, 0.0, 0.0};
    o.atomic_number = 8;
    o.mass = 15.999;
    frag.atoms.push_back(o);

    // H atom 1
    atomistic::AtomRecord h1;
    h1.position = {0.757, 0.586, 0.0};
    h1.atomic_number = 1;
    h1.mass = 1.008;
    frag.atoms.push_back(h1);

    // H atom 2
    atomistic::AtomRecord h2;
    h2.position = {-0.757, 0.586, 0.0};
    h2.atomic_number = 1;
    h2.mass = 1.008;
    frag.atoms.push_back(h2);

    // O-H bonds
    atomistic::BondRecord b1;
    b1.i = 0; b1.j = 1; b1.order = 1;
    frag.bonds.push_back(b1);

    atomistic::BondRecord b2;
    b2.i = 0; b2.j = 2; b2.order = 1;
    frag.bonds.push_back(b2);

    return frag;
}

// ============================================================================
// Phase A — Layer 2: SurfaceStateGrid Correctness
// ============================================================================

static void phase_a_surface_state_grid() {
    std::printf("\n=== Phase A: Layer 2 — SurfaceStateGrid Correctness ===\n");

    // ---- A.1: Grid population from isotropic bead ----
    {
        std::printf("\n--- A.1: Isotropic bead grid population ---\n");

        auto bead = make_isotropic_bead(1.0, 0.5, 0.3);
        auto rec = build_visual_record(bead, 0);

        check(rec.surface_state_grid.populated,
              "grid is populated");
        check(rec.surface_state_grid.n_theta == 24,
              "n_theta = 24");
        check(rec.surface_state_grid.n_phi == 48,
              "n_phi = 48");

        int expected_vertices = (24 + 1) * 48;
        check(static_cast<int>(rec.surface_state_grid.vertices.size()) == expected_vertices,
              "vertex count = (n_theta+1) * n_phi");
    }

    // ---- A.2: Angular coordinates correct ----
    {
        std::printf("\n--- A.2: Angular coordinates ---\n");

        auto bead = make_isotropic_bead(1.0, 0.5, 0.3);
        auto rec = build_visual_record(bead, 0);
        const auto& grid = rec.surface_state_grid;

        // First vertex: theta=0, phi=0 (north pole)
        const auto& v00 = grid.at(0, 0);
        check(std::abs(v00.theta) < 1e-10, "v(0,0) theta = 0");
        check(std::abs(v00.phi) < 1e-10,   "v(0,0) phi = 0");

        // Last theta band: theta=π (south pole)
        const auto& v_south = grid.at(24, 0);
        check(std::abs(v_south.theta - 3.14159265358979323846) < 1e-10,
              "v(n_theta,0) theta = pi");

        // Direction at north pole should be (0, 0, 1)
        check(std::abs(v00.direction_local.x) < 1e-10,
              "north pole dir.x = 0");
        check(std::abs(v00.direction_local.y) < 1e-10,
              "north pole dir.y = 0");
        check(std::abs(v00.direction_local.z - 1.0) < 1e-10,
              "north pole dir.z = 1");
    }

    // ---- A.3: Isotropic bead produces uniform field ----
    {
        std::printf("\n--- A.3: Isotropic uniformity ---\n");

        auto bead = make_isotropic_bead(2.0, 1.0, 0.5);
        auto rec = build_visual_record(bead, 0);
        const auto& grid = rec.surface_state_grid;

        // For l=0 only: all vertices should have same steric value
        // Y_00 = 1/(2*sqrt(pi)), so field = coeff * Y_00
        double y00 = 1.0 / (2.0 * std::sqrt(3.14159265358979323846));
        double expected_steric = 2.0 * y00;

        // Check a few sample vertices
        bool steric_uniform = true;
        for (int i = 0; i < static_cast<int>(grid.vertices.size()); i += 50) {
            if (std::abs(grid.vertices[i].steric - expected_steric) > 1e-8) {
                steric_uniform = false;
                break;
            }
        }
        check(steric_uniform, "isotropic steric field is uniform");

        // Range should be effectively zero width
        check(std::abs(grid.steric_max - grid.steric_min) < 1e-8,
              "isotropic steric range is zero");
    }

    // ---- A.4: Anisotropic bead produces non-uniform field ----
    {
        std::printf("\n--- A.4: Anisotropic variation ---\n");

        auto bead = make_anisotropic_bead();
        auto rec = build_visual_record(bead, 0);
        const auto& grid = rec.surface_state_grid;

        // With l=1 terms, field should vary across the sphere
        check(grid.steric_max > grid.steric_min + 1e-6,
              "anisotropic steric field varies");
        check(grid.elec_max > grid.elec_min + 1e-6,
              "anisotropic elec field varies");
    }

    // ---- A.5: Channel value accessor ----
    {
        std::printf("\n--- A.5: Channel value accessor ---\n");

        auto bead = make_isotropic_bead(1.0, 2.0, 3.0);
        auto rec = build_visual_record(bead, 0);
        const auto& grid = rec.surface_state_grid;
        const auto& v = grid.vertices[0];

        double s_val = grid.channel_value(v, SurfaceStateChannel::Steric);
        double e_val = grid.channel_value(v, SurfaceStateChannel::Electrostatic);
        double d_val = grid.channel_value(v, SurfaceStateChannel::Dispersion);
        double y_val = grid.channel_value(v, SurfaceStateChannel::Synthesized);

        check(std::abs(s_val - v.steric) < 1e-15,
              "channel_value(Steric) matches .steric");
        check(std::abs(e_val - v.electrostatic) < 1e-15,
              "channel_value(Electrostatic) matches .electrostatic");
        check(std::abs(d_val - v.dispersion) < 1e-15,
              "channel_value(Dispersion) matches .dispersion");
        check(std::abs(y_val - v.synthesized) < 1e-15,
              "channel_value(Synthesized) matches .synthesized");
    }

    // ---- A.6: Synthesized channel = (1/3)(S + E + D) ----
    {
        std::printf("\n--- A.6: Synthesized channel ---\n");

        auto bead = make_isotropic_bead(3.0, 6.0, 9.0);
        auto rec = build_visual_record(bead, 0);
        const auto& grid = rec.surface_state_grid;
        const auto& v = grid.vertices[0];

        double expected = (v.steric + v.electrostatic + v.dispersion) / 3.0;
        check(std::abs(v.synthesized - expected) < 1e-12,
              "synthesized = (1/3)(steric + elec + disp)");
    }

    // ---- A.7: Channel range accessor ----
    {
        std::printf("\n--- A.7: Channel range ---\n");

        auto bead = make_anisotropic_bead();
        auto rec = build_visual_record(bead, 0);
        const auto& grid = rec.surface_state_grid;

        double vmin, vmax;
        grid.channel_range(SurfaceStateChannel::Steric, vmin, vmax);
        check(vmin <= vmax, "steric range: min <= max");
        check(std::abs(vmin - grid.steric_min) < 1e-15,
              "channel_range(Steric) min matches grid");
        check(std::abs(vmax - grid.steric_max) < 1e-15,
              "channel_range(Steric) max matches grid");

        grid.channel_range(SurfaceStateChannel::Synthesized, vmin, vmax);
        check(vmin <= vmax, "synthesized range: min <= max");
    }

    // ---- A.8: No descriptor → grid not populated ----
    {
        std::printf("\n--- A.8: No descriptor → no grid ---\n");

        Bead b;
        b.position = {0.0, 0.0, 0.0};
        b.mass = 12.0;
        // No unified descriptor

        auto rec = build_visual_record(b, 0);
        check(!rec.surface_state_grid.populated,
              "no descriptor: grid not populated");
        check(rec.surface_state_grid.vertices.empty(),
              "no descriptor: vertices empty");
    }

    // ---- A.9: Direction vectors are unit length ----
    {
        std::printf("\n--- A.9: Direction unit vectors ---\n");

        auto bead = make_isotropic_bead(1.0, 1.0, 1.0);
        auto rec = build_visual_record(bead, 0);
        const auto& grid = rec.surface_state_grid;

        bool all_unit = true;
        for (const auto& v : grid.vertices) {
            double len = std::sqrt(v.direction_local.x * v.direction_local.x +
                                   v.direction_local.y * v.direction_local.y +
                                   v.direction_local.z * v.direction_local.z);
            if (std::abs(len - 1.0) > 1e-10) {
                all_unit = false;
                break;
            }
        }
        check(all_unit, "all direction vectors are unit length");
    }

    // ---- A.10: Grid wrapping (phi periodicity) ----
    {
        std::printf("\n--- A.10: Grid wrapping ---\n");

        auto bead = make_isotropic_bead(1.0, 1.0, 1.0);
        auto rec = build_visual_record(bead, 0);
        const auto& grid = rec.surface_state_grid;

        // at(i, n_phi) should wrap to at(i, 0)
        const auto& v_wrap = grid.at(5, grid.n_phi);
        const auto& v_orig = grid.at(5, 0);

        check(std::abs(v_wrap.steric - v_orig.steric) < 1e-15,
              "phi wrapping: at(i, n_phi) == at(i, 0)");
    }
}

// ============================================================================
// Phase B — Layer 3: Internal Structural Reference
// ============================================================================

static void phase_b_internal_reference() {
    std::printf("\n=== Phase B: Layer 3 — Internal Structural Reference ===\n");

    // ---- B.1: Fragment population ----
    {
        std::printf("\n--- B.1: Fragment population ---\n");

        auto bead = make_isotropic_bead(1.0, 1.0, 1.0);
        auto frag = make_water_fragment();
        auto rec = build_visual_record(bead, 0, &frag);

        check(rec.source_fragment.populated,
              "source fragment populated");
        check(static_cast<int>(rec.source_fragment.atoms.size()) == 3,
              "3 source atoms (water)");
        check(static_cast<int>(rec.source_fragment.bonds.size()) == 2,
              "2 source bonds (water)");
    }

    // ---- B.2: Element transfer ----
    {
        std::printf("\n--- B.2: Element transfer ---\n");

        auto bead = make_isotropic_bead(1.0, 1.0, 1.0);
        auto frag = make_water_fragment();
        auto rec = build_visual_record(bead, 0, &frag);

        check(rec.source_fragment.atoms[0].atomic_number == 8,
              "atom 0 is oxygen (Z=8)");
        check(rec.source_fragment.atoms[1].atomic_number == 1,
              "atom 1 is hydrogen (Z=1)");
        check(rec.source_fragment.atoms[2].atomic_number == 1,
              "atom 2 is hydrogen (Z=1)");
    }

    // ---- B.3: Bond order transfer ----
    {
        std::printf("\n--- B.3: Bond order transfer ---\n");

        auto bead = make_isotropic_bead(1.0, 1.0, 1.0);
        auto frag = make_water_fragment();
        auto rec = build_visual_record(bead, 0, &frag);

        check(rec.source_fragment.bonds[0].order == 1,
              "bond 0 is single (order=1)");
        check(rec.source_fragment.bonds[1].order == 1,
              "bond 1 is single (order=1)");
    }

    // ---- B.4: VdW radius assignment ----
    {
        std::printf("\n--- B.4: VdW radius ---\n");

        auto bead = make_isotropic_bead(1.0, 1.0, 1.0);
        auto frag = make_water_fragment();
        auto rec = build_visual_record(bead, 0, &frag);

        check(std::abs(rec.source_fragment.atoms[0].radius - 1.52f) < 0.01f,
              "O radius = 1.52 A");
        check(std::abs(rec.source_fragment.atoms[1].radius - 1.20f) < 0.01f,
              "H radius = 1.20 A");
    }

    // ---- B.5: Center of mass populated ----
    {
        std::printf("\n--- B.5: Centers populated ---\n");

        auto bead = make_isotropic_bead(1.0, 1.0, 1.0);
        auto frag = make_water_fragment();
        auto rec = build_visual_record(bead, 0, &frag);

        // COM should be near the oxygen (heavier)
        check(std::abs(rec.source_fragment.center_of_mass.x) < 0.5,
              "COM.x near origin");
        double com_len = std::sqrt(
            rec.source_fragment.center_of_mass.x * rec.source_fragment.center_of_mass.x +
            rec.source_fragment.center_of_mass.y * rec.source_fragment.center_of_mass.y +
            rec.source_fragment.center_of_mass.z * rec.source_fragment.center_of_mass.z);
        check(com_len < 1.0, "COM is close to origin");
    }

    // ---- B.6: No fragment → not populated ----
    {
        std::printf("\n--- B.6: No fragment ---\n");

        auto bead = make_isotropic_bead(1.0, 1.0, 1.0);
        auto rec = build_visual_record(bead, 0);

        check(!rec.source_fragment.populated,
              "no fragment: not populated");
        check(rec.source_fragment.atoms.empty(),
              "no fragment: atoms empty");
    }

    // ---- B.7: InternalRefOptions defaults ----
    {
        std::printf("\n--- B.7: InternalRefOptions defaults ---\n");

        InternalRefOptions opts;
        check(opts.show_atoms,              "default: show_atoms = true");
        check(opts.show_bonds,              "default: show_bonds = true");
        check(opts.show_orientation_markers,"default: show_orientation_markers = true");
        check(opts.element_coloring,        "default: element_coloring = true");
        check(std::abs(opts.atom_scale - 0.25f) < 1e-5f,
              "default: atom_scale = 0.25");
        check(std::abs(opts.bond_width - 1.5f) < 1e-5f,
              "default: bond_width = 1.5");
    }

    // ---- B.8: VdW radius lookup coverage ----
    {
        std::printf("\n--- B.8: VdW radius coverage ---\n");

        // Verify distinct radii for different elements
        check(std::abs(vdw_radius(1) - 1.20f) < 0.01f,  "vdw H = 1.20");
        check(std::abs(vdw_radius(6) - 1.70f) < 0.01f,  "vdw C = 1.70");
        check(std::abs(vdw_radius(7) - 1.55f) < 0.01f,  "vdw N = 1.55");
        check(std::abs(vdw_radius(8) - 1.52f) < 0.01f,  "vdw O = 1.52");
        check(vdw_radius(99) > 0.0f, "unknown element has positive radius");
    }
}

// ============================================================================
// Phase C — ViewMode Coverage
// ============================================================================

static void phase_c_view_mode() {
    std::printf("\n=== Phase C: ViewMode Coverage ===\n");

    // ---- C.1: All view mode names ----
    {
        std::printf("\n--- C.1: ViewMode names ---\n");

        check(std::strcmp(view_mode_name(ViewMode::Shell), "Shell") == 0,
              "Shell name");
        check(std::strcmp(view_mode_name(ViewMode::Scaffold), "Scaffold") == 0,
              "Scaffold name");
        check(std::strcmp(view_mode_name(ViewMode::SurfaceState), "Surface State") == 0,
              "SurfaceState name");
        check(std::strcmp(view_mode_name(ViewMode::InternalRef), "Internal Ref") == 0,
              "InternalRef name");
        check(std::strcmp(view_mode_name(ViewMode::Skeleton), "Skeleton") == 0,
              "Skeleton name");
        check(std::strcmp(view_mode_name(ViewMode::Cutaway), "Cutaway") == 0,
              "Cutaway name");
        check(std::strcmp(view_mode_name(ViewMode::Residual), "Residual") == 0,
              "Residual name");
        check(std::strcmp(view_mode_name(ViewMode::Comparison), "Comparison") == 0,
              "Comparison name");
    }

    // ---- C.2: SurfaceStateChannel names ----
    {
        std::printf("\n--- C.2: SurfaceStateChannel names ---\n");

        check(std::strcmp(surface_state_channel_name(SurfaceStateChannel::Steric), "Steric") == 0,
              "Steric channel name");
        check(std::strcmp(surface_state_channel_name(SurfaceStateChannel::Electrostatic), "Electrostatic") == 0,
              "Electrostatic channel name");
        check(std::strcmp(surface_state_channel_name(SurfaceStateChannel::Dispersion), "Dispersion") == 0,
              "Dispersion channel name");
        check(std::strcmp(surface_state_channel_name(SurfaceStateChannel::Synthesized), "Synthesized") == 0,
              "Synthesized channel name");
    }

    // ---- C.3: Default record fields ----
    {
        std::printf("\n--- C.3: Default record fields ---\n");

        BeadVisualRecord rec;
        check(rec.active_surface_channel == SurfaceStateChannel::Steric,
              "default channel = Steric");
        check(std::abs(rec.surface_state_opacity - 0.85f) < 1e-5f,
              "default opacity = 0.85");
        check(std::abs(rec.radial_exaggeration) < 1e-5f,
              "default radial_exaggeration = 0");
    }
}

// ============================================================================
// Phase D — Builder Integration
// ============================================================================

static void phase_d_builder_integration() {
    std::printf("\n=== Phase D: Builder Integration ===\n");

    // ---- D.1: Full build with descriptor + fragment ----
    {
        std::printf("\n--- D.1: Full build ---\n");

        auto bead = make_anisotropic_bead();
        auto frag = make_water_fragment();

        EnvironmentState env;
        env.eta = 0.75;
        env.rho = 3.5;
        env.target_f = 0.8;
        env.P2 = 0.6;
        env.C = 4.0;

        auto rec = build_visual_record(bead, 42, &frag, &env);

        // Identity
        check(rec.bead_id == 42, "bead_id = 42");

        // Layer 2
        check(rec.surface_state_grid.populated,
              "Layer 2 grid populated");
        check(rec.has_descriptor,
              "has_descriptor = true");

        // Layer 3
        check(rec.source_fragment.populated,
              "Layer 3 fragment populated");
        check(static_cast<int>(rec.source_fragment.atoms.size()) == 3,
              "Layer 3: 3 atoms");

        // Environment
        check(std::abs(rec.eta - 0.75) < 1e-10,
              "environment eta = 0.75");
        check(std::abs(rec.rho - 3.5) < 1e-10,
              "environment rho = 3.5");
        check(std::abs(rec.coordination_C - 4.0) < 1e-10,
              "environment C = 4.0");
    }

    // ---- D.2: Layer 2 and descriptor field consistency ----
    {
        std::printf("\n--- D.2: L2 vs descriptor field consistency ---\n");

        auto bead = make_isotropic_bead(2.0, 1.0, 0.5);
        auto rec = build_visual_record(bead, 0);

        // Both legacy patch samples and L2 grid should be populated
        check(!rec.descriptor_field.empty(),
              "legacy descriptor_field populated");
        check(rec.surface_state_grid.populated,
              "L2 grid populated");

        // The L2 grid has finer resolution
        check(static_cast<int>(rec.surface_state_grid.vertices.size()) >
              static_cast<int>(rec.descriptor_field.size()),
              "L2 grid has more vertices than legacy patch samples");
    }

    // ---- D.3: SH evaluation consistency ----
    {
        std::printf("\n--- D.3: SH evaluation consistency ---\n");

        // Verify that evaluate_sh_expansion_at produces consistent results
        // with the grid values stored in the record
        auto bead = make_isotropic_bead(2.0, 1.0, 0.5);
        auto rec = build_visual_record(bead, 0);
        const auto& grid = rec.surface_state_grid;

        // Pick a vertex and re-evaluate manually
        const auto& v = grid.at(12, 24);  // Mid-latitude vertex
        double manual_steric = evaluate_sh_expansion_at(
            bead.unified->steric.coeffs,
            bead.unified->steric.l_max,
            v.theta, v.phi);

        check(std::abs(v.steric - manual_steric) < 1e-12,
              "grid steric matches manual SH evaluation");
    }

    // ---- D.4: Bead position passed through ----
    {
        std::printf("\n--- D.4: Position passthrough ---\n");

        auto bead = make_anisotropic_bead();  // position = (1, 2, 3)
        auto rec = build_visual_record(bead, 0);

        check(std::abs(rec.center_world.x - 1.0) < 1e-10,
              "center_world.x = 1.0");
        check(std::abs(rec.center_world.y - 2.0) < 1e-10,
              "center_world.y = 2.0");
        check(std::abs(rec.center_world.z - 3.0) < 1e-10,
              "center_world.z = 3.0");
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::printf("Multi-Layer Bead Visualization Model — Unit Tests\n");
    std::printf("Layer 2: Effective Surface-State  |  Layer 3: Internal Reference\n");
    std::printf("================================================================\n");

    phase_a_surface_state_grid();
    phase_b_internal_reference();
    phase_c_view_mode();
    phase_d_builder_integration();

    std::printf("\n================================================================\n");
    std::printf("Bead Layers Results: %d passed, %d failed, %d total\n",
                g_pass, g_fail, g_pass + g_fail);

    return g_fail > 0 ? 1 : 0;
}
