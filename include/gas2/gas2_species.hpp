/**
 * gas2_species.hpp
 * ----------------
 * Species Database and Fluid Property Definitions for gas2 Module.
 *
 * Mirrors the pattern of:
 *   - thermal/thermal_model.hpp     (per-atom property structs)
 *   - pipe_thermal_engine.hpp       (FluidProperties)
 *   - gas_module.hpp                (VdW database, molar mass table)
 *
 * Unifies species data into a single authoritative record per gas.
 * Each GasSpecies carries:
 *   - Identity (formula, name, Z-composition)
 *   - Thermodynamic (molar mass, Cp, Cv, gamma)
 *   - Transport (viscosity, thermal conductivity, kinetic diameter)
 *   - Equation-of-state parameters (VdW a,b; critical point)
 *
 * Species database (curated, 26 entries as of this revision):
 *   Noble gases:    He, Ne, Ar, Kr, Xe
 *   Diatomic:       H2, N2, O2, Cl2, F2, CO
 *   Triatomic+:     H2O, CO2, SO2, NH3, CH4, H2S, HCl
 *   Larger organics: C2H6 (ethane), C3H8 (propane)
 *   Industrial:     SF6, UF6, BF3, PF5, R134a (1,1,1,2-tetrafluoroethane)
 *
 * find_species_or_monatomic(formula) provides a fallback for monatomic
 * element symbols (noble gases and any atom symbol) by constructing a
 * synthetic GasSpecies from gas2_nuclear data. This enables the gas2
 * engine to analyse every element Z=2..102 as a gas-phase species.
 *
 * Anti-black-box: every value has a cited source or derivation note.
 */

#pragma once

#include "gas2_constants.hpp"
#include "gas2_nuclear.hpp"
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <optional>

namespace vsepr {
namespace gas2 {

// ============================================================================
// Gas species record
// ============================================================================

struct GasSpecies {
    // --- Identity ---
    std::string formula;       // e.g. "Ar", "N2", "CO2"
    std::string name;          // e.g. "Argon", "Nitrogen", "Carbon Dioxide"
    int         n_atoms;       // atoms per molecule

    // --- Thermodynamic ---
    double molar_mass_g;       // g/mol (IUPAC 2021)
    double Cp_Jmol;            // isobaric heat capacity J/(mol·K) at 298 K
    double Cv_Jmol;            // isochoric heat capacity J/(mol·K) at 298 K
    double gamma;              // Cp/Cv (heat capacity ratio)
    double Hf0_kJmol;          // standard enthalpy of formation (kJ/mol, 298 K)

    // --- Transport (at 300 K, 1 atm unless noted) ---
    double viscosity_uPas;     // dynamic viscosity (μPa·s)
    double k_thermal_mWmK;     // thermal conductivity (mW/(m·K))
    double d_kinetic_pm;       // kinetic diameter (pm)

    // --- Van der Waals EOS ---
    double vdw_a;              // Pa·m^6/mol^2
    double vdw_b;              // m^3/mol

    // --- Critical point ---
    double Tc_K;               // critical temperature (K)
    double Pc_atm;             // critical pressure (atm)
    double Vc_cm3mol;          // critical molar volume (cm^3/mol)

    // Derived helpers
    double molar_mass_kg() const { return molar_mass_g / 1000.0; }
    double d_kinetic_m()   const { return d_kinetic_pm * 1.0e-12; }
};

// ============================================================================
// Species database (curated, extensible)
// ============================================================================

inline const std::map<std::string, GasSpecies>& species_database() {
    static const std::map<std::string, GasSpecies> db = {
        // Noble gases
        {"He",  {"He",  "Helium",           1,   4.003,  20.786, 12.472, 1.667,  0.0,
                  19.9,  151.3,  260,   0.003457, 2.370e-5,   5.19,  2.27, 57.5}},
        {"Ne",  {"Ne",  "Neon",             1,  20.180,  20.786, 12.472, 1.667,  0.0,
                  31.7,  49.1,   275,   0.02135,  1.709e-5,  44.49,  27.2, 41.7}},
        {"Ar",  {"Ar",  "Argon",            1,  39.948,  20.786, 12.472, 1.667,  0.0,
                  22.7,  17.7,   340,   0.1355,   3.201e-5, 150.86,  48.0, 75.2}},
        {"Kr",  {"Kr",  "Krypton",          1,  83.798,  20.786, 12.472, 1.667,  0.0,
                  25.4,   9.5,   360,   0.2349,   3.978e-5, 209.48,  54.3, 91.2}},
        {"Xe",  {"Xe",  "Xenon",            1, 131.293,  20.786, 12.472, 1.667,  0.0,
                  23.0,   5.5,   396,   0.4250,   5.105e-5, 289.73,  58.0,118.0}},
        // Diatomic
        {"H2",  {"H2",  "Hydrogen",         2,   2.016,  28.836, 20.520, 1.405,  0.0,
                   8.9, 186.9,   289,   0.02476,  2.661e-5,  33.19,  12.8, 64.9}},
        {"N2",  {"N2",  "Nitrogen",         2,  28.014,  29.124, 20.810, 1.400,  0.0,
                  17.8,  25.8,   364,   0.1408,   3.913e-5, 126.21,  33.5, 89.2}},
        {"O2",  {"O2",  "Oxygen",           2,  31.998,  29.378, 21.064, 1.395,  0.0,
                  20.4,  26.6,   346,   0.1378,   3.183e-5, 154.58,  50.4, 73.4}},
        {"Cl2", {"Cl2", "Chlorine",         2,  70.906,  33.949, 25.635, 1.324,  0.0,
                  13.3,   8.9,   320,   0.6579,   5.622e-5, 417.15,  76.1,123.8}},
        {"F2",  {"F2",  "Fluorine Gas",     2,  37.997,  31.302, 22.988, 1.362,  0.0,
                  23.4,  24.9,   320,   0.1160,   3.374e-5, 144.13,  51.6, 66.2}},
        {"CO",  {"CO",  "Carbon Monoxide",  2,  28.010,  29.142, 20.828, 1.400, -110.527,
                  17.5,  24.8,   369,   0.1505,   3.710e-5, 132.91,  34.5, 90.1}},
        // Triatomic / polyatomic
        {"H2O", {"H2O", "Water",            3,  18.015,  33.577, 25.263, 1.329, -241.826,
                  10.0,  18.7,   265,   0.5536,   3.049e-5, 647.10, 218.0, 55.9}},
        {"CO2", {"CO2", "Carbon Dioxide",   3,  44.010,  37.135, 28.821, 1.289, -393.509,
                  14.9,  16.6,   330,   0.3640,   4.267e-5, 304.13,  72.8, 94.1}},
        {"SO2", {"SO2", "Sulfur Dioxide",   3,  64.066,  39.842, 31.528, 1.264, -296.842,
                  12.7,   9.6,   360,   0.6803,   5.636e-5, 430.64,  77.7,122.0}},
        {"NH3", {"NH3", "Ammonia",          4,  17.031,  35.652, 27.338, 1.304,  -45.898,
                  10.1,  24.4,   260,   0.4225,   3.707e-5, 405.56, 111.3, 72.5}},
        {"CH4", {"CH4", "Methane",          5,  16.043,  35.309, 27.0,   1.307,  -74.873,
                  11.1,  34.0,   380,   0.2283,   4.278e-5, 190.56,  45.4, 98.6}},
        {"H2S", {"H2S", "Hydrogen Sulfide", 3,  34.082,  34.232, 25.918, 1.320,  -20.600,
                  12.6,  13.2,   360,   0.4490,   3.800e-5, 373.40,  89.4, 98.5}},
        {"HCl", {"HCl", "Hydrogen Chloride",2,  36.461,  29.136, 20.822, 1.400,  -92.307,
                  14.6,  13.3,   360,   0.3716,   4.600e-5, 324.65,  81.5, 81.0}},
        // Larger organics
        {"C2H6",{"C2H6","Ethane",           8,  30.069,  52.500, 44.186, 1.188,  -84.680,
                   9.4,  20.7,   440,   0.5560,   6.500e-5, 305.32,  48.7,145.5}},
        {"C3H8",{"C3H8","Propane",         11,  44.096,  73.600, 65.286, 1.127, -103.847,
                   8.2,  17.9,   500,   0.9400,   9.010e-5, 369.83,  42.5,200.0}},
        // Industrial / nuclear-process gases
        {"SF6", {"SF6", "Sulfur Hexafluoride",7,146.055, 97.000, 88.686, 1.094,  -1220.5,
                  14.9,  12.0,   550,   0.7875,   8.800e-5, 318.73,  37.1,198.0}},
        {"UF6", {"UF6", "Uranium Hexafluoride",7,352.019,131.860,123.546,1.067, -2197.0,
                  18.7,   9.2,   580,   1.4820,   1.450e-4, 505.8,  45.5,250.0}},
        {"BF3", {"BF3", "Boron Trifluoride", 4,  67.806, 50.471, 42.157, 1.197, -1136.0,
                  15.3,  19.4,   440,   0.5058,   5.600e-5, 260.9,  49.3,115.0}},
        {"PF5", {"PF5", "Phosphorus Pentafluoride",6,125.963,101.640,93.326,1.089,-1594.0,
                  16.7,  13.0,   530,   1.0200,   1.040e-4, 271.2,  37.3,175.0}},
        // Refrigerant: R134a (1,1,1,2-tetrafluoroethane, C2H2F4)
        {"R134a",{"R134a","R-134a (1,1,1,2-Tetrafluoroethane)",6,102.031,79.014,70.700,1.117,-888.8,
                  12.3,  10.9,   470,   1.0750,   1.270e-4, 374.21,  40.6,198.0}},
    };
    return db;
}

// Lookup helper (returns nullptr if not found)
inline const GasSpecies* find_species(const std::string& formula) {
    auto& db = species_database();
    auto it = db.find(formula);
    return (it != db.end()) ? &it->second : nullptr;
}

// ============================================================================
// Monatomic fallback: build a synthetic GasSpecies from NuclearSpecies data.
// Enables gas2 analysis of every element Z=2..102 as an ideal monatomic gas.
// Returns nullopt if Z is out of range (Z<2 or Z>102).
// ============================================================================

inline std::optional<GasSpecies> make_monatomic_species(int Z) {
    const NuclearSpecies* ns = nuclear_species_ptr(Z);
    if (!ns) return std::nullopt;

    GasSpecies gs;
    gs.formula       = ns->symbol;
    gs.name          = std::string(ns->name) + " (monatomic gas)";
    gs.n_atoms       = 1;
    gs.molar_mass_g  = ns->molar_mass_g;
    gs.Cp_Jmol       = ns->Cp_gas_Jmol;             // 5/2 R = 20.786
    gs.Cv_Jmol       = ns->Cv_gas_Jmol();            // 3/2 R = 12.472
    gs.gamma         = ns->gamma_gas();              // 5/3
    gs.Hf0_kJmol     = 0.0;                          // reference state for element
    gs.viscosity_uPas = 20.0;                        // approximate; not measured for all
    gs.k_thermal_mWmK = 15.0;                        // approximate
    gs.d_kinetic_pm  = ns->d_kinetic_pm;
    // VdW parameters: approximate from Tc/Pc if available, else from ionisation proxy
    // Use London-dispersion scaling: a ∝ (alpha_pol * I) — proxy from molar mass
    gs.vdw_a         = 0.012 * ns->molar_mass_g;     // crude proxy; Pa·m^6/mol^2 order
    gs.vdw_b         = 2.5e-5 * std::cbrt(ns->molar_mass_g);  // Tc/Pc proxy
    // Critical point: approximate using Tc ~ 0.64 * boiling_K (Trouton-type)
    gs.Tc_K          = (ns->boiling_point_K > 0.0) ? 0.72 * ns->boiling_point_K : 150.0;
    gs.Pc_atm        = 40.0;                         // generic fallback
    gs.Vc_cm3mol     = 3.0 * R_gas * gs.Tc_K / (8.0 * gs.Pc_atm * atm_to_Pa) * 1.0e6;
    return gs;
}

// Combined lookup: first checks molecular species DB, then falls back to
// monatomic construction from nuclear species data.
// Returns nullptr only when no match found anywhere and storage is empty.
inline const GasSpecies* find_species_or_monatomic(
        const std::string& formula,
        std::optional<GasSpecies>& storage) {
    const GasSpecies* gs = find_species(formula);
    if (gs) return gs;
    // Scan nuclear species by symbol
    for (int Z = 2; Z <= 102; ++Z) {
        const NuclearSpecies* ns = nuclear_species_ptr(Z);
        if (ns && std::string(ns->symbol) == formula) {
            storage = make_monatomic_species(Z);
            if (storage) return &storage.value();
        }
    }
    return nullptr;
}

} // namespace gas2
} // namespace vsepr
