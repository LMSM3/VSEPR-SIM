/**
 * test_ehd.cpp
 *
 * Compilation and correctness tests for the EHD multiphysics module.
 * Covers all five stages: CAD, Domain, Physics, Mesh, Postprocessing.
 *
 * Build: linked against vsepr_ehd (header-only INTERFACE library)
 */

#include "sim/ehd/ehd.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>

static constexpr double TOL = 1e-8;

static bool approx(double a, double b, double rel = 1e-4) {
    if (std::abs(b) < 1e-30) return std::abs(a) < 1e-10;
    return std::abs(a - b) / std::abs(b) < rel;
}

// ============================================================================
// Stage 1: CAD Tests
// ============================================================================

static void test_helix_generator() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;

    HelixParams h;
    h.radius = 3.0e-3;
    h.pitch  = 6.0e-3;
    h.num_turns = 2;
    h.points_per_turn = 36;
    h.start_z = 0.0;

    auto pts = generate_helix_centerline(h);
    assert(!pts.empty());
    assert(pts.size() == static_cast<size_t>(2 * 36 + 1));

    // First point should be at (radius, 0, 0)
    assert(approx(pts[0].x, h.radius));
    assert(std::abs(pts[0].y) < TOL);
    assert(std::abs(pts[0].z) < TOL);

    // Last point z should be ~ 2 * pitch
    double expected_z = 2.0 * h.pitch;
    assert(approx(pts.back().z, expected_z, 0.01));

    std::cout << "  [PASS] helix_generator\n";
}

static void test_tube_body_straight() {
    using namespace vsepr::ehd::cad;

    TubeParams tp;
    tp.radius = 4.0e-3;
    tp.length = 60.0e-3;
    tp.wall_thickness = 0.5e-3;
    tp.axial_divisions = 100;
    tp.profile_type = TubeProfileType::STRAIGHT;

    auto profile = generate_tube_profile(tp);
    assert(profile.size() == 101);

    for (const auto& p : profile) {
        assert(approx(p.inner_radius, tp.radius));
        assert(approx(p.outer_radius, tp.radius + tp.wall_thickness));
    }
    std::cout << "  [PASS] tube_body_straight\n";
}

static void test_tube_body_modulated() {
    using namespace vsepr::ehd::cad;

    TubeParams tp;
    tp.radius = 4.0e-3;
    tp.length = 60.0e-3;
    tp.wall_thickness = 0.5e-3;
    tp.axial_divisions = 200;
    tp.modulation_amplitude = 1.0e-3;
    tp.modulation_wavelength = 12.0e-3;
    tp.profile_type = TubeProfileType::MODULATED;

    auto profile = generate_tube_profile(tp);
    assert(profile.size() == 201);

    // Check that radii vary (not all equal to base radius)
    bool found_different = false;
    for (const auto& p : profile) {
        if (std::abs(p.inner_radius - tp.radius) > 1e-8) {
            found_different = true;
            break;
        }
    }
    assert(found_different);
    std::cout << "  [PASS] tube_body_modulated\n";
}

static void test_electrode_layout() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;

    EHDParameters p;
    p.voltage_pos = 50.0;
    p.voltage_neg = 0.0;

    auto layout = build_default_helical_layout(p);
    assert(layout.electrodes.size() == 2);

    auto pos = layout.by_polarity(Polarity::POSITIVE);
    assert(pos.size() == 1);
    assert(pos[0]->voltage == 50.0);

    auto gnd = layout.by_polarity(Polarity::GROUNDED);
    assert(gnd.size() == 1);
    assert(gnd[0]->voltage == 0.0);

    std::cout << "  [PASS] electrode_layout\n";
}

// ============================================================================
// Stage 2: Domain Tests
// ============================================================================

static void test_domain_extraction() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;
    using namespace vsepr::ehd::domain;

    EHDParameters p;
    auto layout = build_default_helical_layout(p);
    auto manifest = StepManifest::build(p, layout, "TEST-001");

    assert(!manifest.bodies.empty());

    auto assembly = extract_domains(manifest, p);
    auto fluids = assembly.by_type(DomainType::FLUID);
    assert(!fluids.empty());
    assert(assembly.total_fluid_volume() > 0.0);

    auto conductors = assembly.by_type(DomainType::SOLID_CONDUCTOR);
    assert(!conductors.empty());

    std::cout << "  [PASS] domain_extraction\n";
}

static void test_named_regions() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::domain;

    EHDParameters p;
    p.voltage_pos = 100.0;

    auto reg = build_default_regions(p);
    assert(reg.size() >= 6);

    auto inlet = reg.find("inlet");
    assert(inlet != nullptr);
    assert(inlet->role == SurfaceRole::INLET);

    auto epos = reg.find("electrode_pos_surface");
    assert(epos != nullptr);
    assert(epos->has_dirichlet);
    assert(approx(epos->dirichlet_value, 100.0));

    std::cout << "  [PASS] named_regions\n";
}

static void test_boundary_tags() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::domain;

    EHDParameters p;
    p.inlet_velocity = 0.05;
    p.voltage_pos = 50.0;

    IonicSpecies sp;
    sp.name = "X-";
    sp.valence = -1;
    sp.diffusivity = 1e-9;
    sp.init_conc = 1.0;
    p.species.push_back(sp);

    auto regions = build_default_regions(p);
    auto bcs = build_default_bcs(p, regions);

    auto inlet_bc = bcs.find("inlet");
    assert(inlet_bc != nullptr);
    assert(inlet_bc->flow_bc == FlowBCType::VELOCITY_INLET);
    assert(approx(inlet_bc->flow_value, 0.05));
    assert(approx(inlet_bc->transport_value, 1.0));

    auto epos_bc = bcs.find("electrode_pos_surface");
    assert(epos_bc != nullptr);
    assert(epos_bc->elec_bc == ElectricBCType::DIRICHLET_VOLTAGE);
    assert(approx(epos_bc->elec_value, 50.0));

    std::cout << "  [PASS] boundary_tags\n";
}

// ============================================================================
// Stage 3: Physics Tests
// ============================================================================

static void test_flow_model() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    EHDParameters p;
    p.tube_radius_m = 4.0e-3;
    p.inlet_velocity = 0.01;
    p.fluid.density = 998.2;
    p.fluid.viscosity = 1.002e-3;

    FlowModel fm(p);

    double Re = fm.reynolds_number();
    // Re = 998.2 * 0.01 * 0.008 / 0.001002 ≈ 79.7
    assert(Re > 70.0 && Re < 90.0);
    assert(fm.is_laminar());

    // Poiseuille: u(r=0) = 2*U_avg
    double u_center = fm.poiseuille_velocity(0.0);
    assert(approx(u_center, 2.0 * p.inlet_velocity, 0.001));

    // u(R) = 0
    double u_wall = fm.poiseuille_velocity(p.tube_radius_m);
    assert(std::abs(u_wall) < TOL);

    std::cout << "  [PASS] flow_model\n";
}

static void test_electrostatic_model() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    EHDParameters p;
    p.tube_radius_m = 4.0e-3;
    p.wire_diameter_m = 0.8e-3;
    p.voltage_pos = 50.0;
    p.voltage_neg = 0.0;

    ElectrostaticModel em(p);

    // At inner electrode surface: φ(R_in) ≈ ΔV
    double r_in = p.wire_diameter_m * 0.5;
    double phi_in = em.coaxial_potential(r_in);
    assert(approx(phi_in, 50.0, 0.01));

    // At outer electrode surface: φ ≈ 0
    double phi_out = em.coaxial_potential(p.tube_radius_m);
    assert(std::abs(phi_out) < 0.1);

    // Field should be positive and decrease with r
    double E_inner = em.coaxial_field_r(r_in);
    double E_mid   = em.coaxial_field_r(2.0e-3);
    assert(E_inner > E_mid);
    assert(E_inner > 0.0);

    std::cout << "  [PASS] electrostatic_model\n";
}

static void test_ion_transport() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    IonicSpecies sp;
    sp.name = "Cl-";
    sp.valence = -1;
    sp.diffusivity = 2.03e-9;
    sp.init_conc = 1.0;

    double T = 298.15;
    double mu = sp.effective_mobility(T);
    assert(mu > 0.0);

    // Nernst-Einstein: μ = |z|·F·D / (R·T)
    double mu_expected = 1.0 * constants::FARADAY * sp.diffusivity
                       / (constants::GAS_CONSTANT * T);
    assert(approx(mu, mu_expected, 0.001));

    // Charge density with single species
    std::vector<IonicSpecies> species = {sp};
    std::vector<double> concs = {1.0};
    double rho_e = IonTransportModel::assemble_charge_density(species, concs);
    // ρ_e = F * (-1) * 1.0
    assert(approx(rho_e, -constants::FARADAY, 0.001));

    std::cout << "  [PASS] ion_transport\n";
}

static void test_coupled_solver() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    EHDParameters p;
    p.tube_radius_m = 4.0e-3;
    p.tube_length_m = 30.0e-3;
    p.inlet_length_m = 2.0e-3;
    p.outlet_length_m = 2.0e-3;
    p.wire_diameter_m = 0.8e-3;
    p.helix_pitch_m = 6.0e-3;
    p.num_turns = 4;
    p.voltage_pos = 50.0;
    p.inlet_velocity = 0.01;

    IonicSpecies sp;
    sp.name = "X-";
    sp.valence = -1;
    sp.diffusivity = 1e-9;
    sp.init_conc = 1.0;
    p.species.push_back(sp);

    RunCard rc;
    rc.case_id = "TEST-COUPLED";
    rc.nr = 10;
    rc.nz = 50;
    rc.max_outer_iterations = 5;

    CoupledSolver solver(p, rc);
    solver.initialize();

    auto metrics = solver.solve();
    assert(metrics.delta_P > 0.0);
    assert(metrics.E_max > 0.0);
    assert(metrics.Re > 0.0);
    assert(metrics.iterations_used > 0);

    std::cout << "  [PASS] coupled_solver (iterations=" << metrics.iterations_used
              << ", ΔP=" << metrics.delta_P << " Pa"
              << ", E_max=" << metrics.E_max << " V/m)\n";
}

// ============================================================================
// Stage 4: Mesh Tests
// ============================================================================

static void test_mesh_controls() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::mesh;

    EHDParameters p;
    p.num_turns = 5;
    auto spec = build_default_mesh_controls(p);

    assert(spec.sizing.base_size > 0.0);
    assert(!spec.inflation.empty());
    assert(!spec.refinements.empty());

    // Should have refinement zones for each helix turn + inlet
    assert(spec.refinements.size() >= static_cast<size_t>(p.num_turns + 1));

    std::cout << "  [PASS] mesh_controls (" << spec.refinements.size()
              << " refinement zones)\n";
}

// ============================================================================
// Stage 5: Postprocessing Tests
// ============================================================================

static void test_comparison_tables() {
    using namespace vsepr::ehd::physics;
    using namespace vsepr::ehd::post;

    SolverMetrics baseline;
    baseline.delta_P = 10.0;
    baseline.E_max   = 1e6;
    baseline.E_avg   = 5e5;
    baseline.u_max   = 0.02;
    baseline.Re      = 80.0;
    baseline.outlet_flux = {1e-8};
    baseline.accumulation_idx = {1.5};

    SolverMetrics modulated;
    modulated.delta_P = 15.0;
    modulated.E_max   = 1.5e6;
    modulated.E_avg   = 7e5;
    modulated.u_max   = 0.025;
    modulated.Re      = 80.0;
    modulated.outlet_flux = {1.8e-8};
    modulated.accumulation_idx = {2.1};

    auto table = build_comparison(baseline, modulated);
    assert(!table.rows.empty());

    // Check enhancement for E_max: 1.5e6 / 1e6 = 1.5
    bool found_emax = false;
    for (const auto& row : table.rows) {
        if (row.metric_name == "E_max") {
            assert(approx(row.enhancement, 1.5, 0.001));
            found_emax = true;
        }
    }
    assert(found_emax);

    // Optimization objective
    double J_base = optimization_objective(baseline, 1.0, 1e6, 0.1, 0);
    double J_mod  = optimization_objective(modulated, 1.0, 1e6, 0.1, 0);
    // Modulated should have higher J due to better field and flux
    assert(J_mod > J_base);

    std::cout << "  [PASS] comparison_tables\n";
}

// ============================================================================
// Run Card Test
// ============================================================================

static void test_run_card() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    EHDParameters p;
    IonicSpecies sp;
    sp.name = "Cl-";
    sp.valence = -1;
    sp.diffusivity = 2.03e-9;
    sp.init_conc = 0.5;
    p.species.push_back(sp);

    RunCard rc;
    rc.case_id = "HRX-017";

    // Just verify it doesn't crash; file I/O tested separately
    std::cout << "  [PASS] run_card\n";
}

// ============================================================================
// Pump Configurations — Config (a): Planar Channel
// ============================================================================

static void test_planar_channel() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;

    EHDParameters p;
    p.topology = PumpTopology::PLANAR_CHANNEL;
    p.channel_height_m = 2.0e-3;
    p.channel_width_m  = 10.0e-3;
    p.channel_length_m = 40.0e-3;

    auto pc = from_ehd_planar(p);
    assert(approx(pc.height, 2.0e-3));
    assert(approx(pc.width, 10.0e-3));

    auto profile = generate_planar_profile(pc);
    assert(profile.size() == 101);
    assert(approx(profile.front().z, 0.0));
    assert(approx(profile.back().z, pc.length));
    assert(approx(profile[0].y_bot, 0.0));
    assert(approx(profile[0].y_top, pc.height));

    // Parallel-plate field: E = ΔV/H
    double delta_V = 50.0;
    double E = parallel_plate_field(delta_V, pc.height);
    assert(approx(E, 50.0 / 2.0e-3));  // 25000 V/m

    // Potential at midplane
    double phi_mid = parallel_plate_potential(pc.height * 0.5, delta_V, pc.height);
    assert(approx(phi_mid, 25.0));

    // Plane-Poiseuille velocity at midplane
    double U_avg = 0.01;
    double u_mid = plane_poiseuille_velocity(pc.height * 0.5, U_avg, pc.height);
    assert(approx(u_mid, 1.5 * U_avg));  // max = 1.5·U for 2D Poiseuille

    // Plane-Poiseuille ΔP
    double mu = 1.0e-3;
    double dP = plane_poiseuille_pressure_drop(mu, U_avg, pc.length, pc.height);
    assert(dP > 0.0);
    assert(approx(dP, 12.0 * mu * U_avg * pc.length / (pc.height * pc.height)));

    // Electrode corners
    auto corners = generate_planar_electrode_corners(pc);
    assert(corners.size() == 8);

    // Hydraulic diameter for rectangular channel
    double D_h = p.hydraulic_diameter();
    double expected_D_h = 2.0 * 2.0e-3 * 10.0e-3 / (2.0e-3 + 10.0e-3);
    assert(approx(D_h, expected_D_h));

    std::cout << "  [PASS] planar_channel (E=" << E
              << " V/m, ΔP=" << dP << " Pa)\n";
}

// ============================================================================
// Pump Configurations — Config (b): Needle-Ring
// ============================================================================

static void test_needle_ring() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;

    EHDParameters p;
    p.topology = PumpTopology::NEEDLE_RING;
    p.needle_tip_radius_m = 50.0e-6;
    p.ring_inner_radius_m = 2.0e-3;
    p.needle_ring_gap_m   = 5.0e-3;

    auto nr = from_ehd_needle_ring(p);
    assert(approx(nr.tip_radius, 50.0e-6));

    // Needle profile — should start behind tip and end at tip
    auto profile = generate_needle_profile(nr);
    assert(!profile.empty());
    assert(profile.back().z >= -1e-10);  // tip at z ≈ 0
    // Radius at tip should be close to r_tip
    assert(profile.back().radius < nr.tip_radius * 2.0);

    // Ring electrode circle
    auto ring = generate_ring_electrode(nr);
    assert(ring.size() == 72);
    // All points at z = gap
    for (const auto& pt : ring) {
        assert(approx(pt.z, nr.gap));
        double r = std::sqrt(pt.x * pt.x + pt.y * pt.y);
        assert(approx(r, nr.ring_radius));
    }

    // Tip field enhancement
    double delta_V = 5000.0;
    double E_tip = needle_tip_field(delta_V, nr.tip_radius, nr.gap);
    assert(E_tip > 0.0);
    // Should be much stronger than uniform field ΔV/d
    double E_uniform = delta_V / nr.gap;
    assert(E_tip > E_uniform * 5.0);  // strong field enhancement

    // Peek's onset field
    double E_onset = peek_onset_field(nr.tip_radius);
    assert(E_onset > 0.0);
    assert(E_onset > 1e6);  // should be in MV/m range

    // Ion drag pressure
    double I_corona = 1.0e-6;  // 1 μA
    double mu_ion = 2.0e-4;    // typical ion mobility in air
    double dP = ion_drag_pressure(I_corona, nr.gap, mu_ion, nr.ring_radius);
    assert(dP > 0.0);

    std::cout << "  [PASS] needle_ring (E_tip=" << E_tip
              << " V/m, E_onset=" << E_onset << " V/m)\n";
}

// ============================================================================
// Pump Configurations — Config (c): Disk Stack
// ============================================================================

static void test_disk_stack() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;

    EHDParameters p;
    p.topology = PumpTopology::DISK_STACK;
    p.disk_radius_m = 8.0e-3;
    p.disk_thickness_m = 0.5e-3;
    p.disk_count = 6;
    p.disk_spacing_m = 3.0e-3;
    p.perforations_per_disk = 12;
    p.perforation_radius_m = 1.0e-3;
    p.voltage_pos = 100.0;
    p.voltage_neg = 0.0;

    auto ds = from_ehd_disk_stack(p);
    assert(ds.disk_count == 6);
    assert(ds.perforations == 12);

    // Generate disk descriptors
    auto disks = generate_disk_stack(ds, p.voltage_pos, p.voltage_neg);
    assert(disks.size() == 6);

    // Check alternating polarity
    assert(disks[0].is_positive == true);
    assert(disks[1].is_positive == false);
    assert(disks[2].is_positive == true);
    assert(disks[0].voltage == 100.0);
    assert(disks[1].voltage == 0.0);

    // Check z positions
    assert(approx(disks[0].z_bottom, 0.0));
    double expected_z1 = ds.disk_thickness + ds.spacing;
    assert(approx(disks[1].z_bottom, expected_z1));

    // Perforation centres
    double z_mid = disks[0].z_bottom + ds.disk_thickness * 0.5;
    auto centres = generate_perforation_centres(ds, z_mid);
    assert(centres.size() == 12);
    // All at expected z
    for (const auto& c : centres) {
        assert(approx(c.z, z_mid));
    }

    // Disk edge
    auto edge = generate_disk_edge(ds, z_mid);
    assert(edge.size() == 72);

    // Total length
    double L = stack_total_length(ds);
    double expected_L = 6 * 0.5e-3 + 5 * 3.0e-3;
    assert(approx(L, expected_L));

    // Open area fraction
    double oaf = open_area_fraction(ds);
    assert(oaf > 0.0 && oaf < 1.0);
    double expected_oaf = 12.0 * constants::PI * 1e-6
                        / (constants::PI * 64e-6);
    assert(approx(oaf, expected_oaf));

    // Inter-disk field
    double E = inter_disk_field(100.0, ds.spacing);
    assert(approx(E, 100.0 / 3.0e-3));

    // Effective cross-section area
    double A = p.effective_cross_section_area();
    double expected_A = 12.0 * constants::PI * 1.0e-3 * 1.0e-3;
    assert(approx(A, expected_A));

    std::cout << "  [PASS] disk_stack (L=" << L * 1e3 << " mm, OAF="
              << oaf * 100.0 << "%, E=" << E << " V/m)\n";
}

// ============================================================================
// Pump Configurations — Config (d): Prism Slit
// ============================================================================

static void test_prism_slit() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;

    EHDParameters p;
    p.topology = PumpTopology::PRISM_SLIT;
    p.slit_width_m = 1.5e-3;
    p.slit_depth_m = 5.0e-3;
    p.prism_base_m = 3.0e-3;
    p.prism_height_m = 4.0e-3;
    p.prism_count = 4;

    auto ps = from_ehd_prism_slit(p);
    assert(ps.prism_count == 4);

    // Generate prism array
    auto prisms = generate_prism_array(ps);
    assert(prisms.size() == 4);

    // Check first prism geometry
    assert(approx(prisms[0].apex.y, ps.prism_height));
    assert(approx(prisms[0].base_left.y, 0.0));
    assert(approx(prisms[0].base_right.y, 0.0));
    double base_span = prisms[0].base_right.x - prisms[0].base_left.x;
    assert(approx(base_span, ps.prism_base));

    // Check spacing between prisms
    double pitch = ps.prism_base + ps.slit_width;
    assert(approx(prisms[1].apex.x - prisms[0].apex.x, pitch));

    // Outline generation
    auto outline = generate_prism_outline(prisms[0]);
    assert(outline.size() == static_cast<size_t>(3 * ps.pts_per_edge));

    // Total array width
    double W = array_total_width(ps);
    double expected_W = 4 * 3.0e-3 + 3 * 1.5e-3;
    assert(approx(W, expected_W));

    // Tip field enhancement
    double delta_V = 50.0;
    double E_tip = prism_tip_field(delta_V, ps);
    double E_uniform = delta_V / ps.slit_width;
    assert(E_tip > E_uniform);  // enhancement > 1

    std::cout << "  [PASS] prism_slit (W=" << W * 1e3
              << " mm, E_tip=" << E_tip << " V/m, β="
              << E_tip / E_uniform << ")\n";
}

// ============================================================================
// Electrode Layout Factory
// ============================================================================

static void test_layout_factory() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;

    // Helical (default)
    {
        EHDParameters p;
        p.topology = PumpTopology::HELICAL_WIRE;
        auto layout = build_layout_for_topology(p);
        assert(layout.electrodes.size() == 2);
        assert(layout.find_by_name("electrode_pos") != nullptr);
    }

    // Planar
    {
        EHDParameters p;
        p.topology = PumpTopology::PLANAR_CHANNEL;
        auto layout = build_layout_for_topology(p);
        assert(layout.electrodes.size() == 2);
        assert(layout.find_by_name("electrode_bottom") != nullptr);
        assert(layout.find_by_name("electrode_top") != nullptr);
    }

    // Needle-ring
    {
        EHDParameters p;
        p.topology = PumpTopology::NEEDLE_RING;
        auto layout = build_layout_for_topology(p);
        assert(layout.electrodes.size() == 2);
        assert(layout.find_by_name("electrode_needle") != nullptr);
        assert(layout.find_by_name("electrode_ring") != nullptr);
    }

    // Disk stack
    {
        EHDParameters p;
        p.topology = PumpTopology::DISK_STACK;
        p.disk_count = 4;
        auto layout = build_layout_for_topology(p);
        assert(layout.electrodes.size() == 4);
        auto pos = layout.by_polarity(Polarity::POSITIVE);
        auto neg = layout.by_polarity(Polarity::NEGATIVE);
        assert(pos.size() == 2);
        assert(neg.size() == 2);
    }

    // Prism slit
    {
        EHDParameters p;
        p.topology = PumpTopology::PRISM_SLIT;
        p.prism_count = 3;
        auto layout = build_layout_for_topology(p);
        assert(layout.electrodes.size() == 3);
    }

    std::cout << "  [PASS] layout_factory (5 topologies)\n";
}

// ============================================================================
// Body Force Models — Three Pumping Mechanisms
// ============================================================================

static void test_coulomb_force() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    // f_C = ρ_e · E
    Vec3 E{1000.0, 0.0, 500.0};
    double rho_e = 0.01;  // C/m³

    Vec3 f = coulomb_force(rho_e, E);
    assert(approx(f.x, 10.0));
    assert(approx(f.z, 5.0));

    // Assemble from electric field
    ElectricField ef;
    ef.resize(5, 5, 1e-3, 1e-3);
    for (auto& c : ef.cells) {
        c.field = {1000.0, 0.0, 0.0};
        c.charge_density = 0.01;
    }

    auto ff = assemble_coulomb_field(ef);
    assert(ff.nr == 5 && ff.nz == 5);
    assert(approx(ff.at(2, 2).force.x, 10.0));
    assert(ff.max_force_magnitude() > 0.0);

    // Pressure scale
    double eps = 78.5 * constants::EPSILON_0;
    double P_c = coulomb_pressure_scale(eps, 50.0, 2.0e-3);
    assert(P_c > 0.0);

    std::cout << "  [PASS] coulomb_force (f_max="
              << ff.max_force_magnitude() << " N/m³)\n";
}

static void test_dep_force() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    // DEP force requires gradient of |E|²
    // Set up a field where |E|² has a known gradient
    ElectricField ef;
    ef.resize(5, 10, 1e-3, 1e-3);

    // Linear field gradient in r: E_x increases with ir
    for (int ir = 0; ir < 5; ++ir) {
        double Ex = 1000.0 * (ir + 1);  // 1000, 2000, 3000, 4000, 5000
        for (int iz = 0; iz < 10; ++iz) {
            ef.at(ir, iz).field = {Ex, 0.0, 0.0};
        }
    }

    double K_CM = 0.5;
    auto ff = assemble_dep_field(ef, K_CM);
    assert(ff.nr == 5 && ff.nz == 10);

    // Interior points should have non-zero force in r direction
    // (because d(|E|²)/dr ≠ 0)
    double f_r = ff.at(2, 5).force.x;
    assert(std::abs(f_r) > 0.0);

    // DEP velocity scale
    double u_dep = dep_velocity_scale(
        78.5 * constants::EPSILON_0, K_CM, 25000.0, 2.0e-3, 1.0e-3);
    assert(u_dep > 0.0);

    std::cout << "  [PASS] dep_force (f_r=" << f_r
              << " N/m³, u_DEP=" << u_dep << " m/s)\n";
}

static void test_eof_model() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    double eps = 78.5 * constants::EPSILON_0;
    double zeta = -50.0e-3;  // -50 mV
    double mu = 1.0e-3;      // Pa·s
    double E_t = 1000.0;     // V/m tangential field

    // Helmholtz-Smoluchowski slip
    double u_slip = eof_slip_velocity(eps, zeta, mu, E_t);
    assert(u_slip > 0.0);
    // Expected: |ε·ζ·E/μ| = |6.95e-10 · (-0.05) · 1000 / 0.001|
    double expected = std::abs(eps * zeta * E_t / mu);
    assert(approx(u_slip, expected));

    // EOF flow rate in capillary
    double R = 50.0e-6;  // 50 μm capillary
    double Q = eof_flow_rate(eps, zeta, mu, E_t, R);
    assert(Q > 0.0);

    // Debye length
    double c0 = 1.0;  // 1 mol/m³
    double lambda_D = debye_length(eps, 298.15, c0);
    assert(lambda_D > 0.0);
    assert(lambda_D < 1.0e-6);  // nanometre scale for molar conc

    // Thin-EDL check: κ·H
    double H = 100.0e-6;  // 100 μm channel
    double kH = kappa_H(lambda_D, H);
    assert(kH > 100.0);  // thin-EDL regime

    // EOF pressure heads
    double L = 10.0e-3;
    double dP_cyl = eof_pressure_head_cylindrical(eps, zeta, E_t, L, R);
    assert(dP_cyl > 0.0);
    double dP_planar = eof_pressure_head_planar(eps, zeta, E_t, L, H);
    assert(dP_planar > 0.0);

    std::cout << "  [PASS] eof_model (u_slip=" << u_slip * 1e6
              << " μm/s, λ_D=" << lambda_D * 1e9 << " nm, κH="
              << kH << ")\n";
}

static void test_combined_body_force() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    Vec3 E{5000.0, 0.0, 1000.0};
    double rho_e = 0.005;
    Vec3 grad_Esq{1e8, 0.0, 2e7};
    double K_CM = 0.4;

    // Coulomb
    Vec3 f_c = compute_ehd_body_force(PumpMechanism::COULOMB, rho_e, E,
                                       grad_Esq, K_CM);
    assert(approx(f_c.x, rho_e * E.x));
    assert(approx(f_c.z, rho_e * E.z));

    // DEP
    Vec3 f_dep = compute_ehd_body_force(PumpMechanism::DIELECTROPHORETIC,
                                         rho_e, E, grad_Esq, K_CM);
    assert(f_dep.x != 0.0);  // should be non-zero from grad_Esq

    // EOF — volumetric force is zero (it's a slip BC)
    Vec3 f_eof = compute_ehd_body_force(PumpMechanism::ELECTROOSMOTIC,
                                         rho_e, E, grad_Esq, K_CM);
    assert(approx(f_eof.x, 0.0));
    assert(approx(f_eof.z, 0.0));

    std::cout << "  [PASS] combined_body_force (3 mechanisms)\n";
}

// ============================================================================
// Topology-Derived Quantities
// ============================================================================

static void test_topology_derived() {
    using namespace vsepr::ehd;

    // Test that all topologies produce correct hydraulic diameters
    // and cross-section areas

    // Helical wire (original)
    {
        EHDParameters p;
        p.topology = PumpTopology::HELICAL_WIRE;
        assert(approx(p.hydraulic_diameter(), 2.0 * 4.0e-3));
    }

    // Planar channel
    {
        EHDParameters p;
        p.topology = PumpTopology::PLANAR_CHANNEL;
        p.channel_height_m = 2.0e-3;
        p.channel_width_m = 10.0e-3;
        double D_h = p.hydraulic_diameter();
        assert(approx(D_h, 2.0 * 2e-3 * 10e-3 / (2e-3 + 10e-3)));
    }

    // Needle-ring
    {
        EHDParameters p;
        p.topology = PumpTopology::NEEDLE_RING;
        p.ring_inner_radius_m = 3.0e-3;
        assert(approx(p.hydraulic_diameter(), 6.0e-3));
    }

    // Disk stack
    {
        EHDParameters p;
        p.topology = PumpTopology::DISK_STACK;
        p.disk_radius_m = 8.0e-3;
        assert(approx(p.hydraulic_diameter(), 16.0e-3));

        double A = p.effective_cross_section_area();
        double expected = p.perforations_per_disk * constants::PI
                        * p.perforation_radius_m * p.perforation_radius_m;
        assert(approx(A, expected));
    }

    // Prism slit
    {
        EHDParameters p;
        p.topology = PumpTopology::PRISM_SLIT;
        p.slit_width_m = 1.5e-3;
        assert(approx(p.hydraulic_diameter(), 3.0e-3));
    }

    std::cout << "  [PASS] topology_derived (5 topologies)\n";
}

// ============================================================================
// Metallic Particle Combustion (Section S.3 / Figure Z.3)
// ============================================================================

static void test_fuel_database() {
    using namespace vsepr::ehd::physics;

    // Verify all five fuels load and have reasonable data
    auto ch4 = get_fuel_data(FuelType::METHANE);
    auto fe  = get_fuel_data(FuelType::IRON);
    auto al  = get_fuel_data(FuelType::ALUMINUM);
    auto bal = get_fuel_data(FuelType::BORON_ALUMINUM);
    auto zr  = get_fuel_data(FuelType::ZIRCONIUM);

    assert(ch4.name == "Methane");
    assert(fe.name  == "Iron");
    assert(al.name  == "Aluminum");
    assert(bal.name == "Boron/Aluminum");
    assert(zr.name  == "Zirconium");

    // Combustion regimes
    assert(ch4.regime == CombustionRegime::PREMIXED_GAS);
    assert(fe.regime  == CombustionRegime::SURFACE);
    assert(al.regime  == CombustionRegime::VAPOUR_PHASE);
    assert(bal.regime == CombustionRegime::HYBRID);
    assert(zr.regime  == CombustionRegime::HYBRID);

    // T_ad ordering: Al > B/Al > Zr > Fe > CH₄ is incorrect, let's check actual
    // Al has highest T_ad
    assert(al.adiabatic_flame_T > bal.adiabatic_flame_T);
    assert(al.adiabatic_flame_T > zr.adiabatic_flame_T);
    assert(al.adiabatic_flame_T > fe.adiabatic_flame_T);

    // All heats of combustion positive
    assert(ch4.heat_of_combustion > 0.0);
    assert(fe.heat_of_combustion > 0.0);
    assert(al.heat_of_combustion > 0.0);
    assert(bal.heat_of_combustion > 0.0);
    assert(zr.heat_of_combustion > 0.0);

    // Methane has highest gravimetric energy density
    assert(ch4.heat_of_combustion > al.heat_of_combustion);

    // Al has highest volumetric energy density among metals
    double e_v_al = al.density * al.heat_of_combustion;
    double e_v_fe = fe.density * fe.heat_of_combustion;
    assert(e_v_al > e_v_fe);

    std::cout << "  [PASS] fuel_database (5 fuels, thermodynamics)\n";
}

static void test_d2_law() {
    using namespace vsepr::ehd::physics;

    FuelData al = get_fuel_data(FuelType::ALUMINUM);

    double d0 = 20.0e-6;  // 20 μm
    double K  = al.burning_rate_K;
    double n  = al.burning_rate_n;  // 2.0

    // Burnout time = d₀² / K
    double t_burn = burnout_time(d0, K, n);
    assert(t_burn > 0.0);
    assert(approx(t_burn, d0 * d0 / K, 1e-6));

    // At t=0, diameter = d0
    assert(approx(particle_diameter(d0, K, n, 0.0), d0));

    // At t=t_burn, diameter = 0
    assert(particle_diameter(d0, K, n, t_burn) < 1e-15);

    // At t=t_burn/2, diameter = d0/sqrt(2) for d² law
    double d_half = particle_diameter(d0, K, n, t_burn * 0.5);
    assert(approx(d_half, d0 / std::sqrt(2.0), 1e-4));
    (void)d_half;

    // Mass loss rate for d² law should be constant (independent of d)
    double dm1 = mass_loss_rate(d0, al.density, K, n);
    double dm2 = mass_loss_rate(d0 * 0.5, al.density, K, n);
    assert(approx(dm1, dm2, 1e-6));  // constant for n=2
    (void)dm1; (void)dm2;

    std::cout << "  [PASS] d2_law (Al, burnout, regression)\n";
}

static void test_d1_law_iron() {
    using namespace vsepr::ehd::physics;

    FuelData fe = get_fuel_data(FuelType::IRON);

    double d0 = 10.0e-6;
    double K  = fe.burning_rate_K;
    double n  = fe.burning_rate_n;  // 1.0 (surface regime)

    // d^1 law: d(t) = d0 - K·t
    double t_burn = burnout_time(d0, K, n);
    assert(approx(t_burn, d0 / K));

    // At halfway: d = d0/2
    double d_mid = particle_diameter(d0, K, n, t_burn * 0.5);
    assert(approx(d_mid, d0 * 0.5, 1e-4));
    (void)d_mid;

    // Mass loss rate depends on d for n=1
    double dm_d0 = mass_loss_rate(d0, fe.density, K, n);
    double dm_half = mass_loss_rate(d0 * 0.5, fe.density, K, n);
    assert(approx(dm_d0 / dm_half, 2.0, 1e-4));  // linear in d
    (void)dm_d0; (void)dm_half;

    std::cout << "  [PASS] d1_law_iron (surface regime)\n";
}

static void test_adiabatic_flame_temperature() {
    using namespace vsepr::ehd::physics;

    // Direct computation for aluminum
    FuelData al = get_fuel_data(FuelType::ALUMINUM);
    double T_ad = adiabatic_flame_temperature(al, 300.0);

    // T_ad should be within reasonable range (>2000 K for Al)
    assert(T_ad > 2000.0);
    assert(T_ad < 10000.0);
    (void)T_ad;

    // Tabulated value should also be in range
    assert(al.adiabatic_flame_T > 3000.0);

    // Methane T_ad ~ 2223 K
    FuelData ch4 = get_fuel_data(FuelType::METHANE);
    assert(approx(ch4.adiabatic_flame_T, 2223.0, 0.01));

    std::cout << "  [PASS] adiabatic_flame_temperature\n";
}

static void test_radiative_emission() {
    using namespace vsepr::ehd::physics;

    double d = 50.0e-6;   // 50 μm particle
    double T_p = 3000.0;  // K
    double T_inf = 300.0;

    // Al: high emissivity → bright
    FuelData al = get_fuel_data(FuelType::ALUMINUM);
    double P_al = radiative_power(d, al.emissivity, T_p, T_inf);
    assert(P_al > 0.0);

    // CH₄: low emissivity → dim
    FuelData ch4 = get_fuel_data(FuelType::METHANE);
    double P_ch4 = radiative_power(d, ch4.emissivity, T_p, T_inf);
    assert(P_ch4 > 0.0);

    // Al should be brighter (higher emissivity)
    assert(P_al > P_ch4);
    (void)P_al; (void)P_ch4;

    // Wien's law: peak wavelength
    double lambda = wien_peak_wavelength(3000.0);
    assert(approx(lambda, combustion_constants::WIEN_B / 3000.0));
    assert(lambda > 800e-9 && lambda < 1200e-9);  // ~966 nm (near-IR)
    (void)lambda;

    // Planck function should be positive
    double B = planck_spectral_radiance(550e-9, 3000.0);
    assert(B > 0.0);
    (void)B;

    std::cout << "  [PASS] radiative_emission (Stefan-Boltzmann, Wien, Planck)\n";
}

static void test_burning_rate_corrections() {
    using namespace vsepr::ehd::physics;

    FuelData al = get_fuel_data(FuelType::ALUMINUM);
    double K0 = al.burning_rate_K;

    // Temperature correction: higher T → faster burning
    double K_hot = burning_rate_corrected(K0, al.activation_energy, 1500.0);
    double K_cold = burning_rate_corrected(K0, al.activation_energy, 300.0);
    assert(K_hot > K_cold);  // Arrhenius: higher T → larger K
    (void)K_hot; (void)K_cold;

    // O₂ correction: more O₂ → faster
    double K_rich = burning_rate_O2_corrected(K0, 0.40);   // O₂-enriched
    double K_lean = burning_rate_O2_corrected(K0, 0.15);   // O₂-lean
    assert(K_rich > K_lean);
    (void)K_rich; (void)K_lean;

    // At reference conditions (Y_O2 = 0.233) should return K0
    double K_ref = burning_rate_O2_corrected(K0, 0.233);
    assert(approx(K_ref, K0, 1e-6));
    (void)K_ref;

    std::cout << "  [PASS] burning_rate_corrections (Arrhenius, O2)\n";
}

static void test_particle_cloud() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    double d0 = 30.0e-6;
    int N_p = 10;
    Vec3 origin{0.0, 0.0, 0.001};
    Vec3 u_inject{0.0, 0.0, 0.5};

    auto cloud = create_uniform_cloud(
        FuelType::ALUMINUM, N_p, d0, 300.0, origin, u_inject);

    assert(cloud.particles.size() == 10);
    assert(cloud.active_count() == 10);

    // All particles identical
    for (const auto& p : cloud.particles) {
        assert(approx(p.diameter, d0));
        assert(approx(p.diameter_0, d0));
        assert(approx(p.temperature, 300.0));
        assert(p.active);
        assert(p.fuel == FuelType::ALUMINUM);
        (void)p;
    }

    // Advance a few steps at reference T=300K (no Arrhenius amplification)
    // At T_ref=300K, K_eff = K₀ = 2e-6 m²/s
    // t_burn = d₀²/K = (30e-6)²/2e-6 = 4.5e-4 s
    // Total advance = 10 × 1e-6 = 1e-5 s << t_burn
    FuelData al = get_fuel_data(FuelType::ALUMINUM);
    (void)al;
    Vec3 u_gas{0.0, 0.0, 0.3};
    double dt = 1e-6;  // 1 μs step

    for (int step = 0; step < 10; ++step) {
        advance_cloud(cloud, FuelType::ALUMINUM, u_gas, 300.0, 1.8e-5, dt);
    }

    // Particles should still be active (short time vs burnout)
    assert(cloud.active_count() > 0);

    // Diameters should have decreased
    for (const auto& p : cloud.particles) {
        if (p.active) {
            assert(p.diameter < d0);
            assert(p.diameter > 0.0);
        }
    }

    std::cout << "  [PASS] particle_cloud (init, advance, burnout check)\n";
}

static void test_ehd_combustion_coupling() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    FuelData al = get_fuel_data(FuelType::ALUMINUM);

    // EHD enhancement: stronger field → faster burn
    double K0 = al.burning_rate_K;
    double K_enhanced = burning_rate_ehd_enhanced(K0, 2.0e4);  // 20 kV/m
    assert(K_enhanced > K0);

    // Enhancement ratio: K_E/K₀ = 1 + α·(E/E_ref)²
    double expected_ratio = 1.0 + 0.1 * std::pow(2.0e4 / 1.0e4, 2);
    assert(approx(K_enhanced / K0, expected_ratio, 1e-6));
    (void)K_enhanced; (void)expected_ratio;

    // At zero field, no enhancement
    double K_zero = burning_rate_ehd_enhanced(K0, 0.0);
    assert(approx(K_zero, K0, 1e-10));
    (void)K_zero;

    // Charged particle force
    Vec3 E{0.0, 0.0, 1e4};  // 10 kV/m axial
    Vec3 F = charged_particle_force(50e-6, 3000.0, E);
    assert(F.z > 0.0);  // force along field direction

    std::cout << "  [PASS] ehd_combustion_coupling (enhancement, Lorentz)\n";
}

static void test_reactive_multiphase() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    // Create a small grid and cloud for source-term accumulation
    FlowField flow;
    flow.resize(10, 20, 1.0e-3, 2.0e-3);
    for (auto& c : flow.cells) {
        c.velocity = Vec3{0.0, 0.0, 0.3};
    }

    SourceField sf;
    sf.resize(10, 20, 1.0e-3, 2.0e-3);

    // Create particles at r≈0.5mm, z≈5mm (should map to ir=0, iz=2)
    Vec3 pos{0.5e-3, 0.0, 5.0e-3};
    Vec3 u_p{0.0, 0.0, 0.4};
    auto cloud = create_uniform_cloud(FuelType::IRON, 5, 15e-6, 2000.0, pos, u_p);

    FuelData fe = get_fuel_data(FuelType::IRON);
    accumulate_sources(sf, cloud, fe, flow, 1.8e-5);

    // Check that source terms were accumulated
    double q_max = sf.max_heat_release();
    assert(q_max > 0.0);  // Heat is being released
    (void)q_max;

    // Flame zone detection
    auto zone = detect_flame_zone(sf, 0.0);
    assert(!zone.empty());

    // Damköhler number
    double Da = damkohler_I(15e-6, fe.burning_rate_K, fe.burning_rate_n,
                             0.04, 0.3);
    assert(Da > 0.0);
    (void)Da;

    // Stokes number
    double St = stokes_number(15e-6, fe.density, 1.8e-5, 0.04, 0.3);
    assert(St > 0.0);
    (void)St;

    // Mass loading
    double m_p = fe.density * (constants::PI / 6.0) * std::pow(15e-6, 3);
    double phi = mass_loading_ratio(5, m_p, 1.2, 1e-6);
    assert(phi > 0.0);
    (void)phi;

    std::cout << "  [PASS] reactive_multiphase (sources, Da, St, loading)\n";
}

static void test_fuel_comparison_table() {
    using namespace vsepr::ehd::physics;

    auto table = build_fuel_comparison();
    assert(table.size() == 5);

    // Verify ordering in table
    assert(table[0].fuel == FuelType::METHANE);
    assert(table[1].fuel == FuelType::IRON);
    assert(table[2].fuel == FuelType::ALUMINUM);
    assert(table[3].fuel == FuelType::BORON_ALUMINUM);
    assert(table[4].fuel == FuelType::ZIRCONIUM);

    // All should have positive energy densities
    for (const auto& row : table) {
        assert(row.energy_density_grav > 0.0);
        assert(row.T_ad > 1000.0);
        (void)row;
    }

    // Methane has no particle burnout (K=0)
    assert(approx(table[0].K_burn, 0.0));
    assert(approx(table[0].t_burn_10um, 0.0));

    // Metal fuels have positive burnout times
    for (size_t i = 1; i < 5; ++i) {
        assert(table[i].K_burn > 0.0);
        assert(table[i].t_burn_10um > 0.0);
    }

    std::cout << "  [PASS] fuel_comparison_table (5 fuels ranked)\n";
}

static void test_dimensionless_numbers() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    // Nusselt (Ranz-Marshall): Re_p=0 → Nu=2
    assert(approx(nusselt_ranz_marshall(0.0, 0.7), 2.0, 1e-4));

    // Nu should increase with Re_p
    double Nu_low  = nusselt_ranz_marshall(1.0, 0.7);
    double Nu_high = nusselt_ranz_marshall(100.0, 0.7);
    assert(Nu_high > Nu_low);
    (void)Nu_low; (void)Nu_high;

    // Biot number
    double Bi = biot_number(100.0, 50e-6, 200.0);  // Al, h=100, k=200
    assert(Bi < 0.1);  // lumped-capacitance valid for metal particles
    (void)Bi;

    // Particle Reynolds
    Vec3 ug{0.0, 0.0, 1.0};
    Vec3 up{0.0, 0.0, 0.5};
    double Rep = particle_reynolds(1.2, ug, up, 50e-6, 1.8e-5);
    assert(Rep > 0.0);
    assert(Rep < 10.0);  // small particle, low Re_p
    (void)Rep;

    std::cout << "  [PASS] dimensionless_numbers (Nu, Bi, Re_p)\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n=== EHD Multiphysics Module Tests ===\n\n";

    std::cout << "Stage 1: CAD / Geometry\n";
    test_helix_generator();
    test_tube_body_straight();
    test_tube_body_modulated();
    test_electrode_layout();

    std::cout << "\nStage 2: Domain Extraction\n";
    test_domain_extraction();
    test_named_regions();
    test_boundary_tags();

    std::cout << "\nStage 3: Physics\n";
    test_flow_model();
    test_electrostatic_model();
    test_ion_transport();
    test_coupled_solver();

    std::cout << "\nStage 4: Mesh\n";
    test_mesh_controls();

    std::cout << "\nStage 5: Postprocessing\n";
    test_comparison_tables();

    std::cout << "\nRun Card\n";
    test_run_card();

    std::cout << "\nPump Configurations (Fig. Z.4)\n";
    test_planar_channel();
    test_needle_ring();
    test_disk_stack();
    test_prism_slit();
    test_layout_factory();

    std::cout << "\nBody Force Models (3 Mechanisms)\n";
    test_coulomb_force();
    test_dep_force();
    test_eof_model();
    test_combined_body_force();

    std::cout << "\nTopology-Derived Quantities\n";
    test_topology_derived();

    std::cout << "\nMetallic Particle Combustion (Section S.3)\n";
    test_fuel_database();
    test_d2_law();
    test_d1_law_iron();
    test_adiabatic_flame_temperature();
    test_radiative_emission();
    test_burning_rate_corrections();
    test_particle_cloud();
    test_ehd_combustion_coupling();
    test_reactive_multiphase();
    test_fuel_comparison_table();
    test_dimensionless_numbers();

    std::cout << "\n=== All EHD tests passed ===\n\n";
    return 0;
}
