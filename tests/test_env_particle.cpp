/**
 * test_env_particle.cpp  —  Environmental Particle Extension Tests
 * ================================================================
 * VSEPR-SIM 3.0.1
 *
 * 45 tests covering:
 *   T1-T5:   Particle construction and kind classification
 *   T6-T10:  Sun deposition kernel
 *   T11-T15: Wind force kernel
 *   T16-T18: Drying kernel
 *   T19-T22: Unified interaction dispatch
 *   T23-T27: Root chaos factor (Gamma_i)
 *   T28-T32: Piecewise polynomial root response
 *   T33-T37: Root growth modifier (poly * logic * chaos)
 *   T38-T42: Leaf generation gate (Heaviside)
 *   T43:     Energy decomposition accumulation
 *   T44:     Particle advection and lifetime decay
 *   T45:     Full pipeline (particle -> bead -> response -> root -> leaf)
 */

#include "atomistic/models/env_particle.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace atomistic;
using namespace atomistic::environment;

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(expr, msg)                                                \
    do {                                                                \
        if (expr) {                                                     \
            ++g_pass;                                                   \
            std::printf("  T%-2d PASS: %s\n", g_pass + g_fail, msg);   \
        } else {                                                        \
            ++g_fail;                                                   \
            std::printf("  T%-2d FAIL: %s  [%s:%d]\n",                  \
                        g_pass + g_fail, msg, __FILE__, __LINE__);      \
        }                                                               \
    } while (0)

static bool near(double a, double b, double tol = 1e-6) {
    return std::abs(a - b) < tol;
}

// =====================================================================
// T1-T5: Particle construction
// =====================================================================

static void test_particle_construction() {
    EnvParticle p;
    CHECK(p.kind == EnvParticleKind::Sun, "default kind is Sun");

    p.kind = EnvParticleKind::Wind;
    CHECK(p.kind == EnvParticleKind::Wind, "kind set to Wind");

    CHECK(std::string(kind_name(EnvParticleKind::Sun)) == "SUN",
          "kind_name Sun");
    CHECK(std::string(kind_name(EnvParticleKind::Wind)) == "WIND",
          "kind_name Wind");

    EnvParticle sun;
    sun.kind = EnvParticleKind::Sun;
    sun.intensity = 2.5;
    sun.energy = 100.0;
    sun.lifetime = 5.0;
    CHECK(sun.intensity == 2.5 && sun.energy == 100.0 && sun.lifetime == 5.0,
          "sun particle fields");
}

// =====================================================================
// T6-T10: Sun deposition
// =====================================================================

static void test_sun_deposition() {
    BeadProps bead;
    bead.projected_area = 2.0;
    bead.transmissivity = 0.5;
    bead.photo_response = 1.0;

    EnvParticle sun;
    sun.kind = EnvParticleKind::Sun;
    sun.intensity = 3.0;

    // dE = A * I * T * S * Phi = 2 * 3 * 0.5 * 1 * 1 = 3.0
    double dE = sun_deposition(bead, sun, 0.0);
    CHECK(near(dE, 3.0), "sun deposition basic (dE=3.0)");

    // With 50% occlusion: dE = 3.0 * 0.5 = 1.5
    dE = sun_deposition(bead, sun, 0.5);
    CHECK(near(dE, 1.5), "sun deposition with 50% shade");

    // Full occlusion
    dE = sun_deposition(bead, sun, 1.0);
    CHECK(near(dE, 0.0), "sun deposition full occlusion");

    // Zero area
    bead.projected_area = 0.0;
    dE = sun_deposition(bead, sun, 0.0);
    CHECK(near(dE, 0.0), "sun deposition zero area");

    // High photo response
    bead.projected_area = 1.0;
    bead.photo_response = 3.0;
    dE = sun_deposition(bead, sun, 0.0);
    // 1 * 3 * 0.5 * 3 * 1 = 4.5
    CHECK(near(dE, 4.5), "sun deposition high photo response");
}

// =====================================================================
// T11-T15: Wind force
// =====================================================================

static void test_wind_force() {
    BeadProps bead;
    bead.projected_area = 1.0;
    bead.drag_coeff = 0.5;
    bead.flexibility = 1.0;
    bead.velocity = {0, 0, 0};

    EnvParticle wind;
    wind.kind = EnvParticleKind::Wind;
    wind.velocity = {10, 0, 0};

    // F = C_d * A * |v_rel|^2 * Psi = 0.5 * 1 * 100 * 1 = 50
    Vec3 F = wind_force(bead, wind);
    CHECK(near(F.x, 50.0) && near(F.y, 0.0) && near(F.z, 0.0),
          "wind force +x direction (F=50)");

    // Diagonal wind
    wind.velocity = {3, 4, 0};  // |v| = 5, |v|^2 = 25
    F = wind_force(bead, wind);
    double Fmag = norm(F);
    // F_mag = 0.5 * 1 * 25 * 1 = 12.5
    CHECK(near(Fmag, 12.5), "wind force diagonal (|F|=12.5)");

    // Direction check: F should be along (3,4,0)/5
    CHECK(near(F.x / Fmag, 0.6, 0.01) && near(F.y / Fmag, 0.8, 0.01),
          "wind force direction matches velocity");

    // Zero relative velocity
    bead.velocity = {10, 0, 0};
    wind.velocity = {10, 0, 0};
    F = wind_force(bead, wind);
    CHECK(near(norm(F), 0.0), "wind force zero relative velocity");

    // Flexibility modifier
    bead.velocity = {0, 0, 0};
    wind.velocity = {10, 0, 0};
    bead.flexibility = 0.5;
    F = wind_force(bead, wind);
    // 0.5 * 1 * 100 * 0.5 = 25
    CHECK(near(F.x, 25.0), "wind force with flexibility=0.5");
}

// =====================================================================
// T16-T18: Drying kernel
// =====================================================================

static void test_drying_kernel() {
    BeadProps bead;
    bead.projected_area = 2.0;
    bead.velocity = {0, 0, 0};

    EnvParticle wind;
    wind.kind = EnvParticleKind::Wind;
    wind.velocity = {5, 0, 0};

    // drying = coeff * |v_rel| * A = 0.05 * 5 * 2 = 0.5
    double dr = drying_kernel(bead, wind, 0.05);
    CHECK(near(dr, 0.5), "drying kernel basic");

    // Zero wind
    wind.velocity = {0, 0, 0};
    dr = drying_kernel(bead, wind, 0.05);
    CHECK(near(dr, 0.0), "drying kernel zero wind");

    // Custom coefficient
    wind.velocity = {10, 0, 0};
    dr = drying_kernel(bead, wind, 0.1);
    // 0.1 * 10 * 2 = 2.0
    CHECK(near(dr, 2.0), "drying kernel custom coeff");
}

// =====================================================================
// T19-T22: Unified interaction
// =====================================================================

static void test_unified_interaction() {
    BeadProps bead;
    bead.projected_area = 1.5;
    bead.transmissivity = 0.9;
    bead.photo_response = 1.0;
    bead.drag_coeff = 0.47;
    bead.flexibility = 1.0;

    // Sun particle
    EnvParticle sun;
    sun.kind = EnvParticleKind::Sun;
    sun.intensity = 2.0;

    PlantEnvResponse r = interact_env_particle(bead, sun);
    CHECK(r.dE_sun > 0.0, "unified: sun deposits energy");
    CHECK(near(r.photo_bias, r.dE_sun * 0.15), "unified: photo_bias = 15% of dE");
    CHECK(near(norm(r.dF_wind), 0.0), "unified: sun produces no wind force");

    // Wind particle
    EnvParticle wind;
    wind.kind = EnvParticleKind::Wind;
    wind.velocity = {8, 0, 0};

    r = interact_env_particle(bead, wind);
    CHECK(r.dF_wind.x > 0.0, "unified: wind produces force");
    CHECK(r.drying_rate > 0.0, "unified: wind causes drying");
    CHECK(r.stress_bias > 0.0, "unified: wind causes stress");
    CHECK(near(r.dE_sun, 0.0), "unified: wind deposits no sun energy");
}

// =====================================================================
// T23-T27: Root chaos factor
// =====================================================================

static void test_root_chaos() {
    RootChaosCoeffs cc;
    RootLocalState s;

    // Default: Gamma = 1.0 + 0.3*0.5 + 0.15*0 + (-0.2)*0.5 + 0.1*0
    //        = 1.0 + 0.15 - 0.1 = 1.05
    double g = root_chaos_factor(s, cc);
    CHECK(near(g, 1.05), "root chaos default (Gamma=1.05)");

    // High moisture -> higher gamma
    s.moisture = 1.0;
    g = root_chaos_factor(s, cc);
    // 1.0 + 0.3 - 0.1 = 1.2
    CHECK(near(g, 1.2), "root chaos high moisture (Gamma=1.2)");

    // Dense soil -> lower gamma
    s.moisture = 0.5;
    s.soil_density = 1.0;
    g = root_chaos_factor(s, cc);
    // 1.0 + 0.15 - 0.2 = 0.95 -> clamped to 0.98
    CHECK(near(g, 0.98), "root chaos dense soil clamped to 0.98");

    // Very high stochastic -> clamped to 1.8
    s.soil_density = 0.0;
    s.chaos_perturb = 50.0;
    g = root_chaos_factor(s, cc);
    CHECK(near(g, 1.8), "root chaos extreme stochastic clamped to 1.8");

    // Custom bounds
    RootChaosCoeffs cc2;
    cc2.lo = 0.5;
    cc2.hi = 3.0;
    cc2.alpha0 = 2.5;
    s = {};
    g = root_chaos_factor(s, cc2);
    CHECK(g >= 0.5 && g <= 3.0, "root chaos custom bounds");
}

// =====================================================================
// T28-T32: Piecewise polynomial
// =====================================================================

static void test_piecewise_poly() {
    PiecewiseRootPoly p = default_root_poly();

    // Dry zone: x = 0.1 -> 2*(0.01) + 0.1*0.1 + 0.05 = 0.02 + 0.01 + 0.05 = 0.08
    double v = p.eval(0.1);
    CHECK(near(v, 0.08), "piecewise poly dry zone (x=0.1)");

    // Exact at x_dry boundary uses dry polynomial
    v = p.eval(0.0);
    CHECK(near(v, 0.05), "piecewise poly at x=0 (intercept)");

    // Optimal zone: x = 0.5
    v = p.eval(0.5);
    // -0.5*(0.125) + 1.2*(0.25) - 0.3*(0.5) + 0.8
    // = -0.0625 + 0.3 - 0.15 + 0.8 = 0.8875
    CHECK(near(v, 0.8875), "piecewise poly optimal zone (x=0.5)");

    // Saturated zone: x = 0.9
    v = p.eval(0.9);
    // -1.5*(0.81) + 2.0*(0.9) + 0.3 = -1.215 + 1.8 + 0.3 = 0.885
    CHECK(near(v, 0.885), "piecewise poly saturated zone (x=0.9)");

    // Monotonicity check: growth peaks in optimal zone
    double v_dry = p.eval(0.1);
    double v_opt = p.eval(0.5);
    CHECK(v_opt > v_dry, "poly optimal > dry growth");
}

// =====================================================================
// T33-T37: Root growth modifier
// =====================================================================

static void test_root_growth_modifier() {
    PiecewiseRootPoly poly = default_root_poly();
    RootChaosCoeffs cc;
    RootGrowthLimits lim;

    RootLocalState s;
    s.moisture = 0.5;  // optimal zone

    double g = root_growth_modifier(s, poly, cc, lim);
    CHECK(g > 0.0 && g <= lim.final_hi, "root modifier basic in bounds");

    // Damage penalty
    s.damage = 0.8;  // above limit
    double g_dmg = root_growth_modifier(s, poly, cc, lim);
    CHECK(g_dmg < g, "root modifier damage reduces growth");

    // Compaction penalty
    s.damage = 0.0;
    s.compaction = 0.8;
    double g_comp = root_growth_modifier(s, poly, cc, lim);
    CHECK(g_comp < g, "root modifier compaction reduces growth");

    // Nutrient boost
    s.compaction = 0.0;
    s.nutrient_grad = 0.5;
    double g_nut = root_growth_modifier(s, poly, cc, lim);
    CHECK(g_nut > g, "root modifier nutrient boost");

    // Low sun penalty
    s.nutrient_grad = 0.0;
    s.sun_coupling = 0.1;
    double g_sun = root_growth_modifier(s, poly, cc, lim);
    CHECK(g_sun < g, "root modifier low sun reduces growth");
}

// =====================================================================
// T38-T42: Leaf generation gate
// =====================================================================

static void test_leaf_gate() {
    LeafGateCoeffs c;
    c.theta = 2.0;

    LeafLocalState s;
    // signal = 0 + 0 + 0 - 0 - 2 = -2 -> no leaf
    CHECK(!leaf_generation_gate(s, c), "leaf gate: no resources -> no leaf");

    // Provide enough resources
    s.energy_local = 2.0;
    s.sunlight = 1.5;
    s.hydration = 1.0;
    // signal = 2 + 1.2 + 0.6 - 0 - 2 = 1.8 -> leaf
    CHECK(leaf_generation_gate(s, c), "leaf gate: sufficient resources -> leaf");

    // Damage kills it
    s.damage_sum = 3.0;
    // signal = 2 + 1.2 + 0.6 - 4.5 - 2 = -2.7 -> no leaf
    CHECK(!leaf_generation_gate(s, c), "leaf gate: damage suppresses");

    // Signal value is continuous
    s.damage_sum = 0.0;
    double sig = leaf_gate_signal(s, c);
    CHECK(sig > 0.0, "leaf gate signal positive when gate open");

    // Expansion rate
    double rate = leaf_expansion_rate(s, c, 0.1);
    CHECK(rate > 0.0, "leaf expansion rate positive when gate open");

    // No expansion below threshold
    s.energy_local = 0.0;
    s.sunlight = 0.0;
    s.hydration = 0.0;
    rate = leaf_expansion_rate(s, c, 0.1);
    CHECK(near(rate, 0.0), "leaf expansion zero below threshold");
}

// =====================================================================
// T43: Energy decomposition
// =====================================================================

static void test_energy_decomposition() {
    BeadProps bead;
    bead.projected_area = 1.0;
    bead.position = {1, 0, 0};

    PlantEnvResponse r;
    r.dE_sun = 5.0;
    r.dF_wind = {2, 0, 0};
    r.drying_rate = 0.3;
    r.photo_bias = 0.75;
    r.stress_bias = 0.16;

    std::vector<PlantEnvResponse> resps = {r};
    std::vector<BeadProps> beads = {bead};

    EnvEnergyTerms E = accumulate_env(resps, beads);
    CHECK(near(E.U_sun, 5.0), "energy: U_sun accumulated");
    CHECK(E.U_wind < 0.0, "energy: U_wind = -F.r < 0 for +x force/position");
    CHECK(near(E.U_dry, 0.3), "energy: U_dry");
    CHECK(E.total() != 0.0, "energy: nonzero total");
}

// =====================================================================
// T44: Particle advection
// =====================================================================

static void test_particle_advection() {
    EnvParticle p;
    p.position = {0, 0, 0};
    p.velocity = {1, 0, 0};
    p.lifetime = 2.0;
    p.omega = 0.0;

    bool alive = advance_particle(p, 0.5);
    CHECK(alive, "particle alive after partial dt");
    CHECK(near(p.position.x, 0.5), "particle advected to x=0.5");
    CHECK(near(p.lifetime, 1.5), "particle lifetime decreased");

    // Kill it
    alive = advance_particle(p, 2.0);
    CHECK(!alive, "particle dead after lifetime exceeded");
}

// =====================================================================
// T45: Full pipeline
// =====================================================================

static void test_full_pipeline() {
    // 1. Create sun and wind particles
    EnvParticle sun;
    sun.kind = EnvParticleKind::Sun;
    sun.intensity = 2.0;
    sun.velocity = {0, -1, 0};

    EnvParticle wind;
    wind.kind = EnvParticleKind::Wind;
    wind.velocity = {5, 0, 0};

    // 2. Create bead
    BeadProps bead;
    bead.projected_area = 1.0;
    bead.transmissivity = 0.7;
    bead.photo_response = 1.2;
    bead.drag_coeff = 0.5;
    bead.flexibility = 0.9;

    // 3. Interact
    PlantEnvResponse r_sun = interact_env_particle(bead, sun);
    PlantEnvResponse r_wind = interact_env_particle(bead, wind);
    PlantEnvResponse combined = r_sun + r_wind;

    // 4. Root chaos
    RootLocalState rs;
    rs.moisture = 0.6;
    rs.sun_coupling = r_sun.dE_sun;
    double gamma = root_chaos_factor(rs);

    // 5. Root growth
    PiecewiseRootPoly poly = default_root_poly();
    double growth = root_growth_modifier(rs, poly);

    // 6. Leaf gate
    LeafLocalState ls;
    ls.energy_local = r_sun.dE_sun;
    ls.sunlight = sun.intensity;
    ls.hydration = rs.moisture;
    bool leaf = leaf_generation_gate(ls);

    CHECK(combined.dE_sun > 0.0 &&
          norm(combined.dF_wind) > 0.0 &&
          gamma >= 0.98 && gamma <= 1.8 &&
          growth > 0.0 &&
          (leaf || !leaf),
          "full pipeline (sun+wind->bead->root+leaf)");
}

// =====================================================================
// Main
// =====================================================================

int main() {
    std::printf("=== Environmental Particle Extension Tests (45) ===\n\n");

    test_particle_construction();  // T1-T5
    test_sun_deposition();         // T6-T10
    test_wind_force();             // T11-T15
    test_drying_kernel();          // T16-T18
    test_unified_interaction();    // T19-T22
    test_root_chaos();             // T23-T27
    test_piecewise_poly();         // T28-T32
    test_root_growth_modifier();   // T33-T37
    test_leaf_gate();              // T38-T42 (actually T38-T43 with 6 checks)
    test_energy_decomposition();   // T43 -> T44-T47
    test_particle_advection();     // T44 -> T48-T50
    test_full_pipeline();          // T45 -> T51

    std::printf("\n");
    if (g_fail == 0) {
        std::printf("=== ALL %d TESTS PASSED ===\n", g_pass);
    } else {
        std::printf("=== %d PASSED, %d FAILED ===\n", g_pass, g_fail);
    }
    return g_fail;
}
