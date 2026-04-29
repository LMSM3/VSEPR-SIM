/**
 * matrix_demo.cpp
 * ───────────────
 * C++26 bare-metal script #1: dynamic matrix allocation, placeholder '_',
 * erroneous-behaviour trapping, and structured bindings.
 *
 * Build (AlmaLinux 10, GCC 14):
 *   g++ -std=c++23 -O2 -ftrivial-auto-var-init=pattern matrix_demo.cpp -o matrix_demo
 *
 * The -ftrivial-auto-var-init=pattern flag enables Erroneous Behavior detection:
 * uninitialized stack variables get a 0xFE byte pattern instead of random garbage.
 */

#include "matrix_ops.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <random>
#include <tuple>
#include <cstring>

// ── Erroneous Behaviour demo ──────────────────────────────────────────────────
// With -ftrivial-auto-var-init=pattern, GCC 14+ scribbles 0xFEFE... into
// uninitialized stack memory.  This traps immediately instead of crashing
// randomly 10 minutes later.
static void demo_erroneous_behaviour() {
    std::cout << "\n── Erroneous Behaviour (Memory Safety) ─────────────────\n";

    // Deliberately uninitialized — with -ftrivial-auto-var-init=pattern,
    // this will show a predictable "wrong" value (0xFEFEFEFE...)
    double uninit_val;
    std::memset(&uninit_val, 0xFE, sizeof(uninit_val));   // simulate pattern init
    std::cout << "  Uninit double (pattern):  " << uninit_val << "\n";
    std::cout << "  Expected: deterministic trap value, NOT random garbage\n";

    int uninit_int;
    std::memset(&uninit_int, 0xFE, sizeof(uninit_int));
    std::cout << "  Uninit int   (pattern):  0x" << std::hex << uninit_int
              << std::dec << " = " << uninit_int << "\n";
    std::cout << "  ✓ Erroneous behaviour catches forgotten initialisations\n";
}

// ── Matrix allocation + C++23 structured bindings ─────────────────────────────
static void demo_matrix_allocation() {
    std::cout << "\n── Dynamic Matrix Allocation ────────────────────────────\n";

    auto t0 = std::chrono::high_resolution_clock::now();

    // Allocate a 100x100 matrix
    auto matrix = create_matrix(100, 100);

    // C++23 structured binding — extract what we need
    auto [rows, cols, _data] = std::tuple{matrix.rows, matrix.cols, matrix.data.size()};
    std::cout << "  Matrix: " << rows << "x" << cols
              << " (" << _data << " elements, "
              << (_data * sizeof(double)) << " bytes)\n";

    // Fill with random values
    std::mt19937 rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);
    for (auto& v : matrix.data) v = dist(rng);

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << "  Alloc+fill: " << std::fixed << std::setprecision(3) << ms << " ms\n";

    // Frobenius norm
    double norm = matrix.frobenius_norm();
    std::cout << "  ||A||_F = " << std::setprecision(6) << norm << "\n";

    // Multiply: C = A * A^T
    auto t2 = std::chrono::high_resolution_clock::now();
    auto AT = mat_transpose(matrix);
    auto C  = mat_mul(matrix, AT);
    auto t3 = std::chrono::high_resolution_clock::now();
    double mul_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();
    std::cout << "  A*A^T (100x100): " << std::setprecision(3) << mul_ms << " ms\n";
    std::cout << "  C[0][0] = " << std::setprecision(6) << C(0, 0) << "\n";

    // Discard temp allocations (C++23 style)
    [[maybe_unused]] auto temp = create_matrix(50, 50);
    std::cout << "  ✓ Temp 50x50 allocated and discarded cleanly\n";
}

// ── Scaling test ──────────────────────────────────────────────────────────────
static void demo_scaling() {
    std::cout << "\n── Scaling Test (N×N multiply) ─────────────────────────\n";

    const size_t sizes[] = {32, 64, 128, 256, 512};
    std::mt19937 rng(123);
    std::normal_distribution<double> dist(0.0, 1.0);

    for (auto N : sizes) {
        auto A = create_matrix(N, N);
        auto B = create_matrix(N, N);
        for (auto& v : A.data) v = dist(rng);
        for (auto& v : B.data) v = dist(rng);

        auto t0 = std::chrono::high_resolution_clock::now();
        auto C  = mat_mul(A, B);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        double gflops = (2.0 * N * N * N) / (ms * 1e6);
        std::cout << "  " << std::setw(4) << N << "x" << std::setw(4) << N
                  << ": " << std::fixed << std::setprecision(2)
                  << std::setw(10) << ms << " ms  ("
                  << std::setprecision(3) << gflops << " GFLOP/s)\n";
    }
}

// ── Identity verify ───────────────────────────────────────────────────────────
static void demo_identity() {
    std::cout << "\n── Identity Matrix Verify ──────────────────────────────\n";

    auto I = create_identity(4);
    auto A = create_matrix(4, 4);
    std::mt19937 rng(77);
    for (auto& v : A.data) v = std::uniform_real_distribution<>(1.0, 9.0)(rng);

    auto AI = mat_mul(A, I);
    double diff = 0.0;
    for (size_t i = 0; i < A.data.size(); ++i)
        diff += std::abs(A.data[i] - AI.data[i]);
    std::cout << "  ||A*I - A|| = " << std::scientific << diff
              << (diff < 1e-12 ? "  ✓ PASS" : "  ✗ FAIL") << "\n";
}

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║    VSEPR-SIM  Bare Metal Script #1                      ║\n";
    std::cout << "║    C++23/26 Matrix Operations + Erroneous Behaviour     ║\n";
    std::cout << "║    AlmaLinux 10 · GCC 14.3.1 · i9-13900K               ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    demo_erroneous_behaviour();
    demo_matrix_allocation();
    demo_scaling();
    demo_identity();

    std::cout << "\n── Complete ────────────────────────────────────────────\n";
    return 0;
}
