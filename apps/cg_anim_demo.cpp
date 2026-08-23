/**
 * cg_anim_demo.cpp  -  Animated Ensemble Proxy Demo
 *
 * Demonstrates eta relaxation and ensemble proxy evolution across four
 * bead scene families. Reuses the test scene infrastructure directly
 * (scene_factory.hpp / test_runners.hpp) to construct deterministic
 * scenes, run them to convergence, and then visualise the resulting
 * scenes and exports a renderer-neutral Day 89 artifact package. It is the
 * canonical producer for the VTK + Qt3D viewer vsepr-view.
 *
 * Scene families:
 *   1. Cubic lattice (tight, 3 A)    -  high cohesion, regular structure
 *   2. Cubic lattice (loose, 6 A)    -  sparse, reduced cohesion
 *   3. Aligned stack cloud (bias 1)  -  high texture proxy (P2 alignment)
 *   4. Random cluster (N=27)         -  low uniformity, high surface sensitivity
 *
 * Optional stress fixture:
 *   --stress-particles N  append a deterministic lattice of N^3 particles
 *   --headless            print proxy table only, skip windowed display
 *   --help                show usage
 *
 * Architecture position:
 *   test_util scene builders + runners  (test infrastructure reuse)
 *         v
 *   EnvironmentState (converged, 500 steps)
 *         v
 *   EnsembleProxySummary  (printed to terminal)
 *         v
 *   dual-backend 3D data package  (vsepr.dual_backend_3d.v1)
 *         v
 *   vsepr-view  (VTK + Qt3D consumer, optional BUILD_VIS)
 *
 * Reference: Emergent Effective Medium Mapping specification (Suite #5)
 */

#include "tests/scene_factory.hpp"
#include "coarse_grain/analysis/ensemble_proxy.hpp"
#include "coarse_grain/core/bead.hpp"

// Note: BUILD_VISUALIZATION no longer enables an OpenGL/GLFW/ImGui window.
// The supported interactive consumer is vsepr-view (VTK + Qt3D).  When that
// target is available the artifact is consumed through include/vis/day89_artifact.hpp
// and apps/vtk_qt3d_multiscale_viewer.cpp, not by this headless producer.

#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

// ============================================================================
// Default environment parameters  -  mirrors default_params() in Suite #5
// ============================================================================

static coarse_grain::EnvironmentParams make_params() {
    coarse_grain::EnvironmentParams p;
    p.alpha        = 0.5;
    p.beta         = 0.5;
    p.tau          = 100.0;
    p.gamma_steric = 0.2;
    p.gamma_elec   = -0.1;
    p.gamma_disp   = 0.5;
    p.sigma_rho    = 3.0;
    p.r_cutoff     = 8.0;
    p.delta_sw     = 1.0;
    p.rho_max      = 10.0;
    return p;
}

template <typename RunResult>
static void write_demo_artifacts(const std::filesystem::path& output_dir,
                                 const std::vector<RunResult>& results,
                                 int n_steps,
                                 double dt,
                                 double generation_ms) {
    std::filesystem::create_directories(output_dir);

    std::ofstream scenes(output_dir / "scenes.csv");
    std::ofstream particles(output_dir / "particles.csv");
    std::ofstream manifest(output_dir / "manifest.json");
    if (!scenes || !particles || !manifest) {
        throw std::runtime_error("Unable to create dual-backend demo artifacts in " + output_dir.string());
    }

    scenes.imbue(std::locale::classic());
    particles.imbue(std::locale::classic());
    scenes << std::setprecision(12);
    particles << std::setprecision(12);

    scenes << "scene_id,scene_name,particle_count,cohesion_proxy,texture_proxy,"
              "stabilization_proxy,density_mean,coordination_mean\n";
    particles << "scene_id,particle_id,x,y,z,radius,state_value\n";

    for (size_t scene_id = 0; scene_id < results.size(); ++scene_id) {
        const auto& result = results[scene_id];
        std::string scene_name = result.name;
        for (size_t pos = 0; (pos = scene_name.find('"', pos)) != std::string::npos; pos += 2)
            scene_name.insert(pos, 1, '"');

        scenes << scene_id << ",\"" << scene_name << "\"," << result.scene.size() << ','
               << result.proxy.cohesion_proxy << ',' << result.proxy.texture_proxy << ','
               << result.proxy.stabilization_proxy << ',' << result.proxy.mean_rho << ','
               << result.proxy.mean_C << '\n';

        for (size_t particle_id = 0; particle_id < result.scene.size(); ++particle_id) {
            const auto& position = result.scene[particle_id].position;
            particles << scene_id << ',' << particle_id << ','
                      << position.x << ',' << position.y << ',' << position.z << ','
                      << 0.45 << ',' << result.states[particle_id].eta << '\n';
        }
    }

    manifest << "{\n"
             << "  \"schema\": \"vsepr.dual_backend_3d.v1\",\n"
             << "  \"generator\": \"cg-anim-demo\",\n"
             << "  \"steps\": " << n_steps << ",\n"
             << "  \"dt_fs\": " << dt << ",\n"
             << "  \"generation_ms\": " << generation_ms << ",\n"
             << "  \"scene_count\": " << results.size() << ",\n"
             << "  \"particle_count\": " << [&results] {
                    size_t count = 0;
                    for (const auto& result : results) count += result.scene.size();
                    return count;
                }() << ",\n"
             << "  \"scenes_csv\": \"scenes.csv\",\n"
             << "  \"particles_csv\": \"particles.csv\"\n"
             << "}\n";
}

// ============================================================================
// Demo scene descriptor
// ============================================================================

struct DemoScene {
    const char* name;
    std::vector<test_util::SceneBead> beads;
};

static std::vector<DemoScene> build_demo_scenes(int stress_particles) {
    std::vector<DemoScene> demos;

    demos.push_back({"Cubic Lattice  (tight, 3 A, N=27)",
                     test_util::scene_cubic_lattice(3, 3.0)});

    demos.push_back({"Cubic Lattice  (loose, 6 A, N=27)",
                     test_util::scene_cubic_lattice(3, 6.0)});

    demos.push_back({"Aligned Stack  (bias=1.0, N=30)",
                     test_util::scene_biased_stack_cloud(30, 3.5, 1.0, 0.0, 42)});

    demos.push_back({"Random Cluster (N=27, box=12 A)",
                     test_util::scene_random_cluster(27, 12.0, 99)});

    if (stress_particles > 0) {
        const int side = static_cast<int>(std::ceil(std::cbrt(static_cast<double>(stress_particles))));
        auto beads = test_util::scene_cubic_lattice(side, 3.0);
        beads.resize(static_cast<size_t>(stress_particles));
        demos.push_back({"Deterministic Stress Lattice", std::move(beads)});
    }

    return demos;
}

// ============================================================================
// Convert SceneBead + EnvironmentState vectors -> CGSystemState
// ============================================================================

// make_cg_state() and the CGVizViewer OpenGL path are archived. Legacy
// BUILD_VISUALIZATION branches in this file are intentionally idle. The active
// consumer is vsepr-view (VTK + Qt3D) using the Day 89 artifact package.

// ============================================================================
// Terminal output helpers
// ============================================================================

static void print_ruler() {
    std::printf("  %s\n", std::string(110, '-').c_str());
}

static void print_proxy_header() {
    std::printf("  %-42s %9s %9s %9s %9s %9s %5s %5s\n",
                "Scene", "cohesion", "uniform", "texture",
                "stab", "surf_sens", "N", "valid");
    print_ruler();
}

static void print_proxy_row(const char* name,
                             const coarse_grain::EnsembleProxySummary& p) {
    std::printf("  %-42s %9.4f %9.4f %9.4f %9.4f %9.4f %5d %5s\n",
                name,
                p.cohesion_proxy,
                p.uniformity_proxy,
                p.texture_proxy,
                p.stabilization_proxy,
                p.surface_sensitivity_proxy,
                p.bead_count,
                p.valid ? "yes" : "no");
}

// Legacy OpenGL/GLFW animated VizConfig cycle is archived. VTK + Qt3D
// presentation cycles are handled by vsepr-view overlays*(1) and are not
// compiled into this headless producer.
//
// (1) Overlay modes are defined in the viewer runtime; the producer only emits
//     state and proxy data to the Day 89 artifact package.

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    bool headless = false;
    int stress_particles = 0;
    int n_steps = 500;
    bool steps_explicit = false;
    std::filesystem::path output_dir = "out/dual_backend_3d";
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--headless") == 0) headless = true;
        if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc) output_dir = argv[++i];
        if (std::strcmp(argv[i], "--stress-particles") == 0 && i + 1 < argc) stress_particles = std::stoi(argv[++i]);
        if (std::strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
            n_steps = std::stoi(argv[++i]);
            steps_explicit = true;
        }
        if (std::strcmp(argv[i], "--help") == 0 ||
            std::strcmp(argv[i], "-h") == 0) {
            std::printf("Usage: cg-anim-demo [--headless] [--output DIR] [--stress-particles N] [--steps N] [--help]\n");
            std::printf("  --headless   Print proxy table only; skip viewer.\n");
            std::printf("  --output DIR Write shared CSV/JSON artifacts to DIR.\n");
            std::printf("  --stress-particles N  Append a deterministic lattice with N particles.\n");
            std::printf("  --steps N    Environment update steps (stress default: 1).\n");
            return 0;
        }
    }

    if (stress_particles < 0 || n_steps < 1) {
        std::fprintf(stderr, "Particle count must be non-negative and steps must be positive.\n");
        return 2;
    }
    if (stress_particles > 0 && !steps_explicit) n_steps = 1;

    std::printf("\n");
    std::printf("+==============================================================+\n");
    std::printf("|   VSEPR-SIM  Animated Ensemble Proxy Demo                   |\n");
    std::printf("|   η relaxation + macroscopic proxy evolution                |\n");
    std::printf("+==============================================================+\n");
    std::printf("\n");

    constexpr double dt      = 10.0;
    auto params = make_params();
    auto demos  = build_demo_scenes(stress_particles);
    const auto generation_started = std::chrono::steady_clock::now();

    std::printf("Scenes: %zu    Steps: %d    dt: %.0f fs\n\n",
                demos.size(), n_steps, dt);

    // ---- Run all scenes to convergence ----

    struct RunResult {
        const char*                                    name;
        std::vector<test_util::SceneBead>              scene;
        std::vector<coarse_grain::EnvironmentState>    states;
        coarse_grain::EnsembleProxySummary             proxy;
    };

    std::vector<RunResult> results;
    results.reserve(demos.size());

    for (auto& demo : demos) {
        std::printf("  ▸ Running %-42s (N=%zu)  ...",
                    demo.name, demo.beads.size());
        std::fflush(stdout);

        auto states = test_util::run_all_beads(demo.beads, params, dt, n_steps);

        std::vector<atomistic::Vec3> positions;
        positions.reserve(demo.beads.size());
        for (const auto& b : demo.beads) positions.push_back(b.position);

        auto proxy = coarse_grain::compute_ensemble_proxy(
            states, positions, -1.0, params.r_cutoff);

        std::printf("  done\n");

        results.push_back({demo.name,
                           std::move(demo.beads),
                           std::move(states),
                           proxy});
    }

    // ---- Print proxy comparison table ----

    std::printf("\nProxy Summary (converged, %d steps, dt=%.0f fs):\n\n",
                n_steps, dt);
    print_proxy_header();
    for (const auto& r : results)
        print_proxy_row(r.name, r.proxy);
    print_ruler();
    std::printf("\n");

    try {
        const double generation_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - generation_started).count();
        write_demo_artifacts(output_dir, results, n_steps, dt, generation_ms);
        std::printf("Shared artifacts: %s\n\n", output_dir.string().c_str());
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Artifact export failed: %s\n", error.what());
        return 2;
    }

if (headless) {
    std::printf("(--headless: VTK + Qt3D viewer skipped; artifact written)\n\n");
} else {
    (void)headless;
    std::printf("(Headless artifact producer: use vsepr-view --artifact %s to visualize.)\n\n",
                output_dir.string().c_str());
}

return 0;
}
