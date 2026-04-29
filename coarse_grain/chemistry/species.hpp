#pragma once
/**
 * species.hpp — Chemical Species Definition
 *
 * Defines individual atomic and molecular species for the universal
 * chemical reaction engine.  Each species carries full thermodynamic,
 * structural, and atomistic data needed for simulation, bonding
 * prediction, and report generation.
 *
 * Anti-black-box: every property is inspectable and sourced.
 * Deterministic: identical inputs → identical species tables.
 *
 * Reference: copilot-instructions.md §2, §5
 */

#include "atomistic/core/state.hpp"
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace coarse_grain {
namespace chemistry {

// ============================================================================
// Physical Constants
// ============================================================================

static constexpr double R_GAS      = 1.9872036e-3;  // kcal/(mol·K)
static constexpr double KB_EV      = 8.617333e-5;   // eV/K
static constexpr double KCAL_PER_EV = 23.0605;       // kcal/mol per eV
static constexpr double AVOGADRO   = 6.02214076e23;

// ============================================================================
// Phase State
// ============================================================================

enum class Phase : uint8_t {
    SOLID,
    LIQUID,
    GAS,
    AQUEOUS,      // Dissolved in water
    SLUDGE,       // Precipitated / colloidal solid
    ADSORBED,     // Surface-bound
    UNKNOWN
};

inline const char* phase_label(Phase p) {
    switch (p) {
        case Phase::SOLID:    return "(s)";
        case Phase::LIQUID:   return "(l)";
        case Phase::GAS:      return "(g)";
        case Phase::AQUEOUS:  return "(aq)";
        case Phase::SLUDGE:   return "(sludge)";
        case Phase::ADSORBED: return "(ads)";
        default:              return "";
    }
}

// ============================================================================
// Bonding Type Classification
// ============================================================================

enum class BondType : uint8_t {
    COVALENT,
    IONIC,
    METALLIC,
    HYDROGEN,
    VAN_DER_WAALS,
    COORDINATION,
    ELECTROPHILIC,    // Electrophilic aromatic substitution
    NONE
};

inline const char* bond_type_label(BondType b) {
    switch (b) {
        case BondType::COVALENT:       return "Covalent";
        case BondType::IONIC:          return "Ionic";
        case BondType::METALLIC:       return "Metallic";
        case BondType::HYDROGEN:       return "Hydrogen";
        case BondType::VAN_DER_WAALS:  return "van der Waals";
        case BondType::COORDINATION:   return "Coordination";
        case BondType::ELECTROPHILIC:  return "Electrophilic";
        default:                       return "None";
    }
}

// ============================================================================
// Atom Entry (within a species)
// ============================================================================

struct AtomEntry {
    uint8_t Z{};               // Atomic number
    std::string symbol;        // Element symbol
    double mass{};             // Atomic mass (amu)
    double electronegativity{}; // Pauling scale
    double covalent_radius{};  // Å
    double vdw_radius{};       // Å
    atomistic::Vec3 position{}; // Position within molecule (Å)
    int formal_charge{};       // Formal charge on this atom
};

// ============================================================================
// Bond Entry (within a species)
// ============================================================================

struct BondEntry {
    uint32_t atom_i{};         // Index into atoms vector
    uint32_t atom_j{};
    uint8_t  order{1};         // Bond order (1=single, 2=double, 3=triple)
    BondType type{BondType::COVALENT};
    double   length{};         // Å
    double   energy{};         // kcal/mol (dissociation energy)
};

// ============================================================================
// Chemical Species
// ============================================================================

/**
 * ChemicalSpecies — a fully described molecular or atomic species.
 *
 * Carries structural, thermodynamic, and atomistic data for
 * simulation, bonding prediction, and engineering report output.
 */
struct ChemicalSpecies {
    // --- Identity ---
    std::string name;              // Human name: "Benzene", "Nitric acid"
    std::string formula;           // Molecular formula: "C6H6", "HNO3"
    std::string smiles;            // SMILES (if applicable)
    uint32_t    species_id{};      // Unique ID within reaction

    // --- Composition ---
    std::vector<AtomEntry> atoms;
    std::vector<BondEntry> bonds;
    std::map<std::string, uint32_t> element_count;  // {"C":6, "H":6} etc.

    // --- Phase ---
    Phase phase{Phase::UNKNOWN};

    // --- Thermodynamic data (at 298.15 K, 1 atm unless noted) ---
    double molecular_weight{};     // g/mol
    double density{};              // g/cm³
    double melting_point_K{};      // K
    double boiling_point_K{};      // K
    double delta_Hf{};             // Standard enthalpy of formation (kcal/mol)
    double delta_Gf{};             // Standard Gibbs energy of formation (kcal/mol)
    double S_std{};                // Standard entropy (cal/(mol·K))
    double Cp{};                   // Heat capacity at const P (cal/(mol·K))

    // --- LJ parameters (for CG representation) ---
    double lj_sigma{};             // Å
    double lj_epsilon{};           // kcal/mol

    // --- Thermal properties ---
    double thermal_conductivity{}; // W/(m·K)
    double specific_heat{};        // J/(g·K)
    double viscosity{};            // Pa·s

    // --- Electrical ---
    double dielectric_constant{};
    double dipole_moment{};        // Debye

    // --- Derived ---
    double total_mass() const {
        double m = 0.0;
        for (auto& a : atoms) m += a.mass;
        return m;
    }

    uint32_t atom_count() const {
        return static_cast<uint32_t>(atoms.size());
    }

    double max_electronegativity() const {
        double mx = 0.0;
        for (auto& a : atoms)
            if (a.electronegativity > mx) mx = a.electronegativity;
        return mx;
    }

    double mean_electronegativity() const {
        if (atoms.empty()) return 0.0;
        double s = 0.0;
        for (auto& a : atoms) s += a.electronegativity;
        return s / atoms.size();
    }
};

// ============================================================================
// Species Builder Helpers
// ============================================================================

inline AtomEntry make_atom(uint8_t Z, const std::string& sym, double mass,
                           double en, double r_cov, double r_vdw,
                           double x = 0, double y = 0, double z = 0,
                           int charge = 0)
{
    AtomEntry a;
    a.Z = Z;
    a.symbol = sym;
    a.mass = mass;
    a.electronegativity = en;
    a.covalent_radius = r_cov;
    a.vdw_radius = r_vdw;
    a.position = {x, y, z};
    a.formal_charge = charge;
    return a;
}

inline BondEntry make_bond(uint32_t i, uint32_t j, uint8_t order,
                           BondType type, double length, double energy)
{
    BondEntry b;
    b.atom_i = i;
    b.atom_j = j;
    b.order = order;
    b.type = type;
    b.length = length;
    b.energy = energy;
    return b;
}

} // namespace chemistry
} // namespace coarse_grain
