/**
 * test_anisotropic_model.cpp — Tests for Anisotropic Bead Model Specification
 *
 * Validates the consolidated implementation against the formal specification:
 *
 *   1. Channel enum and naming
 *   2. Per-ℓ steric kernel: exp(-α_s·r) / (1+ℓ)
 *   3. Per-ℓ electrostatic kernel: 1 / r^(ℓ+1)
 *   4. Per-ℓ dispersion kernel: -C₆ / r^(6+ℓ)
 *   5. Kernel decay with ℓ (higher ℓ → smaller contribution)
 *   6. SH rotation: ℓ=0 scalar invariance
 *   7. SH rotation: ℓ=1 vector rotation
 *   8. SH rotation: identity rotation preserves coefficients
 *   9. Relative rotation from two frames
 *  10. Channel interaction: zero for inactive channels
 *  11. Channel interaction: nonzero for active channels
 *  12. Channel interaction: per-ℓ decomposition
 *  13. Full interaction_energy: aligned identical beads
 *  14. Full interaction_energy: inactive channels contribute zero
 *  15. Adaptive refinement: below threshold → no promotion
 *  16. Adaptive refinement: above threshold → promotion
 *  17. Adaptive refinement: cap at max_l_max
 *  18. Benzene bead: kernel evaluation at reference distance
 *
 * Anti-black-box: every test prints category and result explicitly.
 */

#include "coarse_grain/core/channel_kernels.hpp"
#include "coarse_grain/core/sh_rotation.hpp"
#include "coarse_grain/core/spherical_harmonics.hpp"
#include "coarse_grain/core/unified_descriptor.hpp"
#include "coarse_grain/models/interaction_engine.hpp"
#include "coarse_grain/models/adaptive_refinement.hpp"
#include "atomistic/core/state.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* msg) {
    if (cond) {
        std::printf("  [PASS] %s\n", msg);
        ++g_pass;
    } else {
        std::printf("  [FAIL] %s\n", msg);
        ++g_fail;
    }
}

// ============================================================================
// 1. Channel enum and naming
// ============================================================================
static void test_channel_enum() {
    std::printf("\n--- 1. Channel enum and naming ---\n");

    check(static_cast<uint8_t>(coarse_grain::Channel::Steric) == 0, "Steric = 0");
    check(static_cast<uint8_t>(coarse_grain::Channel::Electrostatic) == 1, "Electrostatic = 1");
    check(static_cast<uint8_t>(coarse_grain::Channel::Dispersion) == 2, "Dispersion = 2");
    check(static_cast<uint8_t>(coarse_grain::Channel::COUNT) == 3, "COUNT = 3");

    check(std::string(coarse_grain::channel_name(coarse_grain::Channel::Steric)) == "Steric",
          "Steric name");
    check(std::string(coarse_grain::channel_name(coarse_grain::Channel::Electrostatic)) == "Electrostatic",
          "Electrostatic name");
    check(std::string(coarse_grain::channel_name(coarse_grain::Channel::Dispersion)) == "Dispersion",
          "Dispersion name");
}

// ============================================================================
// 2. Per-ℓ steric kernel
// ============================================================================
static void test_steric_kernel() {
    std::printf("\n--- 2. Per-l steric kernel ---\n");

    using coarse_grain::Channel;
    using coarse_grain::channel_kernel;
    coarse_grain::ChannelKernelParams p{1.0, 1.0};

    // K_steric(l=0, r=1) = exp(-1) / 1
    double k0 = channel_kernel(Channel::Steric, 0, 1.0, p);
    check(std::abs(k0 - std::exp(-1.0)) < 1e-12, "steric l=0 r=1");

    // K_steric(l=2, r=1) = exp(-1) / 3
    double k2 = channel_kernel(Channel::Steric, 2, 1.0, p);
    check(std::abs(k2 - std::exp(-1.0) / 3.0) < 1e-12, "steric l=2 r=1");

    // K_steric(l=0, r=2, alpha=0.5) = exp(-1) / 1
    double k0a = channel_kernel(Channel::Steric, 0, 2.0, 0.5, 1.0);
    check(std::abs(k0a - std::exp(-1.0)) < 1e-12, "steric l=0 r=2 alpha=0.5");
}

// ============================================================================
// 3. Per-ℓ electrostatic kernel
// ============================================================================
static void test_electrostatic_kernel() {
    std::printf("\n--- 3. Per-l electrostatic kernel ---\n");

    using coarse_grain::Channel;
    using coarse_grain::channel_kernel;
    coarse_grain::ChannelKernelParams p{1.0, 1.0};

    // K_elec(l=0, r=2) = 1/2
    double k0 = channel_kernel(Channel::Electrostatic, 0, 2.0, p);
    check(std::abs(k0 - 0.5) < 1e-12, "electrostatic l=0 r=2");

    // K_elec(l=1, r=2) = 1/4
    double k1 = channel_kernel(Channel::Electrostatic, 1, 2.0, p);
    check(std::abs(k1 - 0.25) < 1e-12, "electrostatic l=1 r=2");

    // K_elec(l=2, r=2) = 1/8
    double k2 = channel_kernel(Channel::Electrostatic, 2, 2.0, p);
    check(std::abs(k2 - 0.125) < 1e-12, "electrostatic l=2 r=2");
}

// ============================================================================
// 4. Per-ℓ dispersion kernel
// ============================================================================
static void test_dispersion_kernel() {
    std::printf("\n--- 4. Per-l dispersion kernel ---\n");

    using coarse_grain::Channel;
    using coarse_grain::channel_kernel;
    coarse_grain::ChannelKernelParams p{1.0, 1.0};

    // K_disp(l=0, r=1) = -1/1 = -1
    double k0 = channel_kernel(Channel::Dispersion, 0, 1.0, p);
    check(std::abs(k0 - (-1.0)) < 1e-12, "dispersion l=0 r=1 C6=1");

    // K_disp(l=0, r=2) = -1/64
    double k0r2 = channel_kernel(Channel::Dispersion, 0, 2.0, p);
    check(std::abs(k0r2 - (-1.0 / 64.0)) < 1e-12, "dispersion l=0 r=2");

    // K_disp(l=1, r=2) = -1/128
    double k1r2 = channel_kernel(Channel::Dispersion, 1, 2.0, p);
    check(std::abs(k1r2 - (-1.0 / 128.0)) < 1e-12, "dispersion l=1 r=2");
}

// ============================================================================
// 5. Kernel decay with ℓ
// ============================================================================
static void test_kernel_decay() {
    std::printf("\n--- 5. Kernel decay with l ---\n");

    using coarse_grain::Channel;
    using coarse_grain::channel_kernel;
    coarse_grain::ChannelKernelParams p{1.0, 1.0};

    double r = 3.0;

    // Steric: higher l → smaller kernel (1/(1+l) factor)
    double ks0 = channel_kernel(Channel::Steric, 0, r, p);
    double ks2 = channel_kernel(Channel::Steric, 2, r, p);
    double ks4 = channel_kernel(Channel::Steric, 4, r, p);
    check(std::abs(ks0) > std::abs(ks2), "steric: |K(l=0)| > |K(l=2)|");
    check(std::abs(ks2) > std::abs(ks4), "steric: |K(l=2)| > |K(l=4)|");

    // Electrostatic: higher l → smaller kernel (1/r^(l+1))
    double ke0 = channel_kernel(Channel::Electrostatic, 0, r, p);
    double ke2 = channel_kernel(Channel::Electrostatic, 2, r, p);
    check(std::abs(ke0) > std::abs(ke2), "electrostatic: |K(l=0)| > |K(l=2)|");

    // Dispersion: higher l → smaller magnitude (1/r^(6+l))
    double kd0 = channel_kernel(Channel::Dispersion, 0, r, p);
    double kd2 = channel_kernel(Channel::Dispersion, 2, r, p);
    check(std::abs(kd0) > std::abs(kd2), "dispersion: |K(l=0)| > |K(l=2)|");
}

// ============================================================================
// 6. SH rotation: ℓ=0 scalar invariance
// ============================================================================
static void test_rotation_l0() {
    std::printf("\n--- 6. SH rotation: l=0 scalar invariance ---\n");

    coarse_grain::Mat3 R;
    // 90-degree rotation about z
    R(0, 0) = 0; R(0, 1) = -1; R(0, 2) = 0;
    R(1, 0) = 1; R(1, 1) =  0; R(1, 2) = 0;
    R(2, 0) = 0; R(2, 1) =  0; R(2, 2) = 1;

    std::vector<double> coeffs = {3.14};  // l_max=0, single coeff
    auto rotated = coarse_grain::rotate_sh_coefficients(coeffs, 0, R);

    check(std::abs(rotated[0] - 3.14) < 1e-12, "l=0 coeff unchanged under rotation");
}

// ============================================================================
// 7. SH rotation: ℓ=1 vector rotation
// ============================================================================
static void test_rotation_l1() {
    std::printf("\n--- 7. SH rotation: l=1 vector rotation ---\n");

    coarse_grain::Mat3 R;
    // 90-degree rotation about z: x→y, y→-x, z→z
    R(0, 0) = 0; R(0, 1) = -1; R(0, 2) = 0;
    R(1, 0) = 1; R(1, 1) =  0; R(1, 2) = 0;
    R(2, 0) = 0; R(2, 1) =  0; R(2, 2) = 1;

    // l_max=1: coeffs[0]=c00, coeffs[1]=c1-1(y), coeffs[2]=c10(z), coeffs[3]=c11(x)
    // Input: pure x-component: c11=1, rest=0
    std::vector<double> coeffs = {0.0, 0.0, 0.0, 1.0};  // c00=0, c1-1=0, c10=0, c11=1

    auto rotated = coarse_grain::rotate_sh_coefficients(coeffs, 1, R);

    // After 90° about z: x→y  So c11(x)→c1-1(y)
    check(std::abs(rotated[0]) < 1e-12, "c00 unchanged (zero)");
    check(std::abs(rotated[1] - 1.0) < 1e-10, "c1-1 gets x→y contribution");
    check(std::abs(rotated[2]) < 1e-12, "c10 (z) unchanged (zero)");
    check(std::abs(rotated[3]) < 1e-10, "c11 (x) rotated away");
}

// ============================================================================
// 8. SH rotation: identity preserves coefficients
// ============================================================================
static void test_rotation_identity() {
    std::printf("\n--- 8. SH rotation: identity preserves coefficients ---\n");

    coarse_grain::Mat3 I;
    I(0, 0) = 1; I(1, 1) = 1; I(2, 2) = 1;

    // l_max=2: 9 coefficients
    std::vector<double> coeffs = {1.0, 0.5, -0.3, 0.7, 0.2, -0.1, 0.4, 0.6, -0.8};
    auto rotated = coarse_grain::rotate_sh_coefficients(coeffs, 2, I);

    bool all_match = true;
    for (int i = 0; i < 9; ++i) {
        if (std::abs(rotated[i] - coeffs[i]) > 1e-12) {
            all_match = false;
            break;
        }
    }
    check(all_match, "identity rotation preserves all 9 coefficients (l_max=2)");
}

// ============================================================================
// 9. Relative rotation from two frames
// ============================================================================
static void test_relative_rotation() {
    std::printf("\n--- 9. Relative rotation from two frames ---\n");

    coarse_grain::InertiaFrame A, B;
    A.valid = B.valid = true;

    // Frame A = identity
    A.axis1 = {1, 0, 0}; A.axis2 = {0, 1, 0}; A.axis3 = {0, 0, 1};
    // Frame B = identity
    B.axis1 = {1, 0, 0}; B.axis2 = {0, 1, 0}; B.axis3 = {0, 0, 1};

    auto R = coarse_grain::compute_relative_rotation(A, B);

    // Should be identity
    check(std::abs(R(0, 0) - 1.0) < 1e-12 &&
          std::abs(R(1, 1) - 1.0) < 1e-12 &&
          std::abs(R(2, 2) - 1.0) < 1e-12,
          "same frame → identity rotation");

    // Frame B rotated 90° about z
    B.axis1 = {0, 1, 0}; B.axis2 = {-1, 0, 0}; B.axis3 = {0, 0, 1};
    R = coarse_grain::compute_relative_rotation(A, B);

    check(std::abs(R(0, 0)) < 1e-12 &&
          std::abs(R(0, 1) + 1.0) < 1e-12,
          "90-degree z rotation detected correctly");
}

// ============================================================================
// 10. Channel interaction: zero for inactive
// ============================================================================
static void test_interaction_inactive() {
    std::printf("\n--- 10. Channel interaction: zero for inactive ---\n");

    coarse_grain::UnifiedChannel A;
    A.active = false;

    std::vector<double> B_rot = {1.0, 0.5, 0.3};

    auto result = coarse_grain::channel_interaction(
        A, B_rot, coarse_grain::Channel::Steric, 3.0);

    check(std::abs(result.energy) < 1e-15, "inactive channel → zero energy");
}

// ============================================================================
// 11. Channel interaction: nonzero for active
// ============================================================================
static void test_interaction_active() {
    std::printf("\n--- 11. Channel interaction: nonzero for active ---\n");

    coarse_grain::UnifiedChannel A;
    A.init(coarse_grain::DescriptorChannel::STERIC, 0);
    A.coeffs[0] = 1.0;

    std::vector<double> B_rot = {1.0};

    auto result = coarse_grain::channel_interaction(
        A, B_rot, coarse_grain::Channel::Steric, 3.0);

    // K_steric(l=0, r=3, alpha=1) = exp(-3) / 1
    double expected = 1.0 * 1.0 * std::exp(-3.0);
    check(std::abs(result.energy - expected) < 1e-12, "active l=0 steric interaction correct");
    check(result.l_max_used == 0, "l_max_used = 0");
}

// ============================================================================
// 12. Channel interaction: per-ℓ decomposition
// ============================================================================
static void test_interaction_per_l() {
    std::printf("\n--- 12. Channel interaction: per-l decomposition ---\n");

    coarse_grain::UnifiedChannel A;
    A.init(coarse_grain::DescriptorChannel::STERIC, 2);
    A.coeffs[0] = 1.0;  // l=0, m=0
    // l=1 and l=2 coefficients remain zero

    std::vector<double> B_rot(9, 0.0);
    B_rot[0] = 2.0;  // l=0, m=0

    auto result = coarse_grain::channel_interaction(
        A, B_rot, coarse_grain::Channel::Steric, 2.0);

    check(result.per_l_energy.size() == 3, "3 ℓ-bands (0,1,2)");
    check(std::abs(result.per_l_energy[0] - 2.0 * std::exp(-2.0)) < 1e-12,
          "l=0 energy = c_A · c_B · K(0,r)");
    check(std::abs(result.per_l_energy[1]) < 1e-15, "l=1 energy = 0 (zero coeffs)");
    check(std::abs(result.per_l_energy[2]) < 1e-15, "l=2 energy = 0 (zero coeffs)");
}

// ============================================================================
// 13. Full interaction: aligned identical beads
// ============================================================================
static void test_full_interaction_aligned() {
    std::printf("\n--- 13. Full interaction: aligned identical beads ---\n");

    coarse_grain::UnifiedDescriptor desc;
    desc.init(2);  // All 3 channels at l_max=2
    desc.steric.coeffs[0] = 1.0;
    desc.electrostatic.coeffs[0] = 0.5;
    desc.dispersion.coeffs[0] = 0.3;

    desc.frame.valid = true;
    desc.frame.axis1 = {1, 0, 0};
    desc.frame.axis2 = {0, 1, 0};
    desc.frame.axis3 = {0, 0, 1};

    atomistic::Vec3 r_vec = {5.0, 0.0, 0.0};

    auto result = coarse_grain::interaction_energy(desc, desc, r_vec);

    check(result.frames_valid, "both frames valid");
    check(std::abs(result.separation - 5.0) < 1e-12, "separation = 5 Å");
    check(std::abs(result.steric.energy) > 1e-15, "steric contribution nonzero");
    check(std::abs(result.electrostatic.energy) > 1e-15, "electrostatic contribution nonzero");
    check(std::abs(result.dispersion.energy) > 1e-15, "dispersion contribution nonzero");

    // Total should be sum of channels
    double sum = result.steric.energy + result.electrostatic.energy + result.dispersion.energy;
    check(std::abs(result.E_total - sum) < 1e-15, "E_total = sum of channels");
}

// ============================================================================
// 14. Full interaction: inactive channels contribute zero
// ============================================================================
static void test_full_interaction_inactive() {
    std::printf("\n--- 14. Full interaction: inactive channels contribute zero ---\n");

    coarse_grain::UnifiedDescriptor desc;
    desc.init_single_channel(coarse_grain::DescriptorChannel::STERIC, 2);
    desc.steric.coeffs[0] = 1.0;

    desc.frame.valid = true;
    desc.frame.axis1 = {1, 0, 0};
    desc.frame.axis2 = {0, 1, 0};
    desc.frame.axis3 = {0, 0, 1};

    atomistic::Vec3 r_vec = {4.0, 0.0, 0.0};
    auto result = coarse_grain::interaction_energy(desc, desc, r_vec);

    check(std::abs(result.electrostatic.energy) < 1e-15, "inactive electrostatic → zero");
    check(std::abs(result.dispersion.energy) < 1e-15, "inactive dispersion → zero");
    check(std::abs(result.steric.energy) > 1e-15, "active steric → nonzero");
    check(std::abs(result.E_total - result.steric.energy) < 1e-15,
          "E_total = steric only");
}

// ============================================================================
// 15. Adaptive refinement: below threshold → no promotion
// ============================================================================
static void test_adapt_no_promotion() {
    std::printf("\n--- 15. Adaptive refinement: below threshold ---\n");

    coarse_grain::UnifiedDescriptor desc;
    desc.init_single_channel(coarse_grain::DescriptorChannel::STERIC, 2);

    // Set c00 = 1 (isotropic)
    desc.steric.coeffs[0] = 1.0;

    // Create reference data that exactly matches the reconstruction
    // (isotropic function = c00 * Y00 at all directions)
    int N = 20;
    std::vector<double> theta(N), phi(N), reference(N);
    for (int i = 0; i < N; ++i) {
        theta[i] = 3.14159 * i / (N - 1);
        phi[i] = 0.0;
        // Y00 = 1/(2*sqrt(pi)) ≈ 0.28209
        reference[i] = desc.steric.evaluate(theta[i], phi[i]);
    }

    coarse_grain::AdaptConfig config;
    config.error_threshold = 0.05;

    auto result = coarse_grain::adapt_bead_steric(desc, reference, theta, phi, config);

    check(!result.steric.promoted, "no promotion when residual is low");
    check(result.steric.residual < 0.01, "residual near zero for exact match");
    check(desc.steric.l_max == 2, "l_max unchanged");
}

// ============================================================================
// 16. Adaptive refinement: above threshold → promotion
// ============================================================================
static void test_adapt_promotion() {
    std::printf("\n--- 16. Adaptive refinement: above threshold ---\n");

    coarse_grain::UnifiedDescriptor desc;
    desc.init_single_channel(coarse_grain::DescriptorChannel::STERIC, 0);
    desc.steric.coeffs[0] = 1.0;

    // Reference data with strong anisotropy (can't be captured at l=0)
    int N = 30;
    std::vector<double> theta(N), phi(N), reference(N);
    for (int i = 0; i < N; ++i) {
        theta[i] = 3.14159 * i / (N - 1);
        phi[i] = 0.0;
        reference[i] = 1.0 + 2.0 * std::cos(theta[i]);  // Strong l=1 component
    }

    coarse_grain::AdaptConfig config;
    config.error_threshold = 0.05;
    config.l_max_step = 2;

    auto result = coarse_grain::adapt_bead_steric(desc, reference, theta, phi, config);

    check(result.steric.promoted, "promotion triggered by high residual");
    check(result.steric.residual > 0.05, "residual exceeds threshold");
    check(desc.steric.l_max == 2, "l_max promoted from 0 to 2");
    check(result.steric.l_max_before == 0, "recorded l_max_before = 0");
    check(result.steric.l_max_after == 2, "recorded l_max_after = 2");
}

// ============================================================================
// 17. Adaptive refinement: cap at max_l_max
// ============================================================================
static void test_adapt_cap() {
    std::printf("\n--- 17. Adaptive refinement: cap at max_l_max ---\n");

    coarse_grain::UnifiedDescriptor desc;
    desc.init_single_channel(coarse_grain::DescriptorChannel::STERIC, 6);
    desc.steric.coeffs[0] = 1.0;

    // Reference with lots of anisotropy
    int N = 20;
    std::vector<double> theta(N), phi(N), reference(N);
    for (int i = 0; i < N; ++i) {
        theta[i] = 3.14159 * i / (N - 1);
        phi[i] = 0.0;
        reference[i] = std::sin(5.0 * theta[i]);  // High-frequency content
    }

    coarse_grain::AdaptConfig config;
    config.error_threshold = 0.01;
    config.max_l_max = 8;
    config.l_max_step = 4;

    auto result = coarse_grain::adapt_bead_steric(desc, reference, theta, phi, config);

    check(desc.steric.l_max <= 8, "l_max does not exceed max_l_max=8");
}

// ============================================================================
// 18. Benzene bead: kernel evaluation at reference distance
// ============================================================================
static void test_benzene_kernels() {
    std::printf("\n--- 18. Benzene bead: kernel evaluation ---\n");

    using coarse_grain::Channel;
    using coarse_grain::channel_kernel;

    double r_stack = 3.5;  // Typical benzene π-stacking distance (Å)
    coarse_grain::ChannelKernelParams p{1.0, 50.0};  // C6 = 50 kcal/mol·Å⁶

    // Steric at stacking distance
    double K_s0 = channel_kernel(Channel::Steric, 0, r_stack, p);
    double K_s2 = channel_kernel(Channel::Steric, 2, r_stack, p);
    double K_s4 = channel_kernel(Channel::Steric, 4, r_stack, p);

    check(K_s0 > 0, "steric kernel positive at stacking distance");
    check(K_s0 > K_s2 && K_s2 > K_s4, "steric kernel decays with l");

    // Electrostatic at stacking distance
    double K_e0 = channel_kernel(Channel::Electrostatic, 0, r_stack, p);
    check(K_e0 > 0, "electrostatic kernel positive");
    check(std::abs(K_e0 - 1.0 / r_stack) < 1e-12, "K_elec(0, r) = 1/r");

    // Dispersion at stacking distance
    double K_d0 = channel_kernel(Channel::Dispersion, 0, r_stack, p);
    check(K_d0 < 0, "dispersion kernel negative (attractive)");
    double expected_d = -50.0 / std::pow(r_stack, 6.0);
    check(std::abs(K_d0 - expected_d) < 1e-10, "K_disp(0, r) = -C6/r^6");
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::printf("=== Anisotropic Bead Model Specification Tests ===\n");

    test_channel_enum();
    test_steric_kernel();
    test_electrostatic_kernel();
    test_dispersion_kernel();
    test_kernel_decay();
    test_rotation_l0();
    test_rotation_l1();
    test_rotation_identity();
    test_relative_rotation();
    test_interaction_inactive();
    test_interaction_active();
    test_interaction_per_l();
    test_full_interaction_aligned();
    test_full_interaction_inactive();
    test_adapt_no_promotion();
    test_adapt_promotion();
    test_adapt_cap();
    test_benzene_kernels();

    std::printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
