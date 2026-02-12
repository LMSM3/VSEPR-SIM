/**
 * pbc_verification.cpp - Phase 1 Unit Tests for PBC (Orthorhombic)
 * 
 * Technical verification according to strict criteria:
 * - Wrap correctness: canonical range [0,L), idempotence, edge cases
 * - MIC delta: antisymmetry, boundedness, translation invariance
 * - Tolerance: eps = 1e-12 for idempotence/invariant checks
 * 
 * STOP CONDITION: If any Phase 1 test fails, do not proceed to physics.
 */

#include "box/pbc.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <random>
#include <cassert>

using namespace vsepr;

// Tolerance for floating point comparisons
const double EPS = 1e-12;

// Test result tracking
struct TestStats {
    int passed = 0;
    int failed = 0;
    
    void pass() { passed++; }
    void fail(const char* msg) {
        failed++;
        std::cerr << "  ✗ FAILED: " << msg << "\n";
    }
    
    void report(const char* name) {
        if (failed == 0) {
            std::cout << "  ✓ " << name << ": ALL PASSED (" << passed << " checks)\n";
        } else {
            std::cout << "  ✗ " << name << ": " << failed << " FAILURES, " 
                      << passed << " passed\n";
        }
    }
};

// Helper: check if value is in range [min, max)
bool in_range(double val, double min, double max) {
    return val >= min && val < max;
}

// Helper: approximate equality
bool approx_equal(double a, double b, double tol = EPS) {
    return std::abs(a - b) < tol;
}

bool approx_equal(const Vec3& a, const Vec3& b, double tol = EPS) {
    return approx_equal(a.x, b.x, tol) && 
           approx_equal(a.y, b.y, tol) && 
           approx_equal(a.z, b.z, tol);
}

// ============================================================================
// Phase 1.1: Wrap Correctness
// ============================================================================

void test_wrap_canonical_range(TestStats& stats) {
    std::cout << "\n=== Test 1.1: Wrap Canonical Range [0,L) ===\n";
    
    BoxOrtho box(10.0, 15.0, 20.0);
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist_x(-100.0, 100.0);  // [-10Lx, +10Lx]
    std::uniform_real_distribution<double> dist_y(-150.0, 150.0);  // [-10Ly, +10Ly]
    std::uniform_real_distribution<double> dist_z(-200.0, 200.0);  // [-10Lz, +10Lz]
    
    const int N_SAMPLES = 10000;
    int canonical_checks = 0;
    
    for (int i = 0; i < N_SAMPLES; ++i) {
        Vec3 r(dist_x(rng), dist_y(rng), dist_z(rng));
        Vec3 wrapped = box.wrap(r);
        
        if (in_range(wrapped.x, 0.0, box.L.x) &&
            in_range(wrapped.y, 0.0, box.L.y) &&
            in_range(wrapped.z, 0.0, box.L.z)) {
            canonical_checks++;
        } else {
            stats.fail("Wrapped coordinate outside [0,L)");
            std::cerr << "    Input: (" << r.x << ", " << r.y << ", " << r.z << ")\n";
            std::cerr << "    Wrapped: (" << wrapped.x << ", " << wrapped.y << ", " << wrapped.z << ")\n";
            std::cerr << "    Box: (" << box.L.x << ", " << box.L.y << ", " << box.L.z << ")\n";
        }
    }
    
    if (canonical_checks == N_SAMPLES) {
        std::cout << "  ✓ All " << N_SAMPLES << " random wraps in canonical range [0,L)\n";
        stats.pass();
    }
}

void test_wrap_idempotence(TestStats& stats) {
    std::cout << "\n=== Test 1.2: Wrap Idempotence ===\n";
    
    BoxOrtho box(10.0, 15.0, 20.0);
    std::mt19937 rng(123);
    std::uniform_real_distribution<double> dist(-200.0, 200.0);
    
    const int N_SAMPLES = 1000;
    int idempotent_checks = 0;
    
    for (int i = 0; i < N_SAMPLES; ++i) {
        Vec3 r(dist(rng), dist(rng), dist(rng));
        Vec3 wrapped1 = box.wrap(r);
        Vec3 wrapped2 = box.wrap(wrapped1);
        
        if (approx_equal(wrapped1, wrapped2, EPS)) {
            idempotent_checks++;
        } else {
            stats.fail("wrap(wrap(r)) != wrap(r)");
            std::cerr << "    Wrap1: (" << wrapped1.x << ", " << wrapped1.y << ", " << wrapped1.z << ")\n";
            std::cerr << "    Wrap2: (" << wrapped2.x << ", " << wrapped2.y << ", " << wrapped2.z << ")\n";
            std::cerr << "    Diff: " << (wrapped2 - wrapped1).norm() << "\n";
        }
    }
    
    if (idempotent_checks == N_SAMPLES) {
        std::cout << "  ✓ wrap(wrap(r)) == wrap(r) for " << N_SAMPLES << " samples (eps=" << EPS << ")\n";
        stats.pass();
    }
}

void test_wrap_edge_cases(TestStats& stats) {
    std::cout << "\n=== Test 1.3: Wrap Edge Cases ===\n";
    
    BoxOrtho box(10.0, 10.0, 10.0);
    const double tiny = 1e-10;
    
    struct EdgeCase {
        const char* name;
        Vec3 input;
        Vec3 expected;
    };
    
    EdgeCase cases[] = {
        {"Zero", Vec3(0, 0, 0), Vec3(0, 0, 0)},
        {"L exactly", Vec3(10.0, 10.0, 10.0), Vec3(0, 0, 0)},
        {"L - tiny", Vec3(10.0 - tiny, 0, 0), Vec3(10.0 - tiny, 0, 0)},
        {"L + tiny", Vec3(10.0 + tiny, 0, 0), Vec3(tiny, 0, 0)},
        {"-tiny", Vec3(-tiny, 0, 0), Vec3(10.0 - tiny, 0, 0)},
        {"2L", Vec3(20.0, 0, 0), Vec3(0, 0, 0)},
        {"2L + tiny", Vec3(20.0 + tiny, 0, 0), Vec3(tiny, 0, 0)},
        {"-L", Vec3(-10.0, 0, 0), Vec3(0, 0, 0)},
        {"-2L", Vec3(-20.0, 0, 0), Vec3(0, 0, 0)},
        {"-2L - tiny", Vec3(-20.0 - tiny, 0, 0), Vec3(10.0 - tiny, 0, 0)},
        {"3L", Vec3(30.0, 0, 0), Vec3(0, 0, 0)},
        {"-3L", Vec3(-30.0, 0, 0), Vec3(0, 0, 0)},
    };
    
    for (const auto& test : cases) {
        Vec3 wrapped = box.wrap(test.input);
        
        // For boundary cases, allow small tolerance
        double tol = (std::abs(test.input.x) < 1.0) ? EPS : 1e-10;
        
        if (approx_equal(wrapped, test.expected, tol)) {
            std::cout << "  ✓ " << test.name << ": wrap(" << test.input.x 
                      << ") = " << wrapped.x << "\n";
            stats.pass();
        } else {
            stats.fail(test.name);
            std::cerr << "    Input: " << test.input.x << "\n";
            std::cerr << "    Expected: " << test.expected.x << "\n";
            std::cerr << "    Got: " << wrapped.x << "\n";
            std::cerr << "    Diff: " << std::abs(wrapped.x - test.expected.x) << "\n";
        }
    }
}

// ============================================================================
// Phase 1.2: Minimum-Image Delta
// ============================================================================

void test_delta_antisymmetry(TestStats& stats) {
    std::cout << "\n=== Test 2.1: Delta Antisymmetry ===\n";
    
    BoxOrtho box(10.0, 15.0, 20.0);
    std::mt19937 rng(456);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    const int N_SAMPLES = 1000;
    int antisym_checks = 0;
    
    for (int i = 0; i < N_SAMPLES; ++i) {
        Vec3 a(dist(rng) * box.L.x, dist(rng) * box.L.y, dist(rng) * box.L.z);
        Vec3 b(dist(rng) * box.L.x, dist(rng) * box.L.y, dist(rng) * box.L.z);
        
        Vec3 delta_ab = box.delta(a, b);
        Vec3 delta_ba = box.delta(b, a);
        Vec3 sum = delta_ab + delta_ba;
        
        if (approx_equal(sum, Vec3(0, 0, 0), EPS)) {
            antisym_checks++;
        } else {
            stats.fail("delta(a,b) + delta(b,a) != 0");
            std::cerr << "    delta(a,b): (" << delta_ab.x << ", " << delta_ab.y << ", " << delta_ab.z << ")\n";
            std::cerr << "    delta(b,a): (" << delta_ba.x << ", " << delta_ba.y << ", " << delta_ba.z << ")\n";
            std::cerr << "    Sum: (" << sum.x << ", " << sum.y << ", " << sum.z << ")\n";
        }
    }
    
    if (antisym_checks == N_SAMPLES) {
        std::cout << "  ✓ delta(a,b) = -delta(b,a) for " << N_SAMPLES << " pairs (eps=" << EPS << ")\n";
        stats.pass();
    }
}

void test_delta_boundedness(TestStats& stats) {
    std::cout << "\n=== Test 2.2: Delta Boundedness [-L/2, +L/2] ===\n";
    
    BoxOrtho box(10.0, 15.0, 20.0);
    std::mt19937 rng(789);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    const int N_SAMPLES = 10000;
    int bounded_checks = 0;
    
    for (int i = 0; i < N_SAMPLES; ++i) {
        Vec3 a(dist(rng) * box.L.x, dist(rng) * box.L.y, dist(rng) * box.L.z);
        Vec3 b(dist(rng) * box.L.x, dist(rng) * box.L.y, dist(rng) * box.L.z);
        
        Vec3 d = box.delta(a, b);
        
        // Check each component is in [-L/2, +L/2]
        bool x_ok = (d.x >= -box.L.x/2 - EPS) && (d.x <= box.L.x/2 + EPS);
        bool y_ok = (d.y >= -box.L.y/2 - EPS) && (d.y <= box.L.y/2 + EPS);
        bool z_ok = (d.z >= -box.L.z/2 - EPS) && (d.z <= box.L.z/2 + EPS);
        
        if (x_ok && y_ok && z_ok) {
            bounded_checks++;
        } else {
            stats.fail("Delta component outside [-L/2, +L/2]");
            std::cerr << "    a: (" << a.x << ", " << a.y << ", " << a.z << ")\n";
            std::cerr << "    b: (" << b.x << ", " << b.y << ", " << b.z << ")\n";
            std::cerr << "    delta: (" << d.x << ", " << d.y << ", " << d.z << ")\n";
            std::cerr << "    L/2: (" << box.L.x/2 << ", " << box.L.y/2 << ", " << box.L.z/2 << ")\n";
        }
    }
    
    if (bounded_checks == N_SAMPLES) {
        std::cout << "  ✓ All delta components in [-L/2, +L/2] for " << N_SAMPLES << " pairs\n";
        stats.pass();
    }
}

void test_delta_translation_invariance(TestStats& stats) {
    std::cout << "\n=== Test 2.3: Delta Translation Invariance ===\n";
    
    BoxOrtho box(10.0, 15.0, 20.0);
    std::mt19937 rng(101112);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    std::uniform_int_distribution<int> int_dist(-5, 5);
    
    const int N_SAMPLES = 1000;
    int invariant_checks = 0;
    
    for (int i = 0; i < N_SAMPLES; ++i) {
        Vec3 a(dist(rng) * box.L.x, dist(rng) * box.L.y, dist(rng) * box.L.z);
        Vec3 b(dist(rng) * box.L.x, dist(rng) * box.L.y, dist(rng) * box.L.z);
        
        // Random integer translation
        Vec3 n(int_dist(rng) * box.L.x, int_dist(rng) * box.L.y, int_dist(rng) * box.L.z);
        
        Vec3 delta_orig = box.delta(a, b);
        Vec3 delta_shifted = box.delta(a + n, b);
        
        if (approx_equal(delta_orig, delta_shifted, 1e-10)) {
            invariant_checks++;
        } else {
            stats.fail("delta(a+nL, b) != delta(a,b)");
            std::cerr << "    delta(a,b): (" << delta_orig.x << ", " << delta_orig.y << ", " << delta_orig.z << ")\n";
            std::cerr << "    delta(a+nL,b): (" << delta_shifted.x << ", " << delta_shifted.y << ", " << delta_shifted.z << ")\n";
            std::cerr << "    Shift: (" << n.x << ", " << n.y << ", " << n.z << ")\n";
        }
    }
    
    if (invariant_checks == N_SAMPLES) {
        std::cout << "  ✓ delta(a+nL, b) == delta(a,b) for " << N_SAMPLES << " translations\n";
        stats.pass();
    }
}

void test_delta_classic_boundary(TestStats& stats) {
    std::cout << "\n=== Test 2.4: Classic Boundary Case (MUST PASS) ===\n";
    
    BoxOrtho box(10.0, 10.0, 10.0);
    
    Vec3 a(0.1, 0, 0);
    Vec3 b(9.9, 0, 0);
    
    Vec3 d = box.delta(a, b);
    double dist = d.norm();
    
    std::cout << "  Lx = " << box.L.x << "\n";
    std::cout << "  a = (" << a.x << ", " << a.y << ", " << a.z << ")\n";
    std::cout << "  b = (" << b.x << ", " << b.y << ", " << b.z << ")\n";
    std::cout << "  delta = (" << d.x << ", " << d.y << ", " << d.z << ")\n";
    std::cout << "  |delta| = " << dist << "\n";
    
    // Expected: delta.x = -0.2 (shortest path wraps around)
    // Distance should be 0.2
    if (approx_equal(d.x, -0.2, 1e-10) && approx_equal(dist, 0.2, 1e-10)) {
        std::cout << "  ✓ PASS: delta.x = -0.2, |delta| = 0.2 (correct MIC)\n";
        stats.pass();
    } else {
        stats.fail("Classic boundary case failed");
        std::cerr << "    Expected: delta.x = -0.2, |delta| = 0.2\n";
        std::cerr << "    Got: delta.x = " << d.x << ", |delta| = " << dist << "\n";
    }
}

void test_delta_halfbox_tie(TestStats& stats) {
    std::cout << "\n=== Test 2.5: Half-Box Tie Case (Policy Check) ===\n";
    
    BoxOrtho box(10.0, 10.0, 10.0);
    
    Vec3 a(0, 0, 0);
    Vec3 b(5.0, 0, 0);  // Exactly L/2
    
    Vec3 d = box.delta(a, b);
    
    std::cout << "  a = (0, 0, 0)\n";
    std::cout << "  b = (Lx/2, 0, 0) = (5.0, 0, 0)\n";
    std::cout << "  delta = (" << d.x << ", " << d.y << ", " << d.z << ")\n";
    
    // Check that the result is either +L/2 or -L/2 (consistent policy)
    if (approx_equal(std::abs(d.x), box.L.x/2, EPS)) {
        if (d.x > 0) {
            std::cout << "  ✓ Policy: Tie at L/2 returns +L/2 (nearbyint rounds to even)\n";
        } else {
            std::cout << "  ✓ Policy: Tie at L/2 returns -L/2 (nearbyint rounds to even)\n";
        }
        stats.pass();
    } else {
        stats.fail("Half-box tie case inconsistent");
        std::cerr << "    Expected: |delta.x| = L/2 = 5.0\n";
        std::cerr << "    Got: delta.x = " << d.x << "\n";
    }
    
    // Document the policy
    std::cout << "\n  DOCUMENTED POLICY:\n";
    std::cout << "  When |displacement| = L/2 exactly, nearbyint() uses round-to-even.\n";
    std::cout << "  For displacement = +5.0 in box L=10: nearbyint(0.5) may round to 0.\n";
    std::cout << "  Result: delta can be ±L/2 depending on floating point state.\n";
    std::cout << "  This is acceptable as long as behavior is deterministic.\n";
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << std::fixed << std::setprecision(12);
    
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  PBC (Orthorhombic) — Phase 1 Verification Tests         ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\nTolerance: eps = " << EPS << "\n";
    std::cout << "Stop condition: ANY failure blocks Phase 2 (physics)\n";
    
    TestStats total;
    
    // Phase 1.1: Wrap Tests
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "PHASE 1.1 — WRAP CORRECTNESS\n";
    std::cout << std::string(60, '=') << "\n";
    
    TestStats wrap_stats;
    test_wrap_canonical_range(wrap_stats);
    test_wrap_idempotence(wrap_stats);
    test_wrap_edge_cases(wrap_stats);
    
    total.passed += wrap_stats.passed;
    total.failed += wrap_stats.failed;
    
    std::cout << "\nPhase 1.1 Summary: ";
    if (wrap_stats.failed == 0) {
        std::cout << "✓ ALL WRAP TESTS PASSED\n";
    } else {
        std::cout << "✗ " << wrap_stats.failed << " WRAP TEST(S) FAILED\n";
    }
    
    // Phase 1.2: Delta (MIC) Tests
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "PHASE 1.2 — MINIMUM-IMAGE DELTA (MIC)\n";
    std::cout << std::string(60, '=') << "\n";
    
    TestStats delta_stats;
    test_delta_antisymmetry(delta_stats);
    test_delta_boundedness(delta_stats);
    test_delta_translation_invariance(delta_stats);
    test_delta_classic_boundary(delta_stats);
    test_delta_halfbox_tie(delta_stats);
    
    total.passed += delta_stats.passed;
    total.failed += delta_stats.failed;
    
    std::cout << "\nPhase 1.2 Summary: ";
    if (delta_stats.failed == 0) {
        std::cout << "✓ ALL DELTA TESTS PASSED\n";
    } else {
        std::cout << "✗ " << delta_stats.failed << " DELTA TEST(S) FAILED\n";
    }
    
    // Final Verdict
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "PHASE 1 FINAL VERDICT\n";
    std::cout << std::string(60, '=') << "\n";
    std::cout << "Total Checks: " << (total.passed + total.failed) << "\n";
    std::cout << "Passed: " << total.passed << "\n";
    std::cout << "Failed: " << total.failed << "\n";
    
    if (total.failed == 0) {
        std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ✓✓✓ PHASE 1 COMPLETE — PBC IMPLEMENTATION VERIFIED   ✓✓✓ ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n";
        std::cout << "\nPROCEED TO PHASE 2: Physics integration (LJ, bonds, etc.)\n\n";
        return 0;
    } else {
        std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ✗✗✗ PHASE 1 FAILED — DO NOT PROCEED TO PHYSICS       ✗✗✗ ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n";
        std::cout << "\nFix failures before implementing physics with PBC.\n\n";
        return 1;
    }
}
