/*
pbc_phase4_perf.cpp
-------------------
Phase 4 — Performance Baselines

Establish performance baselines before adding optimizations:
1. Microbench delta() - 10-100M calls with checksum to prevent opt-out
2. Pair loop throughput - LJ O(N²) for N = 256, 512, 1024

These baselines help identify:
- If PBC MIC is expensive (it shouldn't be)
- Current throughput before neighbor lists
- Performance scaling with system size
*/

#include "box/pbc.hpp"
#include "core/math_vec3.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>

using namespace vsepr;

// ============================================================================
// Timing Utilities
// ============================================================================
class Timer {
public:
    void start() {
        t0_ = std::chrono::high_resolution_clock::now();
    }
    
    double elapsed_ms() const {
        auto t1 = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0_).count();
    }
    
    double elapsed_sec() const {
        return elapsed_ms() / 1000.0;
    }
    
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> t0_;
};

// ============================================================================
// Test 1: Microbench delta() — 100M calls
// ============================================================================
void microbench_delta() {
    std::cout << "\n=== Microbench 1: delta() Performance ===\n";
    
    BoxOrtho box(10.0, 10.0, 10.0);
    
    // Generate random positions
    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> dist(0.0, 10.0);
    
    const int N_SAMPLES = 1000;
    std::vector<Vec3> positions_a(N_SAMPLES);
    std::vector<Vec3> positions_b(N_SAMPLES);
    
    for (int i = 0; i < N_SAMPLES; ++i) {
        positions_a[i] = {dist(rng), dist(rng), dist(rng)};
        positions_b[i] = {dist(rng), dist(rng), dist(rng)};
    }
    
    std::cout << "  Running 100 million delta() calls...\n";
    
    // Checksum to prevent compiler from optimizing away the loop
    double checksum = 0.0;
    
    Timer timer;
    timer.start();
    
    const long N_CALLS = 100'000'000L;
    for (long call = 0; call < N_CALLS; ++call) {
        int i = call % N_SAMPLES;
        Vec3 dr = box.delta(positions_a[i], positions_b[i]);
        
        // Accumulate something to prevent optimization
        checksum += dr.x + dr.y + dr.z;
    }
    
    double time_sec = timer.elapsed_sec();
    
    std::cout << "  Checksum: " << checksum << " (prevents optimization)\n";
    std::cout << "  Total time: " << time_sec << " seconds\n";
    std::cout << "  Calls/sec: " << (N_CALLS / time_sec) << "\n";
    std::cout << "  ns/call: " << (time_sec * 1e9 / N_CALLS) << "\n";
    
    // Baseline: delta() should be < 10 ns/call (very cheap)
    double ns_per_call = time_sec * 1e9 / N_CALLS;
    if (ns_per_call < 10.0) {
        std::cout << "  ✓ EXCELLENT: MIC is fast (< 10 ns/call)\n";
    } else if (ns_per_call < 50.0) {
        std::cout << "  ✓ GOOD: MIC is reasonably fast (< 50 ns/call)\n";
    } else {
        std::cout << "  ⚠ WARNING: MIC seems slow (> 50 ns/call) - check compiler opts\n";
    }
}

// ============================================================================
// LJ O(N²) Pair Loop (for throughput tests)
// ============================================================================
struct LJParams {
    double sigma = 3.0;
    double epsilon = 0.1;
    double cutoff = 9.0;
};

void compute_lj_pbc(std::vector<Vec3>& positions,
                   std::vector<Vec3>& forces,
                   double& energy,
                   const BoxOrtho& box,
                   const LJParams& params)
{
    int N = positions.size();
    
    // Reset
    for (auto& f : forces) f = Vec3(0, 0, 0);
    energy = 0.0;
    
    double cutoff2 = params.cutoff * params.cutoff;
    
    for (int i = 0; i < N; ++i) {
        for (int j = i + 1; j < N; ++j) {
            Vec3 dr = box.delta(positions[i], positions[j]);
            double r2 = dr.x*dr.x + dr.y*dr.y + dr.z*dr.z;
            
            if (r2 > cutoff2) continue;
            
            double r = std::sqrt(r2);
            if (r < 0.5) r = 0.5;
            
            double s_r = params.sigma / r;
            double s_r6 = s_r * s_r * s_r * s_r * s_r * s_r;
            double s_r12 = s_r6 * s_r6;
            
            double E_pair = 4.0 * params.epsilon * (s_r12 - s_r6);
            double dE_dr = 4.0 * params.epsilon * (-12.0 * s_r12 / r + 6.0 * s_r6 / r);
            
            energy += E_pair;
            
            Vec3 f = (dE_dr / r) * dr;
            forces[i] = forces[i] + f;
            forces[j] = forces[j] - f;
        }
    }
}

// ============================================================================
// Test 2: Pair Loop Throughput Baseline
// ============================================================================
void benchmark_pair_loop(int N) {
    std::cout << "\n=== Benchmark: LJ O(N²) for N = " << N << " ===\n";
    
    // Use box size that gives reasonable density
    double density = 0.5;  // particles/Å³ (moderate density)
    double volume = N / density;
    double L = std::cbrt(volume);
    
    BoxOrtho box(L, L, L);
    LJParams params;
    
    std::cout << "  Box size: " << L << " Å (density = " << density << " particles/Å³)\n";
    std::cout << "  Total pairs: " << (N * (N - 1) / 2) << "\n";
    
    // Generate random initial positions
    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> dist(0.0, L);
    
    std::vector<Vec3> positions(N);
    std::vector<Vec3> forces(N);
    double energy;
    
    for (int i = 0; i < N; ++i) {
        positions[i] = {dist(rng), dist(rng), dist(rng)};
    }
    
    // Warm-up run
    compute_lj_pbc(positions, forces, energy, box, params);
    
    // Timing runs
    const int N_RUNS = 10;
    std::vector<double> times;
    std::vector<double> energies;
    
    std::cout << "  Running " << N_RUNS << " iterations...\n";
    
    for (int run = 0; run < N_RUNS; ++run) {
        Timer timer;
        timer.start();
        
        compute_lj_pbc(positions, forces, energy, box, params);
        
        times.push_back(timer.elapsed_ms());
        energies.push_back(energy);
    }
    
    // Compute statistics
    double avg_time = 0.0;
    for (double t : times) avg_time += t;
    avg_time /= N_RUNS;
    
    double min_time = *std::min_element(times.begin(), times.end());
    double max_time = *std::max_element(times.begin(), times.end());
    
    // Check energy repeatability
    double E_avg = 0.0;
    for (double e : energies) E_avg += e;
    E_avg /= N_RUNS;
    
    double E_std = 0.0;
    for (double e : energies) {
        double diff = e - E_avg;
        E_std += diff * diff;
    }
    E_std = std::sqrt(E_std / N_RUNS);
    
    std::cout << "  Timing:\n";
    std::cout << "    Average: " << avg_time << " ms/step\n";
    std::cout << "    Min:     " << min_time << " ms/step\n";
    std::cout << "    Max:     " << max_time << " ms/step\n";
    std::cout << "    Throughput: " << (N * (N - 1) / 2) / (avg_time / 1000.0) / 1e6 
              << " M pairs/sec\n";
    
    std::cout << "  Energy Repeatability:\n";
    std::cout << "    Mean:   " << E_avg << " kcal/mol\n";
    std::cout << "    StdDev: " << E_std << " kcal/mol\n";
    std::cout << "    CV:     " << (E_std / std::abs(E_avg) * 100.0) << " %\n";
    
    // Check repeatability
    bool repeatable = (E_std / std::abs(E_avg)) < 1e-10;
    if (repeatable) {
        std::cout << "  ✓ EXCELLENT: Energy is perfectly repeatable\n";
    } else {
        std::cout << "  ⚠ WARNING: Energy shows variation (should be deterministic)\n";
    }
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << std::fixed << std::setprecision(6);
    
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  PBC Phase 4 — Performance Baselines                     ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\nEstablishing pre-optimization baselines.\n";
    
    #ifdef NDEBUG
    std::cout << "\n✓ Release build detected (optimizations enabled)\n";
    #else
    std::cout << "\n⚠ WARNING: Debug build detected!\n";
    std::cout << "  For accurate benchmarks, compile with: cmake -DCMAKE_BUILD_TYPE=Release\n";
    #endif
    
    std::cout << "\n============================================================\n";
    std::cout << "PERFORMANCE BENCHMARKS\n";
    std::cout << "============================================================\n";
    
    // Microbench delta()
    microbench_delta();
    
    // Pair loop throughput for different system sizes
    benchmark_pair_loop(256);
    benchmark_pair_loop(512);
    benchmark_pair_loop(1024);
    
    std::cout << "\n============================================================\n";
    std::cout << "BASELINE RECORDING COMPLETE\n";
    std::cout << "============================================================\n";
    std::cout << "\nSave these numbers for future comparison.\n";
    std::cout << "After adding neighbor lists, throughput should improve ~10-100x.\n\n";
    
    return 0;
}
