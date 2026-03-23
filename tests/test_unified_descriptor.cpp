/**
 * test_unified_descriptor.cpp — Tests for Unified Descriptor Strategy
 *
 * Validates the unified adaptive descriptor formalism:
 *   1. Single-channel initialization (low resolution)
 *   2. Multi-channel initialization (all channels active)
 *   3. Resolution level classification
 *   4. Channel promotion (ℓ_max increase, coefficient preservation)
 *   5. Channel truncation (ℓ_max decrease)
 *   6. Channel activation (inactive → active)
 *   7. Reconstruction residual computation
 *   8. Residual-driven promotion recommendation
 *   9. Unified potential — isotropic (no channels)
 *  10. Unified potential — single channel (low resolution)
 *  11. Unified potential — multi-channel (high resolution)
 *  12. Consistency: same coefficients → same energy regardless of resolution path
 *  13. Conversion from legacy MultiChannelDescriptor
 *  14. Conversion from legacy SurfaceDescriptor
 *  15. Promotion preserves existing coefficients
 *  16. Benzene canonical case (low resolution sufficient)
 *
 * Anti-black-box: every test prints category and result explicitly.
 */

#include "coarse_grain/core/unified_descriptor.hpp"
#include "coarse_grain/models/unified_potential.hpp"
#include "coarse_grain/models/descriptor_residual.hpp"
#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/spherical_harmonics.hpp"
#include "coarse_grain/core/multi_channel_descriptor.hpp"
#include "coarse_grain/core/surface_descriptor.hpp"
#include "atomistic/core/state.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
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
// 1. Single-channel initialization
// ============================================================================
static void test_single_channel_init() {
    std::printf("\n--- 1. Single-channel initialization ---\n");

    coarse_grain::UnifiedDescriptor ud;
    ud.init_single_channel(coarse_grain::DescriptorChannel::STERIC, 2);

    check(ud.steric.active, "steric channel is active");
    check(!ud.electrostatic.active, "electrostatic channel is inactive");
    check(!ud.dispersion.active, "dispersion channel is inactive");
    check(ud.steric.l_max == 2, "steric l_max = 2");
    check(ud.steric.num_coeffs() == 9, "steric has 9 coefficients for l_max=2");
    check(ud.num_active_channels() == 1, "1 active channel");
    check(ud.max_l_max() == 2, "max l_max = 2");
}

// ============================================================================
// 2. Multi-channel initialization
// ============================================================================
static void test_multi_channel_init() {
    std::printf("\n--- 2. Multi-channel initialization ---\n");

    coarse_grain::UnifiedDescriptor ud;
    ud.init(4);

    check(ud.steric.active, "steric active");
    check(ud.electrostatic.active, "electrostatic active");
    check(ud.dispersion.active, "dispersion active");
    check(ud.steric.l_max == 4, "steric l_max = 4");
    check(ud.electrostatic.l_max == 4, "electrostatic l_max = 4");
    check(ud.dispersion.l_max == 4, "dispersion l_max = 4");
    check(ud.num_active_channels() == 3, "3 active channels");
    check(ud.total_active_coefficients() == 75, "75 total coefficients (3 × 25)");
}

// ============================================================================
// 3. Resolution level classification
// ============================================================================
static void test_resolution_classification() {
    std::printf("\n--- 3. Resolution level classification ---\n");

    using RL = coarse_grain::ResolutionLevel;

    check(coarse_grain::classify_resolution(0) == RL::ISOTROPIC, "l_max=0 → ISOTROPIC");
    check(coarse_grain::classify_resolution(1) == RL::AXIAL, "l_max=1 → AXIAL");
    check(coarse_grain::classify_resolution(2) == RL::AXIAL, "l_max=2 → AXIAL");
    check(coarse_grain::classify_resolution(3) == RL::MODERATE, "l_max=3 → MODERATE");
    check(coarse_grain::classify_resolution(4) == RL::MODERATE, "l_max=4 → MODERATE");
    check(coarse_grain::classify_resolution(6) == RL::ENRICHED, "l_max=6 → ENRICHED");
    check(coarse_grain::classify_resolution(8) == RL::ENRICHED, "l_max=8 → ENRICHED");

    coarse_grain::UnifiedDescriptor ud;
    ud.init(4);
    check(ud.resolution_level() == RL::MODERATE, "l_max=4 descriptor → MODERATE");
}

// ============================================================================
// 4. Channel promotion
// ============================================================================
static void test_channel_promotion() {
    std::printf("\n--- 4. Channel promotion ---\n");

    coarse_grain::UnifiedChannel ch;
    ch.init(coarse_grain::DescriptorChannel::STERIC, 2);

    // Set some coefficients
    ch.coeffs[0] = 1.0;
    ch.coeffs[1] = 0.5;
    ch.coeffs[8] = -0.3;

    check(ch.l_max == 2, "initial l_max = 2");
    check(ch.num_coeffs() == 9, "initial 9 coefficients");

    // Promote to l_max = 4
    ch.promote(4);

    check(ch.l_max == 4, "promoted l_max = 4");
    check(ch.num_coeffs() == 25, "promoted to 25 coefficients");
    check(std::abs(ch.coeffs[0] - 1.0) < 1e-15, "coefficient [0] preserved");
    check(std::abs(ch.coeffs[1] - 0.5) < 1e-15, "coefficient [1] preserved");
    check(std::abs(ch.coeffs[8] - (-0.3)) < 1e-15, "coefficient [8] preserved");
    check(std::abs(ch.coeffs[9]) < 1e-15, "new coefficient [9] = 0");
    check(std::abs(ch.coeffs[24]) < 1e-15, "new coefficient [24] = 0");
}

// ============================================================================
// 5. Channel truncation
// ============================================================================
static void test_channel_truncation() {
    std::printf("\n--- 5. Channel truncation ---\n");

    coarse_grain::UnifiedChannel ch;
    ch.init(coarse_grain::DescriptorChannel::STERIC, 4);
    ch.coeffs[0] = 1.0;
    ch.coeffs[24] = 0.7;

    ch.truncate(2);

    check(ch.l_max == 2, "truncated l_max = 2");
    check(ch.num_coeffs() == 9, "truncated to 9 coefficients");
    check(std::abs(ch.coeffs[0] - 1.0) < 1e-15, "low-order coefficient preserved");
}

// ============================================================================
// 6. Channel activation
// ============================================================================
static void test_channel_activation() {
    std::printf("\n--- 6. Channel activation ---\n");

    coarse_grain::UnifiedDescriptor ud;
    ud.init_single_channel(coarse_grain::DescriptorChannel::STERIC, 2);

    check(ud.num_active_channels() == 1, "initially 1 active channel");

    ud.activate_channel(coarse_grain::DescriptorChannel::ELECTROSTATIC, 4);

    check(ud.num_active_channels() == 2, "2 active channels after activation");
    check(ud.electrostatic.active, "electrostatic now active");
    check(ud.electrostatic.l_max == 4, "electrostatic l_max = 4");
    check(ud.max_l_max() == 4, "max l_max = 4 (from electrostatic)");
}

// ============================================================================
// 7. Reconstruction residual computation
// ============================================================================
static void test_residual_computation() {
    std::printf("\n--- 7. Reconstruction residual computation ---\n");

    // Create a channel with known coefficients
    coarse_grain::UnifiedChannel ch;
    ch.init(coarse_grain::DescriptorChannel::STERIC, 2);

    constexpr double c00 = 0.28209479177387814;  // 1/(2√π) = Y_00 normalization
    ch.coeffs[0] = 1.0 / c00;  // Uniform surface value of 1.0

    // Reference values: uniform 1.0 at all sample points
    int n = 20;
    std::vector<double> ref(n, 1.0);
    std::vector<double> theta(n), phi(n);
    for (int i = 0; i < n; ++i) {
        theta[i] = 3.14159265358979 * (i + 0.5) / n;
        phi[i] = 0.0;
    }

    double residual = coarse_grain::compute_channel_residual(ref, theta, phi, ch);

    // Perfect isotropic reconstruction should have very low residual
    check(residual < 0.05, "isotropic reconstruction has low residual");

    // Now test with mismatch: reference has angular variation, channel is isotropic
    for (int i = 0; i < n; ++i)
        ref[i] = 1.0 + 0.5 * std::cos(theta[i]);

    residual = coarse_grain::compute_channel_residual(ref, theta, phi, ch);
    check(residual > 0.01, "angular reference with isotropic fit has nonzero residual");
}

// ============================================================================
// 8. Residual-driven promotion recommendation
// ============================================================================
static void test_residual_promotion() {
    std::printf("\n--- 8. Residual-driven promotion recommendation ---\n");

    coarse_grain::UnifiedChannel ch;
    ch.init(coarse_grain::DescriptorChannel::STERIC, 2);
    ch.coeffs[0] = 1.0;

    // Create reference with high-order angular structure
    int n = 50;
    std::vector<double> ref(n), theta(n), phi(n);
    for (int i = 0; i < n; ++i) {
        theta[i] = 3.14159265358979 * (i + 0.5) / n;
        phi[i] = 0.0;
        // Sharp angular feature that l_max=2 cannot capture
        double t = theta[i];
        ref[i] = 1.0 + 0.8 * std::cos(4.0 * t) + 0.3 * std::sin(6.0 * t);
    }

    coarse_grain::ResidualConfig config;
    config.residual_tolerance = 0.05;
    config.l_max_step = 2;
    config.max_l_max = 8;

    auto result = coarse_grain::evaluate_channel_residual(ref, theta, phi, ch, config);

    check(result.residual > config.residual_tolerance, "high residual for under-resolved channel");
    check(result.needs_promotion, "promotion recommended");
    check(result.recommended_l_max == 4, "recommended l_max = 4 (current 2 + step 2)");
}

// ============================================================================
// 9. Unified potential — isotropic (no active channels)
// ============================================================================
static void test_potential_isotropic() {
    std::printf("\n--- 9. Unified potential — isotropic ---\n");

    coarse_grain::UnifiedDescriptor desc_A, desc_B;
    // Both descriptors left uninitialized (no active channels)

    coarse_grain::UnifiedPotentialParams params;
    params.sigma = 3.4;
    params.epsilon = 0.24;

    atomistic::Vec3 r_vec{5.0, 0.0, 0.0};
    auto result = coarse_grain::unified_potential(r_vec, desc_A, desc_B, params);

    check(std::abs(result.E_anisotropic_total) < 1e-15,
          "zero anisotropic energy with no active channels");
    check(std::abs(result.E_steric) < 1e-15, "zero steric");
    check(std::abs(result.E_electrostatic) < 1e-15, "zero electrostatic");
    check(std::abs(result.E_dispersion) < 1e-15, "zero dispersion");

    // Should be pure LJ
    double r = 5.0;
    double sr = params.sigma / r;
    double sr6 = sr * sr * sr * sr * sr * sr;
    double E_lj = 4.0 * params.epsilon * (sr6 * sr6 - sr6);
    check(std::abs(result.E_total - E_lj) < 1e-12, "total energy = pure LJ");
}

// ============================================================================
// 10. Unified potential — single channel
// ============================================================================
static void test_potential_single_channel() {
    std::printf("\n--- 10. Unified potential — single channel ---\n");

    coarse_grain::UnifiedDescriptor desc_A, desc_B;
    desc_A.init_single_channel(coarse_grain::DescriptorChannel::STERIC, 2);
    desc_B.init_single_channel(coarse_grain::DescriptorChannel::STERIC, 2);

    // Set matching coefficients
    desc_A.steric.coeffs[0] = 1.0;
    desc_A.steric.coeffs[1] = 0.3;
    desc_B.steric.coeffs[0] = 1.0;
    desc_B.steric.coeffs[1] = 0.3;

    coarse_grain::UnifiedPotentialParams params;
    params.sigma = 3.4;
    params.epsilon = 0.24;

    atomistic::Vec3 r_vec{5.0, 0.0, 0.0};
    auto result = coarse_grain::unified_potential(r_vec, desc_A, desc_B, params);

    check(result.E_steric != 0.0, "nonzero steric correction");
    check(std::abs(result.E_electrostatic) < 1e-15, "zero electrostatic (inactive)");
    check(std::abs(result.E_dispersion) < 1e-15, "zero dispersion (inactive)");
    check(result.active_channels_A == 1, "bead A: 1 active channel");
    check(result.active_channels_B == 1, "bead B: 1 active channel");
}

// ============================================================================
// 11. Unified potential — multi-channel
// ============================================================================
static void test_potential_multi_channel() {
    std::printf("\n--- 11. Unified potential — multi-channel ---\n");

    coarse_grain::UnifiedDescriptor desc_A, desc_B;
    desc_A.init(4);
    desc_B.init(4);

    // Set coefficients in all channels
    desc_A.steric.coeffs[0] = 1.0;
    desc_A.electrostatic.coeffs[0] = 0.5;
    desc_A.dispersion.coeffs[0] = 0.8;

    desc_B.steric.coeffs[0] = 1.0;
    desc_B.electrostatic.coeffs[0] = 0.5;
    desc_B.dispersion.coeffs[0] = 0.8;

    coarse_grain::UnifiedPotentialParams params;
    params.sigma = 3.4;
    params.epsilon = 0.24;

    atomistic::Vec3 r_vec{5.0, 0.0, 0.0};
    auto result = coarse_grain::unified_potential(r_vec, desc_A, desc_B, params);

    check(result.E_steric != 0.0, "nonzero steric correction");
    check(result.E_electrostatic != 0.0, "nonzero electrostatic correction");
    check(result.E_dispersion != 0.0, "nonzero dispersion correction");
    check(result.active_channels_A == 3, "bead A: 3 active channels");
    check(result.active_channels_B == 3, "bead B: 3 active channels");

    double expected_aniso = result.E_steric + result.E_electrostatic + result.E_dispersion;
    check(std::abs(result.E_anisotropic_total - expected_aniso) < 1e-12,
          "anisotropic total = sum of channels");
}

// ============================================================================
// 12. Consistency: same coefficients → same energy
// ============================================================================
static void test_consistency_across_resolution_paths() {
    std::printf("\n--- 12. Consistency: same coefficients → same energy ---\n");

    // Path A: init at l_max=2, then promote to l_max=4
    coarse_grain::UnifiedDescriptor desc_A;
    desc_A.init_single_channel(coarse_grain::DescriptorChannel::STERIC, 2);
    desc_A.steric.coeffs[0] = 1.0;
    desc_A.steric.coeffs[1] = 0.3;
    desc_A.promote_channel(coarse_grain::DescriptorChannel::STERIC, 4);

    // Path B: init directly at l_max=4 with same low-order coefficients
    coarse_grain::UnifiedDescriptor desc_B;
    desc_B.init_single_channel(coarse_grain::DescriptorChannel::STERIC, 4);
    desc_B.steric.coeffs[0] = 1.0;
    desc_B.steric.coeffs[1] = 0.3;

    coarse_grain::UnifiedPotentialParams params;
    params.sigma = 3.4;
    params.epsilon = 0.24;

    atomistic::Vec3 r_vec{5.0, 0.0, 0.0};
    auto result_A = coarse_grain::unified_potential(r_vec, desc_A, desc_A, params);
    auto result_B = coarse_grain::unified_potential(r_vec, desc_B, desc_B, params);

    check(std::abs(result_A.E_total - result_B.E_total) < 1e-12,
          "same coefficients produce same energy regardless of promotion path");
    check(std::abs(result_A.E_steric - result_B.E_steric) < 1e-12,
          "same steric correction regardless of promotion path");
}

// ============================================================================
// 13. Conversion from legacy MultiChannelDescriptor
// ============================================================================
static void test_conversion_from_multi_channel() {
    std::printf("\n--- 13. Conversion from legacy MultiChannelDescriptor ---\n");

    coarse_grain::MultiChannelDescriptor mcd;
    mcd.init(4);
    mcd.steric.coeffs[0] = 1.0;
    mcd.steric.coeffs[3] = 0.5;
    mcd.electrostatic.coeffs[0] = 0.8;
    mcd.dispersion.coeffs[0] = 0.6;
    mcd.probe_radius = 2.0;
    mcd.n_samples = 100;

    auto ud = coarse_grain::UnifiedDescriptor::from_multi_channel(mcd);

    check(ud.steric.active, "steric active after conversion");
    check(ud.electrostatic.active, "electrostatic active after conversion");
    check(ud.dispersion.active, "dispersion active after conversion");
    check(ud.steric.l_max == 4, "steric l_max preserved");
    check(std::abs(ud.steric.coeffs[0] - 1.0) < 1e-15, "steric coeff[0] preserved");
    check(std::abs(ud.steric.coeffs[3] - 0.5) < 1e-15, "steric coeff[3] preserved");
    check(std::abs(ud.electrostatic.coeffs[0] - 0.8) < 1e-15, "elec coeff[0] preserved");
    check(std::abs(ud.dispersion.coeffs[0] - 0.6) < 1e-15, "disp coeff[0] preserved");
    check(std::abs(ud.probe_radius - 2.0) < 1e-15, "probe_radius preserved");
    check(ud.n_samples == 100, "n_samples preserved");
}

// ============================================================================
// 14. Conversion from legacy SurfaceDescriptor
// ============================================================================
static void test_conversion_from_surface_descriptor() {
    std::printf("\n--- 14. Conversion from legacy SurfaceDescriptor ---\n");

    coarse_grain::SurfaceDescriptor sd;
    sd.coeffs[0] = 1.5;
    sd.coeffs[1] = 0.3;
    sd.probe_radius = 1.5;
    sd.n_samples = 50;

    auto ud = coarse_grain::UnifiedDescriptor::from_surface_descriptor(sd);

    check(ud.steric.active, "steric active after conversion");
    check(!ud.electrostatic.active, "electrostatic inactive (single-channel source)");
    check(!ud.dispersion.active, "dispersion inactive (single-channel source)");
    check(ud.steric.l_max == coarse_grain::SH_L_MAX, "l_max matches SH_L_MAX");
    check(std::abs(ud.steric.coeffs[0] - 1.5) < 1e-15, "steric coeff[0] preserved");
    check(std::abs(ud.steric.coeffs[1] - 0.3) < 1e-15, "steric coeff[1] preserved");
    check(ud.num_active_channels() == 1, "1 active channel");
}

// ============================================================================
// 15. Promotion preserves existing coefficients
// ============================================================================
static void test_promotion_preserves_coefficients() {
    std::printf("\n--- 15. Promotion preserves existing coefficients ---\n");

    coarse_grain::UnifiedDescriptor ud;
    ud.init(2);

    // Set specific values
    ud.steric.coeffs[0] = 1.0;
    ud.steric.coeffs[4] = 0.7;
    ud.steric.coeffs[8] = -0.2;
    ud.electrostatic.coeffs[0] = 0.5;

    // Promote all to l_max=6
    ud.promote_all(6);

    check(ud.steric.l_max == 6, "steric promoted to l_max=6");
    check(ud.electrostatic.l_max == 6, "electrostatic promoted to l_max=6");
    check(ud.dispersion.l_max == 6, "dispersion promoted to l_max=6");
    check(std::abs(ud.steric.coeffs[0] - 1.0) < 1e-15, "steric [0] preserved");
    check(std::abs(ud.steric.coeffs[4] - 0.7) < 1e-15, "steric [4] preserved");
    check(std::abs(ud.steric.coeffs[8] - (-0.2)) < 1e-15, "steric [8] preserved");
    check(std::abs(ud.electrostatic.coeffs[0] - 0.5) < 1e-15, "elec [0] preserved");

    // New coefficients should be zero
    check(std::abs(ud.steric.coeffs[9]) < 1e-15, "new steric [9] = 0");
    check(std::abs(ud.steric.coeffs[48]) < 1e-15, "new steric [48] = 0");
}

// ============================================================================
// 16. Benzene canonical case (low resolution sufficient)
// ============================================================================
static void test_benzene_canonical() {
    std::printf("\n--- 16. Benzene canonical case ---\n");

    using RL = coarse_grain::ResolutionLevel;

    // Benzene: 12 atoms, planar, low angular complexity
    coarse_grain::UnifiedDescriptor ud;
    ud.init_single_channel(coarse_grain::DescriptorChannel::STERIC, 2);

    // Simulate a low-anisotropy benzene descriptor
    constexpr double c00 = 0.28209479177387814;
    ud.steric.coeffs[0] = 5.0 / c00;   // Large isotropic component
    ud.steric.coeffs[4] = 0.3;          // Small Y_20 term (planar)

    check(ud.resolution_level() == RL::AXIAL, "benzene: AXIAL resolution level");
    check(ud.num_active_channels() == 1, "benzene: 1 active channel");
    check(ud.steric.anisotropy_ratio() < 0.5, "benzene: moderate anisotropy");

    // Low residual — no promotion needed
    ud.steric.residual = 0.02;
    check(ud.max_residual() < 0.05, "benzene: residual below tolerance");
}

// ============================================================================
// 17. Bead unified descriptor field
// ============================================================================
static void test_bead_unified_field() {
    std::printf("\n--- 17. Bead unified descriptor field ---\n");

    coarse_grain::Bead bead;
    check(!bead.has_unified_data(), "bead starts without unified data");

    coarse_grain::UnifiedDescriptor ud;
    ud.init(2);
    bead.unified = ud;

    check(bead.has_unified_data(), "bead has unified data after assignment");
    check(bead.unified->num_active_channels() == 3, "3 active channels");
    check(bead.unified->max_l_max() == 2, "max l_max = 2");
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::printf("=== Unified Descriptor Strategy Tests ===\n");

    test_single_channel_init();
    test_multi_channel_init();
    test_resolution_classification();
    test_channel_promotion();
    test_channel_truncation();
    test_channel_activation();
    test_residual_computation();
    test_residual_promotion();
    test_potential_isotropic();
    test_potential_single_channel();
    test_potential_multi_channel();
    test_consistency_across_resolution_paths();
    test_conversion_from_multi_channel();
    test_conversion_from_surface_descriptor();
    test_promotion_preserves_coefficients();
    test_benzene_canonical();
    test_bead_unified_field();

    std::printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
