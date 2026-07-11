/**
 * bench_gpu_cpu.cpp — GPU vs CPU Nonbonded Energy Benchmark
 *
 * Generates random atom clouds of increasing size and measures wall-clock
 * time for nonbonded energy evaluation on both GPU and CPU backends.
 *
 * Output: CSV to stdout (pipe-ready) + human-readable summary to stderr.
 *
 * Usage:
 *   ./bench_gpu_cpu                     # default sizes: 64..8192
 *   ./bench_gpu_cpu --min 100 --max 10000 --steps 8 --repeats 5
 *   ./bench_gpu_cpu --csv > benchmark.csv
 *
 * CSV columns:
 *   n_atoms, cpu_ms, gpu_ms, speedup, energy_cpu, energy_gpu, delta_E
 *
 * Anti-black-box: every timing, every energy value, every deviation is
 * explicitly reported. No hidden averaging.
 */

#include "gpu/gpu_backend.hpp"
#include "gpu/cuda_kernels.hpp"
#include "pot/energy_nonbonded.hpp"
#include "core/types.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

using Clock = std::chrono::high_resolution_clock;

// ============================================================================
// Configuration
// ============================================================================

struct BenchConfig {
    int min_atoms   = 64;
    int max_atoms   = 8192;
    int steps       = 8;       // number of size points (log-spaced)
    int repeats     = 3;       // repeats per size
    bool csv_header = true;
    bool human      = true;
    uint32_t seed   = 42;
};

// ============================================================================
// Atom cloud generator
// ============================================================================

struct AtomCloud {
    std::vector<float>   coords;         // [3*N] x,y,z
    std::vector<uint8_t> atomic_numbers; // [N]
    std::vector<int>     exclusions;     // [N * max_excl]
    int                  max_excl = 4;
    int                  n_atoms  = 0;
};

static AtomCloud generate_cloud(int n, uint32_t seed) {
    AtomCloud cloud;
    cloud.n_atoms = n;
    cloud.coords.resize(3 * n);
    cloud.atomic_numbers.resize(n);
    cloud.exclusions.resize(n * cloud.max_excl, -1);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> pos_dist(-20.0f, 20.0f);
    // Common elements: H(1), C(6), N(7), O(8), Fe(26), Cu(29)
    std::vector<uint8_t> elements = {1, 6, 7, 8, 26, 29};
    std::uniform_int_distribution<int> elem_dist(0, static_cast<int>(elements.size()) - 1);

    for (int i = 0; i < n; ++i) {
        cloud.coords[3 * i + 0] = pos_dist(rng);
        cloud.coords[3 * i + 1] = pos_dist(rng);
        cloud.coords[3 * i + 2] = pos_dist(rng);
        cloud.atomic_numbers[i] = elements[elem_dist(rng)];
    }

    // Simple exclusions: each atom excludes its nearest neighbor by index
    for (int i = 0; i < n; ++i) {
        if (i > 0)     cloud.exclusions[i * cloud.max_excl + 0] = i - 1;
        if (i < n - 1) cloud.exclusions[i * cloud.max_excl + 1] = i + 1;
    }

    return cloud;
}

// ============================================================================
// CPU reference: O(N²) pairwise LJ energy
// ============================================================================

static double cpu_nonbonded_energy(const AtomCloud& cloud) {
    int n = cloud.n_atoms;
    double total = 0.0;

    // Simple LJ 12-6 with uniform epsilon/sigma for benchmark parity
    constexpr double epsilon = 0.01;
    constexpr double sigma   = 3.4;   // Angstrom (Ar-like)
    constexpr double rmin    = 0.5;

    for (int i = 0; i < n; ++i) {
        float xi = cloud.coords[3 * i + 0];
        float yi = cloud.coords[3 * i + 1];
        float zi = cloud.coords[3 * i + 2];

        for (int j = i + 1; j < n; ++j) {
            // Check exclusion
            bool excluded = false;
            for (int e = 0; e < cloud.max_excl; ++e) {
                if (cloud.exclusions[i * cloud.max_excl + e] == j) {
                    excluded = true;
                    break;
                }
            }
            if (excluded) continue;

            float dx = xi - cloud.coords[3 * j + 0];
            float dy = yi - cloud.coords[3 * j + 1];
            float dz = zi - cloud.coords[3 * j + 2];
            double r2 = dx * dx + dy * dy + dz * dz;
            double r  = std::sqrt(r2);
            if (r < rmin) r = rmin;

            double sr6 = std::pow(sigma / r, 6);
            total += 4.0 * epsilon * (sr6 * sr6 - sr6);
        }
    }
    return total;
}

// ============================================================================
// GPU path (calls CUDA kernel if available, otherwise CPU fallback)
// ============================================================================

static double gpu_nonbonded_energy(const AtomCloud& cloud) {
    auto& backend = vsepr::gpu::GPUBackend::instance();

    if (backend.get_backend() == vsepr::gpu::Backend::CUDA) {
#ifdef VSEPR_HAS_CUDA
        return static_cast<double>(vsepr::gpu::cuda_compute_nonbonded_energy(
            cloud.coords.data(),
            cloud.atomic_numbers.data(),
            cloud.exclusions.data(),
            cloud.max_excl,
            cloud.n_atoms
        ));
#endif
    }

    // Fallback: same CPU implementation (measured separately for timing parity)
    return cpu_nonbonded_energy(cloud);
}

// ============================================================================
// Timing helper
// ============================================================================

struct TimingResult {
    double elapsed_ms;
    double energy;
};

template<typename Func>
static TimingResult time_it(Func&& func) {
    auto t0 = Clock::now();
    double energy = func();
    auto t1 = Clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return {ms, energy};
}

// ============================================================================
// Parse arguments
// ============================================================================

static BenchConfig parse_args(int argc, char** argv) {
    BenchConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--min"     && i + 1 < argc) cfg.min_atoms   = std::atoi(argv[++i]);
        if (arg == "--max"     && i + 1 < argc) cfg.max_atoms   = std::atoi(argv[++i]);
        if (arg == "--steps"   && i + 1 < argc) cfg.steps       = std::atoi(argv[++i]);
        if (arg == "--repeats" && i + 1 < argc) cfg.repeats     = std::atoi(argv[++i]);
        if (arg == "--seed"    && i + 1 < argc) cfg.seed        = std::atoi(argv[++i]);
        if (arg == "--csv")    cfg.human = false;
        if (arg == "--no-header") cfg.csv_header = false;
    }
    return cfg;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    BenchConfig cfg = parse_args(argc, argv);

    auto& backend = vsepr::gpu::GPUBackend::instance();
    std::string backend_name;
    switch (backend.get_backend()) {
        case vsepr::gpu::Backend::CUDA:         backend_name = "CUDA"; break;
        case vsepr::gpu::Backend::OpenCL:       backend_name = "OpenCL"; break;
        case vsepr::gpu::Backend::CPU_Fallback:  backend_name = "CPU_Fallback"; break;
    }

    if (cfg.human) {
        std::fprintf(stderr, "╔════════════════════════════════════════════════════════╗\n");
        std::fprintf(stderr, "║  VSEPR-SIM GPU vs CPU Benchmark v3.0.0                ║\n");
        std::fprintf(stderr, "╚════════════════════════════════════════════════════════╝\n");
        std::fprintf(stderr, "  GPU Backend:  %s\n", backend_name.c_str());
        std::fprintf(stderr, "  Device:       %s\n", backend.get_device_name().c_str());
        std::fprintf(stderr, "  Atom range:   %d — %d  (%d steps)\n",
                     cfg.min_atoms, cfg.max_atoms, cfg.steps);
        std::fprintf(stderr, "  Repeats:      %d\n", cfg.repeats);
        std::fprintf(stderr, "  Seed:         %u\n\n", cfg.seed);
    }

    // CSV header to stdout (pipe-ready)
    if (cfg.csv_header) {
        std::printf("n_atoms,cpu_ms,gpu_ms,speedup,energy_cpu,energy_gpu,delta_E,backend\n");
    }

    // Generate log-spaced size points
    std::vector<int> sizes;
    if (cfg.steps <= 1) {
        sizes.push_back(cfg.min_atoms);
    } else {
        double log_min = std::log(static_cast<double>(cfg.min_atoms));
        double log_max = std::log(static_cast<double>(cfg.max_atoms));
        for (int s = 0; s < cfg.steps; ++s) {
            double frac = static_cast<double>(s) / (cfg.steps - 1);
            int n = static_cast<int>(std::exp(log_min + frac * (log_max - log_min)));
            // Deduplicate
            if (sizes.empty() || n != sizes.back()) {
                sizes.push_back(n);
            }
        }
    }

    // Run benchmarks
    for (int n : sizes) {
        std::vector<double> cpu_times, gpu_times;
        double cpu_energy_last = 0.0, gpu_energy_last = 0.0;

        for (int r = 0; r < cfg.repeats; ++r) {
            AtomCloud cloud = generate_cloud(n, cfg.seed + r);

            auto cpu_result = time_it([&]() { return cpu_nonbonded_energy(cloud); });
            auto gpu_result = time_it([&]() { return gpu_nonbonded_energy(cloud); });

            cpu_times.push_back(cpu_result.elapsed_ms);
            gpu_times.push_back(gpu_result.elapsed_ms);
            cpu_energy_last = cpu_result.energy;
            gpu_energy_last = gpu_result.energy;
        }

        // Median timing (robust to outliers)
        std::sort(cpu_times.begin(), cpu_times.end());
        std::sort(gpu_times.begin(), gpu_times.end());
        double cpu_median = cpu_times[cpu_times.size() / 2];
        double gpu_median = gpu_times[gpu_times.size() / 2];
        double speedup = (gpu_median > 1e-9) ? cpu_median / gpu_median : 0.0;
        double delta_E = std::abs(cpu_energy_last - gpu_energy_last);

        // CSV row to stdout
        std::printf("%d,%.4f,%.4f,%.2f,%.6e,%.6e,%.6e,%s\n",
                    n, cpu_median, gpu_median, speedup,
                    cpu_energy_last, gpu_energy_last, delta_E,
                    backend_name.c_str());

        if (cfg.human) {
            std::fprintf(stderr, "  N=%5d  CPU=%.2f ms  GPU=%.2f ms  speedup=%.1fx  ΔE=%.2e\n",
                         n, cpu_median, gpu_median, speedup, delta_E);
        }
    }

    if (cfg.human) {
        std::fprintf(stderr, "\nBenchmark complete. CSV data written to stdout.\n");
    }

    return 0;
}
