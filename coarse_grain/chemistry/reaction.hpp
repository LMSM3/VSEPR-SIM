#pragma once
/**
 * reaction.hpp — Chemical Reaction Framework
 *
 * Models complete chemical reactions with:
 *   - Reactants, products, by-products, catalysts
 *   - Stoichiometric coefficients and mass balance
 *   - Thermodynamics (ΔH, ΔG, ΔS, activation energy)
 *   - Reaction mechanism description
 *   - Bonding analysis (bonds broken / bonds formed)
 *   - Thermal state tracking (temperature, phase changes)
 *
 * Supports three reaction archetypes:
 *   1. Organic synthesis with inorganic reagent (e.g. benzene nitration)
 *   2. Precipitation / sludge formation (e.g. thorium oxalate)
 *   3. Thermal decomposition (e.g. copper nitrate → CuO + gases)
 *
 * Anti-black-box: every thermodynamic value, bond change, and mass
 * balance is explicitly inspectable and traceable.
 *
 * Reference: copilot-instructions.md §2, §5, §7
 */

#include "coarse_grain/chemistry/species.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace coarse_grain {
namespace chemistry {

// ============================================================================
// Reaction Role
// ============================================================================

enum class SpeciesRole : uint8_t {
    REACTANT,
    PRODUCT,
    BYPRODUCT,
    CATALYST,
    SOLVENT
};

inline const char* role_label(SpeciesRole r) {
    switch (r) {
        case SpeciesRole::REACTANT:  return "Reactant";
        case SpeciesRole::PRODUCT:   return "Product";
        case SpeciesRole::BYPRODUCT: return "By-product";
        case SpeciesRole::CATALYST:  return "Catalyst";
        case SpeciesRole::SOLVENT:   return "Solvent";
        default:                     return "";
    }
}

// ============================================================================
// Reaction Entry — species + stoichiometric coefficient + role
// ============================================================================

struct ReactionEntry {
    ChemicalSpecies species;
    double          coefficient{1.0};  // Stoichiometric coefficient
    SpeciesRole     role{SpeciesRole::REACTANT};
};

// ============================================================================
// Bond Change — bonds broken and formed during reaction
// ============================================================================

struct BondChange {
    std::string description;       // "C–H bond broken in benzene"
    std::string atom_pair;         // "C–N" or "O–H"
    uint8_t     old_order{};       // Bond order before (0 = no bond)
    uint8_t     new_order{};       // Bond order after  (0 = no bond)
    BondType    bond_type{BondType::COVALENT};
    double      energy_change{};   // kcal/mol (positive = endothermic)
};

// ============================================================================
// Reaction Mechanism Step
// ============================================================================

struct MechanismStep {
    uint32_t    step_number{};
    std::string description;
    std::string species_involved;  // e.g. "NO2+ electrophile attacks benzene"
    double      activation_energy{}; // kcal/mol for this step
    double      delta_H{};         // Enthalpy change for this step
};

// ============================================================================
// Thermal State of Reaction System
// ============================================================================

struct ReactionThermalState {
    double temperature_K{298.15};      // Current temperature
    double pressure_atm{1.0};          // Current pressure
    double total_enthalpy{};           // Total system enthalpy
    double total_entropy{};            // Total system entropy
    double total_gibbs{};              // Total Gibbs energy

    // Thermal transport
    double thermal_conductivity{};     // W/(m·K) — mixture average
    double heat_capacity_mixture{};    // J/(mol·K)
    double viscosity_mixture{};        // Pa·s

    // Temperature tracking
    std::vector<double> T_history;     // Temperature vs step
    std::vector<double> H_history;     // Enthalpy vs step
};

// ============================================================================
// Mass Balance Report
// ============================================================================

struct MassBalance {
    std::map<std::string, double> reactant_elements;   // Element → total moles
    std::map<std::string, double> product_elements;
    std::map<std::string, double> imbalance;           // Should be ~0

    double total_reactant_mass{};      // g/mol
    double total_product_mass{};       // g/mol
    double mass_error{};               // |reactant - product| / reactant
    bool   balanced{};                 // Mass conserved within tolerance

    static constexpr double BALANCE_TOL = 1.0e-6;
};

// ============================================================================
// Chemical Reaction
// ============================================================================

/**
 * ChemicalReaction — complete representation of a chemical reaction.
 *
 * Supports:
 *   - Full stoichiometry with mass balance verification
 *   - Thermodynamic analysis (ΔH, ΔG, ΔS)
 *   - Bond change tracking (broken/formed)
 *   - Multi-step mechanism description
 *   - Thermal state evolution
 *   - By-product identification
 */
struct ChemicalReaction {
    // --- Identity ---
    std::string name;                  // "Nitration of benzene"
    std::string reaction_type;         // "Electrophilic aromatic substitution"
    std::string equation;              // "C6H6 + HNO3 → C6H5NO2 + H2O"
    std::string conditions;            // "H2SO4 catalyst, 50–60 °C"

    // --- Species ---
    std::vector<ReactionEntry> entries;

    // --- Thermodynamics (at standard conditions unless noted) ---
    double delta_H_rxn{};              // kcal/mol (enthalpy of reaction)
    double delta_G_rxn{};              // kcal/mol (Gibbs energy of reaction)
    double delta_S_rxn{};              // cal/(mol·K) (entropy of reaction)
    double activation_energy{};        // kcal/mol (Ea)
    double equilibrium_constant{};     // K_eq at 298.15 K

    // --- Bond changes ---
    std::vector<BondChange> bond_changes;

    // --- Mechanism ---
    std::vector<MechanismStep> mechanism;

    // --- Thermal state ---
    ReactionThermalState thermal;

    // --- Mass balance ---
    MassBalance mass_balance;

    // ================================================================
    // Accessors
    // ================================================================

    std::vector<ReactionEntry> reactants() const {
        std::vector<ReactionEntry> r;
        for (auto& e : entries)
            if (e.role == SpeciesRole::REACTANT) r.push_back(e);
        return r;
    }

    std::vector<ReactionEntry> products() const {
        std::vector<ReactionEntry> r;
        for (auto& e : entries)
            if (e.role == SpeciesRole::PRODUCT) r.push_back(e);
        return r;
    }

    std::vector<ReactionEntry> byproducts() const {
        std::vector<ReactionEntry> r;
        for (auto& e : entries)
            if (e.role == SpeciesRole::BYPRODUCT) r.push_back(e);
        return r;
    }

    std::vector<ReactionEntry> catalysts() const {
        std::vector<ReactionEntry> r;
        for (auto& e : entries)
            if (e.role == SpeciesRole::CATALYST) r.push_back(e);
        return r;
    }

    // ================================================================
    // Mass Balance Verification
    // ================================================================

    MassBalance verify_mass_balance() const {
        MassBalance mb;

        // Accumulate elements from reactants
        for (auto& e : entries) {
            if (e.role != SpeciesRole::REACTANT) continue;
            for (auto& [elem, count] : e.species.element_count) {
                mb.reactant_elements[elem] += e.coefficient * count;
            }
            mb.total_reactant_mass += e.coefficient * e.species.molecular_weight;
        }

        // Accumulate elements from products + byproducts
        for (auto& e : entries) {
            if (e.role != SpeciesRole::PRODUCT &&
                e.role != SpeciesRole::BYPRODUCT) continue;
            for (auto& [elem, count] : e.species.element_count) {
                mb.product_elements[elem] += e.coefficient * count;
            }
            mb.total_product_mass += e.coefficient * e.species.molecular_weight;
        }

        // Check balance
        mb.balanced = true;
        for (auto& [elem, r_count] : mb.reactant_elements) {
            double p_count = 0.0;
            auto it = mb.product_elements.find(elem);
            if (it != mb.product_elements.end()) p_count = it->second;
            double diff = std::abs(r_count - p_count);
            mb.imbalance[elem] = diff;
            if (diff > MassBalance::BALANCE_TOL) mb.balanced = false;
        }

        mb.mass_error = (mb.total_reactant_mass > 0)
            ? std::abs(mb.total_reactant_mass - mb.total_product_mass) / mb.total_reactant_mass
            : 0.0;
        if (mb.mass_error > MassBalance::BALANCE_TOL) mb.balanced = false;

        return mb;
    }

    // ================================================================
    // Thermodynamic Calculations
    // ================================================================

    /**
     * Compute ΔH_rxn from standard enthalpies of formation.
     * ΔH_rxn = Σ(ΔHf products) − Σ(ΔHf reactants)
     */
    double compute_delta_H() const {
        double H_prod = 0.0, H_react = 0.0;
        for (auto& e : entries) {
            if (e.role == SpeciesRole::PRODUCT || e.role == SpeciesRole::BYPRODUCT)
                H_prod += e.coefficient * e.species.delta_Hf;
            else if (e.role == SpeciesRole::REACTANT)
                H_react += e.coefficient * e.species.delta_Hf;
        }
        return H_prod - H_react;
    }

    /**
     * Compute ΔG_rxn from standard Gibbs energies.
     */
    double compute_delta_G() const {
        double G_prod = 0.0, G_react = 0.0;
        for (auto& e : entries) {
            if (e.role == SpeciesRole::PRODUCT || e.role == SpeciesRole::BYPRODUCT)
                G_prod += e.coefficient * e.species.delta_Gf;
            else if (e.role == SpeciesRole::REACTANT)
                G_react += e.coefficient * e.species.delta_Gf;
        }
        return G_prod - G_react;
    }

    /**
     * Compute ΔS_rxn = (ΔH − ΔG) / T
     */
    double compute_delta_S(double T_K = 298.15) const {
        return (delta_H_rxn - delta_G_rxn) / T_K * 1000.0; // cal/(mol·K)
    }

    /**
     * Equilibrium constant from ΔG: K = exp(−ΔG / RT)
     */
    double compute_K_eq(double T_K = 298.15) const {
        return std::exp(-delta_G_rxn / (R_GAS * T_K));
    }

    /**
     * Rate constant estimate (Arrhenius): k = A·exp(−Ea/RT)
     * Uses typical pre-exponential A ~ 10^13 s^-1
     */
    double arrhenius_rate(double T_K = 298.15, double A = 1.0e13) const {
        return A * std::exp(-activation_energy / (R_GAS * T_K));
    }
};

} // namespace chemistry
} // namespace coarse_grain
