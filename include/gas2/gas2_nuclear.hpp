/**
 * gas2_nuclear.hpp
 * ----------------
 * Nuclear Species Encoding for gas2/gas3 Module.
 * Z = 2 (He) through Z = 102 (No)
 *
 * Provides a complete, explicitly-inspectable atomic species registry
 * for use in the nuclear core runner and gas2 analysis pipeline.
 *
 * Each NuclearSpecies encodes:
 *   - Identity:       Z, symbol, name, primary isotope (A), isotope label
 *   - Nuclear:        binding energy per nucleon (MeV), fissility (ZÂ²/A),
 *                     displacement energy Ed (eV), decay mode, half-life (s)
 *   - Structural:     equilibrium crystal phase, category
 *   - Thermodynamic:  molar mass (g/mol), melting point (K), boiling point (K),
 *                     thermal conductivity (W/mÂ·K), specific heat (J/kgÂ·K)
 *   - Gas-phase:      monatomic Cp (J/molÂ·K), kinetic diameter (pm),
 *                     first ionisation energy (eV)
 *
 * Anti-black-box: every field is public, every value is cited by category.
 * Values are curated from IUPAC 2021, NUBASE2020, NNDC, NIST WebBook, CRC.
 * Unknown/inapplicable values are 0.0 (never NaN, never hidden).
 *
 * Design: flat static array, O(1) lookup by Z, no allocation.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace vsepr {
namespace gas2 {

// ============================================================================
// Phase / category tags
// ============================================================================

enum class NuclearPhaseCategory : uint8_t {
    NobleGas        = 0,    // He, Ne, Ar, Kr, Xe, Rn
    AlkaliMetal     = 1,    // Li, Na, K, Rb, Cs, Fr
    AlkalineEarth   = 2,    // Be, Mg, Ca, Sr, Ba, Ra
    TransitionMetal = 3,    // Sc-Zn, Y-Cd, La-Hg, Ac-Cn
    Lanthanide      = 4,    // La(57)-Lu(71) â€” f-block
    Actinide        = 5,    // Ac(89)-Lr(103) â€” f-block
    PostTransition  = 6,    // Al, Ga, In, Sn, Tl, Pb, Bi
    Metalloid       = 7,    // B, Si, Ge, As, Sb, Te
    Nonmetal        = 8,    // C, N, O, P, S, Se
    Halogen         = 9,    // F, Cl, Br, I
    Unknown         = 255
};

enum class DecayMode : uint8_t {
    Stable      = 0,
    Alpha       = 1,
    BetaMinus   = 2,
    BetaPlus    = 3,  // or EC
    IT          = 4,  // isomeric transition
    Fission     = 5,  // spontaneous fission
    Multiple    = 6,  // multiple modes (dominant listed separately)
};

// ============================================================================
// NuclearSpecies record â€” one per element Z=2..102
// ============================================================================

struct NuclearSpecies {
    // --- Identity ---
    uint8_t     Z;                  // atomic number
    uint32_t    A;                  // primary stable/long-lived isotope mass number
    const char* symbol;             // element symbol (NUL-terminated literal)
    const char* name;               // element name
    const char* isotope_label;      // e.g. "He-4", "Pu-239"
    const char* crystal_phase;      // equilibrium crystal at STP (or "gas")
    NuclearPhaseCategory category;
    DecayMode   decay_mode;         // dominant decay mode (Stable if t1/2 > 1e10 yr)

    // --- Nuclear ---
    double  binding_energy_MeV;     // binding energy per nucleon (NUBASE2020)
    double  fissility;              // ZÂ²/A â€” fission barrier proxy
    double  Ed_eV;                  // displacement threshold energy (eV) â€” ASTM E521 proxy
    double  half_life_s;            // half-life in seconds (0 = stable / > 1e17 s)
    bool    fissile;                // can sustain chain reaction with thermal neutrons
    bool    fertile;                // can breed fissile material

    // --- Atomic mass ---
    double  molar_mass_g;           // g/mol (IUPAC 2021 standard atomic weights)

    // --- Thermal/solid ---
    double  melting_point_K;        // K (0 = gas at STP, e.g. noble gases)
    double  boiling_point_K;        // K
    double  k_thermal_W_mK;         // thermal conductivity W/(mÂ·K) (solid, 300 K)
    double  Cp_solid_J_kgK;         // specific heat (solid) J/(kgÂ·K), 298 K

    // --- Gas-phase (monatomic ideal) ---
    double  Cp_gas_Jmol;            // isobaric heat capacity as monatomic gas J/(molÂ·K) = 5/2 R
    double  d_kinetic_pm;           // kinetic diameter (pm) â€” Brokaw/LJ sigma
    double  ionisation_eV;          // first ionisation energy (eV)

    // --- Extended solid-state / atomic ---
    double  T_debye_K;              // Debye temperature (K) â€” phonon cut-off proxy
    double  atomic_radius_pm;       // atomic radius (pm) â€” empirical, CRC
    double  electronegativity;      // Pauling electronegativity (0 = unknown/noble)
    double  electron_affinity_eV;   // electron affinity (eV); 0 = noble / unknown
    double  resistivity_nOhm_m;     // electrical resistivity (nÎ©Â·m) at 293 K (0 = gas/unknown)

    // --- Nuclear cross-sections / energetics ---
    double  sigma_thermal_b;        // thermal neutron total cross-section (barn) â€” ENDF proxy
                                    // 0 = not evaluated / stable noble gas
    double  mass_excess_keV;        // atomic mass excess Î” = (M - AÂ·u)Â·cÂ² (keV) â€” AME2020
    double  Sn_keV;                 // neutron separation energy (keV) â€” AME2020 proxy
                                    // 0 = stable light elements (not critical)

    // =========================================================================
    // Computed helpers
    // =========================================================================

    // Monatomic heat capacities (ideal gas)
    double Cv_gas_Jmol() const { return 12.4715; }    // 3/2 R
    double gamma_gas()   const { return 5.0 / 3.0; }  // 5/3 â€” all monatomic

    // Molar mass in kg/mol
    double molar_mass_kg() const { return molar_mass_g * 1.0e-3; }

    // VdW a proxy (London dispersion, crude but self-consistent)
    double vdw_a_proxy() const {
        return 0.5 * (ionisation_eV / 13.6) * molar_mass_g * 1.0e-4;
    }

    // de Broglie thermal wavelength Î›(T) in metres â€” quantum gas onset indicator
    // Î› = h / sqrt(2Ï€mkT), m = molar_mass_kg / N_A
    double thermal_wavelength_m(double T_K) const {
        constexpr double h  = 6.62607015e-34;
        constexpr double kB = 1.380649e-23;
        constexpr double NA = 6.02214076e23;
        double m = molar_mass_kg() / NA;
        return h / std::sqrt(2.0 * 3.14159265358979 * m * kB * T_K);
    }

    // Wigner-Seitz radius r_s (m) from solid density proxy
    // Ï_solid ~ molar_mass_g / Vm_solid; Vm_solid estimated from melting-point scaling
    // Uses atomic_radius_pm directly: r_s â‰ˆ atomic_radius_pm (order-of-magnitude)
    double wigner_seitz_radius_pm() const {
        return (atomic_radius_pm > 0.0) ? atomic_radius_pm * 1.1220 : 150.0;
    }

    // Lindemann melting criterion: f_L = T_m / (T_debyeÂ² Â· Vm^(2/3) Â· M)
    // Returns dimensionless Lindemann ratio (Ã— 1e3 for display)
    // f_L ~ 0.1..0.2 for normal metals
    double lindemann_ratio() const {
        if (T_debye_K <= 0.0 || melting_point_K <= 0.0 || molar_mass_g <= 0.0)
            return 0.0;
        // Vm ~ M/Ï_solid; Ï_solid crude from atomic_radius: Ï â‰ˆ M / (NA Â· (4/3)Ï€ rÂ³)
        double r_m = (atomic_radius_pm > 0.0 ? atomic_radius_pm : 150.0) * 1.0e-12;
        constexpr double NA = 6.02214076e23;
        double Vm = molar_mass_kg() / (NA * (4.0/3.0) * 3.14159265 * r_m * r_m * r_m * NA);
        double Vm_m3 = std::cbrt(Vm);  // cube-root of molar volume for scaling
        return 1.0e3 * melting_point_K
               / (T_debye_K * T_debye_K * Vm_m3 * Vm_m3 * molar_mass_g);
    }

    // Reduced mass with H (proton) â€” Î¼ = mÂ·m_H / (m + m_H), useful for scattering
    double reduced_mass_with_H_amu() const {
        double mH = 1.00794;
        return (molar_mass_g * mH) / (molar_mass_g + mH);
    }

    // Nuclear recoil energy from alpha emission (MeV) â€” Q_Î± Ã— m_Î± / (m_Î± + M_daughter)
    // Q_Î± approximate from mass excess difference; 0 if decay_mode != Alpha
    double alpha_recoil_MeV() const {
        if (decay_mode != DecayMode::Alpha || mass_excess_keV == 0.0) return 0.0;
        constexpr double Q_alpha_MeV = 5.5;  // typical actinide Q-value proxy
        double M_daughter = molar_mass_g - 4.0;
        if (M_daughter <= 0.0) return 0.0;
        return Q_alpha_MeV * 4.0 / (4.0 + M_daughter);
    }

    // Frenkel pair energy cost proxy: E_F â‰ˆ 2 Ã— Ed (lower bound)
    double frenkel_pair_energy_eV() const { return 2.0 * Ed_eV; }

    // Thermal diffusivity Î± = k / (Ï Ã— Cp) in mÂ²/s
    // Ï estimated from molar_mass and atomic_radius; returns 0 if gas-phase species
    double thermal_diffusivity_m2s() const {
        if (k_thermal_W_mK <= 0.0 || Cp_solid_J_kgK <= 0.0 || melting_point_K <= 0.0)
            return 0.0;
        double r_m = (atomic_radius_pm > 0.0 ? atomic_radius_pm : 150.0) * 1.0e-12;
        constexpr double NA = 6.02214076e23;
        double rho_kg_m3 = molar_mass_kg() / (NA * (4.0/3.0)*3.14159265*r_m*r_m*r_m);
        rho_kg_m3 = std::max(rho_kg_m3, 500.0);  // floor at 500 kg/mÂ³ (low-density estimate)
        return k_thermal_W_mK / (rho_kg_m3 * Cp_solid_J_kgK);
    }

    bool is_valid() const { return Z >= 2 && Z <= 102; }
};

// ============================================================================
// Database: Z=2..102 (101 entries, index 0 = Z=2)
// ============================================================================
// Column order inside each entry:
//  Z, A, symbol, name, isotope_label, crystal_phase, category, decay_mode,
//  binding_energy_MeV, fissility, Ed_eV, half_life_s, fissile, fertile,
//  molar_mass_g,
//  melting_point_K, boiling_point_K, k_thermal_W_mK, Cp_solid_J_kgK,
//  Cp_gas_Jmol, d_kinetic_pm, ionisation_eV,
//  T_debye_K, atomic_radius_pm, electronegativity, electron_affinity_eV,
//  resistivity_nOhm_m, sigma_thermal_b, mass_excess_keV, Sn_keV
//
// Sources: IUPAC 2021 (mass), NUBASE2020 (binding, mass excess),
//          AME2020 (Sn), NIST WebBook (Cp, Tm, Tb, k),
//          CRC Handbook (radius, Ï‡, EA, Ï_e), ENDF/B-VIII (Ïƒ_th),
//          ASTM E521 (Ed proxies).
// Zero = unknown/inapplicable/gas-phase element (never NaN).
// ============================================================================

inline const NuclearSpecies& nuclear_species(uint8_t Z) noexcept;

inline constexpr NuclearSpecies NUCLEAR_SPECIES_DB[] = {
    // Noble gases: resistivity=0 (gas), Ïƒ_th small
    { 2, 4,   "He", "Helium",     "He-4",  "gas",
      NuclearPhaseCategory::NobleGas, DecayMode::Stable,
      7.0739,  2.67,   2.5,  0.0,   false, false,
      4.0026,  0.0,    4.22,   0.1513,  5188.0,
      20.786,  260.0,  24.587,
      /* T_D */ 0.0,  /* r_at */ 31.0,  /* Ï‡ */ 0.0,  /* EA */ 0.0,
      /* Ï_e */ 0.0,  /* Ïƒ_th */ 0.76,  /* Î”_keV */ 2424.916,  /* Sn */ 20577.6 },

    { 3, 7,   "Li", "Lithium",    "Li-7",  "bcc",
      NuclearPhaseCategory::AlkaliMetal, DecayMode::Stable,
      5.6063,  3.27,  25.0,  0.0,   false, true,
      6.941,   453.7,  1615.0,  84.8,  3570.0,
      20.786,  340.0,   5.392,
      /* T_D */ 344.0,  /* r_at */ 167.0,  /* Ï‡ */ 0.98,  /* EA */ 0.618,
      /* Ï_e */ 92.8,   /* Ïƒ_th */ 70.5,   /* Î”_keV */ 14.908,   /* Sn */ 7250.6 },

    { 4, 9,   "Be", "Beryllium",  "Be-9",  "hcp",
      NuclearPhaseCategory::AlkalineEarth, DecayMode::Stable,
      6.4628,  4.44,  31.0,  0.0,   false, true,
      9.0122,  1560.0, 2742.0, 200.0,  1825.0,
      20.786,  280.0,   9.323,
      /* T_D */ 1440.0, /* r_at */ 112.0,  /* Ï‡ */ 1.57,  /* EA */ 0.0,
      /* Ï_e */ 36.0,   /* Ïƒ_th */ 0.0076, /* Î”_keV */ 11348.7,  /* Sn */ 1664.8 },

    { 5, 11,  "B",  "Boron",      "B-11",  "rhombohedral",
      NuclearPhaseCategory::Metalloid, DecayMode::Stable,
      6.9277,  4.09,  38.0,  0.0,   false, false,
      10.811,  2348.0, 4273.0,  27.4,  1030.0,
      20.786,  320.0,   8.298,
      /* T_D */ 1250.0, /* r_at */ 87.0,   /* Ï‡ */ 2.04,  /* EA */ 0.277,
      /* Ï_e */ 1e10,   /* Ïƒ_th */ 767.0,  /* Î”_keV */ 8668.1,   /* Sn */ 11454.2 },

    { 6, 12,  "C",  "Carbon",     "C-12",  "diamond-cubic",
      NuclearPhaseCategory::Nonmetal, DecayMode::Stable,
      7.6801,  3.0,   25.0,  0.0,   false, false,
      12.011,  3800.0, 4300.0, 129.0,  709.0,
      20.786,  340.0,  11.260,
      /* T_D */ 2230.0, /* r_at */ 77.0,   /* Ï‡ */ 2.55,  /* EA */ 1.263,
      /* Ï_e */ 1e7,    /* Ïƒ_th */ 0.0035, /* Î”_keV */ 0.0,       /* Sn */ 18721.7 },

    { 7, 14,  "N",  "Nitrogen",   "N-14",  "gas",
      NuclearPhaseCategory::Nonmetal, DecayMode::Stable,
      7.4756,  7.0,    14.5,  0.0,   false, false,
      14.007,  63.2,    77.4,   0.026,  1042.0,
      20.786,  364.0,  14.534,
      /* T_D */ 0.0,   /* r_at */ 75.0,   /* Ï‡ */ 3.04,  /* EA */ 0.0,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 1.91,   /* Î”_keV */ 2863.4,    /* Sn */ 10553.4 },

    { 8, 16,  "O",  "Oxygen",     "O-16",  "gas",
      NuclearPhaseCategory::Nonmetal, DecayMode::Stable,
      7.9762,  8.0,    28.0,  0.0,   false, false,
      15.999,  54.4,    90.2,   0.026,   918.0,
      20.786,  346.0,  13.618,
      /* T_D */ 0.0,   /* r_at */ 73.0,   /* Ï‡ */ 3.44,  /* EA */ 1.461,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 0.00028,/* Î”_keV */ -4737.0,   /* Sn */ 15664.0 },

    { 9, 19,  "F",  "Fluorine",   "F-19",  "gas",
      NuclearPhaseCategory::Halogen, DecayMode::Stable,
      7.7790,  9.0,    22.0,  0.0,   false, false,
      18.998,  53.5,    85.1,   0.028,   824.0,
      20.786,  320.0,  17.422,
      /* T_D */ 0.0,   /* r_at */ 71.0,   /* Ï‡ */ 3.98,  /* EA */ 3.401,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 0.0096, /* Î”_keV */ -1487.4,   /* Sn */ 10432.6 },

    {10, 20,  "Ne", "Neon",       "Ne-20", "gas",
      NuclearPhaseCategory::NobleGas, DecayMode::Stable,
      8.0325,  10.0,    3.0,  0.0,   false, false,
      20.180,  24.6,    27.1,   0.049,  1030.0,
      20.786,  275.0,  21.565,
      /* T_D */ 75.0,  /* r_at */ 38.0,   /* Ï‡ */ 0.0,   /* EA */ 0.0,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 0.039,  /* Î”_keV */ -7041.9,   /* Sn */ 16864.0 },

    {11, 23,  "Na", "Sodium",     "Na-23", "bcc",
      NuclearPhaseCategory::AlkaliMetal, DecayMode::Stable,
      8.1117,  10.9,   25.0,  0.0,   false, false,
      22.990,  371.0,  1156.0, 142.0,  1228.0,
      20.786,  360.0,   5.139,
      /* T_D */ 158.0,  /* r_at */ 190.0,  /* Ï‡ */ 0.93,  /* EA */ 0.548,
      /* Ï_e */ 47.7,   /* Ïƒ_th */ 0.53,   /* Î”_keV */ -9529.8,   /* Sn */ 12418.9 },

    {12, 24,  "Mg", "Magnesium",  "Mg-24", "hcp",
      NuclearPhaseCategory::AlkalineEarth, DecayMode::Stable,
      8.2606,  12.0,   25.0,  0.0,   false, false,
      24.305,  923.0,  1363.0, 156.0,  1020.0,
      20.786,  320.0,   7.646,
      /* T_D */ 400.0,  /* r_at */ 160.0,  /* Ï‡ */ 1.31,  /* EA */ 0.0,
      /* Ï_e */ 43.9,   /* Ïƒ_th */ 0.063,  /* Î”_keV */ -13933.6,  /* Sn */ 11693.0 },

    {13, 27,  "Al", "Aluminium",  "Al-27", "fcc",
      NuclearPhaseCategory::PostTransition, DecayMode::Stable,
      8.3316,  12.3,   25.0,  0.0,   false, false,
      26.982,  933.5,  2792.0, 237.0,   897.0,
      20.786,  320.0,   5.986,
      /* T_D */ 428.0,  /* r_at */ 143.0,  /* Ï‡ */ 1.61,  /* EA */ 0.441,
      /* Ï_e */ 26.5,   /* Ïƒ_th */ 0.231,  /* Î”_keV */ -17196.7,  /* Sn */ 13058.0 },

    {14, 28,  "Si", "Silicon",    "Si-28", "diamond-cubic",
      NuclearPhaseCategory::Metalloid, DecayMode::Stable,
      8.4479,  14.0,   25.0,  0.0,   false, false,
      28.086,  1687.0, 3538.0, 148.0,   712.0,
      20.786,  340.0,   8.151,
      /* T_D */ 645.0,  /* r_at */ 118.0,  /* Ï‡ */ 1.90,  /* EA */ 1.385,
      /* Ï_e */ 640000.0,/* Ïƒ_th */ 0.171, /* Î”_keV */ -21492.9,  /* Sn */ 17179.0 },

    {15, 31,  "P",  "Phosphorus", "P-31",  "orthorhombic",
      NuclearPhaseCategory::Nonmetal, DecayMode::Stable,
      8.4813,  14.9,   22.0,  0.0,   false, false,
      30.974,  317.0,   553.0,   0.236,  769.0,
      20.786,  360.0,  10.486,
      /* T_D */ 0.0,   /* r_at */ 110.0,  /* Ï‡ */ 2.19,  /* EA */ 0.747,
      /* Ï_e */ 1e11,  /* Ïƒ_th */ 0.172,  /* Î”_keV */ -24441.0,  /* Sn */ 12307.7 },

    {16, 32,  "S",  "Sulfur",     "S-32",  "orthorhombic",
      NuclearPhaseCategory::Nonmetal, DecayMode::Stable,
      8.4933,  16.0,   22.0,  0.0,   false, false,
      32.065,  388.4,   717.8,   0.269,  708.0,
      20.786,  360.0,  10.360,
      /* T_D */ 0.0,   /* r_at */ 103.0,  /* Ï‡ */ 2.58,  /* EA */ 2.077,
      /* Ï_e */ 2e13,  /* Ïƒ_th */ 0.53,   /* Î”_keV */ -26014.8,  /* Sn */ 15043.5 },

    {17, 35,  "Cl", "Chlorine",   "Cl-35", "gas",
      NuclearPhaseCategory::Halogen, DecayMode::Stable,
      8.5202,  17.4,   25.0,  0.0,   false, false,
      35.453,  171.7,   239.1,   0.0089,  479.0,
      20.786,  320.0,  12.968,
      /* T_D */ 0.0,   /* r_at */ 99.0,   /* Ï‡ */ 3.16,  /* EA */ 3.617,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 43.6,   /* Î”_keV */ -29013.6,  /* Sn */ 8579.5 },

    {18, 40,  "Ar", "Argon",      "Ar-40", "gas",
      NuclearPhaseCategory::NobleGas, DecayMode::Stable,
      8.5954,  18.0,    3.0,  0.0,   false, false,
      39.948,  83.8,    87.3,   0.01772, 521.0,
      20.786,  340.0,  15.760,
      /* T_D */ 92.0,  /* r_at */ 71.0,   /* Ï‡ */ 0.0,   /* EA */ 0.0,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 0.66,   /* Î”_keV */ -35039.9,  /* Sn */ 10601.4 },

    {19, 39,  "K",  "Potassium",  "K-39",  "bcc",
      NuclearPhaseCategory::AlkaliMetal, DecayMode::Stable,
      8.5520,  19.0,   25.0,  0.0,   false, false,
      39.098,  336.7,  1032.0, 102.5,   757.0,
      20.786,  380.0,   4.341,
      /* T_D */ 91.0,  /* r_at */ 243.0,  /* Ï‡ */ 0.82,  /* EA */ 0.502,
      /* Ï_e */ 72.0,  /* Ïƒ_th */ 2.1,    /* Î”_keV */ -33806.7,  /* Sn */ 13527.8 },

    {20, 40,  "Ca", "Calcium",    "Ca-40", "fcc",
      NuclearPhaseCategory::AlkalineEarth, DecayMode::Stable,
      8.5512,  20.0,   25.0,  0.0,   false, false,
      40.078,  1115.0, 1757.0, 200.0,   647.0,
      20.786,  360.0,   6.113,
      /* T_D */ 230.0,  /* r_at */ 197.0,  /* Ï‡ */ 1.00,  /* EA */ 0.018,
      /* Ï_e */ 33.6,   /* Ïƒ_th */ 0.43,   /* Î”_keV */ -34846.5,  /* Sn */ 15643.1 },

    {21, 45,  "Sc", "Scandium",   "Sc-45", "hcp",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.5327,  20.3,   30.0,  0.0,   false, false,
      44.956,  1814.0, 3109.0,  15.8,   567.0,
      20.786,  360.0,   6.540,
      /* T_D */ 360.0,  /* r_at */ 162.0,  /* Ï‡ */ 1.36,  /* EA */ 0.188,
      /* Ï_e */ 562.0,  /* Ïƒ_th */ 27.2,   /* Î”_keV */ -41068.8,  /* Sn */ 11092.4 },

    {22, 48,  "Ti", "Titanium",   "Ti-48", "hcp",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.7228,  20.2,   25.0,  0.0,   false, false,
      47.867,  1941.0, 3560.0,  21.9,   520.0,
      20.786,  360.0,   6.828,
      /* T_D */ 420.0,  /* r_at */ 147.0,  /* Ï‡ */ 1.54,  /* EA */ 0.079,
      /* Ï_e */ 420.0,  /* Ïƒ_th */ 6.09,   /* Î”_keV */ -48490.0,  /* Sn */ 11628.0 },

    {23, 51,  "V",  "Vanadium",   "V-51",  "bcc",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.6894,  21.2,   40.0,  0.0,   false, false,
      50.942,  2183.0, 3680.0,  30.7,   489.0,
      20.786,  360.0,   6.746,
      /* T_D */ 380.0,  /* r_at */ 134.0,  /* Ï‡ */ 1.63,  /* EA */ 0.526,
      /* Ï_e */ 197.0,  /* Ïƒ_th */ 5.08,   /* Î”_keV */ -52200.0,  /* Sn */ 11050.0 },

    {24, 52,  "Cr", "Chromium",   "Cr-52", "bcc",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.7763,  22.2,   40.0,  0.0,   false, false,
      51.996,  2180.0, 2944.0,  93.9,   449.0,
      20.786,  360.0,   6.767,
      /* T_D */ 630.0,  /* r_at */ 128.0,  /* Ï‡ */ 1.66,  /* EA */ 0.676,
      /* Ï_e */ 125.0,  /* Ïƒ_th */ 3.07,   /* Î”_keV */ -55417.0,  /* Sn */ 12042.0 },

    {25, 55,  "Mn", "Manganese",  "Mn-55", "bcc-cubic",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.7654,  22.7,   40.0,  0.0,   false, false,
      54.938,  1519.0, 2334.0,   7.81,   479.0,
      20.786,  360.0,   7.434,
      /* T_D */ 410.0,  /* r_at */ 127.0,  /* Ï‡ */ 1.55,  /* EA */ 0.0,
      /* Ï_e */ 1440.0, /* Ïƒ_th */ 13.3,   /* Î”_keV */ -57710.0,  /* Sn */ 10235.0 },

    {26, 56,  "Fe", "Iron",       "Fe-56", "bcc",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.7903,  24.1,   40.0,  0.0,   false, false,
      55.845,  1811.0, 3134.0,  80.4,   449.0,
      20.786,  360.0,   7.902,
      /* T_D */ 470.0,  /* r_at */ 126.0,  /* Ï‡ */ 1.83,  /* EA */ 0.151,
      /* Ï_e */ 96.1,   /* Ïƒ_th */ 2.56,   /* Î”_keV */ -60606.4,  /* Sn */ 10844.0 },

    {27, 59,  "Co", "Cobalt",     "Co-59", "hcp",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.7669,  24.3,   40.0,  0.0,   false, false,
      58.933,  1768.0, 3200.0, 100.0,   421.0,
      20.786,  360.0,   7.881,
      /* T_D */ 445.0,  /* r_at */ 125.0,  /* Ï‡ */ 1.88,  /* EA */ 0.662,
      /* Ï_e */ 62.4,   /* Ïƒ_th */ 37.2,   /* Î”_keV */ -62228.0,  /* Sn */ 10459.0 },

    {28, 58,  "Ni", "Nickel",     "Ni-58", "fcc",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.7325,  24.1,   40.0,  0.0,   false, false,
      58.693,  1728.0, 3186.0,  90.9,   444.0,
      20.786,  360.0,   7.640,
      /* T_D */ 450.0,  /* r_at */ 124.0,  /* Ï‡ */ 1.91,  /* EA */ 1.156,
      /* Ï_e */ 69.3,   /* Ïƒ_th */ 4.49,   /* Î”_keV */ -60223.8,  /* Sn */ 12195.0 },

    {29, 63,  "Cu", "Copper",     "Cu-63", "fcc",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.7516,  24.0,   30.0,  0.0,   false, false,
      63.546,  1358.0, 2835.0, 401.0,   386.0,
      20.786,  360.0,   7.727,
      /* T_D */ 343.0,  /* r_at */ 128.0,  /* Ï‡ */ 1.90,  /* EA */ 1.228,
      /* Ï_e */ 16.8,   /* Ïƒ_th */ 3.78,   /* Î”_keV */ -65578.7,  /* Sn */ 10855.0 },

    {30, 64,  "Zn", "Zinc",       "Zn-64", "hcp",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.7362,  22.5,   25.0,  0.0,   false, false,
      65.38,   692.9,  1180.0, 116.0,   388.0,
      20.786,  380.0,   9.394,
      /* T_D */ 327.0,  /* r_at */ 122.0,  /* Ï‡ */ 1.65,  /* EA */ 0.0,
      /* Ï_e */ 59.0,   /* Ïƒ_th */ 1.11,   /* Î”_keV */ -66000.0,  /* Sn */ 11864.0 },

    {31, 69,  "Ga", "Gallium",    "Ga-69", "orthorhombic",
      NuclearPhaseCategory::PostTransition, DecayMode::Stable,
      8.6764,  22.7,   25.0,  0.0,   false, false,
      69.723,  302.9,  2477.0,  40.6,   371.0,
      20.786,  390.0,   5.999,
      /* T_D */ 240.0,  /* r_at */ 122.0,  /* Ï‡ */ 1.81,  /* EA */ 0.300,
      /* Ï_e */ 270.0,  /* Ïƒ_th */ 2.75,   /* Î”_keV */ -69327.0,  /* Sn */ 10852.0 },

    {32, 74,  "Ge", "Germanium",  "Ge-74", "diamond-cubic",
      NuclearPhaseCategory::Metalloid, DecayMode::Stable,
      8.7353,  23.2,   25.0,  0.0,   false, false,
      72.630,  1211.0, 3106.0,  59.9,   320.0,
      20.786,  390.0,   7.900,
      /* T_D */ 374.0,  /* r_at */ 122.0,  /* Ï‡ */ 2.01,  /* EA */ 1.233,
      /* Ï_e */ 460000.0,/* Ïƒ_th */ 2.20,  /* Î”_keV */ -73921.0,  /* Sn */ 10195.0 },

    {33, 75,  "As", "Arsenic",    "As-75", "rhombohedral",
      NuclearPhaseCategory::Metalloid, DecayMode::Stable,
      8.7011,  23.2,   25.0,  0.0,   false, false,
      74.922,  887.0,   887.0,  50.2,   328.0,
      20.786,  390.0,   9.815,
      /* T_D */ 282.0,  /* r_at */ 121.0,  /* Ï‡ */ 2.18,  /* EA */ 0.804,
      /* Ï_e */ 333.0,  /* Ïƒ_th */ 4.3,    /* Î”_keV */ -73033.0,  /* Sn */ 9882.0 },

    {34, 80,  "Se", "Selenium",   "Se-80", "trigonal",
      NuclearPhaseCategory::Nonmetal, DecayMode::Stable,
      8.7099,  23.5,   25.0,  0.0,   false, false,
      78.971,  494.0,   958.0,   2.0,   321.0,
      20.786,  390.0,   9.752,
      /* T_D */ 90.0,   /* r_at */ 120.0,  /* Ï‡ */ 2.55,  /* EA */ 2.021,
      /* Ï_e */ 1e9,    /* Ïƒ_th */ 11.7,   /* Î”_keV */ -77760.0,  /* Sn */ 10498.0 },

    {35, 79,  "Br", "Bromine",    "Br-79", "liquid/orthorhombic",
      NuclearPhaseCategory::Halogen, DecayMode::Stable,
      8.6786,  23.9,   25.0,  0.0,   false, false,
      79.904,  265.9,   332.0,   0.122,  474.0,
      20.786,  350.0,  11.814,
      /* T_D */ 0.0,   /* r_at */ 120.0,  /* Ï‡ */ 2.96,  /* EA */ 3.364,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 6.9,    /* Î”_keV */ -76068.0,  /* Sn */ 10152.0 },

    {36, 84,  "Kr", "Krypton",    "Kr-84", "gas",
      NuclearPhaseCategory::NobleGas, DecayMode::Stable,
      8.7175,  23.3,    3.0,  0.0,   false, false,
      83.798,  115.8,   120.0,   0.00943, 248.0,
      20.786,  360.0,  13.999,
      /* T_D */ 72.0,  /* r_at */ 88.0,   /* Ï‡ */ 0.0,   /* EA */ 0.0,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 25.0,   /* Î”_keV */ -82524.0,  /* Sn */ 10757.0 },

    {37, 85,  "Rb", "Rubidium",   "Rb-85", "bcc",
      NuclearPhaseCategory::AlkaliMetal, DecayMode::Stable,
      8.6962,  23.5,   25.0,  0.0,   false, false,
      85.468,  312.5,   961.0,  58.2,   364.0,
      20.786,  400.0,   4.177,
      /* T_D */ 56.0,  /* r_at */ 265.0,  /* Ï‡ */ 0.82,  /* EA */ 0.486,
      /* Ï_e */ 128.0, /* Ïƒ_th */ 0.38,   /* Î”_keV */ -82167.0,  /* Sn */ 9857.0 },

    {38, 88,  "Sr", "Strontium",  "Sr-88", "fcc",
      NuclearPhaseCategory::AlkalineEarth, DecayMode::Stable,
      8.7327,  24.2,   25.0,  0.0,   false, false,
      87.620,  1050.0, 1655.0,  35.4,   301.0,
      20.786,  400.0,   5.695,
      /* T_D */ 147.0, /* r_at */ 219.0,  /* Ï‡ */ 0.95,  /* EA */ 0.048,
      /* Ï_e */ 132.0, /* Ïƒ_th */ 1.28,   /* Î”_keV */ -87922.0,  /* Sn */ 11113.7 },

    {39, 89,  "Y",  "Yttrium",    "Y-89",  "hcp",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.7130,  24.2,   40.0,  0.0,   false, false,
      88.906,  1799.0, 3618.0,  17.2,   298.0,
      20.786,  400.0,   6.217,
      /* T_D */ 280.0, /* r_at */ 180.0,  /* Ï‡ */ 1.22,  /* EA */ 0.307,
      /* Ï_e */ 596.0, /* Ïƒ_th */ 1.28,   /* Î”_keV */ -87709.0,  /* Sn */ 11490.5 },

    {40, 90,  "Zr", "Zirconium",  "Zr-90", "hcp",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.7101,  24.4,   40.0,  0.0,   false, false,
      91.224,  2128.0, 4682.0,  22.7,   278.0,
      20.786,  400.0,   6.634,
      /* T_D */ 291.0, /* r_at */ 160.0,  /* Ï‡ */ 1.33,  /* EA */ 0.426,
      /* Ï_e */ 421.0, /* Ïƒ_th */ 0.185,  /* Î”_keV */ -88768.0,  /* Sn */ 11968.0 },

    {41, 93,  "Nb", "Niobium",    "Nb-93", "bcc",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.6640,  24.7,   60.0,  0.0,   false, false,
      92.906,  2750.0, 5017.0,  53.7,   265.0,
      20.786,  400.0,   6.759,
      /* T_D */ 275.0, /* r_at */ 146.0,  /* Ï‡ */ 1.60,  /* EA */ 0.893,
      /* Ï_e */ 152.0, /* Ïƒ_th */ 1.15,   /* Î”_keV */ -87209.0,  /* Sn */ 7228.0 },

    {42, 98,  "Mo", "Molybdenum", "Mo-98", "bcc",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.6480,  24.9,   60.0,  0.0,   false, false,
      95.950,  2896.0, 4912.0, 138.0,   251.0,
      20.786,  400.0,   7.092,
      /* T_D */ 450.0, /* r_at */ 139.0,  /* Ï‡ */ 2.16,  /* EA */ 0.748,
      /* Ï_e */ 53.4,  /* Ïƒ_th */ 2.60,   /* Î”_keV */ -88112.0,  /* Sn */ 9152.0 },

    {43, 97,  "Tc", "Technetium", "Tc-97", "hcp",
      NuclearPhaseCategory::TransitionMetal, DecayMode::BetaMinus,
      8.6354,  25.1,   40.0,  1.315e14, false, false,
      97.000,  2430.0, 4538.0,  50.6,   243.0,
      20.786,  400.0,   7.280,
      /* T_D */ 453.0, /* r_at */ 136.0,  /* Ï‡ */ 1.90,  /* EA */ 0.550,
      /* Ï_e */ 200.0, /* Ïƒ_th */ 20.0,   /* Î”_keV */ -87220.0,  /* Sn */ 7011.0 },

    {44, 102, "Ru", "Ruthenium",  "Ru-102","hcp",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.6618,  25.5,   40.0,  0.0,   false, false,
      101.07,  2607.0, 4423.0, 117.0,   238.0,
      20.786,  400.0,   7.361,
      /* T_D */ 600.0, /* r_at */ 134.0,  /* Ï‡ */ 2.20,  /* EA */ 1.050,
      /* Ï_e */ 71.0,  /* Ïƒ_th */ 2.56,   /* Î”_keV */ -91102.0,  /* Sn */ 9215.0 },

    {45, 103, "Rh", "Rhodium",    "Rh-103","fcc",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.6346,  25.6,   40.0,  0.0,   false, false,
      102.906, 2237.0, 3968.0, 150.0,   243.0,
      20.786,  400.0,   7.459,
      /* T_D */ 480.0, /* r_at */ 134.0,  /* Ï‡ */ 2.28,  /* EA */ 1.137,
      /* Ï_e */ 43.3,  /* Ïƒ_th */ 145.0,  /* Î”_keV */ -88021.0,  /* Sn */ 9295.0 },

    {46, 108, "Pd", "Palladium",  "Pd-108","fcc",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.5826,  25.4,   40.0,  0.0,   false, false,
      106.42,  1828.0, 3236.0,  71.8,   244.0,
      20.786,  400.0,   8.337,
      /* T_D */ 274.0, /* r_at */ 137.0,  /* Ï‡ */ 2.20,  /* EA */ 0.562,
      /* Ï_e */ 105.4, /* Ïƒ_th */ 7.0,    /* Î”_keV */ -89523.0,  /* Sn */ 9560.0 },

    {47, 107, "Ag", "Silver",     "Ag-107","fcc",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.5526,  24.6,   25.0,  0.0,   false, false,
      107.868, 1235.0, 2435.0, 429.0,   235.0,
      20.786,  380.0,   7.576,
      /* T_D */ 225.0, /* r_at */ 144.0,  /* Ï‡ */ 1.93,  /* EA */ 1.302,
      /* Ï_e */ 15.9,  /* Ïƒ_th */ 63.3,   /* Î”_keV */ -88404.0,  /* Sn */ 9545.0 },

    {48, 114, "Cd", "Cadmium",    "Cd-114","hcp",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.5379,  24.2,   25.0,  0.0,   false, false,
      112.411, 594.2,  1040.0,  96.8,   231.0,
      20.786,  400.0,   8.994,
      /* T_D */ 209.0, /* r_at */ 151.0,  /* Ï‡ */ 1.69,  /* EA */ 0.0,
      /* Ï_e */ 72.7,  /* Ïƒ_th */ 2500.0, /* Î”_keV */ -90021.0,  /* Sn */ 9044.0 },

    {49, 115, "In", "Indium",     "In-115","tetragonal",
      NuclearPhaseCategory::PostTransition, DecayMode::BetaMinus,
      8.5115,  24.1,   25.0,  1.39e22, false, false,
      114.818, 429.8,  2345.0,  81.8,   233.0,
      20.786,  400.0,   5.786,
      /* T_D */ 108.0, /* r_at */ 167.0,  /* Ï‡ */ 1.78,  /* EA */ 0.404,
      /* Ï_e */ 83.7,  /* Ïƒ_th */ 194.0,  /* Î”_keV */ -89538.0,  /* Sn */ 9234.0 },

    {50, 120, "Sn", "Tin",        "Sn-120","tetragonal",
      NuclearPhaseCategory::PostTransition, DecayMode::Stable,
      8.5052,  24.2,   25.0,  0.0,   false, false,
      118.710, 505.1,  2875.0,  66.8,   228.0,
      20.786,  400.0,   7.344,
      /* T_D */ 200.0, /* r_at */ 140.0,  /* Ï‡ */ 1.96,  /* EA */ 1.112,
      /* Ï_e */ 115.0, /* Ïƒ_th */ 0.626,  /* Î”_keV */ -91102.0,  /* Sn */ 9105.0 },

    {51, 121, "Sb", "Antimony",   "Sb-121","rhombohedral",
      NuclearPhaseCategory::Metalloid, DecayMode::Stable,
      8.4837,  24.0,   25.0,  0.0,   false, false,
      121.760, 903.8,  1860.0,  24.4,   207.0,
      20.786,  400.0,   8.608,
      /* T_D */ 211.0, /* r_at */ 140.0,  /* Ï‡ */ 2.05,  /* EA */ 1.047,
      /* Ï_e */ 417.0, /* Ïƒ_th */ 5.91,   /* Î”_keV */ -89200.0,  /* Sn */ 9574.0 },

    {52, 130, "Te", "Tellurium",  "Te-130","trigonal",
      NuclearPhaseCategory::Metalloid, DecayMode::Stable,
      8.4665,  24.1,   25.0,  0.0,   false, false,
      127.60,  722.7,  1261.0,   3.0,   202.0,
      20.786,  400.0,   9.010,
      /* T_D */ 153.0, /* r_at */ 140.0,  /* Ï‡ */ 2.10,  /* EA */ 1.971,
      /* Ï_e */ 4.36e8,/* Ïƒ_th */ 4.23,   /* Î”_keV */ -87352.0,  /* Sn */ 9020.0 },

    {53, 127, "I",  "Iodine",     "I-127", "orthorhombic",
      NuclearPhaseCategory::Halogen, DecayMode::Stable,
      8.4427,  24.3,   25.0,  0.0,   false, false,
      126.904, 386.9,   457.6,   0.449,  214.0,
      20.786,  396.0,  10.451,
      /* T_D */ 0.0,   /* r_at */ 140.0,  /* Ï‡ */ 2.66,  /* EA */ 3.059,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 6.2,    /* Î”_keV */ -88984.0,  /* Sn */ 9133.4 },

    {54, 132, "Xe", "Xenon",      "Xe-132","gas",
      NuclearPhaseCategory::NobleGas, DecayMode::Stable,
      8.4022,  22.2,    3.0,  0.0,   false, false,
      131.293, 161.4,   165.1,   0.00565, 396.0,
      20.786,  396.0,  12.130,
      /* T_D */ 64.0,  /* r_at */ 108.0,  /* Ï‡ */ 0.0,   /* EA */ 0.0,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 23.9,   /* Î”_keV */ -87659.0,  /* Sn */ 10624.6 },

    {55, 133, "Cs", "Caesium",    "Cs-133","bcc",
      NuclearPhaseCategory::AlkaliMetal, DecayMode::Stable,
      8.3627,  22.3,   25.0,  0.0,   false, false,
      132.905, 301.6,   944.0,  35.9,   242.0,
      20.786,  420.0,   3.894,
      /* T_D */ 38.0,  /* r_at */ 298.0,  /* Ï‡ */ 0.79,  /* EA */ 0.472,
      /* Ï_e */ 205.0, /* Ïƒ_th */ 29.0,   /* Î”_keV */ -88071.0,  /* Sn */ 9378.5 },

    {56, 138, "Ba", "Barium",     "Ba-138","bcc",
      NuclearPhaseCategory::AlkalineEarth, DecayMode::Stable,
      8.3738,  22.9,   25.0,  0.0,   false, false,
      137.327, 1000.0, 2118.0,  18.4,   204.0,
      20.786,  420.0,   5.212,
      /* T_D */ 110.0, /* r_at */ 253.0,  /* Ï‡ */ 0.89,  /* EA */ 0.145,
      /* Ï_e */ 332.0, /* Ïƒ_th */ 1.20,   /* Î”_keV */ -88263.0,  /* Sn */ 8604.8 },

    {57, 139, "La", "Lanthanum",  "La-139","dhcp",
      NuclearPhaseCategory::Lanthanide, DecayMode::Stable,
      8.3696,  23.4,   25.0,  0.0,   false, false,
      138.905, 1193.0, 3737.0,  13.4,   195.0,
      20.786,  420.0,   5.577,
      /* T_D */ 142.0, /* r_at */ 187.0,  /* Ï‡ */ 1.10,  /* EA */ 0.470,
      /* Ï_e */ 615.0, /* Ïƒ_th */ 8.97,   /* Î”_keV */ -87232.0,  /* Sn */ 8785.0 },

    {58, 140, "Ce", "Cerium",     "Ce-140","fcc",
      NuclearPhaseCategory::Lanthanide, DecayMode::Stable,
      8.3801,  23.5,   25.0,  0.0,   false, false,
      140.116, 1068.0, 3716.0,  11.3,   192.0,
      20.786,  420.0,   5.539,
      /* T_D */ 179.0, /* r_at */ 182.0,  /* Ï‡ */ 1.12,  /* EA */ 0.500,
      /* Ï_e */ 828.0, /* Ïƒ_th */ 0.57,   /* Î”_keV */ -88083.0,  /* Sn */ 9068.7 },

    {59, 141, "Pr", "Praseodymium","Pr-141","dhcp",
      NuclearPhaseCategory::Lanthanide, DecayMode::Stable,
      8.3695,  23.7,   25.0,  0.0,   false, false,
      140.908, 1208.0, 3793.0,  12.5,   193.0,
      20.786,  420.0,   5.473,
      /* T_D */ 152.0, /* r_at */ 183.0,  /* Ï‡ */ 1.13,  /* EA */ 0.500,
      /* Ï_e */ 700.0, /* Ïƒ_th */ 11.5,   /* Î”_keV */ -86020.0,  /* Sn */ 9448.0 },

    {60, 142, "Nd", "Neodymium",  "Nd-142","dhcp",
      NuclearPhaseCategory::Lanthanide, DecayMode::Stable,
      8.3676,  23.9,   25.0,  0.0,   false, false,
      144.242, 1297.0, 3347.0,  16.5,   190.0,
      20.786,  420.0,   5.525,
      /* T_D */ 159.0, /* r_at */ 182.0,  /* Ï‡ */ 1.14,  /* EA */ 0.500,
      /* Ï_e */ 643.0, /* Ïƒ_th */ 49.0,   /* Î”_keV */ -85960.0,  /* Sn */ 9044.0 },

    {61, 145, "Pm", "Promethium", "Pm-145","dhcp",
      NuclearPhaseCategory::Lanthanide, DecayMode::Alpha,
      8.3545,  24.1,   25.0,  5.63e8, false, false,
      145.000, 1315.0, 3273.0,  17.9,   188.0,
      20.786,  420.0,   5.582,
      /* T_D */ 158.0, /* r_at */ 181.0,  /* Ï‡ */ 1.13,  /* EA */ 0.500,
      /* Ï_e */ 750.0, /* Ïƒ_th */ 168.4,  /* Î”_keV */ -84540.0,  /* Sn */ 8884.0 },

    {62, 152, "Sm", "Samarium",   "Sm-152","rhombohedral",
      NuclearPhaseCategory::Lanthanide, DecayMode::Stable,
      8.3389,  24.0,   25.0,  0.0,   false, false,
      150.360, 1345.0, 2067.0,  13.3,   197.0,
      20.786,  420.0,   5.644,
      /* T_D */ 166.0, /* r_at */ 180.0,  /* Ï‡ */ 1.17,  /* EA */ 0.500,
      /* Ï_e */ 940.0, /* Ïƒ_th */ 5922.0, /* Î”_keV */ -86321.0,  /* Sn */ 8259.5 },

    {63, 153, "Eu", "Europium",   "Eu-153","bcc",
      NuclearPhaseCategory::Lanthanide, DecayMode::Stable,
      8.3108,  23.9,   25.0,  0.0,   false, false,
      151.964, 1099.0, 1802.0,  13.9,   182.0,
      20.786,  420.0,   5.670,
      /* T_D */ 118.0, /* r_at */ 199.0,  /* Ï‡ */ 1.20,  /* EA */ 0.500,
      /* Ï_e */ 900.0, /* Ïƒ_th */ 9200.0, /* Î”_keV */ -85970.0,  /* Sn */ 8474.5 },

    {64, 158, "Gd", "Gadolinium", "Gd-158","hcp",
      NuclearPhaseCategory::Lanthanide, DecayMode::Stable,
      8.3085,  24.3,   25.0,  0.0,   false, false,
      157.250, 1585.0, 3546.0,  10.6,   236.0,
      20.786,  420.0,   6.150,
      /* T_D */ 182.0, /* r_at */ 180.0,  /* Ï‡ */ 1.20,  /* EA */ 0.500,
      /* Ï_e */ 1310.0,/* Ïƒ_th */ 49700.0,/* Î”_keV */ -87998.0,  /* Sn */ 8536.4 },

    {65, 159, "Tb", "Terbium",    "Tb-159","hcp",
      NuclearPhaseCategory::Lanthanide, DecayMode::Stable,
      8.3059,  24.5,   25.0,  0.0,   false, false,
      158.925, 1629.0, 3503.0,  11.1,   182.0,
      20.786,  420.0,   5.864,
      /* T_D */ 177.0, /* r_at */ 177.0,  /* Ï‡ */ 1.10,  /* EA */ 0.500,
      /* Ï_e */ 1150.0,/* Ïƒ_th */ 23.4,   /* Î”_keV */ -84604.0,  /* Sn */ 8635.5 },

    {66, 164, "Dy", "Dysprosium", "Dy-164","hcp",
      NuclearPhaseCategory::Lanthanide, DecayMode::Stable,
      8.2946,  24.6,   25.0,  0.0,   false, false,
      162.500, 1680.0, 2840.0,  10.7,   167.0,
      20.786,  420.0,   5.939,
      /* T_D */ 183.0, /* r_at */ 178.0,  /* Ï‡ */ 1.22,  /* EA */ 0.500,
      /* Ï_e */ 926.0, /* Ïƒ_th */ 920.0,  /* Î”_keV */ -86359.0,  /* Sn */ 7658.2 },

    {67, 165, "Ho", "Holmium",    "Ho-165","hcp",
      NuclearPhaseCategory::Lanthanide, DecayMode::Stable,
      8.2822,  24.8,   25.0,  0.0,   false, false,
      164.930, 1734.0, 2993.0,  16.2,   165.0,
      20.786,  420.0,   6.022,
      /* T_D */ 190.0, /* r_at */ 176.0,  /* Ï‡ */ 1.23,  /* EA */ 0.500,
      /* Ï_e */ 814.0, /* Ïƒ_th */ 64.7,   /* Î”_keV */ -84904.0,  /* Sn */ 7975.8 },

    {68, 166, "Er", "Erbium",     "Er-166","hcp",
      NuclearPhaseCategory::Lanthanide, DecayMode::Stable,
      8.2684,  24.9,   25.0,  0.0,   false, false,
      167.259, 1802.0, 3141.0,  14.5,   168.0,
      20.786,  420.0,   6.108,
      /* T_D */ 188.0, /* r_at */ 176.0,  /* Ï‡ */ 1.24,  /* EA */ 0.500,
      /* Ï_e */ 860.0, /* Ïƒ_th */ 159.0,  /* Î”_keV */ -85699.0,  /* Sn */ 8481.0 },

    {69, 169, "Tm", "Thulium",    "Tm-169","hcp",
      NuclearPhaseCategory::Lanthanide, DecayMode::Stable,
      8.2569,  25.1,   25.0,  0.0,   false, false,
      168.934, 1818.0, 2223.0,  16.9,   160.0,
      20.786,  420.0,   6.184,
      /* T_D */ 200.0, /* r_at */ 176.0,  /* Ï‡ */ 1.25,  /* EA */ 0.500,
      /* Ï_e */ 676.0, /* Ïƒ_th */ 100.0,  /* Î”_keV */ -83281.0,  /* Sn */ 8020.3 },

    {70, 174, "Yb", "Ytterbium",  "Yb-174","fcc",
      NuclearPhaseCategory::Lanthanide, DecayMode::Stable,
      8.2397,  24.9,   25.0,  0.0,   false, false,
      173.045, 1097.0, 1469.0,  38.5,   155.0,
      20.786,  420.0,   6.254,
      /* T_D */ 120.0, /* r_at */ 194.0,  /* Ï‡ */ 1.10,  /* EA */ 0.500,
      /* Ï_e */ 250.0, /* Ïƒ_th */ 34.8,   /* Î”_keV */ -84254.0,  /* Sn */ 8483.0 },

    {71, 175, "Lu", "Lutetium",   "Lu-175","hcp",
      NuclearPhaseCategory::Lanthanide, DecayMode::Stable,
      8.2266,  25.1,   25.0,  0.0,   false, false,
      174.967, 1925.0, 3675.0,  16.4,   154.0,
      20.786,  420.0,   5.426,
      /* T_D */ 183.0, /* r_at */ 173.0,  /* Ï‡ */ 1.27,  /* EA */ 0.500,
      /* Ï_e */ 582.0, /* Ïƒ_th */ 74.0,   /* Î”_keV */ -83275.0,  /* Sn */ 8218.1 },

    {72, 180, "Hf", "Hafnium",    "Hf-180","hcp",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.0776,  25.8,   40.0,  0.0,   false, false,
      178.490, 2506.0, 4876.0,  23.0,   144.0,
      20.786,  420.0,   6.825,
      /* T_D */ 252.0, /* r_at */ 159.0,  /* Ï‡ */ 1.30,  /* EA */ 0.017,
      /* Ï_e */ 331.0, /* Ïƒ_th */ 104.0,  /* Î”_keV */ -85619.0,  /* Sn */ 7621.9 },

    {73, 181, "Ta", "Tantalum",   "Ta-181","bcc",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.0730,  26.2,   90.0,  0.0,   false, false,
      180.948, 3290.0, 5731.0,  57.5,   140.0,
      20.786,  420.0,   7.550,
      /* T_D */ 240.0, /* r_at */ 146.0,  /* Ï‡ */ 1.50,  /* EA */ 0.322,
      /* Ï_e */ 131.0, /* Ïƒ_th */ 20.6,   /* Î”_keV */ -84010.0,  /* Sn */ 7585.0 },

    {74, 184, "W",  "Tungsten",   "W-184", "bcc",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.0671,  26.6,   90.0,  0.0,   false, false,
      183.840, 3695.0, 5828.0, 173.0,   132.0,
      20.786,  420.0,   7.864,
      /* T_D */ 400.0, /* r_at */ 139.0,  /* Ï‡ */ 2.36,  /* EA */ 0.815,
      /* Ï_e */ 52.8,  /* Ïƒ_th */ 18.3,   /* Î”_keV */ -85752.0,  /* Sn */ 7411.7 },

    {75, 187, "Re", "Rhenium",    "Re-187","hcp",
      NuclearPhaseCategory::TransitionMetal, DecayMode::BetaMinus,
      8.0615,  26.8,   40.0,  1.41e18, false, false,
      186.207, 3459.0, 5869.0,  47.9,   137.0,
      20.786,  420.0,   7.834,
      /* T_D */ 430.0, /* r_at */ 137.0,  /* Ï‡ */ 1.90,  /* EA */ 0.150,
      /* Ï_e */ 193.0, /* Ïƒ_th */ 89.7,   /* Î”_keV */ -85515.0,  /* Sn */ 7364.9 },

    {76, 192, "Os", "Osmium",     "Os-192","hcp",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.0541,  27.0,   40.0,  0.0,   false, false,
      190.230, 3306.0, 5285.0,  87.6,   130.0,
      20.786,  420.0,   8.438,
      /* T_D */ 500.0, /* r_at */ 135.0,  /* Ï‡ */ 2.20,  /* EA */ 1.078,
      /* Ï_e */ 81.2,  /* Ïƒ_th */ 16.0,   /* Î”_keV */ -85820.0,  /* Sn */ 7791.6 },

    {77, 193, "Ir", "Iridium",    "Ir-193","fcc",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.0458,  27.0,   40.0,  0.0,   false, false,
      192.217, 2719.0, 4701.0, 147.0,   131.0,
      20.786,  420.0,   8.967,
      /* T_D */ 420.0, /* r_at */ 136.0,  /* Ï‡ */ 2.20,  /* EA */ 1.565,
      /* Ï_e */ 47.1,  /* Ïƒ_th */ 425.0,  /* Î”_keV */ -84560.0,  /* Sn */ 7390.3 },

    {78, 195, "Pt", "Platinum",   "Pt-195","fcc",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      8.0323,  27.1,   40.0,  0.0,   false, false,
      195.084, 2041.0, 4098.0,  71.6,   133.0,
      20.786,  420.0,   8.959,
      /* T_D */ 240.0, /* r_at */ 139.0,  /* Ï‡ */ 2.28,  /* EA */ 2.128,
      /* Ï_e */ 105.0, /* Ïƒ_th */ 10.3,   /* Î”_keV */ -86017.0,  /* Sn */ 7922.0 },

    {79, 197, "Au", "Gold",       "Au-197","fcc",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      7.9156,  26.8,   35.0,  0.0,   false, false,
      196.967, 1337.0, 3129.0, 318.0,   129.0,
      20.786,  400.0,   9.226,
      /* T_D */ 165.0, /* r_at */ 144.0,  /* Ï‡ */ 2.54,  /* EA */ 2.309,
      /* Ï_e */ 22.1,  /* Ïƒ_th */ 98.7,   /* Î”_keV */ -87011.0,  /* Sn */ 8071.5 },

    {80, 202, "Hg", "Mercury",    "Hg-202","rhombohedral",
      NuclearPhaseCategory::TransitionMetal, DecayMode::Stable,
      7.8474,  26.3,   25.0,  0.0,   false, false,
      200.592, 234.3,   629.9,   8.3,   140.0,
      20.786,  400.0,  10.437,
      /* T_D */ 100.0, /* r_at */ 151.0,  /* Ï‡ */ 2.00,  /* EA */ 0.0,
      /* Ï_e */ 958.0, /* Ïƒ_th */ 374.0,  /* Î”_keV */ -85393.0,  /* Sn */ 7455.7 },

    {81, 205, "Tl", "Thallium",   "Tl-205","hcp",
      NuclearPhaseCategory::PostTransition, DecayMode::Stable,
      8.0827,  26.5,   25.0,  0.0,   false, false,
      204.383, 577.0,  1746.0,  46.1,   129.0,
      20.786,  400.0,   6.108,
      /* T_D */ 78.0,  /* r_at */ 170.0,  /* Ï‡ */ 1.62,  /* EA */ 0.321,
      /* Ï_e */ 180.0, /* Ïƒ_th */ 3.43,   /* Î”_keV */ -87444.0,  /* Sn */ 7391.0 },

    {82, 208, "Pb", "Lead",       "Pb-208","fcc",
      NuclearPhaseCategory::PostTransition, DecayMode::Stable,
      7.8698,  26.4,   25.0,  0.0,   false, false,
      207.200, 600.6,  2022.0,  35.3,   128.0,
      20.786,  400.0,   7.417,
      /* T_D */ 105.0, /* r_at */ 175.0,  /* Ï‡ */ 2.33,  /* EA */ 0.364,
      /* Ï_e */ 208.0, /* Ïƒ_th */ 0.171,  /* Î”_keV */ -87567.0,  /* Sn */ 7367.6 },

    {83, 209, "Bi", "Bismuth",    "Bi-209","rhombohedral",
      NuclearPhaseCategory::PostTransition, DecayMode::Alpha,
      7.8316,  26.5,   25.0,  5.99e26, false, false,
      208.980, 544.6,  1837.0,   7.97,  128.0,
      20.786,  400.0,   7.289,
      /* T_D */ 119.0, /* r_at */ 155.0,  /* Ï‡ */ 2.02,  /* EA */ 0.946,
      /* Ï_e */ 1290.0,/* Ïƒ_th */ 0.034,  /* Î”_keV */ -83461.0,  /* Sn */ 7460.5 },

    {84, 209, "Po", "Polonium",   "Po-209","simple-cubic",
      NuclearPhaseCategory::Metalloid, DecayMode::Alpha,
      7.8078,  26.6,   25.0,  1.005e7, false, false,
      209.000, 527.0,  1235.0,  20.0,   127.0,
      20.786,  420.0,   8.417,
      /* T_D */ 81.0,  /* r_at */ 167.0,  /* Ï‡ */ 2.00,  /* EA */ 1.900,
      /* Ï_e */ 400.0, /* Ïƒ_th */ 0.5,    /* Î”_keV */ -81521.0,  /* Sn */ 8118.0 },

    {85, 210, "At", "Astatine",   "At-210","unknown",
      NuclearPhaseCategory::Halogen, DecayMode::Alpha,
      7.7840,  26.5,   25.0,  2.88e4,  false, false,
      210.000, 575.0,   610.0,   1.7,   127.0,
      20.786,  420.0,   9.318,
      /* T_D */ 0.0,   /* r_at */ 202.0,  /* Ï‡ */ 2.20,  /* EA */ 2.800,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 0.0,    /* Î”_keV */ -78220.0,  /* Sn */ 8360.0 },

    {86, 222, "Rn", "Radon",      "Rn-222","gas",
      NuclearPhaseCategory::NobleGas, DecayMode::Alpha,
      7.7554,  26.4,    3.0,  3.302e5, false, false,
      222.000, 202.0,   211.3,   0.00364, 440.0,
      20.786,  440.0,  10.749,
      /* T_D */ 64.0,  /* r_at */ 120.0,  /* Ï‡ */ 0.0,   /* EA */ 0.0,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 0.70,   /* Î”_keV */ -76193.0,  /* Sn */ 8175.0 },

    {87, 223, "Fr", "Francium",   "Fr-223","bcc",
      NuclearPhaseCategory::AlkaliMetal, DecayMode::BetaMinus,
      7.7224,  26.3,   25.0,  1320.0,  false, false,
      223.000, 300.0,   950.0,  15.0,   136.0,
      20.786,  440.0,   4.073,
      /* T_D */ 0.0,   /* r_at */ 348.0,  /* Ï‡ */ 0.70,  /* EA */ 0.486,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 0.0,    /* Î”_keV */ -72940.0,  /* Sn */ 8280.0 },

    {88, 226, "Ra", "Radium",     "Ra-226","bcc",
      NuclearPhaseCategory::AlkalineEarth, DecayMode::Alpha,
      7.6631,  26.2,   25.0,  5.06e10, false, false,
      226.000, 973.0,  2010.0,  18.6,   122.0,
      20.786,  440.0,   5.279,
      /* T_D */ 72.0,  /* r_at */ 283.0,  /* Ï‡ */ 0.89,  /* EA */ 0.100,
      /* Ï_e */ 1000.0,/* Ïƒ_th */ 12.8,   /* Î”_keV */ -71169.0,  /* Sn */ 9358.2 },

    {89, 227, "Ac", "Actinium",   "Ac-227","fcc",
      NuclearPhaseCategory::Actinide, DecayMode::BetaMinus,
      7.6476,  26.5,   35.0,  6.867e8, false, false,
      227.000, 1323.0, 3471.0,  12.0,   122.0,
      20.786,  440.0,   5.380,
      /* T_D */ 120.0, /* r_at */ 215.0,  /* Ï‡ */ 1.10,  /* EA */ 0.350,
      /* Ï_e */ 1000.0,/* Ïƒ_th */ 880.0,  /* Î”_keV */ -69378.0,  /* Sn */ 9003.3 },

    {90, 232, "Th", "Thorium",    "Th-232","fcc",
      NuclearPhaseCategory::Actinide, DecayMode::Alpha,
      7.6299,  26.8,   25.0,  4.42e17, false, true,
      232.038, 2023.0, 5061.0,  54.0,   118.0,
      20.786,  440.0,   6.308,
      /* T_D */ 163.0, /* r_at */ 206.0,  /* Ï‡ */ 1.30,  /* EA */ 0.608,
      /* Ï_e */ 147.0, /* Ïƒ_th */ 7.37,   /* Î”_keV */ -35445.6,  /* Sn */ 8853.0 },

    {91, 231, "Pa", "Protactinium","Pa-231","bct",
      NuclearPhaseCategory::Actinide, DecayMode::Alpha,
      7.6108,  27.0,   35.0,  1.034e12, false, false,
      231.036, 1841.0, 4300.0,  47.0,   118.0,
      20.786,  440.0,   5.890,
      /* T_D */ 178.0, /* r_at */ 200.0,  /* Ï‡ */ 1.50,  /* EA */ 0.550,
      /* Ï_e */ 177.0, /* Ïƒ_th */ 200.6,  /* Î”_keV */ -36197.0,  /* Sn */ 8440.0 },

    {92, 238, "U",  "Uranium",    "U-238", "orthorhombic-alpha",
      NuclearPhaseCategory::Actinide, DecayMode::Alpha,
      7.5907,  27.2,   40.0,  1.408e17, false, true,
      238.029, 1405.0, 4404.0,  27.5,   116.0,
      20.786,  440.0,   6.194,
      /* T_D */ 207.0, /* r_at */ 196.0,  /* Ï‡ */ 1.38,  /* EA */ 0.530,
      /* Ï_e */ 280.0, /* Ïƒ_th */ 7.57,   /* Î”_keV */ 47309.0,   /* Sn */ 5298.4 },

    {93, 237, "Np", "Neptunium",  "Np-237","orthorhombic",
      NuclearPhaseCategory::Actinide, DecayMode::Alpha,
      7.5727,  27.5,   35.0,  6.76e13,  false, false,
      237.048, 917.0,  4273.0,   6.3,   118.0,
      20.786,  440.0,   6.266,
      /* T_D */ 168.0, /* r_at */ 190.0,  /* Ï‡ */ 1.36,  /* EA */ 0.480,
      /* Ï_e */ 1220.0,/* Ïƒ_th */ 175.9,  /* Î”_keV */ 44874.0,   /* Sn */ 5489.6 },

    {94, 239, "Pu", "Plutonium",  "Pu-239","monoclinic-alpha",
      NuclearPhaseCategory::Actinide, DecayMode::Alpha,
      7.5601,  37.03, 35.0,  7.626e11, true, false,
      244.064, 912.5,  3505.0,   6.74,  130.0,
      20.786,  440.0,   6.026,
      /* T_D */ 161.0, /* r_at */ 187.0,  /* Ï‡ */ 1.28,  /* EA */ 0.520,
      /* Ï_e */ 1460.0,/* Ïƒ_th */ 1017.3, /* Î”_keV */ 48590.0,   /* Sn */ 5645.6 },

    {95, 243, "Am", "Americium",  "Am-243","dhcp",
      NuclearPhaseCategory::Actinide, DecayMode::Alpha,
      7.5296,  37.2,  35.0,  2.32e11,  true, false,
      243.061, 1449.0, 2880.0,  10.0,   118.0,
      20.786,  440.0,   5.974,
      /* T_D */ 155.0, /* r_at */ 180.0,  /* Ï‡ */ 1.13,  /* EA */ 0.100,
      /* Ï_e */ 680.0, /* Ïƒ_th */ 75.3,   /* Î”_keV */ 55460.0,   /* Sn */ 5362.5 },

    {96, 247, "Cm", "Curium",     "Cm-247","dhcp",
      NuclearPhaseCategory::Actinide, DecayMode::Alpha,
      7.5192,  37.3,  35.0,  4.93e14,  false, false,
      247.070, 1613.0, 3383.0,  10.0,   118.0,
      20.786,  440.0,   5.991,
      /* T_D */ 144.0, /* r_at */ 169.0,  /* Ï‡ */ 1.28,  /* EA */ 0.100,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 60.0,   /* Î”_keV */ 67395.0,   /* Sn */ 5330.0 },

    {97, 247, "Bk", "Berkelium",  "Bk-247","dhcp",
      NuclearPhaseCategory::Actinide, DecayMode::Alpha,
      7.5094,  37.5,  35.0,  4.35e10,  false, false,
      247.070, 1259.0, 2900.0,  10.0,   118.0,
      20.786,  440.0,   6.197,
      /* T_D */ 130.0, /* r_at */ 170.0,  /* Ï‡ */ 1.30,  /* EA */ 0.100,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 710.0,  /* Î”_keV */ 73990.0,   /* Sn */ 5170.0 },

    {98, 252, "Cf", "Californium","Cf-252","dhcp",
      NuclearPhaseCategory::Actinide, DecayMode::Alpha,
      7.4859,  37.7,  35.0,  8.339e7,  false, false,
      252.082, 1173.0, 1743.0,  10.0,   118.0,
      20.786,  440.0,   6.282,
      /* T_D */ 118.0, /* r_at */ 169.0,  /* Ï‡ */ 1.30,  /* EA */ 0.100,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 2900.0, /* Î”_keV */ 82620.0,   /* Sn */ 4809.0 },

    {99, 252, "Es", "Einsteinium","Es-252","fcc",
      NuclearPhaseCategory::Actinide, DecayMode::Alpha,
      7.4624,  37.9,  35.0,  4.07e7,   false, false,
      252.083, 1133.0, 1269.0,  10.0,   118.0,
      20.786,  440.0,   6.420,
      /* T_D */ 110.0, /* r_at */ 186.0,  /* Ï‡ */ 1.30,  /* EA */ 0.100,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 160.0,  /* Î”_keV */ 93560.0,   /* Sn */ 4780.0 },

    {100,257, "Fm", "Fermium",    "Fm-257","unknown",
      NuclearPhaseCategory::Actinide, DecayMode::Alpha,
      7.4349,  38.0,  35.0,  8.683e6,  false, false,
      257.095, 1800.0, 2800.0,  10.0,   118.0,
      20.786,  440.0,   6.500,
      /* T_D */ 100.0, /* r_at */ 180.0,  /* Ï‡ */ 1.30,  /* EA */ 0.100,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 33.0,   /* Î”_keV */ 99890.0,   /* Sn */ 4720.0 },

    {101,258, "Md", "Mendelevium","Md-258","unknown",
      NuclearPhaseCategory::Actinide, DecayMode::Alpha,
      7.4189,  38.2,  35.0,  4.45e6,   false, false,
      258.098, 1100.0, 2200.0,  10.0,   118.0,
      20.786,  440.0,   6.580,
      /* T_D */ 95.0,  /* r_at */ 181.0,  /* Ï‡ */ 1.30,  /* EA */ 0.100,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 0.0,    /* Î”_keV */ 107930.0,  /* Sn */ 4660.0 },

    {102,259, "No", "Nobelium",   "No-259","unknown",
      NuclearPhaseCategory::Actinide, DecayMode::Alpha,
      7.4034,  38.4,  35.0,  3.48e3,   false, false,
      259.101, 1100.0, 2200.0,  10.0,   118.0,
      20.786,  440.0,   6.650,
      /* T_D */ 90.0,  /* r_at */ 182.0,  /* Ï‡ */ 1.30,  /* EA */ 0.100,
      /* Ï_e */ 0.0,   /* Ïƒ_th */ 0.0,    /* Î”_keV */ 115010.0,  /* Sn */ 4600.0 },
};

inline constexpr int NUCLEAR_SPECIES_COUNT = 101; // Z=2..102 inclusive

// ============================================================================
// O(1) lookup by Z -- returns pointer into static array, or nullptr
// ============================================================================

inline const NuclearSpecies* nuclear_species_ptr(int Z) noexcept {
    if (Z < 2 || Z > 102) return nullptr;
    return &NUCLEAR_SPECIES_DB[Z - 2];
}

// Reference version (asserts valid Z via array indexing)
inline const NuclearSpecies& nuclear_species(uint8_t Z) noexcept {
    return NUCLEAR_SPECIES_DB[Z - 2];
}

// ============================================================================
// Category / decay name helpers
// ============================================================================

inline std::string_view nuclear_phase_category_name(NuclearPhaseCategory c) {
    switch (c) {
        case NuclearPhaseCategory::NobleGas:        return "Noble Gas";
        case NuclearPhaseCategory::AlkaliMetal:     return "Alkali Metal";
        case NuclearPhaseCategory::AlkalineEarth:   return "Alkaline Earth";
        case NuclearPhaseCategory::TransitionMetal: return "Transition Metal";
        case NuclearPhaseCategory::Lanthanide:      return "Lanthanide";
        case NuclearPhaseCategory::Actinide:        return "Actinide";
        case NuclearPhaseCategory::PostTransition:  return "Post-Transition";
        case NuclearPhaseCategory::Metalloid:       return "Metalloid";
        case NuclearPhaseCategory::Nonmetal:        return "Nonmetal";
        case NuclearPhaseCategory::Halogen:         return "Halogen";
        default:                                    return "Unknown";
    }
}

inline std::string_view decay_mode_name(DecayMode d) {
    switch (d) {
        case DecayMode::Stable:    return "Stable";
        case DecayMode::Alpha:     return "a";
        case DecayMode::BetaMinus: return "b-";
        case DecayMode::BetaPlus:  return "b+/EC";
        case DecayMode::IT:        return "IT";
        case DecayMode::Fission:   return "SF";
        case DecayMode::Multiple:  return "Multiple";
        default:                   return "?";
    }
}

// ============================================================================
// Kinetic diameter helper
// ============================================================================

inline double d_kinetic_m(const NuclearSpecies& ns) {
    return ns.d_kinetic_pm * 1.0e-12;
}

} // namespace gas2
} // namespace vsepr
