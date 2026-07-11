/**
 * bench_ehd.cpp
 *
 * Performance benchmarks for the EHD multiphysics module.
 * Measures throughput and timing across all five stages:
 *   Stage 1  CAD / Geometry generation
 *   Stage 2  Domain extraction & region tagging
 *   Stage 3  Physics solvers (flow, electrostatic, transport, coupled)
 *   Stage 4  Mesh control specification
 *   Stage 5  Postprocessing (comparison, probes, streamlines, contour)
 *
 * Additional cross-cutting benchmarks:
 *   - Grid-scaling analysis for field initialisation
 *   - Multi-species solver scaling
 *   - Memory footprint estimation
 *   - Parameter sensitivity sweeps (voltage, velocity)
 *   - Determinism verification
 *
 * Build:  cmake --build <dir> --target bench_ehd --config Release
 * Run:    ./bench_ehd
 */

#include "sim/ehd/ehd.hpp"
#include <cassert>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <vector>

// ============================================================================
// Timing Utilities
// ============================================================================

class Timer {
public:
    void start() {
        t0_ = std::chrono::high_resolution_clock::now();
    }

    double elapsed_us() const {
        auto t1 = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(t1 - t0_).count();
    }

    double elapsed_ms() const {
        return elapsed_us() / 1000.0;
    }

    double elapsed_sec() const {
        return elapsed_us() / 1.0e6;
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> t0_;
};

// ============================================================================
// Optimisation Barrier — prevent dead-code elimination in Release builds
// ============================================================================

static volatile double g_sink = 0.0;

template <typename T>
static void do_not_optimise(const T& val) {
    g_sink += static_cast<double>(reinterpret_cast<uintptr_t>(&val) & 0xFF);
}

// ============================================================================
// Statistics Helper
// ============================================================================

struct BenchStats {
    double avg   = 0.0;
    double min   = 0.0;
    double max   = 0.0;
    double stdev = 0.0;
    int    n     = 0;
};

static BenchStats compute_stats(const std::vector<double>& vals) {
    BenchStats s;
    s.n = static_cast<int>(vals.size());
    if (s.n == 0) return s;

    s.min = vals[0];
    s.max = vals[0];
    double sum = 0.0;
    for (double v : vals) {
        sum += v;
        if (v < s.min) s.min = v;
        if (v > s.max) s.max = v;
    }
    s.avg = sum / s.n;

    double var = 0.0;
    for (double v : vals) {
        double d = v - s.avg;
        var += d * d;
    }
    s.stdev = std::sqrt(var / s.n);
    return s;
}

static void print_stats(const char* label, const BenchStats& s,
                        const char* unit = "ms") {
    std::cout << "  " << std::left << std::setw(44) << label
              << std::right << std::fixed << std::setprecision(3)
              << "  avg " << std::setw(10) << s.avg << " " << unit
              << "  min " << std::setw(10) << s.min << " " << unit
              << "  max " << std::setw(10) << s.max << " " << unit
              << "  σ "   << std::setw(8)  << s.stdev << " " << unit
              << "  (n=" << s.n << ")\n";
}

// ============================================================================
// Benchmark Parameters
// ============================================================================

static constexpr int WARMUP = 3;
static constexpr int RUNS   = 50;

static vsepr::ehd::EHDParameters make_default_params(int n_species = 1) {
    using namespace vsepr::ehd;
    EHDParameters p;
    p.tube_radius_m    = 4.0e-3;
    p.tube_length_m    = 30.0e-3;
    p.inlet_length_m   = 2.0e-3;
    p.outlet_length_m  = 2.0e-3;
    p.wire_diameter_m  = 0.8e-3;
    p.helix_pitch_m    = 6.0e-3;
    p.num_turns        = 4;
    p.voltage_pos      = 50.0;
    p.voltage_neg      = 0.0;
    p.inlet_velocity   = 0.01;
    p.fluid.density    = 998.2;
    p.fluid.viscosity  = 1.002e-3;

    for (int s = 0; s < n_species; ++s) {
        IonicSpecies sp;
        sp.name        = "S" + std::to_string(s);
        sp.valence     = ((s % 2) == 0) ? -1 : 1;
        sp.diffusivity = 2.03e-9 * (1.0 + 0.1 * s);
        sp.init_conc   = 1.0;
        p.species.push_back(sp);
    }
    return p;
}

// ============================================================================
// Stage 1: CAD / Geometry Benchmarks
// ============================================================================

static void bench_helix_generation() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;

    auto p = make_default_params();
    auto h = from_ehd(p);
    std::vector<double> times;

    for (int i = 0; i < WARMUP + RUNS; ++i) {
        Timer t; t.start();
        auto pts = generate_helix_centerline(h);
        double elapsed = t.elapsed_ms();
        do_not_optimise(pts);
        if (i >= WARMUP) times.push_back(elapsed);
    }
    print_stats("helix_centerline", compute_stats(times));
}

static void bench_helix_scaling() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;

    int turn_counts[] = {4, 16, 64, 256};
    for (int turns : turn_counts) {
        HelixParams h;
        h.radius    = 3.0e-3;
        h.pitch     = 6.0e-3;
        h.num_turns = turns;
        h.points_per_turn = 72;

        std::vector<double> times;
        for (int i = 0; i < WARMUP + RUNS; ++i) {
            Timer t; t.start();
            auto pts = generate_helix_centerline(h);
            double elapsed = t.elapsed_ms();
            do_not_optimise(pts);
            if (i >= WARMUP) times.push_back(elapsed);
        }
        std::string label = "helix (" + std::to_string(turns) + " turns, "
                          + std::to_string(turns * 72 + 1) + " pts)";
        print_stats(label.c_str(), compute_stats(times));
    }
}

static void bench_wire_section() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;

    Vec3 center{0.003, 0.0, 0.01};
    Vec3 tangent{0.0, 0.5, 0.866};
    double wire_r = 0.4e-3;

    std::vector<double> times;
    for (int i = 0; i < WARMUP + RUNS; ++i) {
        Timer t; t.start();
        for (int k = 0; k < 1000; ++k) {
            auto sec = generate_wire_section(center, tangent, wire_r, 32);
            do_not_optimise(sec);
        }
        if (i >= WARMUP) times.push_back(t.elapsed_ms());
    }
    print_stats("wire_section (1000 calls, 32 pts)", compute_stats(times));
}

static void bench_tube_body() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;

    auto p = make_default_params();
    auto tp = from_ehd_modulated(p, 1.0e-3, 12.0e-3);
    std::vector<double> times;

    for (int i = 0; i < WARMUP + RUNS; ++i) {
        Timer t; t.start();
        auto profile = generate_tube_profile(tp);
        double elapsed = t.elapsed_ms();
        do_not_optimise(profile);
        if (i >= WARMUP) times.push_back(elapsed);
    }
    print_stats("tube_profile (modulated)", compute_stats(times));
}

static void bench_tube_scaling() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;

    int divs[] = {100, 500, 2000, 10000};
    for (int nd : divs) {
        auto p = make_default_params();
        auto tp = from_ehd_modulated(p, 1.0e-3, 12.0e-3);
        tp.axial_divisions = nd;

        std::vector<double> times;
        for (int i = 0; i < WARMUP + RUNS; ++i) {
            Timer t; t.start();
            auto profile = generate_tube_profile(tp);
            do_not_optimise(profile);
            if (i >= WARMUP) times.push_back(t.elapsed_ms());
        }
        std::string label = "tube_profile (" + std::to_string(nd) + " divs)";
        print_stats(label.c_str(), compute_stats(times));
    }
}

static void bench_ring_generation() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;

    std::vector<double> times;
    for (int i = 0; i < WARMUP + RUNS; ++i) {
        Timer t; t.start();
        for (int k = 0; k < 1000; ++k) {
            auto ring = generate_ring(0.01 * k, 4.0e-3, 72);
            do_not_optimise(ring);
        }
        if (i >= WARMUP) times.push_back(t.elapsed_ms());
    }
    print_stats("ring_generation (1000 rings, 72 pts)", compute_stats(times));
}

static void bench_electrode_layout() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;

    auto p = make_default_params();
    std::vector<double> times;

    for (int i = 0; i < WARMUP + RUNS; ++i) {
        Timer t; t.start();
        auto layout = build_default_helical_layout(p);
        do_not_optimise(layout);
        if (i >= WARMUP) times.push_back(t.elapsed_ms());
    }
    print_stats("electrode_layout", compute_stats(times));
}

static void bench_step_manifest() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;

    auto p = make_default_params();
    auto layout = build_default_helical_layout(p);
    std::vector<double> times;

    for (int i = 0; i < WARMUP + RUNS; ++i) {
        Timer t; t.start();
        auto manifest = StepManifest::build(p, layout, "BENCH-001");
        do_not_optimise(manifest);
        if (i >= WARMUP) times.push_back(t.elapsed_ms());
    }
    print_stats("step_manifest_build", compute_stats(times));
}

// ============================================================================
// Stage 2: Domain Extraction Benchmarks
// ============================================================================

static void bench_domain_extraction() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;
    using namespace vsepr::ehd::domain;

    auto p = make_default_params();
    auto layout = build_default_helical_layout(p);
    auto manifest = StepManifest::build(p, layout, "BENCH-002");
    std::vector<double> times;

    for (int i = 0; i < WARMUP + RUNS; ++i) {
        Timer t; t.start();
        auto assembly = extract_domains(manifest, p);
        do_not_optimise(assembly);
        if (i >= WARMUP) times.push_back(t.elapsed_ms());
    }
    print_stats("domain_extraction", compute_stats(times));
}

static void bench_region_and_bc_setup() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::domain;

    auto p = make_default_params();
    std::vector<double> times;

    for (int i = 0; i < WARMUP + RUNS; ++i) {
        Timer t; t.start();
        auto regions = build_default_regions(p);
        auto bcs = build_default_bcs(p, regions);
        do_not_optimise(bcs);
        if (i >= WARMUP) times.push_back(t.elapsed_ms());
    }
    print_stats("regions + boundary_conditions", compute_stats(times));
}

static void bench_bc_multi_species() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::domain;

    int species_counts[] = {1, 4, 16};
    for (int ns : species_counts) {
        auto p = make_default_params(ns);
        std::vector<double> times;

        for (int i = 0; i < WARMUP + RUNS; ++i) {
            Timer t; t.start();
            auto regions = build_default_regions(p);
            auto bcs = build_default_bcs(p, regions);
            do_not_optimise(bcs);
            if (i >= WARMUP) times.push_back(t.elapsed_ms());
        }
        std::string label = "regions+bcs (" + std::to_string(ns) + " species)";
        print_stats(label.c_str(), compute_stats(times));
    }
}

// ============================================================================
// Stage 3: Physics Benchmarks — Point Evaluations
// ============================================================================

static void bench_poiseuille_field(int n_points) {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    auto p = make_default_params();
    FlowModel fm(p);
    std::vector<double> times;

    for (int i = 0; i < WARMUP + RUNS; ++i) {
        volatile double checksum = 0.0;
        Timer t; t.start();
        for (int k = 0; k < n_points; ++k) {
            double r = p.tube_radius_m * static_cast<double>(k) / n_points;
            checksum += fm.poiseuille_velocity(r);
        }
        double elapsed_ns = t.elapsed_us() * 1000.0 / n_points;
        if (i >= WARMUP) times.push_back(elapsed_ns);
    }

    std::string label = "poiseuille_velocity (" + std::to_string(n_points) + " pts)";
    print_stats(label.c_str(), compute_stats(times), "ns");
}

static void bench_pressure_drop() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    auto p = make_default_params();
    FlowModel fm(p);
    std::vector<double> times;

    for (int i = 0; i < WARMUP + RUNS; ++i) {
        volatile double checksum = 0.0;
        Timer t; t.start();
        for (int k = 0; k < 10000; ++k) {
            checksum += fm.poiseuille_pressure_drop(0.03 + k * 1e-6);
        }
        double elapsed_ns = t.elapsed_us() * 1000.0 / 10000;
        if (i >= WARMUP) times.push_back(elapsed_ns);
    }
    print_stats("poiseuille_pressure_drop (10k calls)", compute_stats(times), "ns");
}

static void bench_coaxial_field(int n_points) {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    auto p = make_default_params();
    ElectrostaticModel em(p);
    double r_in  = p.wire_diameter_m * 0.5;
    double r_out = p.tube_radius_m;
    std::vector<double> times;

    for (int i = 0; i < WARMUP + RUNS; ++i) {
        volatile double checksum = 0.0;
        Timer t; t.start();
        for (int k = 1; k <= n_points; ++k) {
            double r = r_in + (r_out - r_in) * static_cast<double>(k) / (n_points + 1);
            checksum += em.coaxial_potential(r);
            checksum += em.coaxial_field_r(r);
        }
        double elapsed_ns = t.elapsed_us() * 1000.0 / n_points;
        if (i >= WARMUP) times.push_back(elapsed_ns);
    }

    std::string label = "coaxial_potential+E_r (" + std::to_string(n_points) + " pts)";
    print_stats(label.c_str(), compute_stats(times), "ns");
}

static void bench_nernst_einstein() {
    using namespace vsepr::ehd;

    IonicSpecies sp;
    sp.name = "Cl-"; sp.valence = -1; sp.diffusivity = 2.03e-9;
    std::vector<double> times;

    for (int i = 0; i < WARMUP + RUNS; ++i) {
        volatile double checksum = 0.0;
        Timer t; t.start();
        for (int k = 0; k < 100000; ++k) {
            checksum += sp.effective_mobility(298.15 + k * 1e-6);
        }
        double elapsed_ns = t.elapsed_us() * 1000.0 / 100000;
        if (i >= WARMUP) times.push_back(elapsed_ns);
    }
    print_stats("nernst_einstein_mobility (100k calls)", compute_stats(times), "ns");
}

static void bench_ion_flux(int n_points) {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    auto p = make_default_params();
    FlowModel fm(p);
    ElectrostaticModel em(p);

    auto flow_field = fm.initialize_poiseuille(10, 50);
    auto e_field    = em.initialize_coaxial(10, 50);

    std::vector<double> times;

    for (int i = 0; i < WARMUP + RUNS; ++i) {
        volatile double checksum = 0.0;
        Timer t; t.start();
        for (int k = 0; k < n_points; ++k) {
            size_t idx = static_cast<size_t>(k) % e_field.cells.size();
            auto flux = IonTransportModel::compute_flux(
                p.species[0],
                1.0,
                Vec3{0.0, 0.0, 0.0},
                e_field.cells[idx].field,
                flow_field.cells[idx].velocity,
                298.15
            );
            checksum += flux.x + flux.y + flux.z;
        }
        double elapsed_ns = t.elapsed_us() * 1000.0 / n_points;
        if (i >= WARMUP) times.push_back(elapsed_ns);
    }

    std::string label = "compute_flux (" + std::to_string(n_points) + " pts)";
    print_stats(label.c_str(), compute_stats(times), "ns");
}

static void bench_charge_density() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    auto p = make_default_params(4);
    std::vector<double> concs(4, 1.0);
    std::vector<double> times;

    for (int i = 0; i < WARMUP + RUNS; ++i) {
        volatile double checksum = 0.0;
        Timer t; t.start();
        for (int k = 0; k < 100000; ++k) {
            checksum += IonTransportModel::assemble_charge_density(p.species, concs);
        }
        double elapsed_ns = t.elapsed_us() * 1000.0 / 100000;
        if (i >= WARMUP) times.push_back(elapsed_ns);
    }
    print_stats("assemble_charge_density (100k, 4 sp)", compute_stats(times), "ns");
}

// ============================================================================
// Stage 3: Physics Benchmarks — Field Initialisation (grid scaling)
// ============================================================================

static void bench_field_init_scaling() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    auto p = make_default_params();
    FlowModel fm(p);
    ElectrostaticModel em(p);
    IonTransportModel itm(p);

    int grids[][2] = {{10, 50}, {20, 100}, {40, 200}, {80, 400}};

    for (auto& g : grids) {
        int nr = g[0], nz = g[1];

        // Poiseuille init
        {
            std::vector<double> times;
            for (int i = 0; i < WARMUP + RUNS; ++i) {
                Timer t; t.start();
                auto ff = fm.initialize_poiseuille(nr, nz);
                do_not_optimise(ff);
                if (i >= WARMUP) times.push_back(t.elapsed_ms());
            }
            std::string label = "init_poiseuille (" + std::to_string(nr)
                              + "x" + std::to_string(nz) + ")";
            print_stats(label.c_str(), compute_stats(times));
        }

        // Coaxial init
        {
            std::vector<double> times;
            for (int i = 0; i < WARMUP + RUNS; ++i) {
                Timer t; t.start();
                auto ef = em.initialize_coaxial(nr, nz);
                do_not_optimise(ef);
                if (i >= WARMUP) times.push_back(t.elapsed_ms());
            }
            std::string label = "init_coaxial (" + std::to_string(nr)
                              + "x" + std::to_string(nz) + ")";
            print_stats(label.c_str(), compute_stats(times));
        }

        // Species uniform init
        {
            std::vector<double> times;
            for (int i = 0; i < WARMUP + RUNS; ++i) {
                Timer t; t.start();
                auto sfs = itm.initialize_uniform(nr, nz);
                do_not_optimise(sfs);
                if (i >= WARMUP) times.push_back(t.elapsed_ms());
            }
            std::string label = "init_species (" + std::to_string(nr)
                              + "x" + std::to_string(nz) + ")";
            print_stats(label.c_str(), compute_stats(times));
        }
    }
}

// ============================================================================
// Stage 3: Physics — Field metric extraction
// ============================================================================

static void bench_field_metrics() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    auto p = make_default_params();
    FlowModel fm(p);
    ElectrostaticModel em(p);

    int grids[][2] = {{20, 100}, {80, 400}};

    for (auto& g : grids) {
        int nr = g[0], nz = g[1];
        auto ff = fm.initialize_poiseuille(nr, nz);
        auto ef = em.initialize_coaxial(nr, nz);
        double vol = constants::PI * p.tube_radius_m * p.tube_radius_m
                   * (p.tube_length_m + p.inlet_length_m + p.outlet_length_m);

        // max_velocity_magnitude
        {
            std::vector<double> times;
            for (int i = 0; i < WARMUP + RUNS; ++i) {
                volatile double v = 0.0;
                Timer t; t.start();
                v = ff.max_velocity_magnitude();
                double elapsed_us = t.elapsed_us();
                (void)v;
                if (i >= WARMUP) times.push_back(elapsed_us);
            }
            std::string label = "FlowField::max_velocity (" + std::to_string(nr)
                              + "x" + std::to_string(nz) + ")";
            print_stats(label.c_str(), compute_stats(times), "us");
        }

        // E_max
        {
            std::vector<double> times;
            for (int i = 0; i < WARMUP + RUNS; ++i) {
                volatile double v = 0.0;
                Timer t; t.start();
                v = ef.E_max();
                double elapsed_us = t.elapsed_us();
                (void)v;
                if (i >= WARMUP) times.push_back(elapsed_us);
            }
            std::string label = "ElectricField::E_max (" + std::to_string(nr)
                              + "x" + std::to_string(nz) + ")";
            print_stats(label.c_str(), compute_stats(times), "us");
        }

        // E_avg
        {
            std::vector<double> times;
            for (int i = 0; i < WARMUP + RUNS; ++i) {
                volatile double v = 0.0;
                Timer t; t.start();
                v = ef.E_avg(vol);
                double elapsed_us = t.elapsed_us();
                (void)v;
                if (i >= WARMUP) times.push_back(elapsed_us);
            }
            std::string label = "ElectricField::E_avg (" + std::to_string(nr)
                              + "x" + std::to_string(nz) + ")";
            print_stats(label.c_str(), compute_stats(times), "us");
        }

        // compute_pressure_drop
        {
            std::vector<double> times;
            for (int i = 0; i < WARMUP + RUNS; ++i) {
                volatile double v = 0.0;
                Timer t; t.start();
                v = FlowModel::compute_pressure_drop(ff);
                double elapsed_us = t.elapsed_us();
                (void)v;
                if (i >= WARMUP) times.push_back(elapsed_us);
            }
            std::string label = "compute_pressure_drop (" + std::to_string(nr)
                              + "x" + std::to_string(nz) + ")";
            print_stats(label.c_str(), compute_stats(times), "us");
        }

        // compute_field_from_potential (finite-difference E = -∇φ)
        {
            std::vector<double> times;
            for (int i = 0; i < WARMUP + RUNS; ++i) {
                auto ef_copy = ef;
                Timer t; t.start();
                ElectrostaticModel::compute_field_from_potential(ef_copy);
                double elapsed_us = t.elapsed_us();
                do_not_optimise(ef_copy);
                if (i >= WARMUP) times.push_back(elapsed_us);
            }
            std::string label = "compute_field_from_potential (" + std::to_string(nr)
                              + "x" + std::to_string(nz) + ")";
            print_stats(label.c_str(), compute_stats(times), "us");
        }
    }
}

// ============================================================================
// Stage 3: Coupled Solver — Grid Scaling
// ============================================================================

static void bench_coupled_solver() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    auto p = make_default_params();

    int grid_configs[][2] = {
        {  5,  25 },
        { 10,  50 },
        { 20, 100 },
        { 40, 200 },
        { 80, 400 }
    };

    for (auto& cfg : grid_configs) {
        int nr = cfg[0], nz = cfg[1];

        RunCard rc;
        rc.case_id = "BENCH";
        rc.nr = nr;
        rc.nz = nz;
        rc.max_outer_iterations = 10;

        std::vector<double> times;

        for (int i = 0; i < WARMUP + RUNS; ++i) {
            CoupledSolver solver(p, rc);
            solver.initialize();

            Timer t; t.start();
            auto metrics = solver.solve();
            double elapsed = t.elapsed_ms();

            do_not_optimise(metrics);
            if (i >= WARMUP) times.push_back(elapsed);
        }

        std::string label = "coupled_solve (" + std::to_string(nr)
                          + "x" + std::to_string(nz)
                          + ", " + std::to_string(nr * nz) + " cells)";
        print_stats(label.c_str(), compute_stats(times));
    }
}

// ============================================================================
// Stage 3: Coupled Solver — Multi-Species Scaling
// ============================================================================

static void bench_coupled_species_scaling() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    int species_counts[] = {1, 2, 4, 8};

    for (int ns : species_counts) {
        auto p = make_default_params(ns);

        RunCard rc;
        rc.case_id = "BENCH-SP";
        rc.nr = 10;
        rc.nz = 50;
        rc.max_outer_iterations = 10;

        std::vector<double> times;

        for (int i = 0; i < WARMUP + RUNS; ++i) {
            CoupledSolver solver(p, rc);
            solver.initialize();

            Timer t; t.start();
            auto metrics = solver.solve();
            do_not_optimise(metrics);
            if (i >= WARMUP) times.push_back(t.elapsed_ms());
        }

        std::string label = "coupled_solve (10x50, "
                          + std::to_string(ns) + " species)";
        print_stats(label.c_str(), compute_stats(times));
    }
}

// ============================================================================
// Stage 4: Mesh Control Specification
// ============================================================================

static void bench_mesh_controls() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::mesh;

    auto p = make_default_params();
    std::vector<double> times;

    for (int i = 0; i < WARMUP + RUNS; ++i) {
        Timer t; t.start();
        auto spec = build_default_mesh_controls(p);
        do_not_optimise(spec);
        if (i >= WARMUP) times.push_back(t.elapsed_ms());
    }
    print_stats("mesh_controls (4 turns)", compute_stats(times));
}

static void bench_mesh_turn_scaling() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::mesh;

    int turn_counts[] = {4, 16, 64, 256};
    for (int nt : turn_counts) {
        auto p = make_default_params();
        p.num_turns = nt;
        std::vector<double> times;

        for (int i = 0; i < WARMUP + RUNS; ++i) {
            Timer t; t.start();
            auto spec = build_default_mesh_controls(p);
            do_not_optimise(spec);
            if (i >= WARMUP) times.push_back(t.elapsed_ms());
        }
        std::string label = "mesh_controls (" + std::to_string(nt) + " turns, "
                          + std::to_string(nt + 1) + " zones)";
        print_stats(label.c_str(), compute_stats(times));
    }
}

// ============================================================================
// Stage 5: Postprocessing Benchmarks
// ============================================================================

static void bench_comparison_table() {
    using namespace vsepr::ehd::physics;
    using namespace vsepr::ehd::post;

    SolverMetrics baseline;
    baseline.delta_P = 10.0; baseline.E_max = 1e6; baseline.E_avg = 5e5;
    baseline.u_max = 0.02; baseline.Re = 80.0;
    baseline.outlet_flux = {1e-8, 2e-8};
    baseline.accumulation_idx = {1.5, 1.2};

    SolverMetrics modulated;
    modulated.delta_P = 15.0; modulated.E_max = 1.5e6; modulated.E_avg = 7e5;
    modulated.u_max = 0.025; modulated.Re = 80.0;
    modulated.outlet_flux = {1.8e-8, 3.1e-8};
    modulated.accumulation_idx = {2.1, 1.9};

    std::vector<double> times;

    for (int i = 0; i < WARMUP + RUNS; ++i) {
        Timer t; t.start();
        auto table = build_comparison(baseline, modulated);
        double J_b = optimization_objective(baseline,  1.0, 1e6, 0.1, 0);
        double J_m = optimization_objective(modulated, 1.0, 1e6, 0.1, 0);
        (void)J_b; (void)J_m;
        do_not_optimise(table);
        if (i >= WARMUP) times.push_back(t.elapsed_ms());
    }
    print_stats("comparison + objective", compute_stats(times));
}

static void bench_streamline_tracing() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;
    using namespace vsepr::ehd::post;

    auto p = make_default_params();
    FlowModel fm(p);
    auto ff = fm.initialize_poiseuille(40, 200);

    int seed_counts[] = {5, 20, 50};
    for (int ns : seed_counts) {
        std::vector<double> times;
        for (int i = 0; i < WARMUP + RUNS; ++i) {
            Timer t; t.start();
            auto lines = generate_inlet_streamlines(ff, ns);
            do_not_optimise(lines);
            if (i >= WARMUP) times.push_back(t.elapsed_ms());
        }
        std::string label = "streamlines (40x200, "
                          + std::to_string(ns) + " seeds)";
        print_stats(label.c_str(), compute_stats(times));
    }
}

static void bench_section_probes() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;
    using namespace vsepr::ehd::post;

    auto p = make_default_params();
    RunCard rc;
    rc.case_id = "PROBE"; rc.nr = 40; rc.nz = 200;
    rc.max_outer_iterations = 5;
    CoupledSolver solver(p, rc);
    solver.initialize();
    solver.solve();

    FieldQuantity quantities[] = {
        FieldQuantity::VELOCITY_MAGNITUDE,
        FieldQuantity::POTENTIAL,
        FieldQuantity::FIELD_MAGNITUDE
    };
    const char* qnames[] = {"|u|", "phi", "|E|"};

    for (int q = 0; q < 3; ++q) {
        // Radial probe at mid-plane
        {
            std::vector<double> times;
            for (int i = 0; i < WARMUP + RUNS; ++i) {
                Timer t; t.start();
                auto prof = radial_probe(solver, quantities[q], 100);
                do_not_optimise(prof);
                if (i >= WARMUP) times.push_back(t.elapsed_us());
            }
            std::string label = std::string("radial_probe(") + qnames[q]
                              + ", z=mid, 40 pts)";
            print_stats(label.c_str(), compute_stats(times), "us");
        }

        // Axial probe at center
        {
            std::vector<double> times;
            for (int i = 0; i < WARMUP + RUNS; ++i) {
                Timer t; t.start();
                auto prof = axial_probe(solver, quantities[q], 0);
                do_not_optimise(prof);
                if (i >= WARMUP) times.push_back(t.elapsed_us());
            }
            std::string label = std::string("axial_probe(") + qnames[q]
                              + ", r=0, 200 pts)";
            print_stats(label.c_str(), compute_stats(times), "us");
        }
    }
}

// ============================================================================
// Full Pipeline Benchmark (end-to-end)
// ============================================================================

static void bench_full_pipeline() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::cad;
    using namespace vsepr::ehd::domain;
    using namespace vsepr::ehd::physics;
    using namespace vsepr::ehd::mesh;
    using namespace vsepr::ehd::post;

    auto p = make_default_params();
    std::vector<double> times;

    for (int i = 0; i < WARMUP + RUNS; ++i) {
        Timer t; t.start();

        // Stage 1
        auto pts     = generate_helix_centerline(from_ehd(p));
        auto profile = generate_tube_profile(from_ehd_modulated(p, 1.0e-3, 12.0e-3));
        auto layout  = build_default_helical_layout(p);
        auto manifest = StepManifest::build(p, layout, "PIPE-001");

        // Stage 2
        auto assembly = extract_domains(manifest, p);
        auto regions  = build_default_regions(p);
        auto bcs      = build_default_bcs(p, regions);

        // Stage 3
        RunCard rc;
        rc.case_id = "PIPE"; rc.nr = 10; rc.nz = 50;
        rc.max_outer_iterations = 5;
        CoupledSolver solver(p, rc);
        solver.initialize();
        auto metrics = solver.solve();

        // Stage 4
        auto mesh_spec = build_default_mesh_controls(p);

        // Stage 5
        SolverMetrics baseline_m = metrics;
        baseline_m.u_max *= 0.8;
        auto table = build_comparison(baseline_m, metrics);
        double J = optimization_objective(metrics, 1.0, 1e6, 0.1, 0);
        (void)J;

        do_not_optimise(pts);
        do_not_optimise(profile);
        do_not_optimise(assembly);
        do_not_optimise(metrics);
        do_not_optimise(mesh_spec);
        do_not_optimise(table);

        if (i >= WARMUP) times.push_back(t.elapsed_ms());
    }
    print_stats("full_pipeline (end-to-end, 10x50)", compute_stats(times));
}

// ============================================================================
// Parameter Sensitivity Sweeps
// ============================================================================

static void bench_voltage_sweep() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    std::cout << "\n  Voltage sweep (ΔV = 10..200 V, 10x50 grid):\n";
    double voltages[] = {10.0, 25.0, 50.0, 100.0, 200.0};

    for (double V : voltages) {
        auto p = make_default_params();
        p.voltage_pos = V;

        RunCard rc;
        rc.case_id = "V-SWEEP"; rc.nr = 10; rc.nz = 50;
        rc.max_outer_iterations = 10;

        std::vector<double> times;
        SolverMetrics last_m;

        for (int i = 0; i < WARMUP + RUNS; ++i) {
            CoupledSolver solver(p, rc);
            solver.initialize();
            Timer t; t.start();
            last_m = solver.solve();
            do_not_optimise(last_m);
            if (i >= WARMUP) times.push_back(t.elapsed_ms());
        }

        auto st = compute_stats(times);
        std::cout << "    ΔV=" << std::setw(6) << V << " V"
                  << "  avg=" << std::setw(8) << std::fixed << std::setprecision(3) << st.avg << " ms"
                  << "  E_max=" << std::scientific << std::setprecision(3) << last_m.E_max << " V/m"
                  << "  iters=" << last_m.iterations_used << "\n";
    }
}

static void bench_velocity_sweep() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    std::cout << "\n  Velocity sweep (U = 0.001..0.1 m/s, 10x50 grid):\n";
    double velocities[] = {0.001, 0.005, 0.01, 0.05, 0.1};

    for (double U : velocities) {
        auto p = make_default_params();
        p.inlet_velocity = U;

        RunCard rc;
        rc.case_id = "U-SWEEP"; rc.nr = 10; rc.nz = 50;
        rc.max_outer_iterations = 10;

        std::vector<double> times;
        SolverMetrics last_m;

        for (int i = 0; i < WARMUP + RUNS; ++i) {
            CoupledSolver solver(p, rc);
            solver.initialize();
            Timer t; t.start();
            last_m = solver.solve();
            do_not_optimise(last_m);
            if (i >= WARMUP) times.push_back(t.elapsed_ms());
        }

        auto st = compute_stats(times);
        std::cout << "    U=" << std::setw(8) << std::fixed << std::setprecision(4) << U << " m/s"
                  << "  avg=" << std::setw(8) << std::setprecision(3) << st.avg << " ms"
                  << "  Re=" << std::setw(8) << std::setprecision(2) << last_m.Re
                  << "  ΔP=" << std::scientific << std::setprecision(3) << last_m.delta_P << " Pa"
                  << "  iters=" << last_m.iterations_used << "\n";
    }
}

// ============================================================================
// Memory Footprint Estimation
// ============================================================================

static void bench_memory_footprint() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    std::cout << "\n  Memory footprint estimates (bytes per grid cell):\n";
    std::cout << "    FlowCell:       " << sizeof(FlowCell) << " B\n";
    std::cout << "    ElectricCell:   " << sizeof(ElectricCell) << " B\n";

    int grids[][2] = {{10, 50}, {40, 200}, {80, 400}, {160, 800}};

    for (auto& g : grids) {
        int nr = g[0], nz = g[1];
        size_t cells = static_cast<size_t>(nr) * nz;

        // Species cell from ion_transport_model
        // SpeciesCell has: concentration(8) + flux(24) = 32 bytes
        size_t flow_bytes    = cells * sizeof(FlowCell);
        size_t elec_bytes    = cells * sizeof(ElectricCell);
        size_t species_bytes = cells * 32;  // approximate SpeciesCell

        size_t total = flow_bytes + elec_bytes + species_bytes;
        double total_mb = total / (1024.0 * 1024.0);

        std::cout << "    " << nr << "x" << nz
                  << " (" << cells << " cells): "
                  << "flow=" << flow_bytes / 1024 << " KB"
                  << "  elec=" << elec_bytes / 1024 << " KB"
                  << "  species=" << species_bytes / 1024 << " KB"
                  << "  total=" << std::fixed << std::setprecision(2) << total_mb << " MB\n";
    }
}

// ============================================================================
// Determinism Verification
// ============================================================================

static void verify_determinism() {
    using namespace vsepr::ehd;
    using namespace vsepr::ehd::physics;

    std::cout << "\n  Determinism check (10 identical coupled solves, 10x50)...\n";

    auto p = make_default_params();
    RunCard rc;
    rc.case_id = "DET"; rc.nr = 10; rc.nz = 50;
    rc.max_outer_iterations = 10;

    std::vector<double> dp, emax, umax;

    for (int i = 0; i < 10; ++i) {
        CoupledSolver solver(p, rc);
        solver.initialize();
        auto m = solver.solve();
        dp.push_back(m.delta_P);
        emax.push_back(m.E_max);
        umax.push_back(m.u_max);
    }

    bool ok = true;
    for (int i = 1; i < 10; ++i) {
        if (dp[i] != dp[0] || emax[i] != emax[0] || umax[i] != umax[0])
            ok = false;
    }

    if (ok) {
        std::cout << "  DETERMINISTIC: All 10 runs bit-identical\n";
        std::cout << "    delta_P = " << dp[0] << " Pa\n";
        std::cout << "    E_max   = " << emax[0] << " V/m\n";
        std::cout << "    u_max   = " << umax[0] << " m/s\n";
    } else {
        std::cout << "  WARNING: Non-deterministic!\n";
        for (int i = 0; i < 10; ++i)
            std::cout << "    run " << i << "  dP=" << dp[i]
                      << " E=" << emax[i] << " u=" << umax[i] << "\n";
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << std::fixed;

    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  EHD Multiphysics Module — Performance Benchmarks\n";
    std::cout << "  Runs: " << RUNS << " (+ " << WARMUP << " warmup)\n";
    std::cout << "================================================================\n";

#ifdef NDEBUG
    std::cout << "\n  Build: Release (optimizations enabled)\n";
#else
    std::cout << "\n  Build: Debug — timings are indicative only\n";
    std::cout << "  For accurate benchmarks: cmake --build <dir> --config Release\n";
#endif

    // ---- Stage 1 ----
    std::cout << "\n--- Stage 1: CAD / Geometry ---\n";
    bench_helix_generation();
    bench_helix_scaling();
    bench_wire_section();
    bench_tube_body();
    bench_tube_scaling();
    bench_ring_generation();
    bench_electrode_layout();
    bench_step_manifest();

    // ---- Stage 2 ----
    std::cout << "\n--- Stage 2: Domain Extraction ---\n";
    bench_domain_extraction();
    bench_region_and_bc_setup();
    bench_bc_multi_species();

    // ---- Stage 3: per-point ----
    std::cout << "\n--- Stage 3: Physics (per-point evaluations) ---\n";
    bench_poiseuille_field(1000);
    bench_poiseuille_field(1000000);
    bench_pressure_drop();
    bench_coaxial_field(1000);
    bench_coaxial_field(1000000);
    bench_nernst_einstein();
    bench_ion_flux(1000);
    bench_ion_flux(1000000);
    bench_charge_density();

    // ---- Stage 3: field init scaling ----
    std::cout << "\n--- Stage 3: Field Initialisation (grid scaling) ---\n";
    bench_field_init_scaling();

    // ---- Stage 3: field metrics ----
    std::cout << "\n--- Stage 3: Field Metric Extraction ---\n";
    bench_field_metrics();

    // ---- Stage 3: coupled solver ----
    std::cout << "\n--- Stage 3: Coupled Solver (grid scaling) ---\n";
    bench_coupled_solver();

    std::cout << "\n--- Stage 3: Coupled Solver (multi-species scaling) ---\n";
    bench_coupled_species_scaling();

    // ---- Stage 4 ----
    std::cout << "\n--- Stage 4: Mesh Controls ---\n";
    bench_mesh_controls();
    bench_mesh_turn_scaling();

    // ---- Stage 5 ----
    std::cout << "\n--- Stage 5: Postprocessing ---\n";
    bench_comparison_table();
    bench_streamline_tracing();
    bench_section_probes();

    // ---- Full Pipeline ----
    std::cout << "\n--- Full Pipeline ---\n";
    bench_full_pipeline();

    // ---- Parameter Sensitivity ----
    std::cout << "\n--- Parameter Sensitivity ---\n";
    bench_voltage_sweep();
    bench_velocity_sweep();

    // ---- Memory ----
    std::cout << "\n--- Memory Footprint ---\n";
    bench_memory_footprint();

    // ---- Determinism ----
    std::cout << "\n--- Determinism Verification ---\n";
    verify_determinism();

    std::cout << "\n================================================================\n";
    std::cout << "  Benchmark complete.\n";
    std::cout << "================================================================\n\n";

    return 0;
}
