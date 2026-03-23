/**
 * test_model_selection.cpp — Tests for Two-Tier Model Selection
 *
 * Validates the model selection heuristic and tiered interaction routing:
 *   1. Small molecules → Tier 1 (reduced)
 *   2. Large molecules → Tier 2 (enriched) by atom count
 *   3. Metal center → Tier 2 (enriched) by structural criterion
 *   4. High anisotropy → Tier 2 (enriched) by definitive criterion
 *   5. Anisotropy overrides atom count (definitive criterion)
 *   6. From-metrics convenience function
 *   7. Tier names and reason strings
 *   8. Tiered interaction routing (Tier 1 path)
 *   9. Tiered interaction routing (Tier 2 path)
 *  10. Tier 2 fallback when data unavailable
 *  11. Custom thresholds
 *  12. Benzene canonical case (Tier 1 expected)
 *
 * Anti-black-box: every test prints category and result explicitly.
 */

#include "coarse_grain/models/model_selector.hpp"
#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/multi_channel_descriptor.hpp"
#include "coarse_grain/core/surface_descriptor.hpp"
#include "coarse_grain/core/spherical_harmonics.hpp"
#include "coarse_grain/core/orientation.hpp"
#include "coarse_grain/models/orientation_potential.hpp"
#include "coarse_grain/models/multi_channel_potential.hpp"
#include "atomistic/core/state.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

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
// Helper: create a bead with N parent atoms
// ============================================================================
static coarse_grain::Bead make_bead(uint32_t n_atoms, double x = 0.0) {
    coarse_grain::Bead b;
    b.position = {x, 0.0, 0.0};
    for (uint32_t i = 0; i < n_atoms; ++i)
        b.parent_atom_indices.push_back(i);
    b.mass = n_atoms * 12.0;
    return b;
}

// ============================================================================
// 1. Small molecule → Tier 1
// ============================================================================
static void test_small_molecule_tier1() {
    std::printf("\n=== Small Molecule → Tier 1 ===\n");
    using namespace coarse_grain;

    auto bead = make_bead(12);  // benzene: 12 atoms
    auto result = select_model_tier(bead);

    check(result.tier == ModelTier::REDUCED, "12-atom bead → Tier 1");
    check(result.atom_count == 12, "atom_count recorded as 12");
    check(!result.promoted_by_atom_count, "not promoted by atom count");
    check(!result.promoted_by_anisotropy, "not promoted by anisotropy");
    check(!result.promoted_by_metal_center, "not promoted by metal center");
}

// ============================================================================
// 2. Large molecule → Tier 2 by atom count
// ============================================================================
static void test_large_molecule_tier2() {
    std::printf("\n=== Large Molecule → Tier 2 (atom count) ===\n");
    using namespace coarse_grain;

    auto bead = make_bead(24);  // organometallic cluster
    auto result = select_model_tier(bead);

    check(result.tier == ModelTier::ENRICHED, "24-atom bead → Tier 2");
    check(result.promoted_by_atom_count, "promoted by atom count");
    check(result.atom_count == 24, "atom_count recorded as 24");
}

// ============================================================================
// 3. Metal center → Tier 2
// ============================================================================
static void test_metal_center_promotion() {
    std::printf("\n=== Metal Center → Tier 2 ===\n");
    using namespace coarse_grain;

    auto bead = make_bead(8);  // small but metal-centered
    auto result_sel = select_model_tier_from_metrics(8, 0.1, true);

    check(result_sel.tier == ModelTier::ENRICHED, "metal center → Tier 2");
    check(result_sel.promoted_by_metal_center, "promoted_by_metal_center flag set");
    check(!result_sel.promoted_by_atom_count, "not by atom count (only 8 atoms)");
}

// ============================================================================
// 4. High anisotropy → Tier 2 (definitive criterion)
// ============================================================================
static void test_high_anisotropy_promotion() {
    std::printf("\n=== High Anisotropy → Tier 2 (definitive) ===\n");
    using namespace coarse_grain;

    auto bead = make_bead(10);  // small atom count

    // Add a surface descriptor with high anisotropy
    SurfaceDescriptor sd;
    sd.coeffs[0] = 1.0;
    sd.coeffs[coarse_grain::sh_index(2, 0)] = 3.0;  // Strong ℓ=2 → high ratio
    bead.surface = sd;

    auto result = select_model_tier(bead);

    check(result.tier == ModelTier::ENRICHED,
          "high anisotropy overrides small atom count → Tier 2");
    check(result.promoted_by_anisotropy, "promoted_by_anisotropy flag set");
    check(result.anisotropy_ratio > 0.45, "anisotropy ratio above threshold");
}

// ============================================================================
// 5. Anisotropy overrides atom count (definitive criterion)
// ============================================================================
static void test_anisotropy_definitive() {
    std::printf("\n=== Anisotropy is Definitive Criterion ===\n");
    using namespace coarse_grain;

    // Low atom count, low anisotropy → Tier 1
    auto r1 = select_model_tier_from_metrics(6, 0.05);
    check(r1.tier == ModelTier::REDUCED, "6 atoms, low anisotropy → Tier 1");

    // Low atom count, high anisotropy → Tier 2 (definitive override)
    auto r2 = select_model_tier_from_metrics(6, 0.8);
    check(r2.tier == ModelTier::ENRICHED, "6 atoms, high anisotropy → Tier 2");
    check(r2.promoted_by_anisotropy, "anisotropy override is definitive");

    // High atom count, low anisotropy → Tier 2 (by atom count)
    auto r3 = select_model_tier_from_metrics(30, 0.05);
    check(r3.tier == ModelTier::ENRICHED, "30 atoms, low anisotropy → Tier 2 by count");
    check(r3.promoted_by_atom_count, "promoted by atom count, not anisotropy");
}

// ============================================================================
// 6. From-metrics convenience function
// ============================================================================
static void test_from_metrics() {
    std::printf("\n=== From-Metrics Convenience ===\n");
    using namespace coarse_grain;

    auto r1 = select_model_tier_from_metrics(10, 0.1, false);
    check(r1.tier == ModelTier::REDUCED, "10 atoms, ratio=0.1, no metal → Tier 1");

    auto r2 = select_model_tier_from_metrics(20, 0.1, false);
    check(r2.tier == ModelTier::ENRICHED, "20 atoms, ratio=0.1, no metal → Tier 2");

    auto r3 = select_model_tier_from_metrics(10, 0.1, true);
    check(r3.tier == ModelTier::ENRICHED, "10 atoms, ratio=0.1, metal → Tier 2");

    auto r4 = select_model_tier_from_metrics(10, 0.9, false);
    check(r4.tier == ModelTier::ENRICHED, "10 atoms, ratio=0.9, no metal → Tier 2");
}

// ============================================================================
// 7. Tier names and reason strings
// ============================================================================
static void test_tier_names_and_reasons() {
    std::printf("\n=== Tier Names and Reasons ===\n");
    using namespace coarse_grain;

    check(std::string(tier_name(ModelTier::REDUCED)) == "Tier 1: Reduced Anisotropic",
          "tier_name REDUCED correct");
    check(std::string(tier_name(ModelTier::ENRICHED)) == "Tier 2: Enriched Descriptor",
          "tier_name ENRICHED correct");

    // Default (no promotion)
    auto r1 = select_model_tier_from_metrics(6, 0.05);
    check(std::string(r1.reason()).find("sufficient") != std::string::npos,
          "default reason mentions 'sufficient'");

    // Anisotropy promotion
    auto r2 = select_model_tier_from_metrics(6, 0.9);
    check(std::string(r2.reason()).find("angular") != std::string::npos,
          "anisotropy reason mentions 'angular'");

    // Metal promotion
    auto r3 = select_model_tier_from_metrics(6, 0.1, true);
    check(std::string(r3.reason()).find("metal") != std::string::npos,
          "metal reason mentions 'metal'");

    // Atom count promotion
    auto r4 = select_model_tier_from_metrics(25, 0.1);
    check(std::string(r4.reason()).find("atom count") != std::string::npos,
          "atom count reason mentions 'atom count'");
}

// ============================================================================
// 8. Tiered interaction routing — Tier 1 path
// ============================================================================
static void test_tiered_interaction_tier1() {
    std::printf("\n=== Tiered Interaction Routing — Tier 1 ===\n");
    using namespace coarse_grain;

    auto bead_A = make_bead(12, 0.0);
    auto bead_B = make_bead(12, 5.0);
    bead_A.orientation.normal = {0.0, 0.0, 1.0};
    bead_B.orientation.normal = {0.0, 0.0, 1.0};
    bead_A.has_orientation = true;
    bead_B.has_orientation = true;

    OrientationPotentialParams reduced_params;
    reduced_params.sigma = 3.4;
    reduced_params.epsilon = 0.24;
    reduced_params.lambda1 = -0.1;

    MultiChannelPotentialParams enriched_params;

    auto result = evaluate_tiered_interaction(bead_A, bead_B,
                                               ModelTier::REDUCED,
                                               reduced_params, enriched_params);

    check(result.tier == ModelTier::REDUCED, "Tier 1 path used");
    check(std::isfinite(result.E_total), "Tier 1 energy is finite");
    check(std::isfinite(result.E_isotropic), "Tier 1 isotropic energy is finite");
    check(std::abs(result.E_steric) < 1e-20, "Tier 1 has no steric channel");
    check(std::abs(result.E_electrostatic) < 1e-20, "Tier 1 has no elec channel");
    check(std::abs(result.E_dispersion) < 1e-20, "Tier 1 has no disp channel");
}

// ============================================================================
// 9. Tiered interaction routing — Tier 2 path
// ============================================================================
static void test_tiered_interaction_tier2() {
    std::printf("\n=== Tiered Interaction Routing — Tier 2 ===\n");
    using namespace coarse_grain;

    auto bead_A = make_bead(24, 0.0);
    auto bead_B = make_bead(24, 5.0);

    // Create multi-channel descriptors
    MultiChannelDescriptor mc_A, mc_B;
    mc_A.init(4);
    mc_B.init(4);
    mc_A.steric.coeffs[0] = 1.0;
    mc_B.steric.coeffs[0] = 1.0;
    mc_A.electrostatic.coeffs[0] = 0.5;
    mc_B.electrostatic.coeffs[0] = 0.5;
    bead_A.multi_channel = mc_A;
    bead_B.multi_channel = mc_B;

    OrientationPotentialParams reduced_params;
    MultiChannelPotentialParams enriched_params;
    enriched_params.sigma = 3.4;
    enriched_params.epsilon = 0.24;

    auto result = evaluate_tiered_interaction(bead_A, bead_B,
                                               ModelTier::ENRICHED,
                                               reduced_params, enriched_params);

    check(result.tier == ModelTier::ENRICHED, "Tier 2 path used");
    check(std::isfinite(result.E_total), "Tier 2 energy is finite");
    check(std::isfinite(result.E_steric), "Tier 2 steric contribution is finite");
    check(std::isfinite(result.E_electrostatic), "Tier 2 electrostatic contribution is finite");
}

// ============================================================================
// 10. Tier 2 fallback when data unavailable
// ============================================================================
static void test_tier2_fallback() {
    std::printf("\n=== Tier 2 Fallback to Tier 1 ===\n");
    using namespace coarse_grain;

    auto bead_A = make_bead(24, 0.0);
    auto bead_B = make_bead(24, 5.0);
    bead_A.orientation.normal = {0.0, 0.0, 1.0};
    bead_B.orientation.normal = {0.0, 0.0, 1.0};
    // No multi_channel data → should fall back to Tier 1

    OrientationPotentialParams reduced_params;
    reduced_params.sigma = 3.4;
    reduced_params.epsilon = 0.24;

    MultiChannelPotentialParams enriched_params;

    auto result = evaluate_tiered_interaction(bead_A, bead_B,
                                               ModelTier::ENRICHED,
                                               reduced_params, enriched_params);

    check(result.tier == ModelTier::REDUCED,
          "Tier 2 requested but no data → falls back to Tier 1");
    check(std::isfinite(result.E_total), "fallback energy is finite");
}

// ============================================================================
// 11. Custom thresholds
// ============================================================================
static void test_custom_thresholds() {
    std::printf("\n=== Custom Thresholds ===\n");
    using namespace coarse_grain;

    ModelSelectionConfig config;
    config.atom_count_threshold = 10;   // Lower threshold
    config.anisotropy_threshold = 0.3;  // More sensitive

    auto bead = make_bead(12);  // Would be Tier 1 at default, Tier 2 at 10
    auto result = select_model_tier(bead, config);

    check(result.tier == ModelTier::ENRICHED,
          "12 atoms with threshold=10 → Tier 2");

    // Anisotropy at lower threshold
    auto r2 = select_model_tier_from_metrics(6, 0.35, false, config);
    check(r2.tier == ModelTier::ENRICHED,
          "anisotropy=0.35 with threshold=0.3 → Tier 2");
    check(r2.promoted_by_anisotropy, "promoted by anisotropy at lower threshold");

    // Still below even custom threshold
    auto r3 = select_model_tier_from_metrics(6, 0.1, false, config);
    check(r3.tier == ModelTier::REDUCED,
          "anisotropy=0.1, 6 atoms with threshold=10 → Tier 1");
}

// ============================================================================
// 12. Benzene canonical case — Tier 1 expected
// ============================================================================
static void test_benzene_canonical() {
    std::printf("\n=== Benzene Canonical Case ===\n");
    using namespace coarse_grain;

    // Benzene: 12 atoms, moderately anisotropic but smoothly so
    auto bead = make_bead(12);

    // Benzene typical anisotropy ratio from SH analysis ~ 0.2-0.3
    SurfaceDescriptor sd;
    sd.coeffs[0] = 2.0;
    sd.coeffs[sh_index(2, 0)] = 0.5;  // Moderate ℓ=2 contribution
    bead.surface = sd;

    auto result = select_model_tier(bead);

    // Anisotropy ratio: (2^2 + 0.5^2 - 2^2) / (2^2 + 0.5^2) = 0.25/4.25 ≈ 0.059
    // This is well below the default threshold of 0.45 → Tier 1
    check(result.tier == ModelTier::REDUCED,
          "benzene (12 atoms, moderate anisotropy) → Tier 1");

    std::printf("  anisotropy_ratio = %.4f (threshold = 0.45)\n",
                result.anisotropy_ratio);
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::printf("================================================================\n");
    std::printf("  MODEL SELECTION TEST SUITE\n");
    std::printf("  Two-Tier Hierarchical Model Selection\n");
    std::printf("================================================================\n");

    test_small_molecule_tier1();
    test_large_molecule_tier2();
    test_metal_center_promotion();
    test_high_anisotropy_promotion();
    test_anisotropy_definitive();
    test_from_metrics();
    test_tier_names_and_reasons();
    test_tiered_interaction_tier1();
    test_tiered_interaction_tier2();
    test_tier2_fallback();
    test_custom_thresholds();
    test_benzene_canonical();

    std::printf("\n================================================================\n");
    std::printf("  RESULTS: %d passed, %d failed, %d total\n", g_pass, g_fail, g_pass + g_fail);
    std::printf("================================================================\n");

    return g_fail > 0 ? 1 : 0;
}
