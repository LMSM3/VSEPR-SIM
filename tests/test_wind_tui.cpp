/**
 * test_wind_tui.cpp — Wind particle + Crystal TUI tests
 * VSEPR-SIM 3.0.0
 *
 * T1: Wind force is additive to state.F and state.E.Uext
 * T2: Force clamp respects F_max
 * T3: Ramp schedule reaches full strength after ramp_steps
 * T4: dt_factor headroom is correctly computed
 * T5: Gaussian envelope tapers force spatially
 * T6: TUI snapshot capture populates all fields
 * T7: TUI renders non-empty string with ANSI codes
 * T8: Wind + lattice round-trip through TUI
 */

#include "atomistic/models/wind_particle.hpp"
#include "atomistic/tui/crystal_tui.hpp"
#include "atomistic/crystal/lattice.hpp"
#include "atomistic/crystal/unit_cell.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>

using namespace atomistic;
using namespace atomistic::perturbation;
using namespace atomistic::tui;
using namespace atomistic::crystal;

// ── Helpers ──

static State make_test_state(int N, double spacing = 2.0) {
    State s;
    s.N = N;
    s.X.resize(N);
    s.V.resize(N);
    s.Q.resize(N, 0.0);
    s.M.resize(N, 12.0);  // carbon-like
    s.type.resize(N, 6);
    s.F.resize(N);

    int side = static_cast<int>(std::ceil(std::cbrt(N)));
    int idx = 0;
    for (int ix = 0; ix < side && idx < N; ++ix)
        for (int iy = 0; iy < side && idx < N; ++iy)
            for (int iz = 0; iz < side && idx < N; ++iz) {
                s.X[idx] = {ix * spacing, iy * spacing, iz * spacing};
                s.V[idx] = {0, 0, 0};
                s.F[idx] = {0, 0, 0};
                ++idx;
            }
    return s;
}

// ── T1: Wind force is additive ──

static void test_wind_additive() {
    State s = make_test_state(8);
    // Pre-set some existing forces
    for (auto& f : s.F) f = {1.0, 0.0, 0.0};
    double pre_Uext = s.E.Uext;
    (void)pre_Uext;

    WindParticle wind;
    wind.params.direction = {0.0, 1.0, 0.0};
    wind.params.strength  = 0.3;
    wind.params.ramp_steps = 0;  // instant full strength
    wind.apply(s);

    // Forces should now have both x and y components
    for (uint32_t i = 0; i < s.N; ++i) {
        assert(std::abs(s.F[i].x - 1.0) < 1e-10);  // original preserved
        assert(std::abs(s.F[i].y - 0.3) < 1e-10);   // wind added
    }
    // Uext should have changed
    assert(s.E.Uext != pre_Uext);

    std::printf("  T1 PASS: wind force is additive\n");
}

// ── T2: Force clamp ──

static void test_force_clamp() {
    State s = make_test_state(4);
    WindParticle wind;
    wind.params.direction  = {1, 0, 0};
    wind.params.strength   = 10.0;   // very strong
    wind.params.F_max      = 2.0;    // but clamped
    wind.params.ramp_steps = 0;
    wind.apply(s);

    for (uint32_t i = 0; i < s.N; ++i) {
        double fmag = norm(s.F[i]);
        (void)fmag;
        assert(fmag <= 2.0 + 1e-10);
    }

    std::printf("  T2 PASS: force clamp respects F_max\n");
}

// ── T3: Ramp schedule ──

static void test_ramp_schedule() {
    State s = make_test_state(4);
    WindParticle wind;
    wind.params.direction  = {1, 0, 0};
    wind.params.strength   = 1.0;
    wind.params.F_max      = 5.0;
    wind.params.ramp_steps = 100;

    // At step 0, ramp = 0 → no force
    assert(wind.ramp_fraction() < 1e-10);

    // Apply 50 steps
    for (int t = 0; t < 50; ++t) {
        // Reset forces each step
        for (auto& f : s.F) f = {0, 0, 0};
        s.E = EnergyTerms{};
        wind.apply(s);
    }

    // Should be at 50% ramp
    assert(std::abs(wind.ramp_fraction() - 0.5) < 0.02);

    // Apply 50 more
    for (int t = 0; t < 50; ++t) {
        for (auto& f : s.F) f = {0, 0, 0};
        s.E = EnergyTerms{};
        wind.apply(s);
    }

    // Should be at full ramp
    assert(std::abs(wind.ramp_fraction() - 1.0) < 0.01);

    std::printf("  T3 PASS: ramp schedule reaches full strength\n");
}

// ── T4: dt_factor headroom ──

static void test_dt_headroom() {
    WindParticle wind;
    wind.params.dt_factor = 1.5;
    assert(std::abs(wind.effective_dt(1.0) - 1.5) < 1e-10);
    assert(std::abs(wind.effective_dt(0.001) - 0.0015) < 1e-14);

    wind.params.dt_factor = 2.0;
    assert(std::abs(wind.effective_dt(1.0) - 2.0) < 1e-10);

    std::printf("  T4 PASS: dt_factor headroom correct\n");
}

// ── T5: Gaussian envelope taper ──

static void test_gaussian_taper() {
    State s;
    s.N = 2;
    s.X = {{0, 0, 0}, {100, 0, 0}};  // one near, one far
    s.V = {{0, 0, 0}, {0, 0, 0}};
    s.Q = {0, 0};
    s.M = {12, 12};
    s.type = {6, 6};
    s.F = {{0, 0, 0}, {0, 0, 0}};

    WindParticle wind;
    wind.params.direction  = {0, 1, 0};
    wind.params.strength   = 1.0;
    wind.params.F_max      = 5.0;
    wind.params.sigma      = 5.0;    // 5 Å Gaussian
    wind.params.origin     = {0, 0, 0};
    wind.params.ramp_steps = 0;
    wind.apply(s);

    // Atom at origin should get ~full force
    double f_near = norm(s.F[0]);
    // Atom at 100 Å should get ~zero (exp(-100²/(2*25)) ≈ 0)
    double f_far = norm(s.F[1]);
    (void)f_near; (void)f_far;
    assert(f_near > 0.9);
    assert(f_far  < 1e-6);

    std::printf("  T5 PASS: Gaussian envelope tapers force\n");
}

// ── T6: TUI snapshot capture ──

static void test_snapshot_capture() {
    State s = make_test_state(8, 3.0);
    Lattice lat = Lattice::cubic(6.0);

    WindParticle wind;
    wind.params.strength = 0.5;
    wind.params.ramp_steps = 0;
    // Apply once so step_count > 0
    wind.apply(s);

    TUISnapshot snap = TUISnapshot::capture(s, lat, 42, 0.001, &wind);

    assert(snap.step == 42);
    assert(std::abs(snap.dt - 0.001) < 1e-15);
    assert(snap.positions.size() == 8);
    assert(snap.types.size() == 8);
    assert(snap.forces.size() == 8);
    assert(std::abs(snap.a - 6.0) < 1e-10);
    assert(std::abs(snap.alpha_deg - 90.0) < 1e-6);
    assert(snap.wind_active);
    assert(snap.volume > 0);

    std::printf("  T6 PASS: TUI snapshot populates all fields\n");
}

// ── T7: TUI renders non-empty ANSI ──

static void test_tui_render() {
    State s = make_test_state(8, 3.0);
    Lattice lat = Lattice::cubic(6.0);
    TUISnapshot snap = TUISnapshot::capture(s, lat, 10, 0.001);

    CrystalTUI tui;
    std::string frame = tui.render(snap);

    assert(!frame.empty());
    assert(frame.find("\033[") != std::string::npos);  // contains ANSI
    assert(frame.find("MATHEMATICS") != std::string::npos);
    assert(frame.find("Energy") != std::string::npos);
    assert(frame.find("Lattice") != std::string::npos);

    std::printf("  T7 PASS: TUI renders ANSI with math panel\n");
}

// ── T8: Wind + lattice round-trip ──

static void test_wind_lattice_roundtrip() {
    // Build a NaCl-like state
    State s = make_test_state(27, 2.82);
    // Alternate Na/Cl types
    for (uint32_t i = 0; i < s.N; ++i)
        s.type[i] = (i % 2 == 0) ? 11 : 17;

    Lattice lat = Lattice::cubic(8.46);

    WindParticle wind;
    wind.params.direction  = {1, 1, 0};
    wind.params.strength   = 0.8;
    wind.params.F_max      = 2.0;
    wind.params.dt_factor  = 1.5;
    wind.params.ramp_steps = 0;
    wind.apply(s);

    TUISnapshot snap = TUISnapshot::capture(s, lat, 100, 0.001, &wind);
    CrystalTUI tui;
    std::string frame = tui.render(snap);

    assert(frame.find("Wind Particle") != std::string::npos);
    assert(frame.find("headroom") != std::string::npos);
    assert(frame.find("dt_factor") != std::string::npos);

    std::printf("  T8 PASS: wind + lattice round-trip through TUI\n");
}

// ============================================================================

int main() {
    std::printf("=== Wind Particle + Crystal TUI Tests ===\n\n");

    test_wind_additive();
    test_force_clamp();
    test_ramp_schedule();
    test_dt_headroom();
    test_gaussian_taper();
    test_snapshot_capture();
    test_tui_render();
    test_wind_lattice_roundtrip();

    std::printf("\n  ALL 8 TESTS PASSED\n");
    return 0;
}
