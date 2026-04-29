#pragma once
/**
 * energetic_signature.hpp — Metal Combustion & Energetic Material Descriptors
 *
 * Deterministic computation of energetic signatures for metallic fuels.
 * NO DFT, NO Monte Carlo — all values derived from MetalRecord thermochemistry.
 *
 * Physical basis:
 *   - Heat of combustion ΔH_comb from oxide formation enthalpies
 *   - Volumetric energy density from ΔH_comb × ρ_bulk
 *   - Ignition sensitivity from T_ign / T_melt ratio
 *   - Surface-to-volume scaling for nanoparticle enhancement
 *
 * Application contexts (from user research direction):
 *   - Energetic materials: metal powder fuels (Al, Fe, Ti, Zr, B/Al)
 *   - Reactive mixtures: AuNP nano-antenna triggered detonation
 *   - Combustion signature prediction: flame colour from electronic transitions
 *   - Energy deposition tuning: thermal conductivity × surface area
 *
 * The flame colour image (methane/iron/aluminium/boron-aluminium/zirconium)
 * maps directly to the emission properties computed here:
 *   - Methane: CH₄ + 2O₂ → CO₂ + 2H₂O (blue — C₂/CH band emission)
 *   - Iron: 4Fe + 3O₂ → 2Fe₂O₃ (orange-red — FeO* band 580–620 nm)
 *   - Aluminium: 4Al + 3O₂ → 2Al₂O₃ (white — AlO band 484 nm + continuum)
 *   - Boron/Al: mixture → green (BO₂ band 518 nm dominant)
 *   - Zirconium: Zr + O₂ → ZrO₂ (yellow sparks — ZrO band + continuum)
 *
 * References:
 *   - Dreizin, Prog. Energy Combust. Sci. 35, 141 (2009)
 *   - Yetter, Risha & Son, Proc. Combust. Inst. 32, 1819 (2009)
 *   - CRC Handbook, 105th ed. (formation enthalpies)
 */

#include "coarse_grain/metals/metal_registry.hpp"
#include <cmath>
#include <string>
#include <vector>

namespace coarse_grain {
namespace metals {

// ============================================================================
// Energetic Signature Record
// ============================================================================

struct EnergeticSignature {
    // Identity
    std::string material;
    std::string primary_oxide;
    std::string flame_colour;

    // Thermochemistry
    double heat_of_combustion_kJ_g{};    ///< ΔH_comb (kJ/g)
    double heat_of_combustion_kJ_mol{};  ///< ΔH_comb (kJ/mol of metal)
    double oxide_enthalpy_eV_atom{};     ///< ΔH_f oxide per metal atom (eV)
    double energy_density_MJ_L{};        ///< Volumetric: ΔH × ρ (MJ/L)

    // Ignition characteristics
    double ignition_temperature_K{};     ///< T_ign for powder
    double melting_point_K{};            ///< T_melt
    double ign_to_melt_ratio{};          ///< T_ign / T_melt — lower = easier ignition
    double thermal_conductivity{};       ///< κ (W/m·K)

    // Nanoparticle enhancement factors
    double surface_area_factor{};        ///< SA/V relative to 1 mm particle
    double nano_enhancement{};           ///< Combined SA × κ enhancement proxy
    double reactivity_class{};           ///< 0–1 normalized reactivity

    // Classification
    std::string fuel_grade;              ///< "High-Energy" / "Moderate" / "Inert"
    bool is_viable_fuel{};               ///< ΔH_comb > 2 kJ/g AND T_ign < T_melt × 1.5
};

// ============================================================================
// Bulk density estimate from lattice constant and mass
// ============================================================================

/**
 * Estimate bulk density (g/cm³) from lattice data.
 *
 * For FCC: ρ = 4M / (N_A · a³)  (4 atoms per unit cell)
 * For BCC: ρ = 2M / (N_A · a³)  (2 atoms per unit cell)
 * For HCP: ρ = 6M / (N_A · √2 · a³)  (6 atoms per HCP unit cell, c/a ideal)
 */
inline double estimate_bulk_density(const MetalRecord& m) {
    constexpr double N_A = 6.02214076e23;
    double a_cm = m.lattice_constant_ang * 1.0e-8; // Å → cm
    if (a_cm <= 0) return 0.0;
    double V_cell = a_cm * a_cm * a_cm; // cm³

    int n_atoms = 4; // FCC default
    switch (m.structure) {
        case CrystalStructure::FCC: n_atoms = 4; break;
        case CrystalStructure::BCC: n_atoms = 2; break;
        case CrystalStructure::HCP: n_atoms = 6; V_cell *= std::sqrt(2.0); break;
    }

    return n_atoms * m.atomic_mass_amu / (N_A * V_cell);
}

// ============================================================================
// Compute energetic signature
// ============================================================================

inline EnergeticSignature compute_energetic_signature(
    const MetalRecord& metal,
    double particle_diameter_um = 0.001) // default 1 nm nanoparticle
{
    EnergeticSignature sig;
    sig.material              = metal.symbol;
    sig.primary_oxide         = metal.primary_oxide;
    sig.flame_colour          = metal.flame_colour;
    sig.heat_of_combustion_kJ_g = metal.heat_of_combustion_kJ_g;
    sig.oxide_enthalpy_eV_atom  = metal.oxide_formation_enthalpy_eV;
    sig.ignition_temperature_K  = metal.ignition_temperature_K;
    sig.melting_point_K         = metal.melting_point_K;
    sig.thermal_conductivity    = metal.thermal_conductivity_W_mK;

    // kJ/g → kJ/mol
    sig.heat_of_combustion_kJ_mol = metal.heat_of_combustion_kJ_g * metal.atomic_mass_amu;

    // Volumetric energy density (MJ/L)
    double rho = estimate_bulk_density(metal);
    sig.energy_density_MJ_L = metal.heat_of_combustion_kJ_g * rho; // kJ/g × g/cm³ = kJ/cm³ = MJ/L

    // Ignition ratio
    if (metal.melting_point_K > 0)
        sig.ign_to_melt_ratio = metal.ignition_temperature_K / metal.melting_point_K;

    // Nanoparticle surface area enhancement
    // SA/V = 6/d for a sphere; relative to 1mm (1000 μm) reference particle
    double d_ref = 1000.0; // μm
    sig.surface_area_factor = (particle_diameter_um > 0)
                            ? d_ref / particle_diameter_um
                            : 1.0;

    // Combined nano-enhancement: high SA + high κ = better energy deposition
    double kappa_norm = metal.thermal_conductivity_W_mK / 400.0; // normalized to Cu
    sig.nano_enhancement = std::sqrt(sig.surface_area_factor * kappa_norm);

    // Reactivity class: normalized composite
    // High ΔH, low T_ign/T_melt, high nano_enhancement → high reactivity
    double dH_norm = std::min(metal.heat_of_combustion_kJ_g / 31.0, 1.0); // Al is ceiling
    double ign_norm = (sig.ign_to_melt_ratio > 0)
                    ? std::max(1.0 - sig.ign_to_melt_ratio, 0.0)
                    : 0.0;
    sig.reactivity_class = 0.5 * dH_norm + 0.3 * ign_norm + 0.2 * std::min(kappa_norm, 1.0);

    // Classification
    if (metal.heat_of_combustion_kJ_g >= 15.0) {
        sig.fuel_grade = "High-Energy";
    } else if (metal.heat_of_combustion_kJ_g >= 3.0) {
        sig.fuel_grade = "Moderate";
    } else if (metal.heat_of_combustion_kJ_g > 0.0) {
        sig.fuel_grade = "Low";
    } else {
        sig.fuel_grade = "Inert (noble)";
    }

    sig.is_viable_fuel = (metal.heat_of_combustion_kJ_g > 2.0) &&
                         (metal.ignition_temperature_K > 0) &&
                         (sig.ign_to_melt_ratio < 1.5);

    return sig;
}

// ============================================================================
// Convenience: all metal signatures
// ============================================================================

inline std::vector<EnergeticSignature>
compute_all_energetic_signatures(
    const std::vector<MetalRecord>& registry,
    double particle_diameter_um = 0.001)
{
    std::vector<EnergeticSignature> sigs;
    for (const auto& m : registry)
        sigs.push_back(compute_energetic_signature(m, particle_diameter_um));
    return sigs;
}

// ============================================================================
// Terminal formatter
// ============================================================================

inline std::string format_energetic_table_terminal(
    const std::vector<EnergeticSignature>& sigs)
{
    const char* BOLD = "\033[1m"; const char* CYAN = "\033[36m";
    const char* GREEN = "\033[32m"; const char* YELLOW = "\033[33m";
    const char* RED = "\033[31m"; const char* RESET = "\033[0m";

    std::string out;
    out += BOLD;
    out += "\n  Metal Combustion Signatures (Energetic Fuel Analysis)\n";
    out += RESET;
    out += "  "; out += std::string(100, '-'); out += "\n";

    char hdr[256];
    std::snprintf(hdr, sizeof(hdr),
        "  %-5s %-9s %-8s %-10s %-8s %-8s %-8s %-16s %s\n",
        "Metal", "ΔH kJ/g", "MJ/L", "T_ign (K)", "T_ign/Tm", "Flame", "Grade",
        "Oxide", "Reactivity");
    out += hdr;
    out += "  "; out += std::string(100, '-'); out += "\n";

    for (const auto& s : sigs) {
        const char* grade_col = RESET;
        if (s.fuel_grade == "High-Energy") grade_col = RED;
        else if (s.fuel_grade == "Moderate") grade_col = YELLOW;
        else if (s.fuel_grade == "Inert (noble)") grade_col = CYAN;
        else grade_col = GREEN;

        char line[256];
        std::snprintf(line, sizeof(line),
            "  %-5s %7.2f  %7.2f  %8.0f  %7.2f  %-14s %s%-12s%s %-8s  %.3f\n",
            s.material.c_str(),
            s.heat_of_combustion_kJ_g,
            s.energy_density_MJ_L,
            s.ignition_temperature_K,
            s.ign_to_melt_ratio,
            s.flame_colour.c_str(),
            grade_col, s.fuel_grade.c_str(), RESET,
            s.primary_oxide.c_str(),
            s.reactivity_class);
        out += line;
    }
    out += "\n";
    return out;
}

// ============================================================================
// Markdown formatter
// ============================================================================

inline std::string format_energetic_table_markdown(
    const std::vector<EnergeticSignature>& sigs)
{
    std::string md = "## Metal Combustion Signatures\n\n";
    md += "| Metal | ΔH (kJ/g) | MJ/L | T_ign (K) | T_ign/T_m | Flame | Grade | Oxide | Reactivity |\n";
    md += "|---|---|---|---|---|---|---|---|---|\n";
    for (const auto& s : sigs) {
        char row[256];
        std::snprintf(row, sizeof(row),
            "| %s | %.2f | %.2f | %.0f | %.2f | %s | %s | %s | %.3f |\n",
            s.material.c_str(),
            s.heat_of_combustion_kJ_g,
            s.energy_density_MJ_L,
            s.ignition_temperature_K,
            s.ign_to_melt_ratio,
            s.flame_colour.c_str(),
            s.fuel_grade.c_str(),
            s.primary_oxide.c_str(),
            s.reactivity_class);
        md += row;
    }
    md += "\n";
    return md;
}

} // namespace metals
} // namespace coarse_grain
