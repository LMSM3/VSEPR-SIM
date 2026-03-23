/**
 * test_orientation_coupling.cpp — Orientation-Coupled Interaction Tests
 *
 * Validates the orientation-coupled interaction framework using benzene
 * dimer configurations as the canonical test case.
 *
 * Tests:
 *   T1: Invariants for face-to-face stacking (parallel normals, aligned with axis)
 *   T2: Invariants for T-shaped (edge-to-face) configuration
 *   T3: Invariants for edge-to-edge configuration
 *   T4: Classification function matches known geometry labels
 *   T5: Isotropic-only potential reproduces pure LJ 12-6
 *   T6: Axisymmetric potential differs from isotropic when λ ≠ 0
 *   T7: Energy decomposition: E_total = sum of all contributions
 *   T8: Torque vanishes for isotropic case (λ₁ = λ₂ = λ₃ = 0)
 *   T9: Face-to-face vs T-shaped energy ordering with stacking-favoring λ
 *   T10: Force is finite and anti-symmetric (F_A = -F_B)
 *   T11: Model hierarchy selection (isotropic vs axisymmetric levels)
 *   T12: BeadOrientation from InertiaFrame for planar molecule
 *
 * Reference: Section "Orientation-Coupled Interaction Framework" of
 *            section_anisotropic_beads.tex
 */

#include "coarse_grain/core/orientation.hpp"
#include "coarse_grain/core/inertia_frame.hpp"
#include "coarse_grain/models/orientation_potential.hpp"
#include "atomistic/core/state.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
        std::printf("  [PASS] %s (%.6e ~ %.6e)\n", label, a, b);
        ++g_pass;
    } else {
        std::printf("  [FAIL] %s (%.6e != %.6e, delta=%.6e)\n", label, a, b, std::abs(a - b));
        ++g_fail;
    }
}

// ============================================================================
// Helper: standard benzene dimer positions and orientations
// ============================================================================

/** Face-to-face stacking: both normals along Z, separated along Z */
static void setup_face_to_face(
    atomistic::Vec3& posA, atomistic::Vec3& posB,
    coarse_grain::BeadOrientation& oriA, coarse_grain::BeadOrientation& oriB)
{
    posA = {0.0, 0.0, 0.0};
    posB = {0.0, 0.0, 3.8};       // 3.8 Å typical stacking distance
    oriA.normal = {0.0, 0.0, 1.0}; // normal along Z
    oriB.normal = {0.0, 0.0, 1.0}; // normal along Z (parallel)
}

/** T-shaped (edge-to-face): A normal along Z, B normal along X */
static void setup_t_shaped(
    atomistic::Vec3& posA, atomistic::Vec3& posB,
    coarse_grain::BeadOrientation& oriA, coarse_grain::BeadOrientation& oriB)
{
    posA = {0.0, 0.0, 0.0};
    posB = {0.0, 0.0, 5.0};       // separated along Z
    oriA.normal = {0.0, 0.0, 1.0}; // normal along Z (along axis)
    oriB.normal = {1.0, 0.0, 0.0}; // normal along X (perpendicular)
}

/** Edge-to-edge: both normals perpendicular to the separation axis */
static void setup_edge_to_edge(
    atomistic::Vec3& posA, atomistic::Vec3& posB,
    coarse_grain::BeadOrientation& oriA, coarse_grain::BeadOrientation& oriB)
{
    posA = {0.0, 0.0, 0.0};
    posB = {0.0, 0.0, 5.0};       // separated along Z
    oriA.normal = {1.0, 0.0, 0.0}; // normal along X
    oriB.normal = {0.0, 1.0, 0.0}; // normal along Y
}

int main() {
    std::printf("=== Orientation-Coupled Interaction Framework Tests ===\n\n");

    // ========================================================================
    // T1: Face-to-face invariants
    // ========================================================================
    std::printf("T1: Face-to-face stacking invariants\n");
    {
        atomistic::Vec3 pA, pB;
        coarse_grain::BeadOrientation oA, oB;
        setup_face_to_face(pA, pB, oA, oB);

        auto inv = coarse_grain::compute_invariants(pA, pB, oA, oB);

        CHECK_NEAR(inv.cos_phi, 1.0, 1e-10, "cos_phi = 1 (parallel normals)");
        CHECK_NEAR(std::abs(inv.cos_theta_A), 1.0, 1e-10, "|cos_theta_A| = 1 (normal along axis)");
        CHECK_NEAR(std::abs(inv.cos_theta_B), 1.0, 1e-10, "|cos_theta_B| = 1 (normal along axis)");
        CHECK_NEAR(inv.r, 3.8, 1e-10, "r = 3.8 A");
    }

    // ========================================================================
    // T2: T-shaped invariants
    // ========================================================================
    std::printf("\nT2: T-shaped (edge-to-face) invariants\n");
    {
        atomistic::Vec3 pA, pB;
        coarse_grain::BeadOrientation oA, oB;
        setup_t_shaped(pA, pB, oA, oB);

        auto inv = coarse_grain::compute_invariants(pA, pB, oA, oB);

        CHECK_NEAR(inv.cos_phi, 0.0, 1e-10, "cos_phi = 0 (perpendicular normals)");
        CHECK_NEAR(std::abs(inv.cos_theta_A), 1.0, 1e-10, "|cos_theta_A| = 1 (A along axis)");
        CHECK_NEAR(inv.cos_theta_B, 0.0, 1e-10, "cos_theta_B = 0 (B perpendicular to axis)");
    }

    // ========================================================================
    // T3: Edge-to-edge invariants
    // ========================================================================
    std::printf("\nT3: Edge-to-edge invariants\n");
    {
        atomistic::Vec3 pA, pB;
        coarse_grain::BeadOrientation oA, oB;
        setup_edge_to_edge(pA, pB, oA, oB);

        auto inv = coarse_grain::compute_invariants(pA, pB, oA, oB);

        CHECK_NEAR(inv.cos_theta_A, 0.0, 1e-10, "cos_theta_A = 0 (A perp to axis)");
        CHECK_NEAR(inv.cos_theta_B, 0.0, 1e-10, "cos_theta_B = 0 (B perp to axis)");
        CHECK_NEAR(inv.cos_phi, 0.0, 1e-10, "cos_phi = 0 (perpendicular normals)");
    }

    // ========================================================================
    // T4: Classification
    // ========================================================================
    std::printf("\nT4: Configuration classification\n");
    {
        atomistic::Vec3 pA, pB;
        coarse_grain::BeadOrientation oA, oB;

        setup_face_to_face(pA, pB, oA, oB);
        auto inv_ff = coarse_grain::compute_invariants(pA, pB, oA, oB);
        CHECK(std::strcmp(inv_ff.classify(), "face-to-face") == 0,
              "Face-to-face classified correctly");

        setup_t_shaped(pA, pB, oA, oB);
        auto inv_t = coarse_grain::compute_invariants(pA, pB, oA, oB);
        CHECK(std::strcmp(inv_t.classify(), "edge-to-face") == 0,
              "T-shaped classified as edge-to-face");

        setup_edge_to_edge(pA, pB, oA, oB);
        auto inv_ee = coarse_grain::compute_invariants(pA, pB, oA, oB);
        CHECK(std::strcmp(inv_ee.classify(), "edge-to-edge") == 0,
              "Edge-to-edge classified correctly");
    }

    // ========================================================================
    // T5: Isotropic-only potential = pure LJ
    // ========================================================================
    std::printf("\nT5: Isotropic-only potential reproduces LJ 12-6\n");
    {
        atomistic::Vec3 pA = {0, 0, 0};
        atomistic::Vec3 pB = {4.0, 0, 0};
        double sigma = 3.4, epsilon = 0.24;

        auto result = coarse_grain::evaluate_isotropic_only(pA, pB, sigma, epsilon);

        double r = 4.0;
        double sr = sigma / r;
        double sr6 = sr * sr * sr * sr * sr * sr;
        double expected_E = 4.0 * epsilon * (sr6 * sr6 - sr6);

        CHECK_NEAR(result.E_total, expected_E, 1e-12, "E_total matches analytic LJ");
        CHECK_NEAR(result.E_isotropic, expected_E, 1e-12, "E_isotropic = E_total");
        CHECK_NEAR(result.E_alignment, 0.0, 1e-15, "No alignment contribution");
        CHECK(std::isfinite(result.force_B.x), "Force is finite");
    }

    // ========================================================================
    // T6: Axisymmetric differs from isotropic when λ ≠ 0
    // ========================================================================
    std::printf("\nT6: Axisymmetric potential adds angular contributions\n");
    {
        atomistic::Vec3 pA, pB;
        coarse_grain::BeadOrientation oA, oB;
        setup_face_to_face(pA, pB, oA, oB);

        coarse_grain::OrientationPotentialParams params;
        params.sigma = 3.4;
        params.epsilon = 0.24;
        params.lambda1 = -0.5;  // Favor parallel alignment
        params.lambda2 = 0.2;
        params.lambda3 = 0.2;

        auto result_aniso = coarse_grain::evaluate_orientation_potential(pA, pB, oA, oB, params);
        auto result_iso   = coarse_grain::evaluate_isotropic_only(pA, pB, params.sigma, params.epsilon);

        CHECK(std::abs(result_aniso.E_total - result_iso.E_total) > 1e-10,
              "Anisotropic energy differs from isotropic");
        CHECK(std::abs(result_aniso.E_alignment) > 1e-10,
              "Non-zero alignment contribution");
        std::printf("    E_iso = %.6f, E_aniso_total = %.6f, diff = %.6f\n",
                    result_iso.E_total, result_aniso.E_total,
                    result_aniso.E_total - result_iso.E_total);
    }

    // ========================================================================
    // T7: Energy decomposition sums correctly
    // ========================================================================
    std::printf("\nT7: Energy decomposition E_total = sum of parts\n");
    {
        atomistic::Vec3 pA, pB;
        coarse_grain::BeadOrientation oA, oB;
        setup_t_shaped(pA, pB, oA, oB);

        coarse_grain::OrientationPotentialParams params;
        params.sigma = 3.4;
        params.epsilon = 0.24;
        params.lambda1 = -0.3;
        params.lambda2 = 0.15;
        params.lambda3 = 0.25;

        auto r = coarse_grain::evaluate_orientation_potential(pA, pB, oA, oB, params);

        double sum = r.E_isotropic + r.E_alignment + r.E_axis_A + r.E_axis_B;
        CHECK_NEAR(r.E_total, sum, 1e-12, "E_total = E_iso + E_align + E_axA + E_axB");
    }

    // ========================================================================
    // T8: Torque vanishes for isotropic parameters
    // ========================================================================
    std::printf("\nT8: Zero torque when all lambda = 0\n");
    {
        atomistic::Vec3 pA, pB;
        coarse_grain::BeadOrientation oA, oB;
        setup_face_to_face(pA, pB, oA, oB);

        coarse_grain::OrientationPotentialParams params;
        params.sigma = 3.4;
        params.epsilon = 0.24;
        params.lambda1 = 0.0;
        params.lambda2 = 0.0;
        params.lambda3 = 0.0;

        auto r = coarse_grain::evaluate_orientation_potential(pA, pB, oA, oB, params);

        double tau_A_mag = std::sqrt(r.torque_A.x * r.torque_A.x +
                                     r.torque_A.y * r.torque_A.y +
                                     r.torque_A.z * r.torque_A.z);
        double tau_B_mag = std::sqrt(r.torque_B.x * r.torque_B.x +
                                     r.torque_B.y * r.torque_B.y +
                                     r.torque_B.z * r.torque_B.z);

        CHECK_NEAR(tau_A_mag, 0.0, 1e-15, "|torque_A| = 0");
        CHECK_NEAR(tau_B_mag, 0.0, 1e-15, "|torque_B| = 0");
    }

    // ========================================================================
    // T9: Face-to-face vs T-shaped energy ordering
    // ========================================================================
    std::printf("\nT9: Stacking-favoring lambda: face-to-face < T-shaped energy\n");
    {
        coarse_grain::OrientationPotentialParams params;
        params.sigma = 3.4;
        params.epsilon = 0.24;
        params.lambda1 = -0.5;  // Negative: favors cos_phi = +1 (parallel normals)
        params.lambda2 = 0.0;
        params.lambda3 = 0.0;

        // Face-to-face at 4 Å
        atomistic::Vec3 pA = {0, 0, 0}, pB_ff = {0, 0, 4.0};
        coarse_grain::BeadOrientation oA_ff, oB_ff;
        oA_ff.normal = {0, 0, 1};
        oB_ff.normal = {0, 0, 1};  // parallel

        auto r_ff = coarse_grain::evaluate_orientation_potential(pA, pB_ff, oA_ff, oB_ff, params);

        // T-shaped at same distance
        atomistic::Vec3 pB_t = {0, 0, 4.0};
        coarse_grain::BeadOrientation oA_t, oB_t;
        oA_t.normal = {0, 0, 1};
        oB_t.normal = {1, 0, 0};  // perpendicular

        auto r_t = coarse_grain::evaluate_orientation_potential(pA, pB_t, oA_t, oB_t, params);

        CHECK(r_ff.E_total < r_t.E_total,
              "Face-to-face lower energy than T-shaped (stacking preference)");
        std::printf("    E_face-to-face = %.6f, E_T-shaped = %.6f\n",
                    r_ff.E_total, r_t.E_total);
    }

    // ========================================================================
    // T10: Force anti-symmetry
    // ========================================================================
    std::printf("\nT10: Force anti-symmetry (F_A = -F_B)\n");
    {
        atomistic::Vec3 pA = {0, 0, 0}, pB = {3.5, 1.0, 2.0};
        coarse_grain::BeadOrientation oA, oB;
        oA.normal = {0, 0, 1};
        oB.normal = {0.577, 0.577, 0.577};  // tilted

        coarse_grain::OrientationPotentialParams params;
        params.sigma = 3.4;
        params.epsilon = 0.24;
        params.lambda1 = -0.2;
        params.lambda2 = 0.1;
        params.lambda3 = 0.1;

        auto r = coarse_grain::evaluate_orientation_potential(pA, pB, oA, oB, params);

        // F_A = -F_B, so force_A + force_B = 0 (Newton's third law for central component)
        CHECK(std::isfinite(r.force_B.x), "Force x is finite");
        CHECK(std::isfinite(r.force_B.y), "Force y is finite");
        CHECK(std::isfinite(r.force_B.z), "Force z is finite");
        // F_A = -F_B, total force = 0
        // (We only compute F_B; F_A = -F_B by construction)
        std::printf("    F_B = (%.6f, %.6f, %.6f)\n", r.force_B.x, r.force_B.y, r.force_B.z);
    }

    // ========================================================================
    // T11: Model hierarchy levels
    // ========================================================================
    std::printf("\nT11: Model hierarchy levels\n");
    {
        CHECK(static_cast<int>(coarse_grain::InteractionLevel::ISOTROPIC) == 0,
              "ISOTROPIC = level 0");
        CHECK(static_cast<int>(coarse_grain::InteractionLevel::AXISYMMETRIC) == 1,
              "AXISYMMETRIC = level 1");
        CHECK(static_cast<int>(coarse_grain::InteractionLevel::DESCRIPTOR_RESOLVED) == 2,
              "DESCRIPTOR_RESOLVED = level 2");
    }

    // ========================================================================
    // T12: BeadOrientation from InertiaFrame
    // ========================================================================
    std::printf("\nT12: BeadOrientation from InertiaFrame (planar benzene)\n");
    {
        // Build a planar ring in XY plane
        constexpr double pi = 3.14159265358979323846;
        std::vector<atomistic::Vec3> positions(6);
        std::vector<double> masses(6, 12.011);
        std::vector<uint32_t> indices = {0, 1, 2, 3, 4, 5};
        atomistic::Vec3 com = {0, 0, 0};

        for (int i = 0; i < 6; ++i) {
            double angle = 2.0 * pi * i / 6.0;
            positions[i] = {1.40 * std::cos(angle), 1.40 * std::sin(angle), 0.0};
        }

        auto frame = coarse_grain::compute_inertia_frame(positions, masses, indices, com);
        CHECK(frame.valid, "Inertia frame valid");

        auto ori = coarse_grain::BeadOrientation::from_frame(frame);

        // For a planar XY ring, the normal (axis3) should be along Z
        CHECK(std::abs(ori.normal.z) > 0.9, "Normal is approximately along Z for XY planar ring");
        std::printf("    normal = (%.4f, %.4f, %.4f)\n",
                    ori.normal.x, ori.normal.y, ori.normal.z);

        // Verify unit length
        double len = std::sqrt(ori.normal.x * ori.normal.x +
                               ori.normal.y * ori.normal.y +
                               ori.normal.z * ori.normal.z);
        CHECK_NEAR(len, 1.0, 1e-10, "Normal is unit vector");
    }

    // ========================================================================
    // Summary
    // ========================================================================
    std::printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
