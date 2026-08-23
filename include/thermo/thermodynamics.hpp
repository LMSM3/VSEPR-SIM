/**
 * thermodynamics.hpp
 * ==================
 * Gibbs free energy, enthalpy, entropy calculations
 * Reference molecule database with experimental data
 */

#pragma once

#include "sim/molecule.hpp"
#include <string>
#include <unordered_map>
#include <optional>

namespace vsepr {
namespace thermo {

// ============================================================================
// Thermodynamic Properties
// ============================================================================

struct ThermodynamicState {
    double temperature_K = 298.15;      // Standard temperature (25°C)
    double pressure_atm = 1.0;          // Standard pressure (1 atm)
    
    bool is_standard() const {
        return std::abs(temperature_K - 298.15) < 0.01 && 
               std::abs(pressure_atm - 1.0) < 0.01;
    }
};

struct ThermoData {
    // Standard formation values (298.15 K, 1 atm)
    double H_f;          // Enthalpy of formation (kcal/mol)
    double S;            // Entropy (cal/mol·K)
    double G_f;          // Gibbs free energy of formation (kcal/mol)
    double Cp;           // Heat capacity at constant pressure (cal/mol·K)
    
    // Molecular properties
    double dipole_moment_D;       // Debye
    double polarizability_A3;     // Å³
    
    // Phase
    enum Phase { GAS, LIQUID, SOLID, AQUEOUS } phase = GAS;
    
    // Calculate G at different temperature
    double gibbs_at_temp(double T_K) const;
};

// ============================================================================
// Reference Molecule Database
// ============================================================================

struct ReferenceMolecule {
    std::string formula;
    std::string name;
    std::string smiles;              // SMILES notation
    
    ThermoData thermo;
    
    // Structure
    int n_atoms;
    int n_bonds;
    std::string geometry;            // VSEPR geometry
    
    // Experimental data sources
    std::string source;              // e.g., "NIST", "CRC", "Computational"
};

class ThermoDatabase {
    std::unordered_map<std::string, ReferenceMolecule> data_;
    
    void load_common_molecules();
    void load_hydrocarbons();
    void load_alcohols();
    void load_amines();
    void load_aromatics();
    void load_inorganics();
    
public:
    ThermoDatabase();
    
    // Query by formula
    std::optional<ReferenceMolecule> get(const std::string& formula) const;
    
    // Query by name
    std::optional<ReferenceMolecule> get_by_name(const std::string& name) const;
    
    // List all molecules
    std::vector<std::string> list_formulas() const;
    std::vector<std::string> list_names() const;
    
    // Add custom molecule
    void add(const std::string& formula, const ReferenceMolecule& mol);
    
    // Statistics
    size_t count() const { return data_.size(); }
};

// ============================================================================
// Gibbs Energy Calculator
// ============================================================================

class GibbsCalculator {
    const ThermoDatabase* db_;
    
public:
    explicit GibbsCalculator(const ThermoDatabase* db = nullptr);
    
    // Calculate Gibbs free energy
    double calculate(const Molecule& mol, const ThermodynamicState& state) const;
    
    // Calculate from components
    double calculate_from_enthalpy_entropy(double H, double S, double T) const {
        return H - T * S / 1000.0;  // S in cal/mol·K, convert to kcal
    }
    
    // Estimate from structure (if no database entry)
    double estimate_H_formation(const Molecule& mol) const;
    double estimate_entropy(const Molecule& mol) const;
    double estimate_gibbs(const Molecule& mol, const ThermodynamicState& state) const;
    
    // Reaction Gibbs energy
    double reaction_gibbs(const std::vector<Molecule>& reactants,
                         const std::vector<Molecule>& products,
                         const ThermodynamicState& state) const;
    
    // Equilibrium constant from Gibbs energy
    double equilibrium_constant(double delta_G, double T) const;
};

// ============================================================================
// Position-Dependent Properties (Thermodynamic Geometry)
// ============================================================================

struct PositionThermodynamics {
    // Energy as function of position
    double potential_energy_kcal_mol;
    
    // Force (gradient of potential)
    double force_x, force_y, force_z;  // kcal/mol/Å
    
    // Vibrational contribution to entropy (from local curvature)
    double vibrational_entropy_cal_mol_K;
    
    // Local Gibbs energy
    double local_gibbs_kcal_mol;
};

class PositionDependentThermo {
public:
    // Calculate thermodynamic properties at specific atomic position
    PositionThermodynamics calculate_at_position(
        const Molecule& mol,
        size_t atom_index,
        const ThermodynamicState& state) const;
    
    // Energy landscape (potential energy surface sampling)
    std::vector<PositionThermodynamics> scan_energy_surface(
        const Molecule& mol,
        size_t atom_index,
        double scan_radius_A,
        int n_points) const;
};

// ============================================================================
// Seeded Reference Molecules
// ============================================================================

// Common molecules everyone loves 😊
namespace seeded {

// Hydrocarbons
inline ReferenceMolecule methane() {
    return {
        "CH4", "Methane", "C",
        {-17.89, 44.5, -12.14, 8.54, 0.0, 2.6, ThermoData::GAS},
        5, 4, "Tetrahedral", "NIST"
    };
}

inline ReferenceMolecule ethane() {
    return {
        "C2H6", "Ethane", "CC",
        {-20.04, 54.85, -7.86, 12.58, 0.0, 4.47, ThermoData::GAS},
        8, 7, "Tetrahedral(C)", "NIST"
    };
}

inline ReferenceMolecule ethylene() {
    return {
        "C2H4", "Ethylene", "C=C",
        {12.54, 52.45, 16.28, 10.41, 0.0, 4.26, ThermoData::GAS},
        6, 5, "Trigonal Planar(C)", "NIST"
    };
}

inline ReferenceMolecule acetylene() {
    return {
        "C2H2", "Acetylene", "C#C",
        {54.19, 48.0, 50.00, 10.5, 0.0, 3.33, ThermoData::GAS},
        4, 3, "Linear", "NIST"
    };
}

// Simple molecules
inline ReferenceMolecule water() {
    return {
        "H2O", "Water", "O",
        {-57.80, 45.11, -54.64, 18.0, 1.85, 1.45, ThermoData::LIQUID},
        3, 2, "Bent", "NIST"
    };
}

inline ReferenceMolecule ammonia() {
    return {
        "NH3", "Ammonia", "N",
        {-11.04, 46.01, -3.93, 8.9, 1.47, 2.26, ThermoData::GAS},
        4, 3, "Trigonal Pyramidal", "NIST"
    };
}

inline ReferenceMolecule carbon_dioxide() {
    return {
        "CO2", "Carbon Dioxide", "O=C=O",
        {-94.05, 51.07, -94.26, 8.9, 0.0, 2.91, ThermoData::GAS},
        3, 2, "Linear", "NIST"
    };
}

inline ReferenceMolecule methanol() {
    return {
        "CH3OH", "Methanol", "CO",
        {-48.08, 57.3, -38.7, 11.0, 1.70, 3.29, ThermoData::LIQUID},
        6, 5, "Tetrahedral(C)/Bent(O)", "NIST"
    };
}

inline ReferenceMolecule ethanol() {
    return {
        "C2H5OH", "Ethanol", "CCO",
        {-56.24, 67.4, -41.77, 15.7, 1.69, 5.11, ThermoData::LIQUID},
        9, 8, "Tetrahedral", "NIST"
    };
}

// Aromatics
inline ReferenceMolecule benzene() {
    return {
        "C6H6", "Benzene", "c1ccccc1",
        {19.82, 64.3, 30.99, 19.5, 0.0, 10.0, ThermoData::LIQUID},
        12, 12, "Planar Hexagon", "NIST"
    };
}

// Halogens
inline ReferenceMolecule hydrogen_chloride() {
    return {
        "HCl", "Hydrogen Chloride", "Cl",
        {-22.06, 44.65, -22.78, 6.96, 1.08, 2.63, ThermoData::GAS},
        2, 1, "Linear", "NIST"
    };
}

// Sulfur compounds
inline ReferenceMolecule hydrogen_sulfide() {
    return {
        "H2S", "Hydrogen Sulfide", "S",
        {-4.82, 49.15, -8.02, 8.18, 0.97, 3.78, ThermoData::GAS},
        3, 2, "Bent", "NIST"
    };
}

inline ReferenceMolecule sulfur_dioxide() {
    return {
        "SO2", "Sulfur Dioxide", "O=S=O",
        {-70.96, 59.40, -71.79, 9.51, 1.63, 4.29, ThermoData::GAS},
        3, 2, "Bent", "NIST"
    };
}

// Nitrogen compounds
inline ReferenceMolecule nitrogen_dioxide() {
    return {
        "NO2", "Nitrogen Dioxide", "[N+](=O)[O-]",
        {8.09, 57.47, 12.39, 9.05, 0.316, 3.03, ThermoData::GAS},
        3, 2, "Bent", "NIST"
    };
}

inline ReferenceMolecule nitric_oxide() {
    return {
        "NO", "Nitric Oxide", "[N]=O",
        {21.58, 50.34, 20.72, 7.14, 0.159, 1.70, ThermoData::GAS},
        2, 1, "Linear", "NIST"
    };
}

} // namespace seeded

// ============================================================================
// Global Access
// ============================================================================

const ThermoDatabase& thermo_database();
void init_thermo_database();

} // namespace thermo
} // namespace vsepr
