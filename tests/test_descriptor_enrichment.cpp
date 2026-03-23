/**
 * test_descriptor_enrichment.cpp — Tests for Multi-Channel & Higher-Order Descriptors
 *
 * Validates the descriptor enrichment subsystem:
 *   1. Dynamic SH evaluation (runtime ℓ_max)
 *   2. Multi-channel descriptor construction
 *   3. Channel independence and isolation
 *   4. Multi-channel mapper (benzene canonical test)
 *   5. Multi-channel potential decomposition
 *   6. Adaptive complexity selection
 *   7. Higher-order convergence
 *   8. Backward compatibility with fixed API
 *
 * Anti-black-box: every test prints its category and result explicitly.
 */

#include "coarse_grain/core/spherical_harmonics.hpp"
#include "coarse_grain/core/multi_channel_descriptor.hpp"
#include "coarse_grain/core/inertia_frame.hpp"
#include "coarse_grain/mapping/multi_channel_mapper.hpp"
#include "coarse_grain/models/multi_channel_potential.hpp"
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
// 1. Dynamic SH evaluation
// ============================================================================
static void test_dynamic_sh() {
    std::printf("\n=== Dynamic SH Evaluation ===\n");
    using namespace coarse_grain;

    // sh_num_coeffs
    check(sh_num_coeffs(0) == 1,  "sh_num_coeffs(0) == 1");
    check(sh_num_coeffs(1) == 4,  "sh_num_coeffs(1) == 4");
    check(sh_num_coeffs(4) == 25, "sh_num_coeffs(4) == 25");
    check(sh_num_coeffs(6) == 49, "sh_num_coeffs(6) == 49");
    check(sh_num_coeffs(8) == 81, "sh_num_coeffs(8) == 81");

    // Dynamic evaluation at ℓ_max=4 must match fixed evaluation
    double theta = 1.2;
    double phi = 0.8;
    auto Y_fixed = evaluate_all_harmonics(theta, phi);
    auto Y_dyn   = evaluate_all_harmonics_dynamic(theta, phi, 4);

    check(Y_dyn.size() == 25, "dynamic ℓ_max=4 returns 25 coefficients");

    double max_diff = 0.0;
    for (int i = 0; i < 25; ++i) {
        double diff = std::abs(Y_fixed[i] - Y_dyn[i]);
        if (diff > max_diff) max_diff = diff;
    }
    check(max_diff < 1e-12, "dynamic ℓ_max=4 matches fixed API to machine precision");

    // Higher-order evaluation (ℓ_max=8) returns more coefficients
    auto Y_high = evaluate_all_harmonics_dynamic(theta, phi, 8);
    check(static_cast<int>(Y_high.size()) == 81, "dynamic ℓ_max=8 returns 81 coefficients");

    // Y_0^0 = 1/√(4π) at any angle (constant harmonic)
    double Y00_expected = 1.0 / std::sqrt(4.0 * 3.14159265358979323846);
    check(std::abs(Y_high[0] - Y00_expected) < 1e-12, "Y_0^0 constant harmonic at ℓ_max=8");

    // First 25 coefficients of ℓ_max=8 must match ℓ_max=4
    double max_diff_8 = 0.0;
    for (int i = 0; i < 25; ++i) {
        double diff = std::abs(Y_high[i] - Y_dyn[i]);
        if (diff > max_diff_8) max_diff_8 = diff;
    }
    check(max_diff_8 < 1e-12, "higher-order first 25 coeffs match ℓ_max=4");
}

// ============================================================================
// 2. Dynamic SH expansion and metrics
// ============================================================================
static void test_dynamic_sh_metrics() {
    std::printf("\n=== Dynamic SH Metrics ===\n");
    using namespace coarse_grain;

    // Create an isotropic descriptor (only c_00 nonzero)
    std::vector<double> iso_coeffs(sh_num_coeffs(6), 0.0);
    iso_coeffs[0] = 1.0;

    check(std::abs(sh_power_dynamic(iso_coeffs) - 1.0) < 1e-12,
          "isotropic descriptor power == 1.0");
    check(std::abs(sh_anisotropy_ratio_dynamic(iso_coeffs)) < 1e-12,
          "isotropic descriptor anisotropy ratio == 0.0");

    // Add ℓ=2 power
    iso_coeffs[sh_index(2, 0)] = 0.5;
    double ratio = sh_anisotropy_ratio_dynamic(iso_coeffs);
    check(ratio > 0.0 && ratio < 1.0,
          "mixed descriptor has intermediate anisotropy");

    // Band power
    auto bands = sh_band_power_dynamic(iso_coeffs, 6);
    check(static_cast<int>(bands.size()) == 7, "band_power_dynamic returns l_max+1 bands");
    check(std::abs(bands[0] - 1.0) < 1e-12, "ℓ=0 band power = 1.0");
    check(std::abs(bands[2] - 0.25) < 1e-12, "ℓ=2 band power = 0.25");
    check(std::abs(bands[1]) < 1e-12, "ℓ=1 band power = 0.0");

    // Evaluation
    double val = evaluate_sh_expansion_dynamic(iso_coeffs, 0.5, 1.0, 6);
    check(std::isfinite(val), "dynamic expansion evaluation is finite");
}

// ============================================================================
// 3. Multi-channel descriptor construction
// ============================================================================
static void test_multi_channel_descriptor() {
    std::printf("\n=== Multi-Channel Descriptor Construction ===\n");
    using namespace coarse_grain;

    MultiChannelDescriptor desc;
    desc.init(4, 6, 8);

    check(desc.steric.l_max == 4, "steric channel ℓ_max = 4");
    check(desc.electrostatic.l_max == 6, "electrostatic channel ℓ_max = 6");
    check(desc.dispersion.l_max == 8, "dispersion channel ℓ_max = 8");

    check(static_cast<int>(desc.steric.coeffs.size()) == 25,
          "steric channel has 25 coefficients");
    check(static_cast<int>(desc.electrostatic.coeffs.size()) == 49,
          "electrostatic channel has 49 coefficients");
    check(static_cast<int>(desc.dispersion.coeffs.size()) == 81,
          "dispersion channel has 81 coefficients");

    check(desc.steric.channel == DescriptorChannel::STERIC,
          "steric channel enum correct");
    check(desc.electrostatic.channel == DescriptorChannel::ELECTROSTATIC,
          "electrostatic channel enum correct");
    check(desc.dispersion.channel == DescriptorChannel::DISPERSION,
          "dispersion channel enum correct");

    // Uniform init
    MultiChannelDescriptor desc2;
    desc2.init(6);
    check(desc2.steric.l_max == 6, "uniform init: steric ℓ_max = 6");
    check(desc2.electrostatic.l_max == 6, "uniform init: electrostatic ℓ_max = 6");
    check(desc2.dispersion.l_max == 6, "uniform init: dispersion ℓ_max = 6");

    // Channel access by enum
    check(&desc.channel(DescriptorChannel::STERIC) == &desc.steric,
          "channel(STERIC) returns steric");
    check(&desc.channel(DescriptorChannel::DISPERSION) == &desc.dispersion,
          "channel(DISPERSION) returns dispersion");

    // Channel names
    check(std::string(channel_name(DescriptorChannel::STERIC)) == "steric",
          "channel_name(STERIC) correct");
    check(std::string(channel_name(DescriptorChannel::ELECTROSTATIC)) == "electrostatic",
          "channel_name(ELECTROSTATIC) correct");
}

// ============================================================================
// 4. Channel independence
// ============================================================================
static void test_channel_independence() {
    std::printf("\n=== Channel Independence ===\n");
    using namespace coarse_grain;

    MultiChannelDescriptor desc;
    desc.init(4);

    // Set only steric channel
    desc.steric.coeffs[0] = 2.0;
    desc.steric.coeffs[sh_index(2, 0)] = 1.0;

    check(desc.steric.total_power() > 0.0, "steric channel has power");
    check(desc.electrostatic.total_power() < 1e-30, "electrostatic channel remains zero");
    check(desc.dispersion.total_power() < 1e-30, "dispersion channel remains zero");

    // Set electrostatic channel independently
    desc.electrostatic.coeffs[0] = 3.0;
    check(desc.electrostatic.total_power() > 0.0, "electrostatic channel now has power");
    check(std::abs(desc.steric.total_power() - 5.0) < 1e-12,
          "steric channel unchanged after electrostatic modification");

    // Max anisotropy
    double max_a = desc.max_anisotropy();
    check(max_a > 0.0, "max_anisotropy > 0 with anisotropic steric channel");
}

// ============================================================================
// 5. Multi-channel mapper (benzene canonical)
// ============================================================================
static void test_multi_channel_mapper() {
    std::printf("\n=== Multi-Channel Mapper (Benzene) ===\n");
    using namespace coarse_grain;

    // Build benzene: 6 C + 6 H in XY plane
    atomistic::State state;
    state.N = 12;
    state.X.resize(12);
    state.M.resize(12);
    state.Q.resize(12);

    constexpr double R_CC = 1.40;  // Å
    constexpr double R_CH = 1.09;
    constexpr double pi = 3.14159265358979323846;

    for (int i = 0; i < 6; ++i) {
        double angle = i * pi / 3.0;
        state.X[i] = { R_CC * std::cos(angle), R_CC * std::sin(angle), 0.0 };
        state.M[i] = 12.011;
        state.Q[i] = -0.115;  // Partial charge on C
    }
    for (int i = 0; i < 6; ++i) {
        double angle = i * pi / 3.0;
        state.X[6 + i] = { (R_CC + R_CH) * std::cos(angle),
                            (R_CC + R_CH) * std::sin(angle), 0.0 };
        state.M[6 + i] = 1.008;
        state.Q[6 + i] = 0.115;  // Partial charge on H
    }

    std::vector<uint32_t> indices(12);
    for (int i = 0; i < 12; ++i) indices[i] = i;

    atomistic::Vec3 com = {0.0, 0.0, 0.0};
    for (int i = 0; i < 12; ++i) {
        com.x += state.M[i] * state.X[i].x;
        com.y += state.M[i] * state.X[i].y;
        com.z += state.M[i] * state.X[i].z;
    }
    double total_mass = 0.0;
    for (int i = 0; i < 12; ++i) total_mass += state.M[i];
    com.x /= total_mass;
    com.y /= total_mass;
    com.z /= total_mass;

    MultiChannelMapperConfig config;
    config.n_samples = 200;
    config.probe_radius = 3.0;
    config.l_max_steric = 4;
    config.l_max_electrostatic = 6;
    config.l_max_dispersion = 4;

    MultiChannelMapper mapper;
    auto desc = mapper.compute(state, indices, com, config);

    // All channels should be populated
    check(desc.steric.total_power() > 0.0, "benzene steric channel has power");
    check(desc.electrostatic.total_power() > 0.0, "benzene electrostatic channel has power");
    check(desc.dispersion.total_power() > 0.0, "benzene dispersion channel has power");

    // Steric channel should be anisotropic (planar molecule)
    check(desc.steric.anisotropy_ratio() > 0.01,
          "benzene steric channel is anisotropic");

    // Electrostatic channel should have higher angular resolution
    check(static_cast<int>(desc.electrostatic.coeffs.size()) == 49,
          "electrostatic channel has 49 coefficients (ℓ_max=6)");

    // Frame should be valid
    check(desc.frame.valid, "inertia frame is valid");

    // Probe parameters stored
    check(std::abs(desc.probe_radius - 3.0) < 1e-12, "probe_radius stored");
    check(desc.n_samples == 200, "n_samples stored");
}

// ============================================================================
// 6. Multi-channel potential decomposition
// ============================================================================
static void test_multi_channel_potential() {
    std::printf("\n=== Multi-Channel Potential Decomposition ===\n");
    using namespace coarse_grain;

    // Create two descriptors with known coefficients
    MultiChannelDescriptor A, B;
    A.init(4);
    B.init(4);

    // Identical steric profiles → positive coupling
    A.steric.coeffs[0] = 1.0;
    A.steric.coeffs[sh_index(2, 0)] = 0.5;
    B.steric.coeffs[0] = 1.0;
    B.steric.coeffs[sh_index(2, 0)] = 0.5;

    // Different electrostatic → lower coupling
    A.electrostatic.coeffs[0] = 1.0;
    B.electrostatic.coeffs[0] = -1.0;

    // Zero dispersion → zero dispersion contribution
    // (already zero from init)

    MultiChannelPotentialParams params;
    params.sigma = 3.4;
    params.epsilon = 0.05;
    params.steric_kernel = {0.1, 6.0};
    params.electrostatic_kernel = {0.05, 3.0};
    params.dispersion_kernel = {0.08, 6.0};

    atomistic::Vec3 r_vec = {5.0, 0.0, 0.0};
    auto result = multi_channel_potential(r_vec, A, B, params);

    // Isotropic LJ should be finite
    check(std::isfinite(result.E_isotropic), "isotropic LJ energy is finite");

    // Steric coupling: dot = 1.0*1.0 + 0.5*0.5 = 1.25
    check(std::abs(result.coupling_steric - 1.25) < 1e-12,
          "steric coupling = 1.25 (dot product)");

    // Electrostatic coupling: dot = 1.0 * (-1.0) = -1.0
    check(std::abs(result.coupling_electrostatic - (-1.0)) < 1e-12,
          "electrostatic coupling = -1.0 (opposite charges)");

    // Dispersion coupling: 0 (both zero)
    check(std::abs(result.coupling_dispersion) < 1e-12,
          "dispersion coupling = 0.0 (both zero)");

    // Steric contribution should be positive (positive coupling * positive lambda)
    check(result.E_steric > 0.0, "steric energy correction is positive");

    // Electrostatic contribution should be negative (negative coupling * positive lambda)
    check(result.E_electrostatic < 0.0, "electrostatic energy correction is negative");

    // Dispersion contribution should be zero
    check(std::abs(result.E_dispersion) < 1e-20, "dispersion contribution is zero");

    // Total is sum
    double expected_total = result.E_isotropic + result.E_steric +
                           result.E_electrostatic + result.E_dispersion;
    check(std::abs(result.E_total - expected_total) < 1e-12,
          "E_total = E_iso + E_steric + E_elec + E_disp");

    // E_anisotropic_total is sum of channel contributions
    double expected_aniso = result.E_steric + result.E_electrostatic + result.E_dispersion;
    check(std::abs(result.E_anisotropic_total - expected_aniso) < 1e-12,
          "E_anisotropic_total = sum of channel corrections");
}

// ============================================================================
// 7. Adaptive complexity selection
// ============================================================================
static void test_adaptive_complexity() {
    std::printf("\n=== Adaptive Complexity Selection ===\n");
    using namespace coarse_grain;

    check(MultiChannelDescriptor::suggest_l_max(0.05) == 2,
          "nearly isotropic → ℓ_max = 2");
    check(MultiChannelDescriptor::suggest_l_max(0.2) == 4,
          "moderate anisotropy → ℓ_max = 4");
    check(MultiChannelDescriptor::suggest_l_max(0.5) == 6,
          "strong anisotropy → ℓ_max = 6");
    check(MultiChannelDescriptor::suggest_l_max(0.8) == 8,
          "complex anisotropy → ℓ_max = 8");
    check(MultiChannelDescriptor::suggest_l_max(0.0) == 2,
          "zero anisotropy → ℓ_max = 2");
    check(MultiChannelDescriptor::suggest_l_max(1.0) == 8,
          "maximum anisotropy → ℓ_max = 8");
}

// ============================================================================
// 8. Self-coupling (identical beads)
// ============================================================================
static void test_self_coupling() {
    std::printf("\n=== Self-Coupling (Multi-Channel) ===\n");
    using namespace coarse_grain;

    MultiChannelDescriptor desc;
    desc.init(4);
    desc.steric.coeffs[0] = 2.0;
    desc.steric.coeffs[sh_index(2, 0)] = 0.8;
    desc.electrostatic.coeffs[0] = 1.5;
    desc.dispersion.coeffs[0] = 0.7;

    // Self-coupling should equal power for each channel
    double self_steric = channel_coupling(desc.steric, desc.steric);
    check(std::abs(self_steric - desc.steric.total_power()) < 1e-12,
          "self-coupling steric == channel power");

    double self_elec = channel_coupling(desc.electrostatic, desc.electrostatic);
    check(std::abs(self_elec - desc.electrostatic.total_power()) < 1e-12,
          "self-coupling electrostatic == channel power");

    double self_disp = channel_coupling(desc.dispersion, desc.dispersion);
    check(std::abs(self_disp - desc.dispersion.total_power()) < 1e-12,
          "self-coupling dispersion == channel power");
}

// ============================================================================
// 9. Backward compatibility with fixed API
// ============================================================================
static void test_backward_compatibility() {
    std::printf("\n=== Backward Compatibility ===\n");
    using namespace coarse_grain;

    // Existing compile-time constants still valid
    check(SH_L_MAX == 4, "SH_L_MAX still 4");
    check(SH_NUM_COEFFS == 25, "SH_NUM_COEFFS still 25");
    check(sh_index(2, 1) == 6, "sh_index(2,1) still 6");

    // Fixed API still works
    auto Y = evaluate_all_harmonics(0.5, 1.0);
    check(static_cast<int>(Y.size()) == 25, "fixed evaluate_all_harmonics still 25");

    std::array<double, SH_NUM_COEFFS> coeffs{};
    coeffs[0] = 1.0;
    double val = evaluate_sh_expansion(coeffs, 0.5, 1.0);
    check(std::isfinite(val), "fixed evaluate_sh_expansion works");

    double pw = sh_power(coeffs);
    check(std::abs(pw - 1.0) < 1e-12, "fixed sh_power works");

    auto bp = sh_band_power(coeffs);
    check(static_cast<int>(bp.size()) == 5, "fixed sh_band_power returns 5 bands");

    double ar = sh_anisotropy_ratio(coeffs);
    check(std::abs(ar) < 1e-12, "fixed sh_anisotropy_ratio works");
}

// ============================================================================
// 10. Channel-specific radial kernels
// ============================================================================
static void test_radial_kernels() {
    std::printf("\n=== Channel-Specific Radial Kernels ===\n");
    using namespace coarse_grain;

    double sigma = 3.4;
    double r = 5.0;

    double K6 = radial_kernel(r, sigma, 6.0);
    double K3 = radial_kernel(r, sigma, 3.0);

    // K(r) = (σ/r)^n, so K6 < K3 for r > σ (faster decay)
    check(K6 < K3, "r^-6 kernel decays faster than r^-3 at r > σ");
    check(K6 > 0.0, "r^-6 kernel is positive");
    check(K3 > 0.0, "r^-3 kernel is positive");

    // At r = sigma, K = 1.0
    double K_at_sigma = radial_kernel(sigma, sigma, 6.0);
    check(std::abs(K_at_sigma - 1.0) < 1e-12, "K(σ, σ, n) = 1.0");

    // Different exponents give different range behavior
    double K12 = radial_kernel(r, sigma, 12.0);
    check(K12 < K6, "r^-12 decays faster than r^-6");
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::printf("================================================================\n");
    std::printf("  DESCRIPTOR ENRICHMENT TEST SUITE\n");
    std::printf("  Multi-Channel & Higher-Order SH Descriptors\n");
    std::printf("================================================================\n");

    test_dynamic_sh();
    test_dynamic_sh_metrics();
    test_multi_channel_descriptor();
    test_channel_independence();
    test_multi_channel_mapper();
    test_multi_channel_potential();
    test_adaptive_complexity();
    test_self_coupling();
    test_backward_compatibility();
    test_radial_kernels();

    std::printf("\n================================================================\n");
    std::printf("  RESULTS: %d passed, %d failed, %d total\n", g_pass, g_fail, g_pass + g_fail);
    std::printf("================================================================\n");

    return g_fail > 0 ? 1 : 0;
}
