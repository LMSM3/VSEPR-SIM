#pragma once
/**
 * radiation_interaction.hpp — Deterministic Radiation Interaction Descriptors
 *
 * Computes radiation shielding and displacement damage descriptors from
 * MetalRecord data and bead system geometry. NO Monte Carlo, NO DFT.
 *
 * Physical basis:
 *   - Beer–Lambert attenuation: I(x) = I₀ · exp(−μ·ρ·x)
 *   - Kinchin–Pease displacement model: N_d = 0.8·T_dam / (2·E_d)
 *   - Z-dependent form factor: f(q) ∝ Z · exp(−B·q²)
 *
 * Application contexts (from user research direction):
 *   - Radiation hardening (satellite/nuclear shielding)
 *   - High-Z armor / X-ray attenuation layers
 *   - Displacement-resilient material screening
 *   - AuNP sacrificial layer analysis
 *
 * All quantities are deterministic functions of Z, ρ, E_d, and geometry.
 *
 * References:
 *   - NIST XCOM photon cross-section database
 *   - Kinchin & Pease, Rep. Prog. Phys. 18, 1 (1955)
 *   - Norgett, Robinson & Torrens (NRT), Nucl. Eng. Des. 33, 50 (1975)
 *   - Hubbell & Seltzer, NIST Tables of X-ray Attenuation (2004)
 */

#include "coarse_grain/metals/metal_registry.hpp"
#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/bead_system.hpp"
#include <cmath>
#include <string>
#include <vector>

namespace coarse_grain {
namespace metals {

// ============================================================================
// Radiation Attenuation Descriptors
// ============================================================================

/**
 * AttenuationResult — X-ray/gamma shielding effectiveness for a slab.
 */
struct AttenuationResult {
    std::string material;
    double photon_energy_keV{};     ///< Incident photon energy
    double mu_mass_cm2g{};          ///< Mass attenuation coefficient (cm²/g)
    double density_g_cm3{};         ///< Material density (g/cm³)
    double mu_linear_cm{};          ///< Linear attenuation μ = (μ/ρ)·ρ (1/cm)
    double thickness_cm{};          ///< Slab thickness
    double transmission{};          ///< I/I₀ = exp(−μ·ρ·x)
    double attenuation_pct{};       ///< (1 − transmission) × 100%
    double half_value_layer_cm{};   ///< ln(2)/μ_linear
    double tenth_value_layer_cm{};  ///< ln(10)/μ_linear
    bool   above_k_edge{};          ///< Photon energy > K-edge
};

/**
 * Compute X-ray attenuation through a slab of given metal and thickness.
 *
 * Uses Beer–Lambert law: I/I₀ = exp(−(μ/ρ)·ρ·x)
 *
 * @param metal           MetalRecord with mu_mass_100keV_cm2g
 * @param density_g_cm3   Bulk density (g/cm³) — approximated from atomic data
 * @param thickness_cm    Slab thickness (cm)
 * @param photon_keV      Photon energy (keV), default = 100 keV
 */
inline AttenuationResult compute_attenuation(
    const MetalRecord& metal,
    double density_g_cm3,
    double thickness_cm,
    double photon_keV = 100.0)
{
    AttenuationResult r;
    r.material       = metal.symbol;
    r.photon_energy_keV = photon_keV;
    r.density_g_cm3  = density_g_cm3;
    r.thickness_cm   = thickness_cm;
    r.above_k_edge   = (photon_keV > metal.k_edge_keV);

    // Scale μ/ρ from 100 keV reference using power-law approximation
    // μ/ρ ∝ Z^(4..5) / E^3 for photoelectric, but near 100 keV we use
    // a simple E^-3 scaling from the stored reference value
    double E_ref = 100.0;
    if (metal.mu_mass_100keV_cm2g > 0 && photon_keV > 0) {
        double scale = std::pow(E_ref / photon_keV, 2.8); // approximate exponent
        r.mu_mass_cm2g = metal.mu_mass_100keV_cm2g * scale;
    }

    r.mu_linear_cm = r.mu_mass_cm2g * density_g_cm3;

    if (r.mu_linear_cm > 0) {
        r.transmission           = std::exp(-r.mu_linear_cm * thickness_cm);
        r.attenuation_pct        = (1.0 - r.transmission) * 100.0;
        r.half_value_layer_cm    = std::log(2.0) / r.mu_linear_cm;
        r.tenth_value_layer_cm   = std::log(10.0) / r.mu_linear_cm;
    }

    return r;
}

// ============================================================================
// Displacement Damage (Kinchin–Pease / NRT model)
// ============================================================================

/**
 * DisplacementResult — radiation damage estimate for a bead cluster.
 */
struct DisplacementResult {
    std::string material;
    double E_d_eV{};                ///< Displacement threshold energy (eV)
    double T_damage_eV{};           ///< Damage energy transferred to PKA (eV)
    double N_displacements{};       ///< Kinchin–Pease: 0.8 · T_dam / (2·E_d)
    double dpa_per_fluence{};       ///< Displacements per atom per unit fluence
    double hardness_factor{};       ///< E_d / 25 eV (relative to standard Cu)
    uint32_t n_beads{};             ///< Beads in the irradiated cluster
    double total_displaced_beads{}; ///< N_d × n_beads (max damage estimate)
    bool   displacement_resistant{};///< E_d > 40 eV (refractory threshold)
};

/**
 * Compute Kinchin–Pease displacement cascade estimate.
 *
 * @param metal       MetalRecord
 * @param n_beads     Number of beads in cluster
 * @param T_damage_eV Damage energy transferred to primary knock-on atom (eV)
 */
inline DisplacementResult compute_displacement(
    const MetalRecord& metal,
    uint32_t n_beads,
    double T_damage_eV = 1000.0)
{
    DisplacementResult r;
    r.material      = metal.symbol;
    r.E_d_eV        = metal.displacement_energy_ev;
    r.T_damage_eV   = T_damage_eV;
    r.n_beads       = n_beads;

    if (r.E_d_eV > 0) {
        // NRT/Kinchin–Pease: N_d = 0.8 · T_dam / (2 · E_d)
        r.N_displacements = 0.8 * T_damage_eV / (2.0 * r.E_d_eV);
        if (r.N_displacements < 1.0 && T_damage_eV > r.E_d_eV)
            r.N_displacements = 1.0;
        r.dpa_per_fluence = r.N_displacements / std::max((double)n_beads, 1.0);
    }

    r.hardness_factor = r.E_d_eV / 25.0; // normalized to Cu-like standard
    r.total_displaced_beads = r.N_displacements * n_beads;
    r.displacement_resistant = (r.E_d_eV > 40.0);

    return r;
}

// ============================================================================
// Composite Shielding Score
// ============================================================================

/**
 * ShieldingScore — unified shielding quality metric.
 *
 * Combines attenuation (high Z → high μ) with displacement resistance
 * (high E_d → fewer Frenkel pairs). A material that both blocks radiation
 * AND resists damage is ideal for satellite/nuclear shielding.
 *
 * Score ∈ [0, 1]: higher = better shielding candidate.
 */
struct ShieldingScore {
    std::string material;
    double attenuation_score{};     ///< μ/ρ normalized to Au reference
    double displacement_score{};    ///< E_d / E_d_max normalized
    double combined_score{};        ///< weighted: 0.6·atten + 0.4·displ
    std::string grade;              ///< "A" / "B" / "C" / "D"
};

inline ShieldingScore compute_shielding_score(
    const MetalRecord& metal,
    double mu_ref_au = 5.16,      // Au at 100 keV as normalization
    double Ed_max    = 90.0)      // W displacement energy as ceiling
{
    ShieldingScore s;
    s.material = metal.symbol;
    s.attenuation_score  = std::min(metal.mu_mass_100keV_cm2g / mu_ref_au, 1.0);
    s.displacement_score = std::min(metal.displacement_energy_ev / Ed_max, 1.0);
    s.combined_score     = 0.6 * s.attenuation_score + 0.4 * s.displacement_score;

    if      (s.combined_score >= 0.75) s.grade = "A";
    else if (s.combined_score >= 0.50) s.grade = "B";
    else if (s.combined_score >= 0.25) s.grade = "C";
    else                               s.grade = "D";

    return s;
}

// ============================================================================
// Convenience: compute all shielding scores
// ============================================================================

inline std::vector<ShieldingScore>
rank_shielding(const std::vector<MetalRecord>& registry)
{
    std::vector<ShieldingScore> scores;
    for (const auto& m : registry)
        scores.push_back(compute_shielding_score(m));

    // Sort descending by combined score
    std::sort(scores.begin(), scores.end(),
              [](const ShieldingScore& a, const ShieldingScore& b) {
                  return a.combined_score > b.combined_score;
              });
    return scores;
}

} // namespace metals
} // namespace coarse_grain
