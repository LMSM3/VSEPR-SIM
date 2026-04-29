#pragma once
/**
 * metal_registry.hpp — Canonical Metals Research Registry
 *
 * Authoritative per-element data for the metals research layer.
 * Covers FCC noble metals, FCC transition metals, BCC refractory metals,
 * and HCP-proxy metals — plus binary alloy pair descriptors.
 *
 * Crystal structure abbreviations:
 *   FCC  — face-centred cubic  (CN_bulk = 12)
 *   BCC  — body-centred cubic  (CN_bulk = 8)
 *   HCP  — hexagonal close-packed (modelled as FCC proxy, CN_bulk = 12)
 *
 * All energies in kcal/mol (1 eV = 23.0605 kcal/mol).
 * All lengths in Angstrom.
 * All temperatures in Kelvin.
 *
 * Sources:
 *   - CRC Handbook of Chemistry and Physics, 105th ed.
 *   - Kittel, Introduction to Solid State Physics, 8th ed.
 *   - Tyson & Miller, Surf. Sci. 62, 267 (1977)
 *   - Baletto & Ferrando, Rev. Mod. Phys. 77, 371 (2005)
 *
 * Anti-black-box: every field is labelled with its physical meaning
 * and traceable to a published source.
 */

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace coarse_grain {
namespace metals {

constexpr double EV_TO_KCAL = 23.0605;   // 1 eV → kcal/mol
constexpr double J_M2_TO_KCAL_ANG2 = 1.4393e-3; // 1 J/m² → kcal/(mol·Å²)

// ============================================================================
// Crystal Structure
// ============================================================================

enum class CrystalStructure {
    FCC,        ///< Face-centred cubic  — Au, Ag, Cu, Ni, Pt, Al, Pb
    BCC,        ///< Body-centred cubic  — Fe, Cr, W, Mo, V, Nb, Ta
    HCP         ///< Hexagonal close-packed (proxy) — Co, Ti, Zr, Mg
};

inline const char* crystal_structure_name(CrystalStructure cs) {
    switch (cs) {
        case CrystalStructure::FCC: return "FCC";
        case CrystalStructure::BCC: return "BCC";
        case CrystalStructure::HCP: return "HCP";
    }
    return "Unknown";
}

// ============================================================================
// MetalRecord — single-element canonical data
// ============================================================================

struct MetalRecord {
    // Identity
    uint32_t    Z{};                    ///< Atomic number
    std::string symbol;                 ///< Element symbol
    std::string name;                   ///< Full element name
    CrystalStructure structure{CrystalStructure::FCC};

    // Structural
    double lattice_constant_ang{};      ///< a₀ (Å) at 300 K
    double atomic_radius_ang{};         ///< Wigner–Seitz / metallic radius (Å)
    double atomic_mass_amu{};           ///< Standard atomic weight (amu)
    double bulk_CN{};                   ///< Bulk coordination number
    double surface_CN{};                ///< Mean surface coordination (mixed facets)

    // Energetic
    double cohesive_energy_ev{};        ///< E_coh per atom (eV, negative = bound)
    double surface_energy_J_m2{};       ///< γ_surface (J/m²) — (111)/(110) mean
    double edge_energy_ev{};            ///< Excess energy per edge atom (eV)

    // Thermal
    double melting_point_K{};           ///< Bulk T_melt (K)
    double debye_temperature_K{};       ///< Θ_D (K)
    double thermal_conductivity_W_mK{}; ///< κ (W/m·K) at 300 K
    double linear_exp_coeff_1e6K{};     ///< α_L (×10⁻⁶ K⁻¹) at 300 K

    // Electronic (qualitative)
    double electronegativity_pauling{};  ///< χ_P (Pauling scale)
    double work_function_ev{};           ///< φ (eV) — polycrystalline average
    double fermi_energy_ev{};            ///< E_F (eV)

    // Mechanical
    double bulk_modulus_GPa{};          ///< B (GPa)
    double shear_modulus_GPa{};         ///< G (GPa)
    double youngs_modulus_GPa{};        ///< E (GPa)
    double poisson_ratio{};             ///< ν

    // LJ proxy parameters (derived from E_coh and a₀)
    double lj_sigma_ang{};              ///< σ = a₀ / 2^(1/6)
    double lj_epsilon_kcal{};           ///< ε = |E_coh| / (CN_bulk / 2)

    // Research flags
    bool is_noble_metal{false};          ///< Ag, Au, Pt, Pd — corrosion resistant
    bool is_refractory{false};           ///< W, Mo, Re, Ta, Nb — high T_melt
    bool is_magnetic{false};             ///< Fe, Co, Ni — ferromagnetic at 300 K
    std::string source;                  ///< Primary bibliographic reference

    // Radiation interaction (Z-dependent, deterministic)
    double displacement_energy_ev{25.0}; ///< E_d threshold for Frenkel pair (eV)
    double mu_mass_100keV_cm2g{};        ///< Mass attenuation μ/ρ at 100 keV (cm²/g)
    double k_edge_keV{};                 ///< K-absorption edge (keV)

    // Energetic / combustion properties
    double heat_of_combustion_kJ_g{};    ///< ΔH_comb (kJ/g) in O₂ environment
    double ignition_temperature_K{};     ///< T_ign for micron-scale powder in air (K)
    double oxide_formation_enthalpy_eV{};///< ΔH_f per metal atom for primary oxide (eV)
    std::string primary_oxide;           ///< Most stable oxide formula
    std::string flame_colour;            ///< Characteristic combustion emission colour
};

// ============================================================================
// Derived LJ parameters helper
// ============================================================================

inline void fill_lj_params(MetalRecord& m) {
    if (m.lattice_constant_ang > 0.0)
        m.lj_sigma_ang = m.lattice_constant_ang / 1.122462; // 2^(1/6)
    double n_pairs = m.bulk_CN / 2.0;
    if (n_pairs > 0.0 && m.cohesive_energy_ev != 0.0)
        m.lj_epsilon_kcal = std::abs(m.cohesive_energy_ev) * EV_TO_KCAL / n_pairs;
}

// ============================================================================
// FCC Noble / Near-Noble Metals
// ============================================================================

inline MetalRecord gold() {
    MetalRecord m;
    m.Z = 79; m.symbol = "Au"; m.name = "Gold";
    m.structure = CrystalStructure::FCC;
    m.lattice_constant_ang   = 4.078;
    m.atomic_radius_ang      = 1.44;
    m.atomic_mass_amu        = 196.967;
    m.bulk_CN                = 12.0;
    m.surface_CN             = 8.5;
    m.cohesive_energy_ev     = -3.81;
    m.surface_energy_J_m2    = 1.50;
    m.edge_energy_ev         = 0.80;
    m.melting_point_K        = 1337.3;
    m.debye_temperature_K    = 165.0;
    m.thermal_conductivity_W_mK = 317.0;
    m.linear_exp_coeff_1e6K  = 14.2;
    m.electronegativity_pauling = 2.54;
    m.work_function_ev       = 5.10;
    m.fermi_energy_ev        = 5.53;
    m.bulk_modulus_GPa       = 180.0;
    m.shear_modulus_GPa      = 27.0;
    m.youngs_modulus_GPa     = 79.0;
    m.poisson_ratio          = 0.44;
    m.is_noble_metal         = true;
    m.source = "Kittel8+Tyson1977+CRC105";
    // Radiation: Au Z=79, high attenuation, K-edge at 80.7 keV
    m.displacement_energy_ev  = 36.0;   // Displaced.org recommended
    m.mu_mass_100keV_cm2g     = 5.16;   // NIST XCOM at 100 keV
    m.k_edge_keV              = 80.7;
    // Energetic: Au does not combust easily (noble)
    m.heat_of_combustion_kJ_g = 0.0;    // Not applicable — noble metal
    m.ignition_temperature_K  = 0.0;
    m.oxide_formation_enthalpy_eV = -0.32; // Au₂O₃ very weakly bound
    m.primary_oxide           = "Au2O3";
    m.flame_colour            = "N/A (noble, no combustion)";
    fill_lj_params(m);
    return m;
}

inline MetalRecord silver() {
    MetalRecord m;
    m.Z = 47; m.symbol = "Ag"; m.name = "Silver";
    m.structure = CrystalStructure::FCC;
    m.lattice_constant_ang   = 4.086;
    m.atomic_radius_ang      = 1.44;
    m.atomic_mass_amu        = 107.868;
    m.bulk_CN                = 12.0;
    m.surface_CN             = 8.5;
    m.cohesive_energy_ev     = -2.95;
    m.surface_energy_J_m2    = 1.25;
    m.edge_energy_ev         = 0.60;
    m.melting_point_K        = 1234.9;
    m.debye_temperature_K    = 225.0;
    m.thermal_conductivity_W_mK = 429.0;
    m.linear_exp_coeff_1e6K  = 18.9;
    m.electronegativity_pauling = 1.93;
    m.work_function_ev       = 4.26;
    m.fermi_energy_ev        = 5.49;
    m.bulk_modulus_GPa       = 103.0;
    m.shear_modulus_GPa      = 30.0;
    m.youngs_modulus_GPa     = 83.0;
    m.poisson_ratio          = 0.37;
    m.is_noble_metal         = true;
    m.source = "Kittel8+Tyson1977+CRC105";
    m.displacement_energy_ev  = 25.0;
    m.mu_mass_100keV_cm2g     = 2.32;   // NIST XCOM
    m.k_edge_keV              = 25.5;
    m.heat_of_combustion_kJ_g = 0.0;
    m.ignition_temperature_K  = 0.0;
    m.oxide_formation_enthalpy_eV = -0.32;
    m.primary_oxide           = "Ag2O";
    m.flame_colour            = "N/A (noble)";
    fill_lj_params(m);
    return m;
}

inline MetalRecord copper() {
    MetalRecord m;
    m.Z = 29; m.symbol = "Cu"; m.name = "Copper";
    m.structure = CrystalStructure::FCC;
    m.lattice_constant_ang   = 3.615;
    m.atomic_radius_ang      = 1.28;
    m.atomic_mass_amu        = 63.546;
    m.bulk_CN                = 12.0;
    m.surface_CN             = 8.0;
    m.cohesive_energy_ev     = -3.49;
    m.surface_energy_J_m2    = 1.79;
    m.edge_energy_ev         = 0.70;
    m.melting_point_K        = 1357.8;
    m.debye_temperature_K    = 343.0;
    m.thermal_conductivity_W_mK = 401.0;
    m.linear_exp_coeff_1e6K  = 16.5;
    m.electronegativity_pauling = 1.90;
    m.work_function_ev       = 4.65;
    m.fermi_energy_ev        = 7.04;
    m.bulk_modulus_GPa       = 142.0;
    m.shear_modulus_GPa      = 48.0;
    m.youngs_modulus_GPa     = 130.0;
    m.poisson_ratio          = 0.34;
    m.source = "Kittel8+Tyson1977+CRC105";
    m.displacement_energy_ev  = 22.0;   // Cu displacement threshold
    m.mu_mass_100keV_cm2g     = 0.458;  // NIST XCOM
    m.k_edge_keV              = 8.98;
    m.heat_of_combustion_kJ_g = 2.50;   // Cu → CuO
    m.ignition_temperature_K  = 1173.0; // micron-scale powder
    m.oxide_formation_enthalpy_eV = -1.63; // CuO per Cu atom
    m.primary_oxide           = "CuO";
    m.flame_colour            = "blue-green";
    fill_lj_params(m);
    return m;
}

inline MetalRecord platinum() {
    MetalRecord m;
    m.Z = 78; m.symbol = "Pt"; m.name = "Platinum";
    m.structure = CrystalStructure::FCC;
    m.lattice_constant_ang   = 3.924;
    m.atomic_radius_ang      = 1.39;
    m.atomic_mass_amu        = 195.084;
    m.bulk_CN                = 12.0;
    m.surface_CN             = 8.5;
    m.cohesive_energy_ev     = -5.84;
    m.surface_energy_J_m2    = 2.49;
    m.edge_energy_ev         = 1.00;
    m.melting_point_K        = 2041.4;
    m.debye_temperature_K    = 240.0;
    m.thermal_conductivity_W_mK = 71.6;
    m.linear_exp_coeff_1e6K  = 8.8;
    m.electronegativity_pauling = 2.28;
    m.work_function_ev       = 5.65;
    m.fermi_energy_ev        = 5.36;
    m.bulk_modulus_GPa       = 278.0;
    m.shear_modulus_GPa      = 61.0;
    m.youngs_modulus_GPa     = 168.0;
    m.poisson_ratio          = 0.38;
    m.is_noble_metal         = true;
    m.is_refractory          = true;
    m.source = "Kittel8+Tyson1977+CRC105";
    m.displacement_energy_ev  = 34.0;
    m.mu_mass_100keV_cm2g     = 4.99;   // NIST XCOM
    m.k_edge_keV              = 78.4;
    m.heat_of_combustion_kJ_g = 0.0;
    m.ignition_temperature_K  = 0.0;
    m.oxide_formation_enthalpy_eV = -0.75; // PtO₂
    m.primary_oxide           = "PtO2";
    m.flame_colour            = "N/A (noble)";
    fill_lj_params(m);
    return m;
}

inline MetalRecord nickel() {
    MetalRecord m;
    m.Z = 28; m.symbol = "Ni"; m.name = "Nickel";
    m.structure = CrystalStructure::FCC;
    m.lattice_constant_ang   = 3.524;
    m.atomic_radius_ang      = 1.25;
    m.atomic_mass_amu        = 58.693;
    m.bulk_CN                = 12.0;
    m.surface_CN             = 8.0;
    m.cohesive_energy_ev     = -4.44;
    m.surface_energy_J_m2    = 2.08;
    m.edge_energy_ev         = 0.85;
    m.melting_point_K        = 1728.0;
    m.debye_temperature_K    = 450.0;
    m.thermal_conductivity_W_mK = 90.7;
    m.linear_exp_coeff_1e6K  = 13.4;
    m.electronegativity_pauling = 1.91;
    m.work_function_ev       = 5.15;
    m.fermi_energy_ev        = 7.41;
    m.bulk_modulus_GPa       = 180.0;
    m.shear_modulus_GPa      = 76.0;
    m.youngs_modulus_GPa     = 200.0;
    m.poisson_ratio          = 0.31;
    m.is_magnetic            = true;
    m.source = "Kittel8+CRC105";
    m.displacement_energy_ev  = 23.0;
    m.mu_mass_100keV_cm2g     = 0.441;  // NIST XCOM
    m.k_edge_keV              = 8.33;
    m.heat_of_combustion_kJ_g = 4.10;   // Ni → NiO
    m.ignition_temperature_K  = 1223.0;
    m.oxide_formation_enthalpy_eV = -2.49; // NiO per Ni atom
    m.primary_oxide           = "NiO";
    m.flame_colour            = "silvery-white";
    fill_lj_params(m);
    return m;
}

inline MetalRecord aluminium() {
    MetalRecord m;
    m.Z = 13; m.symbol = "Al"; m.name = "Aluminium";
    m.structure = CrystalStructure::FCC;
    m.lattice_constant_ang   = 4.050;
    m.atomic_radius_ang      = 1.43;
    m.atomic_mass_amu        = 26.982;
    m.bulk_CN                = 12.0;
    m.surface_CN             = 8.0;
    m.cohesive_energy_ev     = -3.39;
    m.surface_energy_J_m2    = 1.14;
    m.edge_energy_ev         = 0.55;
    m.melting_point_K        = 933.5;
    m.debye_temperature_K    = 428.0;
    m.thermal_conductivity_W_mK = 237.0;
    m.linear_exp_coeff_1e6K  = 23.1;
    m.electronegativity_pauling = 1.61;
    m.work_function_ev       = 4.20;
    m.fermi_energy_ev        = 11.63;
    m.bulk_modulus_GPa       = 76.0;
    m.shear_modulus_GPa      = 26.0;
    m.youngs_modulus_GPa     = 70.0;
    m.poisson_ratio          = 0.35;
    m.source = "Kittel8+CRC105";
    m.displacement_energy_ev  = 16.0;   // Al low E_d
    m.mu_mass_100keV_cm2g     = 0.170;  // NIST XCOM — light Z
    m.k_edge_keV              = 1.56;
    m.heat_of_combustion_kJ_g = 31.07;  // Al → Al₂O₃, highest energy metal fuel
    m.ignition_temperature_K  = 933.0;  // near melting, micron powder
    m.oxide_formation_enthalpy_eV = -5.82; // Al₂O₃ per 2Al → per atom = -2.91
    m.primary_oxide           = "Al2O3";
    m.flame_colour            = "brilliant white";
    fill_lj_params(m);
    return m;
}

// ============================================================================
// BCC Metals
// ============================================================================

inline MetalRecord iron() {
    MetalRecord m;
    m.Z = 26; m.symbol = "Fe"; m.name = "Iron";
    m.structure = CrystalStructure::BCC;
    m.lattice_constant_ang   = 2.870;
    m.atomic_radius_ang      = 1.26;
    m.atomic_mass_amu        = 55.845;
    m.bulk_CN                = 8.0;
    m.surface_CN             = 5.5;
    m.cohesive_energy_ev     = -4.28;
    m.surface_energy_J_m2    = 2.42;
    m.edge_energy_ev         = 0.90;
    m.melting_point_K        = 1811.0;
    m.debye_temperature_K    = 470.0;
    m.thermal_conductivity_W_mK = 80.4;
    m.linear_exp_coeff_1e6K  = 11.8;
    m.electronegativity_pauling = 1.83;
    m.work_function_ev       = 4.50;
    m.fermi_energy_ev        = 11.10;
    m.bulk_modulus_GPa       = 170.0;
    m.shear_modulus_GPa      = 82.0;
    m.youngs_modulus_GPa     = 211.0;
    m.poisson_ratio          = 0.29;
    m.is_magnetic            = true;
    m.source = "Kittel8+Tyson1977+CRC105";
    m.displacement_energy_ev  = 24.0;   // Fe BCC
    m.mu_mass_100keV_cm2g     = 0.372;  // NIST XCOM
    m.k_edge_keV              = 7.11;
    m.heat_of_combustion_kJ_g = 7.38;   // Fe → Fe₂O₃ (thermite oxidant product)
    m.ignition_temperature_K  = 588.0;  // iron powder ignites easily
    m.oxide_formation_enthalpy_eV = -2.76; // Fe₂O₃ per Fe atom
    m.primary_oxide           = "Fe2O3";
    m.flame_colour            = "orange-red";
    fill_lj_params(m);
    return m;
}

inline MetalRecord tungsten() {
    MetalRecord m;
    m.Z = 74; m.symbol = "W"; m.name = "Tungsten";
    m.structure = CrystalStructure::BCC;
    m.lattice_constant_ang   = 3.165;
    m.atomic_radius_ang      = 1.41;
    m.atomic_mass_amu        = 183.840;
    m.bulk_CN                = 8.0;
    m.surface_CN             = 5.5;
    m.cohesive_energy_ev     = -8.90;
    m.surface_energy_J_m2    = 3.67;
    m.edge_energy_ev         = 1.40;
    m.melting_point_K        = 3695.0;
    m.debye_temperature_K    = 400.0;
    m.thermal_conductivity_W_mK = 173.0;
    m.linear_exp_coeff_1e6K  = 4.5;
    m.electronegativity_pauling = 2.36;
    m.work_function_ev       = 4.55;
    m.fermi_energy_ev        = 7.65;
    m.bulk_modulus_GPa       = 310.0;
    m.shear_modulus_GPa      = 161.0;
    m.youngs_modulus_GPa     = 411.0;
    m.poisson_ratio          = 0.28;
    m.is_refractory          = true;
    m.source = "Kittel8+CRC105";
    m.displacement_energy_ev  = 90.0;   // W — highest E_d of common metals
    m.mu_mass_100keV_cm2g     = 4.44;   // NIST XCOM — high Z
    m.k_edge_keV              = 69.5;
    m.heat_of_combustion_kJ_g = 4.59;   // W → WO₃
    m.ignition_temperature_K  = 1473.0; // refractory — hard to ignite
    m.oxide_formation_enthalpy_eV = -2.85; // WO₃ per W
    m.primary_oxide           = "WO3";
    m.flame_colour            = "dull yellow";
    fill_lj_params(m);
    return m;
}

inline MetalRecord molybdenum() {
    MetalRecord m;
    m.Z = 42; m.symbol = "Mo"; m.name = "Molybdenum";
    m.structure = CrystalStructure::BCC;
    m.lattice_constant_ang   = 3.147;
    m.atomic_radius_ang      = 1.39;
    m.atomic_mass_amu        = 95.960;
    m.bulk_CN                = 8.0;
    m.surface_CN             = 5.5;
    m.cohesive_energy_ev     = -6.82;
    m.surface_energy_J_m2    = 2.91;
    m.edge_energy_ev         = 1.10;
    m.melting_point_K        = 2896.0;
    m.debye_temperature_K    = 450.0;
    m.thermal_conductivity_W_mK = 138.0;
    m.linear_exp_coeff_1e6K  = 4.8;
    m.electronegativity_pauling = 2.16;
    m.work_function_ev       = 4.36;
    m.fermi_energy_ev        = 8.67;
    m.bulk_modulus_GPa       = 230.0;
    m.shear_modulus_GPa      = 120.0;
    m.youngs_modulus_GPa     = 329.0;
    m.poisson_ratio          = 0.31;
    m.is_refractory          = true;
    m.source = "Kittel8+CRC105";
    m.displacement_energy_ev  = 60.0;
    m.mu_mass_100keV_cm2g     = 1.93;   // NIST XCOM
    m.k_edge_keV              = 20.0;
    m.heat_of_combustion_kJ_g = 5.88;   // Mo → MoO₃
    m.ignition_temperature_K  = 1373.0;
    m.oxide_formation_enthalpy_eV = -2.47;
    m.primary_oxide           = "MoO3";
    m.flame_colour            = "greenish-white";
    fill_lj_params(m);
    return m;
}

inline MetalRecord chromium() {
    MetalRecord m;
    m.Z = 24; m.symbol = "Cr"; m.name = "Chromium";
    m.structure = CrystalStructure::BCC;
    m.lattice_constant_ang   = 2.885;
    m.atomic_radius_ang      = 1.28;
    m.atomic_mass_amu        = 51.996;
    m.bulk_CN                = 8.0;
    m.surface_CN             = 5.5;
    m.cohesive_energy_ev     = -4.10;
    m.surface_energy_J_m2    = 2.35;
    m.edge_energy_ev         = 0.85;
    m.melting_point_K        = 2180.0;
    m.debye_temperature_K    = 630.0;
    m.thermal_conductivity_W_mK = 93.9;
    m.linear_exp_coeff_1e6K  = 4.9;
    m.electronegativity_pauling = 1.66;
    m.work_function_ev       = 4.50;
    m.fermi_energy_ev        = 7.00;
    m.bulk_modulus_GPa       = 160.0;
    m.shear_modulus_GPa      = 115.0;
    m.youngs_modulus_GPa     = 279.0;
    m.poisson_ratio          = 0.21;
    m.is_refractory          = true;
    m.source = "Kittel8+CRC105";
    m.displacement_energy_ev  = 28.0;
    m.mu_mass_100keV_cm2g     = 0.316;  // NIST XCOM
    m.k_edge_keV              = 5.99;
    m.heat_of_combustion_kJ_g = 10.80;  // Cr → Cr₂O₃
    m.ignition_temperature_K  = 1173.0;
    m.oxide_formation_enthalpy_eV = -3.70; // Cr₂O₃ per Cr
    m.primary_oxide           = "Cr2O3";
    m.flame_colour            = "silvery sparks";
    fill_lj_params(m);
    return m;
}

// ============================================================================
// HCP Metals
// ============================================================================

inline MetalRecord titanium() {
    MetalRecord m;
    m.Z = 22; m.symbol = "Ti"; m.name = "Titanium";
    m.structure = CrystalStructure::HCP;
    m.lattice_constant_ang   = 2.950;   // a-axis of HCP; used as proxy a₀
    m.atomic_radius_ang      = 1.47;
    m.atomic_mass_amu        = 47.867;
    m.bulk_CN                = 12.0;    // HCP ideal
    m.surface_CN             = 8.5;
    m.cohesive_energy_ev     = -4.85;
    m.surface_energy_J_m2    = 1.99;
    m.edge_energy_ev         = 0.90;
    m.melting_point_K        = 1941.0;
    m.debye_temperature_K    = 420.0;
    m.thermal_conductivity_W_mK = 21.9;
    m.linear_exp_coeff_1e6K  = 8.6;
    m.electronegativity_pauling = 1.54;
    m.work_function_ev       = 4.33;
    m.fermi_energy_ev        = 6.78;
    m.bulk_modulus_GPa       = 110.0;
    m.shear_modulus_GPa      = 44.0;
    m.youngs_modulus_GPa     = 116.0;
    m.poisson_ratio          = 0.32;
    m.is_refractory          = true;
    m.source = "Kittel8+CRC105";
    m.displacement_energy_ev  = 19.0;   // Ti HCP
    m.mu_mass_100keV_cm2g     = 0.271;  // NIST XCOM
    m.k_edge_keV              = 4.97;
    m.heat_of_combustion_kJ_g = 19.70;  // Ti → TiO₂, very high energy
    m.ignition_temperature_K  = 1473.0; // needs high T to ignite
    m.oxide_formation_enthalpy_eV = -4.85; // TiO₂ per Ti
    m.primary_oxide           = "TiO2";
    m.flame_colour            = "brilliant white (sparks)";
    fill_lj_params(m);
    return m;
}

inline MetalRecord cobalt() {
    MetalRecord m;
    m.Z = 27; m.symbol = "Co"; m.name = "Cobalt";
    m.structure = CrystalStructure::HCP;
    m.lattice_constant_ang   = 2.507;
    m.atomic_radius_ang      = 1.25;
    m.atomic_mass_amu        = 58.933;
    m.bulk_CN                = 12.0;
    m.surface_CN             = 8.0;
    m.cohesive_energy_ev     = -4.39;
    m.surface_energy_J_m2    = 2.52;
    m.edge_energy_ev         = 0.88;
    m.melting_point_K        = 1768.0;
    m.debye_temperature_K    = 445.0;
    m.thermal_conductivity_W_mK = 100.0;
    m.linear_exp_coeff_1e6K  = 13.0;
    m.electronegativity_pauling = 1.88;
    m.work_function_ev       = 5.00;
    m.fermi_energy_ev        = 7.00;
    m.bulk_modulus_GPa       = 180.0;
    m.shear_modulus_GPa      = 75.0;
    m.youngs_modulus_GPa     = 211.0;
    m.poisson_ratio          = 0.31;
    m.is_magnetic            = true;
    m.source = "Kittel8+CRC105";
    m.displacement_energy_ev  = 22.0;
    m.mu_mass_100keV_cm2g     = 0.410;  // NIST XCOM
    m.k_edge_keV              = 7.71;
    m.heat_of_combustion_kJ_g = 5.00;   // Co → Co₃O₄
    m.ignition_temperature_K  = 1073.0;
    m.oxide_formation_enthalpy_eV = -2.10; // Co₃O₄ per Co
    m.primary_oxide           = "Co3O4";
    m.flame_colour            = "silvery-blue";
    fill_lj_params(m);
    return m;
}

// ============================================================================
// Convenience: full registry
// ============================================================================

inline std::vector<MetalRecord> all_metals() {
    return {
        gold(), silver(), copper(), platinum(), nickel(), aluminium(),
        iron(), tungsten(), molybdenum(), chromium(),
        titanium(), cobalt()
    };
}

inline std::vector<MetalRecord> fcc_metals() {
    return { gold(), silver(), copper(), platinum(), nickel(), aluminium() };
}

inline std::vector<MetalRecord> bcc_metals() {
    return { iron(), tungsten(), molybdenum(), chromium() };
}

inline std::vector<MetalRecord> hcp_metals() {
    return { titanium(), cobalt() };
}

inline std::vector<MetalRecord> noble_metals() {
    return { gold(), silver(), platinum() };
}

inline std::vector<MetalRecord> refractory_metals() {
    return { tungsten(), molybdenum(), chromium(), titanium(), platinum() };
}

inline std::vector<MetalRecord> magnetic_metals() {
    return { iron(), nickel(), cobalt() };
}

/// Look up by symbol, returns nullptr if not found
inline const MetalRecord* find_metal(const std::vector<MetalRecord>& registry,
                                     const std::string& symbol)
{
    for (const auto& m : registry)
        if (m.symbol == symbol) return &m;
    return nullptr;
}

// ============================================================================
// Binary Alloy Pair Descriptor
// ============================================================================

/**
 * AlloyPairDescriptor — mixing parameters for a binary A-B metal pair.
 *
 * Provides Lorentz–Berthelot cross parameters and a qualitative miscibility
 * flag derived from electronegativity difference and atomic radius mismatch.
 *
 * Reference: Hume-Rothery rules for solid solution formation.
 */
struct AlloyPairDescriptor {
    std::string symbol_A;
    std::string symbol_B;
    std::string name;               ///< Common alloy name (e.g. "Cu-Ni Monel-like")

    // Lorentz–Berthelot cross parameters
    double sigma_AB_ang{};          ///< σ_AB = (σ_A + σ_B) / 2
    double epsilon_AB_kcal{};       ///< ε_AB = √(ε_A · ε_B)

    // Hume-Rothery mixing indicators
    double delta_r_frac{};          ///< |r_A - r_B| / r_A — radius mismatch
    double delta_chi{};             ///< |χ_A - χ_B| — electronegativity diff
    double delta_Ecoh_ev{};         ///< ||E_A| - |E_B|| — cohesive energy diff (eV)
    bool hume_rothery_soluble{};    ///< true if δr < 15% AND δχ < 0.4
    std::string structure_compatibility; ///< e.g. "FCC-FCC" / "FCC-BCC"
};

inline AlloyPairDescriptor make_alloy_pair(const MetalRecord& A,
                                            const MetalRecord& B,
                                            const std::string& alloy_name = "")
{
    AlloyPairDescriptor p;
    p.symbol_A = A.symbol;
    p.symbol_B = B.symbol;
    p.name     = alloy_name.empty() ? (A.symbol + "-" + B.symbol) : alloy_name;

    p.sigma_AB_ang    = (A.lj_sigma_ang + B.lj_sigma_ang) * 0.5;
    p.epsilon_AB_kcal = std::sqrt(A.lj_epsilon_kcal * B.lj_epsilon_kcal);

    double rA = A.atomic_radius_ang > 0 ? A.atomic_radius_ang : 1.5;
    double rB = B.atomic_radius_ang > 0 ? B.atomic_radius_ang : 1.5;
    p.delta_r_frac = std::abs(rA - rB) / rA;
    p.delta_chi    = std::abs(A.electronegativity_pauling - B.electronegativity_pauling);
    p.delta_Ecoh_ev = std::abs(std::abs(A.cohesive_energy_ev) - std::abs(B.cohesive_energy_ev));

    p.hume_rothery_soluble = (p.delta_r_frac < 0.15) && (p.delta_chi < 0.4);

    p.structure_compatibility =
        std::string(crystal_structure_name(A.structure)) + "-" +
        crystal_structure_name(B.structure);

    return p;
}

/// Convenience: well-known research alloy pairs
inline std::vector<AlloyPairDescriptor> canonical_alloy_pairs() {
    auto reg = all_metals();
    const MetalRecord& Au = *find_metal(reg, "Au");
    const MetalRecord& Ag = *find_metal(reg, "Ag");
    const MetalRecord& Cu = *find_metal(reg, "Cu");
    const MetalRecord& Ni = *find_metal(reg, "Ni");
    const MetalRecord& Fe = *find_metal(reg, "Fe");
    const MetalRecord& Cr = *find_metal(reg, "Cr");
    const MetalRecord& Al = *find_metal(reg, "Al");
    const MetalRecord& Ti = *find_metal(reg, "Ti");
    const MetalRecord& W  = *find_metal(reg, "W");
    const MetalRecord& Mo = *find_metal(reg, "Mo");

    return {
        make_alloy_pair(Au, Ag, "Au-Ag (electrum)"),
        make_alloy_pair(Cu, Ni, "Cu-Ni (Monel-like)"),
        make_alloy_pair(Fe, Cr, "Fe-Cr (stainless base)"),
        make_alloy_pair(Fe, Ni, "Fe-Ni (Invar-like)"),
        make_alloy_pair(Al, Cu, "Al-Cu (2xxx series)"),
        make_alloy_pair(Al, Ti, "Al-Ti (lightweight structural)"),
        make_alloy_pair(Ni, Mo, "Ni-Mo (Hastelloy-like)"),
        make_alloy_pair(W,  Mo, "W-Mo (refractory pair)"),
        make_alloy_pair(Cu, Au, "Cu-Au (ordered intermetallic)"),
        make_alloy_pair(Ni, Cr, "Ni-Cr (Nichrome-like)"),
    };
}

} // namespace metals
} // namespace coarse_grain
