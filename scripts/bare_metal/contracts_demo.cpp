/**
 * contracts_demo.cpp
 * ──────────────────
 * C++26 bare-metal script #2: Runtime Contracts, Stacktrace,
 * and hot-reload-friendly shared-lib architecture.
 *
 * Build (AlmaLinux 10, GCC 14):
 *   g++ -std=c++23 -O2 -ftrivial-auto-var-init=pattern contracts_demo.cpp \
 *       -lstdc++exp -o contracts_demo
 *
 * Note: -lstdc++exp enables <stacktrace> on GCC 14/15.
 *       C++26 contracts [[pre:]] / [[post:]] are not yet in GCC 14;
 *       we emulate them with a macro that fires in debug builds.
 */

#include "matrix_ops.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <random>
#include <cassert>
#include <cstdlib>
#include <functional>
#include <sstream>
#include <cstring>

// ── Stacktrace (C++23 / GCC 14 -lstdc++exp) ──────────────────────────────────
#if __has_include(<stacktrace>)
  #include <stacktrace>
  #define HAS_STACKTRACE 1
#else
  #define HAS_STACKTRACE 0
#endif

static void print_trace(const char* label) {
#if HAS_STACKTRACE
    std::cout << "  [TRACE] " << label << ":\n"
              << std::stacktrace::current() << "\n";
#else
    std::cout << "  [TRACE] " << label
              << ": <stacktrace> not available (link with -lstdc++exp)\n";
#endif
}

// ── Contract emulation ────────────────────────────────────────────────────────
// C++26 [[pre: cond]] / [[post: cond]] syntax is not yet in GCC 14.
// We emulate it as executable documentation that fires immediately on violation.

#define CONTRACT_PRE(cond)                                                    \
    do { if (!(cond)) {                                                       \
        std::cerr << "  CONTRACT VIOLATION [pre]: " #cond                     \
                  << "\n    at " << __FILE__ << ":" << __LINE__ << "\n";      \
        print_trace("precondition failure");                                  \
        std::abort();                                                         \
    }} while(0)

#define CONTRACT_POST(cond)                                                   \
    do { if (!(cond)) {                                                       \
        std::cerr << "  CONTRACT VIOLATION [post]: " #cond                    \
                  << "\n    at " << __FILE__ << ":" << __LINE__ << "\n";      \
        print_trace("postcondition failure");                                 \
        std::abort();                                                         \
    }} while(0)

// ── Contracted matrix element access ──────────────────────────────────────────
auto get_element(const Matrix& m, size_t r, size_t c) -> double {
    // C++26 contract (emulated): [[pre: r < m.rows && c < m.cols]]
    CONTRACT_PRE(r < m.rows && c < m.cols);
    double val = m.data[r * m.cols + c];
    // [[post: std::isfinite(val)]]
    CONTRACT_POST(std::isfinite(val));
    return val;
}

// ── Contracted matrix multiply ────────────────────────────────────────────────
auto safe_mat_mul(const Matrix& A, const Matrix& B) -> Matrix {
    CONTRACT_PRE(A.cols == B.rows);
    auto C = mat_mul(A, B);
    CONTRACT_POST(C.rows == A.rows && C.cols == B.cols);
    return C;
}

// ── Contracted norm ───────────────────────────────────────────────────────────
auto safe_norm(const Matrix& m) -> double {
    CONTRACT_PRE(m.size() > 0);
    double n = m.frobenius_norm();
    CONTRACT_POST(n >= 0.0);
    return n;
}

// ── Demo: contracts in action ─────────────────────────────────────────────────
static void demo_contracts() {
    std::cout << "\n── Runtime Contracts as Executable Documentation ───────\n";

    auto A = create_matrix(4, 4);
    std::mt19937 rng(42);
    for (auto& v : A.data) v = std::uniform_real_distribution<>(-5.0, 5.0)(rng);

    // Valid access
    double val = get_element(A, 2, 3);
    std::cout << "  A[2,3] = " << val << "  ✓ contract passed\n";

    // Valid multiply
    auto I = create_identity(4);
    auto AI = safe_mat_mul(A, I);
    double diff = 0.0;
    for (size_t i = 0; i < A.data.size(); ++i)
        diff += std::abs(A.data[i] - AI.data[i]);
    std::cout << "  ||A*I - A|| = " << std::scientific << diff
              << "  ✓ postcondition passed\n";

    // Norm
    double n = safe_norm(A);
    std::cout << "  ||A||_F = " << std::fixed << std::setprecision(6) << n
              << "  ✓ postcondition (norm >= 0) passed\n";

    std::cout << "  Contracts: 6 checked, 0 violations\n";
}

// ── Demo: stacktrace ──────────────────────────────────────────────────────────
static void inner_function() { print_trace("inner_function call site"); }
static void middle_function() { inner_function(); }
static void outer_function() { middle_function(); }

static void demo_stacktrace() {
    std::cout << "\n── Native Stack Traces (C++23 <stacktrace>) ───────────\n";
    outer_function();
}

// ── Demo: hot-reload architecture ─────────────────────────────────────────────
static void demo_hot_reload_concept() {
    std::cout << "\n── Hot Reload Architecture (Shared Library Pattern) ────\n";
    std::cout << "  Compile matrix_ops as shared lib:\n";
    std::cout << "    g++ -std=c++23 -shared -fPIC -O2 matrix_ops.cpp -o libmatrix.so\n";
    std::cout << "  Main program loads via dlopen():\n";
    std::cout << "    void* lib = dlopen(\"./libmatrix.so\", RTLD_NOW);\n";
    std::cout << "    auto create = (create_fn)dlsym(lib, \"create_matrix\");\n";
    std::cout << "  Modify matrix_ops.cpp → recompile → dlclose + dlopen\n";
    std::cout << "  → Functions update without restarting the process.\n";
    std::cout << "  Tools: Jet-Live, Live++, or manual dlopen cycle.\n";
    std::cout << "  ✓ Pattern documented (not executed in this demo)\n";
}

// ── Demo: erroneous behaviour trapping ────────────────────────────────────────
static void demo_erroneous_pattern() {
    std::cout << "\n── Erroneous Behaviour Pattern Init ────────────────────\n";
    std::cout << "  Compile flag: -ftrivial-auto-var-init=pattern\n";
    std::cout << "  GCC 14/15 scribbles 0xFE into uninitialised stack vars\n";

    // Show what the pattern looks like
    alignas(8) unsigned char buf[32];
    memset(buf, 0xFE, sizeof(buf));
    std::cout << "  Pattern bytes: ";
    for (int i = 0; i < 8; ++i)
        std::cout << std::hex << std::setfill('0') << std::setw(2)
                  << (int)buf[i] << " ";
    std::cout << std::dec << "...\n";

    double trap_val;
    memset(&trap_val, 0xFE, sizeof(trap_val));
    std::cout << "  Trap double value: " << trap_val << "\n";
    std::cout << "  ✓ Predictable crash instead of silent corruption\n";
}

// ── Stress test with contracts ────────────────────────────────────────────────
static void demo_stress_contracts() {
    std::cout << "\n── Stress: 10000 Contracted Operations ─────────────────\n";

    auto t0 = std::chrono::high_resolution_clock::now();
    size_t checks = 0;

    auto A = create_matrix(16, 16);
    auto B = create_matrix(16, 16);
    std::mt19937 rng(99);
    for (auto& v : A.data) v = std::uniform_real_distribution<>(-1.0, 1.0)(rng);
    for (auto& v : B.data) v = std::uniform_real_distribution<>(-1.0, 1.0)(rng);

    for (int iter = 0; iter < 10000; ++iter) {
        auto C = safe_mat_mul(A, B);   checks += 2;  // pre + post
        [[maybe_unused]] double n = safe_norm(C);     checks += 2;
        [[maybe_unused]] double v = get_element(C, iter % 16, iter % 16);
        checks += 2;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "  Iterations: 10000\n";
    std::cout << "  Contract checks: " << checks << "\n";
    std::cout << "  Violations: 0\n";
    std::cout << "  Time: " << std::fixed << std::setprecision(2) << ms << " ms\n";
    std::cout << "  Rate: " << std::setprecision(0) << (checks / (ms / 1000.0))
              << " checks/sec\n";
}

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║    VSEPR-SIM  Bare Metal Script #2                      ║\n";
    std::cout << "║    C++26 Contracts · Stacktrace · Hot Reload            ║\n";
    std::cout << "║    AlmaLinux 10 · GCC 14.3.1 · i9-13900K               ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    demo_contracts();
    demo_stacktrace();
    demo_erroneous_pattern();
    demo_hot_reload_concept();
    demo_stress_contracts();

    std::cout << "\n── Complete ────────────────────────────────────────────\n";
    return 0;
}
