/**
 * test_anisotropic_beads.cpp — Anisotropic Surface-Mapped Bead Validation
 *
 * Verifies the anisotropic bead pipeline using benzene (C6H6) as the
 * canonical test case (12 atoms → 1 anisotropic bead).
 *
 * Tests:
 *   T1: Spherical harmonics orthonormality (Y_00 = 1/√(4π))
 *   T2: Inertia frame of benzene ring (planar → κ > 0)
 *   T3: Surface descriptor symmetry (benzene has strong ℓ=2 m=0)
 *   T4: Single-atom bead is isotropic (anisotropy_ratio ≈ 0)
 *   T5: Anisotropic potential decomposes isotropic + correction
 *   T6: Surface coupling self-dot is positive
 *   T7: SH band power sums to total power
 *   T8: Asphericity of benzene > 0, asphericity of single atom ≈ 0
 *
 * Reference: section_anisotropic_beads.tex
 */

#include "coarse_grain/core/spherical_harmonics.hpp"
#include "coarse_grain/core/inertia_frame.hpp"
#include "coarse_grain/core/surface_descriptor.hpp"
#include "coarse_grain/mapping/surface_mapper.hpp"
#include "coarse_grain/models/anisotropic_potential.hpp"
#include "atomistic/core/state.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

static int g_pass = 0;
static int g_fail = 0;

static void CHECK(bool cond, const char* label) {
    if (cond) {
        std::printf("  [PASS] %s\n", label);
        ++g_pass;
    } else {
        std::printf("  [FAIL] %s\n", label);
        ++g_fail;
    }
}

static void CHECK_NEAR(double a, double b, double tol, const char* label) {
    bool ok = std::abs(a - b) < tol;
    if (ok) {
        std::printf("  [PASS] %s (%.6e ≈ %.6e)\n", label, a, b);
        ++g_pass;
    } else {
        std::printf("  [FAIL] %s (%.6e != %.6e, delta=%.6e)\n", label, a, b, std::abs(a - b));
        ++g_fail;
    }
}

/**
 * Build a benzene C6H6 atomistic state.
 * 6 C atoms in a planar hexagonal ring (bond length 1.40 Å)
 * 6 H atoms radially outward (C-H bond 1.09 Å)
 * All atoms in the XY plane (z = 0).
 */
static atomistic::State make_benzene() {
    atomistic::State s;
    s.N = 12;
    s.X.resize(12);
    s.V.resize(12, {0, 0, 0});
    s.Q.resize(12, 0.0);
    s.M.resize(12);
    s.type.resize(12);
    s.F.resize(12, {0, 0, 0});

    constexpr double pi = 3.14159265358979323846;
    constexpr double r_cc = 1.40;   // C-C bond length (Å)
    constexpr double r_ch = 1.09;   // C-H bond length (Å)
    constexpr double r_h = r_cc + r_ch;

    // Carbon ring
    for (int i = 0; i < 6; ++i) {
        double angle = 2.0 * pi * i / 6.0;
        s.X[i] = {r_cc * std::cos(angle), r_cc * std::sin(angle), 0.0};
        s.M[i] = 12.011;   // Carbon mass
        s.type[i] = 6;     // Carbon Z
    }

    // Hydrogen atoms
    for (int i = 0; i < 6; ++i) {
        double angle = 2.0 * pi * i / 6.0;
        s.X[6 + i] = {r_h * std::cos(angle), r_h * std::sin(angle), 0.0};
        s.M[6 + i] = 1.008;  // Hydrogen mass
        s.type[6 + i] = 1;   // Hydrogen Z
    }

    // Bonds: C-C ring + C-H
    for (int i = 0; i < 6; ++i) {
        s.B.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>((i + 1) % 6)});
        s.B.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>(6 + i)});
    }

    // Minimal energy terms + box
    s.E = {};
    s.box = {};

    return s;
}

/**
 * Build a single-argon-atom state (isotropic reference).
 */
static atomistic::State make_single_atom() {
    atomistic::State s;
    s.N = 1;
    s.X = {{0.0, 0.0, 0.0}};
    s.V = {{0.0, 0.0, 0.0}};
    s.Q = {0.0};
    s.M = {39.948};
    s.type = {18};
    s.F = {{0.0, 0.0, 0.0}};
    s.E = {};
    s.box = {};
    return s;
}

int main() {
    std::printf("=== Anisotropic Surface-Mapped Bead Tests ===\n\n");

    // ========================================================================
    // T1: Spherical harmonics sanity
    // ========================================================================
    std::printf("T1: Spherical harmonics basic checks\n");
    {
        auto Y = coarse_grain::evaluate_all_harmonics(0.0, 0.0);
        // Y_00(0,0) = 1/(2√π) ≈ 0.28209
        double expected_Y00 = 1.0 / (2.0 * std::sqrt(3.14159265358979323846));
        CHECK_NEAR(Y[coarse_grain::sh_index(0, 0)], expected_Y00, 1e-6, "Y_00(0,0) = 1/(2*sqrt(pi))");

        // Y_00 should be constant over the sphere
        auto Y2 = coarse_grain::evaluate_all_harmonics(1.5, 2.3);
        CHECK_NEAR(Y2[coarse_grain::sh_index(0, 0)], expected_Y00, 1e-6, "Y_00 is constant");

        // Total coefficient count
        CHECK(coarse_grain::SH_NUM_COEFFS == 25, "25 SH coefficients for l_max=4");
    }

    // ========================================================================
    // T2: Inertia frame of benzene
    // ========================================================================
    std::printf("\nT2: Inertia frame of benzene\n");
    atomistic::State benzene = make_benzene();
    {
        std::vector<uint32_t> all_atoms(12);
        for (uint32_t i = 0; i < 12; ++i) all_atoms[i] = i;

        // COM
        atomistic::Vec3 com{};
        double total_mass = 0.0;
        for (uint32_t i : all_atoms) {
            com.x += benzene.M[i] * benzene.X[i].x;
            com.y += benzene.M[i] * benzene.X[i].y;
            com.z += benzene.M[i] * benzene.X[i].z;
            total_mass += benzene.M[i];
        }
        com.x /= total_mass;
        com.y /= total_mass;
        com.z /= total_mass;

        auto frame = coarse_grain::compute_inertia_frame(benzene.X, benzene.M, all_atoms, com);
        CHECK(frame.valid, "Inertia frame converged");
        CHECK(frame.asphericity > 0.0, "Benzene asphericity > 0 (planar anisotropy)");
        std::printf("    asphericity = %.6f\n", frame.asphericity);
        std::printf("    eigenvalues = [%.4f, %.4f, %.4f]\n",
                    frame.eigenvalues[0], frame.eigenvalues[1], frame.eigenvalues[2]);

        // For a planar ring, I3 should be approximately I1 + I2 (perpendicular axis theorem)
        double sum_12 = frame.eigenvalues[0] + frame.eigenvalues[1];
        double ratio = frame.eigenvalues[2] / sum_12;
        CHECK_NEAR(ratio, 1.0, 0.05, "I3 ≈ I1 + I2 (perpendicular axis theorem)");
    }

    // ========================================================================
    // T3: Surface descriptor of benzene
    // ========================================================================
    std::printf("\nT3: Surface descriptor of benzene\n");
    coarse_grain::SurfaceDescriptor benzene_desc;
    {
        std::vector<uint32_t> all_atoms(12);
        for (uint32_t i = 0; i < 12; ++i) all_atoms[i] = i;

        atomistic::Vec3 com{};
        double total_mass = 0.0;
        for (uint32_t i : all_atoms) {
            com.x += benzene.M[i] * benzene.X[i].x;
            com.y += benzene.M[i] * benzene.X[i].y;
            com.z += benzene.M[i] * benzene.X[i].z;
            total_mass += benzene.M[i];
        }
        com.x /= total_mass;
        com.y /= total_mass;
        com.z /= total_mass;

        coarse_grain::SurfaceMapper mapper;
        coarse_grain::SurfaceMapperConfig config;
        config.n_samples = 200;
        config.probe_radius = 3.0;
        config.probe_sigma = 1.0;

        benzene_desc = mapper.compute(benzene, all_atoms, com, config);

        CHECK(benzene_desc.total_power() > 0.0, "Non-zero surface power");
        CHECK(benzene_desc.anisotropy_ratio() > 0.01, "Benzene is anisotropic (ratio > 0.01)");
        std::printf("    anisotropy_ratio = %.6f\n", benzene_desc.anisotropy_ratio());
        std::printf("    total_power      = %.6f\n", benzene_desc.total_power());
        std::printf("    isotropic_comp   = %.6f\n", benzene_desc.isotropic_component());

        auto bp = benzene_desc.band_power();
        std::printf("    band power: l=0: %.4f, l=1: %.4f, l=2: %.4f, l=3: %.4f, l=4: %.4f\n",
                    bp[0], bp[1], bp[2], bp[3], bp[4]);

        // Benzene should have significant ℓ=2 content (quadrupolar shape)
        int dom = benzene_desc.dominant_band();
        std::printf("    dominant band: l=%d\n", dom);
        CHECK(dom >= 1, "Dominant band is l >= 1 (anisotropic)");
    }

    // ========================================================================
    // T4: Single atom is isotropic
    // ========================================================================
    std::printf("\nT4: Single atom is isotropic\n");
    {
        atomistic::State single = make_single_atom();
        std::vector<uint32_t> idx = {0};

        coarse_grain::SurfaceMapper mapper;
        auto desc = mapper.compute(single, idx, single.X[0]);

        CHECK_NEAR(desc.anisotropy_ratio(), 0.0, 1e-10, "Single atom anisotropy ≈ 0");
        std::printf("    c_00 = %.6f\n", desc.coeffs[0]);
    }

    // ========================================================================
    // T5: Anisotropic potential decomposition
    // ========================================================================
    std::printf("\nT5: Anisotropic potential decomposition\n");
    {
        atomistic::Vec3 r_vec = {4.0, 0.0, 0.0};
        double sigma = 3.4;
        double epsilon = 0.24;

        auto result = coarse_grain::anisotropic_potential(
            r_vec, sigma, epsilon, benzene_desc, benzene_desc, 0.1);

        CHECK(std::isfinite(result.E_total), "Total energy is finite");
        CHECK(std::isfinite(result.E_isotropic), "Isotropic energy is finite");
        CHECK(std::isfinite(result.E_anisotropic), "Anisotropic energy is finite");
        CHECK_NEAR(result.E_total, result.E_isotropic + result.E_anisotropic, 1e-12,
                   "E_total = E_iso + E_aniso");

        std::printf("    E_isotropic   = %.6f kcal/mol\n", result.E_isotropic);
        std::printf("    E_anisotropic = %.6f kcal/mol\n", result.E_anisotropic);
        std::printf("    E_total       = %.6f kcal/mol\n", result.E_total);
        std::printf("    coupling      = %.6f\n", result.coupling);
    }

    // ========================================================================
    // T6: Surface coupling self-dot is positive
    // ========================================================================
    std::printf("\nT6: Surface coupling self-dot\n");
    {
        double self_coupling = coarse_grain::surface_coupling(benzene_desc, benzene_desc);
        CHECK(self_coupling > 0.0, "Self-coupling is positive");
        std::printf("    self_coupling = %.6f\n", self_coupling);

        // Self-coupling should equal total power
        CHECK_NEAR(self_coupling, benzene_desc.total_power(), 1e-10,
                   "Self-coupling = total_power (Parseval)");
    }

    // ========================================================================
    // T7: Band power sums to total power
    // ========================================================================
    std::printf("\nT7: Band power sum\n");
    {
        auto bp = benzene_desc.band_power();
        double sum = 0.0;
        for (int l = 0; l <= coarse_grain::SH_L_MAX; ++l)
            sum += bp[l];
        CHECK_NEAR(sum, benzene_desc.total_power(), 1e-10,
                   "Sum of band powers = total power");
    }

    // ========================================================================
    // T8: Asphericity comparison
    // ========================================================================
    std::printf("\nT8: Asphericity comparison\n");
    {
        // Single atom
        atomistic::State single = make_single_atom();
        std::vector<uint32_t> idx_single = {0};
        auto frame_single = coarse_grain::compute_inertia_frame(
            single.X, single.M, idx_single, single.X[0]);
        CHECK_NEAR(frame_single.asphericity, 0.0, 1e-10,
                   "Single atom asphericity = 0");

        // Benzene (already computed in T2, verify via frame)
        CHECK(benzene_desc.asphericity() > 0.0,
              "Benzene asphericity > 0 via descriptor");
        std::printf("    benzene asphericity = %.6f\n", benzene_desc.asphericity());
    }

    // ========================================================================
    // Summary
    // ========================================================================
    std::printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
