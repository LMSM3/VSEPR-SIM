/**
 * report_engine.cpp
 * -----------------
 * Autonomous report-generation engine implementation.
 * Work Order: WO-TMS-CRG-001
 *
 * Implements: MaterialPropertyEngine, CaseGenerator, ExperimentRunner,
 *             ReportWriter, AutonomousEngine
 */

#include "core/report_engine.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <iostream>

namespace vsepr {
namespace report {

// ============================================================================
// String Helpers
// ============================================================================

const char* complexity_name(ComplexityLevel level) {
    switch (level) {
        case ComplexityLevel::L1_SIMPLE:      return "L1-Simple";
        case ComplexityLevel::L2_BINARY:      return "L2-Binary";
        case ComplexityLevel::L3_ANISOTROPIC: return "L3-Anisotropic";
        case ComplexityLevel::L4_TRANSIENT:   return "L4-Transient";
        case ComplexityLevel::L5_EXOTIC:      return "L5-Exotic";
    }
    return "Unknown";
}

const char* experiment_name(ExperimentType type) {
    switch (type) {
        case ExperimentType::STEADY_STATE_CONDUCTION: return "Steady-State Conduction";
        case ExperimentType::TRANSIENT_HEATING:       return "Transient Heating";
        case ExperimentType::TRANSIENT_COOLING:       return "Transient Cooling";
        case ExperimentType::THERMAL_EXPANSION:       return "Thermal Expansion";
        case ExperimentType::THERMAL_STRESS:          return "Thermal Stress";
        case ExperimentType::FATIGUE_CYCLING:         return "Fatigue Cycling";
        case ExperimentType::SENSITIVITY_SWEEP:       return "Sensitivity Sweep";
        case ExperimentType::PHASE_CHANGE_APPROX:     return "Phase Change Approximation";
        case ExperimentType::OXIDATION_PROXY:         return "Oxidation Proxy";
        case ExperimentType::CRACK_INITIATION_PROXY:  return "Crack Initiation Proxy";
        case ExperimentType::DIFFUSION_VARIATION:     return "Diffusion Variation";
    }
    return "Unknown";
}

static std::string fmt_double(double v, int prec = 4) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << v;
    return ss.str();
}

static std::string fmt_sci(double v, int prec = 3) {
    std::ostringstream ss;
    ss << std::scientific << std::setprecision(prec) << v;
    return ss.str();
}

static std::string timestamp_now() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// ============================================================================
// Material Property Engine — Element Table
// ============================================================================

// Curated table of real element properties (selected elements with known values)
// Sources: CRC Handbook, MatWeb, ASM International
const std::vector<MaterialPropertyEngine::ElementRecord>&
MaterialPropertyEngine::element_table() {
    static const std::vector<ElementRecord> table = {
        // Z,  sym,  name,         density, E(GPa), Sy(MPa), Su(MPa), k(W/mK), Cp(J/kgK), alpha(1/K), Tm(K), Tb(K)
        { 1,  "H",  "Hydrogen",       0.089,   0.0,    0.0,    0.0,     0.18,  14304.0, 0.0,         14.0,    20.3},
        { 2,  "He", "Helium",         0.164,   0.0,    0.0,    0.0,     0.15,   5193.0, 0.0,          0.95,    4.2},
        { 3,  "Li", "Lithium",        534.0,   4.9,   15.0,   15.0,    84.8,   3582.0, 46.0e-6,     453.7,  1603.0},
        { 4,  "Be", "Beryllium",     1850.0, 287.0,  240.0,  370.0,   200.0,   1825.0, 11.3e-6,    1560.0,  2744.0},
        { 5,  "B",  "Boron",         2340.0, 440.0, 5500.0, 5500.0,    27.0,   1026.0,  5.0e-6,    2349.0,  4200.0},
        { 6,  "C",  "Carbon",        2260.0,  33.0,   50.0,   50.0,   129.0,    709.0,  7.1e-6,    3823.0,  4098.0},
        { 7,  "N",  "Nitrogen",        1.25,   0.0,    0.0,    0.0,     0.026,  1040.0,  0.0,         63.2,   77.4},
        { 8,  "O",  "Oxygen",          1.43,   0.0,    0.0,    0.0,     0.027,   919.0,  0.0,         54.4,   90.2},
        { 9,  "F",  "Fluorine",        1.70,   0.0,    0.0,    0.0,     0.028,   824.0,  0.0,         53.5,   85.0},
        {10,  "Ne", "Neon",            0.90,   0.0,    0.0,    0.0,     0.049,  1030.0,  0.0,         24.6,   27.1},
        {11,  "Na", "Sodium",        971.0,  10.0,    0.5,    2.0,    141.0,   1228.0, 71.0e-6,     371.0,  1156.0},
        {12,  "Mg", "Magnesium",    1738.0,  45.0,  100.0,  200.0,    156.0,   1023.0, 24.8e-6,     923.0,  1363.0},
        {13,  "Al", "Aluminum",     2700.0,  69.0,  110.0,  200.0,    237.0,    897.0, 23.1e-6,     933.5,  2792.0},
        {14,  "Si", "Silicon",      2330.0, 130.0, 7000.0, 7000.0,    149.0,    705.0,  2.6e-6,    1687.0,  3538.0},
        {15,  "P",  "Phosphorus",   1823.0,   3.5,    1.0,    1.0,      0.24,   769.0, 124.0e-6,    317.3,   553.6},
        {16,  "S",  "Sulfur",       2070.0,   7.7,   10.0,   10.0,      0.27,   710.0, 64.0e-6,     388.4,   717.8},
        {17,  "Cl", "Chlorine",        3.21,   0.0,    0.0,    0.0,      0.009,  479.0,  0.0,        171.6,   239.1},
        {18,  "Ar", "Argon",           1.78,   0.0,    0.0,    0.0,      0.018,  520.0,  0.0,         83.8,   87.3},
        {19,  "K",  "Potassium",     862.0,   3.5,    0.3,    1.0,    102.5,    757.0, 83.3e-6,     336.5,  1032.0},
        {20,  "Ca", "Calcium",      1550.0,  20.0,   15.0,   50.0,    201.0,    647.0, 22.3e-6,    1115.0,  1757.0},
        {21,  "Sc", "Scandium",     2985.0,  74.4,  170.0,  250.0,     15.8,    568.0, 10.2e-6,    1814.0,  3109.0},
        {22,  "Ti", "Titanium",     4507.0, 116.0,  275.0,  345.0,     21.9,    523.0,  8.6e-6,    1941.0,  3560.0},
        {23,  "V",  "Vanadium",     6110.0, 128.0,  310.0,  490.0,     30.7,    489.0,  8.4e-6,    2183.0,  3680.0},
        {24,  "Cr", "Chromium",     7190.0, 279.0,  280.0,  420.0,     93.9,    449.0,  4.9e-6,    2180.0,  2944.0},
        {25,  "Mn", "Manganese",    7210.0, 198.0,  290.0,  490.0,      7.8,    479.0, 21.7e-6,    1519.0,  2334.0},
        {26,  "Fe", "Iron",         7874.0, 211.0,  290.0,  400.0,     80.4,    449.0, 11.8e-6,    1811.0,  3134.0},
        {27,  "Co", "Cobalt",       8900.0, 209.0,  345.0,  500.0,    100.0,    421.0, 13.0e-6,    1768.0,  3200.0},
        {28,  "Ni", "Nickel",       8908.0, 200.0,  195.0,  462.0,     90.9,    444.0, 13.4e-6,    1728.0,  3186.0},
        {29,  "Cu", "Copper",       8960.0, 130.0,   70.0,  220.0,    401.0,    385.0, 16.5e-6,    1357.8,  2835.0},
        {30,  "Zn", "Zinc",         7134.0, 108.0,  110.0,  200.0,    116.0,    388.0, 30.2e-6,     692.7,  1180.0},
        {31,  "Ga", "Gallium",      5904.0,   9.8,   40.0,   60.0,     40.6,    371.0, 18.0e-6,     302.9,  2477.0},
        {32,  "Ge", "Germanium",    5323.0, 103.0, 6000.0, 6000.0,     60.2,    320.0,  6.0e-6,    1211.4,  3106.0},
        {33,  "As", "Arsenic",      5727.0,   8.0,   30.0,   30.0,     50.2,    329.0,  5.6e-6,    1090.0,   887.0},
        {34,  "Se", "Selenium",     4819.0,  10.0,   20.0,   20.0,      0.52,   321.0, 37.0e-6,     494.0,   958.0},
        {35,  "Br", "Bromine",      3120.0,   0.0,    0.0,    0.0,      0.12,   474.0,  0.0,        265.8,   332.0},
        {36,  "Kr", "Krypton",         3.75,   0.0,    0.0,    0.0,      0.009,  248.0,  0.0,        115.8,   119.9},
        {37,  "Rb", "Rubidium",     1532.0,   2.4,    0.2,    0.8,     58.2,    363.0, 90.0e-6,     312.5,   961.0},
        {38,  "Sr", "Strontium",    2640.0,  15.7,   10.0,   30.0,     35.4,    301.0, 22.5e-6,    1050.0,  1655.0},
        {39,  "Y",  "Yttrium",      4472.0,  63.5,   60.0,  130.0,     17.2,    298.0, 10.6e-6,    1799.0,  3609.0},
        {40,  "Zr", "Zirconium",    6506.0,  88.0,  230.0,  370.0,     22.6,    278.0,  5.7e-6,    2128.0,  4682.0},
        {41,  "Nb", "Niobium",      8570.0, 105.0,  207.0,  275.0,     53.7,    265.0,  7.3e-6,    2750.0,  5017.0},
        {42,  "Mo", "Molybdenum",  10220.0, 329.0,  500.0,  655.0,    138.0,    251.0,  4.8e-6,    2896.0,  4912.0},
        {43,  "Tc", "Technetium",  11500.0, 250.0,  400.0,  500.0,     50.6,    210.0,  7.1e-6,    2430.0,  4538.0},
        {44,  "Ru", "Ruthenium",   12370.0, 447.0,  372.0,  500.0,    117.0,    238.0,  6.4e-6,    2607.0,  4423.0},
        {45,  "Rh", "Rhodium",     12410.0, 275.0,  200.0,  350.0,    150.0,    243.0,  8.2e-6,    2237.0,  3968.0},
        {46,  "Pd", "Palladium",   12020.0, 121.0,  200.0,  310.0,     71.8,    244.0, 11.8e-6,    1828.0,  3236.0},
        {47,  "Ag", "Silver",      10490.0,  83.0,  170.0,  300.0,    429.0,    235.0, 18.9e-6,    1234.9,  2435.0},
        {48,  "Cd", "Cadmium",      8650.0,  50.0,   69.0,  100.0,     96.6,    232.0, 30.8e-6,     594.2,  1040.0},
        {49,  "In", "Indium",       7310.0,  11.0,    3.0,    5.0,     81.8,    233.0, 32.1e-6,     429.7,  2345.0},
        {50,  "Sn", "Tin",          7265.0,  50.0,   14.0,   22.0,     66.8,    228.0, 22.0e-6,     505.1,  2875.0},
        {51,  "Sb", "Antimony",     6697.0,  55.0,   11.0,   11.0,     24.4,    207.0, 11.0e-6,     903.8,  1860.0},
        {52,  "Te", "Tellurium",    6240.0,  43.0,   10.0,   10.0,      2.35,   202.0, 16.8e-6,     722.7,  1261.0},
        {53,  "I",  "Iodine",       4930.0,   0.0,    0.0,    0.0,      0.45,   214.0,  0.0,        386.9,   457.4},
        {54,  "Xe", "Xenon",           5.89,   0.0,    0.0,    0.0,      0.006,  158.0,  0.0,        161.4,   165.1},
        {55,  "Cs", "Cesium",       1873.0,   1.7,    0.1,    0.5,     35.9,    242.0, 97.0e-6,     301.6,   944.0},
        {56,  "Ba", "Barium",       3510.0,  13.0,    5.0,   20.0,     18.4,    204.0, 20.6e-6,    1000.0,  2170.0},
        {72,  "Hf", "Hafnium",     13310.0, 109.0,  250.0,  400.0,     23.0,    144.0,  5.9e-6,    2506.0,  4876.0},
        {73,  "Ta", "Tantalum",    16654.0, 186.0,  200.0,  285.0,     57.5,    140.0,  6.3e-6,    3290.0,  5731.0},
        {74,  "W",  "Tungsten",    19250.0, 411.0,  750.0,  980.0,    173.0,    132.0,  4.5e-6,    3695.0,  5828.0},
        {75,  "Re", "Rhenium",     21020.0, 463.0, 1070.0, 1130.0,     48.0,    137.0,  6.2e-6,    3459.0,  5869.0},
        {76,  "Os", "Osmium",      22590.0, 564.0,  500.0,  700.0,     87.6,    130.0,  5.1e-6,    3306.0,  5285.0},
        {77,  "Ir", "Iridium",     22560.0, 528.0,  350.0,  550.0,    147.0,    131.0,  6.4e-6,    2719.0,  4701.0},
        {78,  "Pt", "Platinum",    21450.0, 168.0,  150.0,  240.0,     71.6,    133.0,  8.8e-6,    2041.4,  4098.0},
        {79,  "Au", "Gold",        19300.0,  78.0,  120.0,  220.0,    318.0,    129.0, 14.2e-6,    1337.3,  3129.0},
        {80,  "Hg", "Mercury",     13534.0,   0.0,    0.0,    0.0,      8.3,    140.0, 60.4e-6,     234.3,   629.9},
        {81,  "Tl", "Thallium",    11850.0,   8.0,    4.0,   10.0,     46.1,    129.0, 29.9e-6,     577.0,  1746.0},
        {82,  "Pb", "Lead",        11340.0,  16.0,   11.0,   18.0,     35.3,    129.0, 28.9e-6,     600.6,  2022.0},
        {83,  "Bi", "Bismuth",      9780.0,  32.0,    7.0,   12.0,      7.97,   122.0, 13.4e-6,     544.4,  1837.0},
        {90,  "Th", "Thorium",     11724.0,  79.0,  144.0,  220.0,     54.0,    113.0, 11.0e-6,    2115.0,  5061.0},
        {92,  "U",  "Uranium",     19050.0, 208.0,  200.0,  400.0,     27.5,    116.0, 13.9e-6,    1405.3,  4404.0},
    };
    return table;
}

const MaterialPropertyEngine::ElementRecord*
MaterialPropertyEngine::find_element(uint8_t Z) const {
    for (const auto& rec : element_table()) {
        if (rec.Z == Z) return &rec;
    }
    return nullptr;
}

// ============================================================================
// Material Property Engine — Implementation
// ============================================================================

MaterialPropertyEngine::MaterialPropertyEngine() {}

MaterialProperties MaterialPropertyEngine::element_properties(uint8_t Z) const {
    MaterialProperties props;
    const auto* rec = find_element(Z);

    if (rec) {
        props.name = rec->name;
        props.formula = rec->symbol;
        props.category = "element";
        props.primary_Z = Z;
        props.density_kg_m3 = rec->density;
        props.elastic_modulus_GPa = rec->elastic_modulus;
        props.yield_strength_MPa = rec->yield_strength;
        props.ultimate_strength_MPa = rec->ult_strength;
        props.thermal_conductivity_W_mK = rec->thermal_cond;
        props.specific_heat_J_kgK = rec->specific_heat;
        props.thermal_expansion_1_K = rec->thermal_exp;
        props.melting_point_K = rec->melting_point;
        props.boiling_point_K = rec->boiling_point;
        props.confidence_score = 0.95;

        // Estimate fatigue endurance (~0.4 * Sut for steels, ~0.35 general)
        if (rec->ult_strength > 0) {
            props.fatigue_endurance_MPa = rec->ult_strength * 0.35;
        }

        // Condensed-phase fallback for gases/non-metals with zero mechanical data.
        // Provides physically-reasonable lower-bound estimates so experiments
        // produce meaningful (if approximate) results.  Confidence is reduced.
        bool needs_fallback = (props.elastic_modulus_GPa < 1.0e-6 &&
                               props.thermal_expansion_1_K < 1.0e-12);
        if (needs_fallback) {
            // Estimate E from bulk modulus of condensed phase or nearest solid
            // Noble gases solidify under pressure; use ~1 GPa as lower bound.
            // Halogens are soft molecular solids; use ~5–10 GPa.
            bool is_noble = (Z == 2 || Z == 10 || Z == 18 || Z == 36 || Z == 54);
            bool is_halogen = (Z == 9 || Z == 17 || Z == 35 || Z == 53);
            bool is_diatomic_gas = (Z == 1 || Z == 7 || Z == 8);

            if (is_noble) {
                props.elastic_modulus_GPa    = 1.1;    // solid noble gas estimate
                props.yield_strength_MPa     = 0.5;
                props.ultimate_strength_MPa  = 1.0;
                props.thermal_expansion_1_K  = 40.0e-6;
                props.poisson_ratio          = 0.35;
            } else if (is_halogen) {
                props.elastic_modulus_GPa    = 7.7;    // solid halogen estimate (I2 ~7.7)
                props.yield_strength_MPa     = 5.0;
                props.ultimate_strength_MPa  = 10.0;
                props.thermal_expansion_1_K  = 90.0e-6;
                props.poisson_ratio          = 0.33;
            } else if (is_diatomic_gas) {
                props.elastic_modulus_GPa    = 0.5;    // cryogenic solid estimate
                props.yield_strength_MPa     = 0.3;
                props.ultimate_strength_MPa  = 0.6;
                props.thermal_expansion_1_K  = 50.0e-6;
                props.poisson_ratio          = 0.38;
            } else {
                // Mercury or other liquid-at-STP elements
                props.elastic_modulus_GPa    = 25.0;
                props.yield_strength_MPa     = 5.0;
                props.ultimate_strength_MPa  = 10.0;
                props.thermal_expansion_1_K  = 60.0e-6;
                props.poisson_ratio          = 0.35;
            }

            if (props.ultimate_strength_MPa > 0) {
                props.fatigue_endurance_MPa = props.ultimate_strength_MPa * 0.35;
            }
            props.confidence_score = 0.55;  // reduced — condensed-phase estimate
        }
    } else {
        // Synthetic interpolation for elements not in table
        props.name = "Element-" + std::to_string(Z);
        props.formula = "Z" + std::to_string(Z);
        props.category = "synthetic";
        props.primary_Z = Z;
        props.is_synthetic = true;

        // Interpolate from neighbors
        double frac = static_cast<double>(Z) / 118.0;
        props.density_kg_m3 = 1000.0 + frac * 18000.0;
        props.elastic_modulus_GPa = 10.0 + frac * 400.0;
        props.yield_strength_MPa = 20.0 + frac * 700.0;
        props.ultimate_strength_MPa = props.yield_strength_MPa * 1.4;
        props.thermal_conductivity_W_mK = 5.0 + frac * 300.0;
        props.specific_heat_J_kgK = 130.0 + (1.0 - frac) * 1000.0;
        props.thermal_expansion_1_K = 5.0e-6 + frac * 50.0e-6;
        props.melting_point_K = 300.0 + frac * 3500.0;
        props.boiling_point_K = props.melting_point_K * 1.8;
        props.confidence_score = 0.3;
    }

    return props;
}

MaterialProperties MaterialPropertyEngine::synthetic_properties(
    std::mt19937_64& rng, double exoticism) const {

    MaterialProperties props;
    props.category = "synthetic";
    props.is_synthetic = true;

    std::uniform_real_distribution<double> u01(0.0, 1.0);
    std::normal_distribution<double> norm(0.0, 1.0);

    // Base properties with exoticism-scaled variance
    double e = std::clamp(exoticism, 0.0, 1.0);
    double base_density = 2000.0 + u01(rng) * 15000.0;
    double base_E = 10.0 + u01(rng) * 500.0;

    props.density_kg_m3 = base_density * (1.0 + e * norm(rng) * 0.5);
    props.elastic_modulus_GPa = base_E * (1.0 + e * norm(rng) * 0.3);
    props.yield_strength_MPa = props.elastic_modulus_GPa * (0.001 + u01(rng) * 0.01) * 1000.0;
    props.ultimate_strength_MPa = props.yield_strength_MPa * (1.1 + u01(rng) * 0.8);
    props.thermal_conductivity_W_mK = 0.1 + u01(rng) * 400.0;
    props.specific_heat_J_kgK = 100.0 + u01(rng) * 4000.0;
    props.thermal_expansion_1_K = 1.0e-6 + u01(rng) * 100.0e-6;
    props.melting_point_K = 200.0 + u01(rng) * 3800.0;
    props.boiling_point_K = props.melting_point_K * (1.2 + u01(rng) * 1.5);
    props.fatigue_endurance_MPa = props.ultimate_strength_MPa * (0.25 + u01(rng) * 0.2);
    props.anisotropy_factor = 1.0 + e * u01(rng) * 2.0;
    props.uncertainty_factor = 0.05 + e * 0.3;
    props.confidence_score = std::max(0.1, 0.8 - e * 0.5);

    // Generate name
    static const char* prefixes[] = {
        "Neo", "Hyper", "Ultra", "Proto", "Syn", "Meta", "Para",
        "Iso", "Poly", "Mono", "Tri", "Quad", "Hex", "Octa"
    };
    static const char* roots[] = {
        "titanite", "ferride", "chromate", "vanadin", "niobide",
        "tungstate", "rhenide", "osmite", "iridiate", "platinate",
        "cobaltide", "nickellate", "zirconate", "hafnide", "tantalate",
        "molybdate", "rhodite", "palladate", "auride", "argentite"
    };
    int pi = static_cast<int>(u01(rng) * 14) % 14;
    int ri = static_cast<int>(u01(rng) * 20) % 20;
    props.name = std::string(prefixes[pi]) + roots[ri];
    props.formula = "Syn" + std::to_string(static_cast<int>(u01(rng) * 9000 + 1000));

    return props;
}

MaterialProperties MaterialPropertyEngine::mix_alloy(
    const std::vector<MaterialProperties>& components,
    const std::vector<double>& fractions,
    std::mt19937_64& rng,
    double perturbation) const {

    MaterialProperties mixed;
    mixed.category = "alloy";

    if (components.empty()) return mixed;

    // Rule of mixtures
    double total_f = 0.0;
    for (size_t i = 0; i < components.size(); ++i) {
        double f = (i < fractions.size()) ? fractions[i] : 1.0 / components.size();
        total_f += f;
        const auto& c = components[i];

        mixed.density_kg_m3          += f * c.density_kg_m3;
        mixed.elastic_modulus_GPa    += f * c.elastic_modulus_GPa;
        mixed.yield_strength_MPa     += f * c.yield_strength_MPa;
        mixed.ultimate_strength_MPa  += f * c.ultimate_strength_MPa;
        mixed.thermal_conductivity_W_mK += f * c.thermal_conductivity_W_mK;
        mixed.specific_heat_J_kgK    += f * c.specific_heat_J_kgK;
        mixed.thermal_expansion_1_K  += f * c.thermal_expansion_1_K;
        mixed.melting_point_K        += f * c.melting_point_K;
        mixed.boiling_point_K        += f * c.boiling_point_K;
    }

    // Normalize if fractions don't sum to 1
    if (total_f > 0 && std::abs(total_f - 1.0) > 0.01) {
        double inv = 1.0 / total_f;
        mixed.density_kg_m3 *= inv;
        mixed.elastic_modulus_GPa *= inv;
        mixed.yield_strength_MPa *= inv;
        mixed.ultimate_strength_MPa *= inv;
        mixed.thermal_conductivity_W_mK *= inv;
        mixed.specific_heat_J_kgK *= inv;
        mixed.thermal_expansion_1_K *= inv;
        mixed.melting_point_K *= inv;
        mixed.boiling_point_K *= inv;
    }

    // Apply perturbation for non-ideal mixing
    std::normal_distribution<double> norm(0.0, 1.0);
    auto perturb_val = [&](double v) {
        return v * (1.0 + perturbation * norm(rng));
    };

    mixed.density_kg_m3          = perturb_val(mixed.density_kg_m3);
    mixed.elastic_modulus_GPa    = perturb_val(mixed.elastic_modulus_GPa);
    mixed.yield_strength_MPa     = perturb_val(mixed.yield_strength_MPa);
    mixed.ultimate_strength_MPa  = perturb_val(mixed.ultimate_strength_MPa);
    mixed.thermal_conductivity_W_mK = perturb_val(mixed.thermal_conductivity_W_mK);
    mixed.specific_heat_J_kgK    = perturb_val(mixed.specific_heat_J_kgK);
    mixed.thermal_expansion_1_K  = perturb_val(mixed.thermal_expansion_1_K);
    mixed.melting_point_K        = perturb_val(mixed.melting_point_K);

    mixed.fatigue_endurance_MPa = mixed.ultimate_strength_MPa * 0.35;
    mixed.confidence_score = 0.7;

    // Build name
    std::string name;
    for (size_t i = 0; i < components.size(); ++i) {
        if (i > 0) name += "-";
        name += components[i].formula;
        if (i < fractions.size()) {
            name += "(" + std::to_string(static_cast<int>(fractions[i] * 100)) + "%)";
        }
    }
    mixed.name = name;
    mixed.formula = name;

    return mixed;
}

MaterialProperties MaterialPropertyEngine::perturb(
    const MaterialProperties& base, std::mt19937_64& rng, double sigma) const {

    MaterialProperties p = base;
    std::normal_distribution<double> norm(0.0, 1.0);

    auto pv = [&](double v) { return v * (1.0 + sigma * norm(rng)); };

    p.density_kg_m3 = std::max(0.01, pv(p.density_kg_m3));
    p.elastic_modulus_GPa = std::max(0.01, pv(p.elastic_modulus_GPa));
    p.yield_strength_MPa = std::max(0.0, pv(p.yield_strength_MPa));
    p.ultimate_strength_MPa = std::max(p.yield_strength_MPa, pv(p.ultimate_strength_MPa));
    p.thermal_conductivity_W_mK = std::max(0.001, pv(p.thermal_conductivity_W_mK));
    p.specific_heat_J_kgK = std::max(10.0, pv(p.specific_heat_J_kgK));
    p.thermal_expansion_1_K = std::max(0.0, pv(p.thermal_expansion_1_K));
    p.melting_point_K = std::max(10.0, pv(p.melting_point_K));
    p.uncertainty_factor = sigma;
    p.confidence_score = std::max(0.1, base.confidence_score - sigma * 0.5);

    return p;
}

// ============================================================================
// Case Generator
// ============================================================================

CaseGenerator::CaseGenerator(uint64_t base_seed)
    : rng_(base_seed), prop_engine_() {}

void CaseGenerator::set_escalation_thresholds(int l2, int l3, int l4, int l5) {
    threshold_l2_ = l2;
    threshold_l3_ = l3;
    threshold_l4_ = l4;
    threshold_l5_ = l5;
}

void CaseGenerator::escalate_if_needed() {
    if (cases_generated_ >= threshold_l5_)
        current_level_ = ComplexityLevel::L5_EXOTIC;
    else if (cases_generated_ >= threshold_l4_)
        current_level_ = ComplexityLevel::L4_TRANSIENT;
    else if (cases_generated_ >= threshold_l3_)
        current_level_ = ComplexityLevel::L3_ANISOTROPIC;
    else if (cases_generated_ >= threshold_l2_)
        current_level_ = ComplexityLevel::L2_BINARY;
    else
        current_level_ = ComplexityLevel::L1_SIMPLE;

    gamma_ = 1.0 + static_cast<double>(cases_generated_) * 0.002;
}

std::string CaseGenerator::generate_case_name(const MaterialCase& c) {
    std::string base;
    if (!c.components.empty()) {
        base = c.components[0].name;
        if (c.components.size() > 1) {
            base += "+" + c.components[1].formula;
        }
        if (c.components.size() > 2) {
            base += "+" + std::to_string(c.components.size() - 2) + "more";
        }
    } else {
        base = "Case";
    }
    return base + "-" + std::to_string(c.case_id);
}

MaterialCase CaseGenerator::generate_next() {
    escalate_if_needed();
    MaterialCase mc;

    switch (current_level_) {
        case ComplexityLevel::L1_SIMPLE:      mc = generate_l1(); break;
        case ComplexityLevel::L2_BINARY:      mc = generate_l2(); break;
        case ComplexityLevel::L3_ANISOTROPIC: mc = generate_l3(); break;
        case ComplexityLevel::L4_TRANSIENT:   mc = generate_l4(); break;
        case ComplexityLevel::L5_EXOTIC:      mc = generate_l5(); break;
    }

    mc.case_id = static_cast<uint64_t>(cases_generated_);
    mc.seed = rng_();
    mc.level = current_level_;
    mc.gamma_factor = gamma_;
    mc.case_name = generate_case_name(mc);

    cases_generated_++;
    return mc;
}

MaterialCase CaseGenerator::generate_at_level(ComplexityLevel level) {
    // Bypass escalation — generate at the explicitly requested level
    MaterialCase mc;

    switch (level) {
        case ComplexityLevel::L1_SIMPLE:      mc = generate_l1(); break;
        case ComplexityLevel::L2_BINARY:      mc = generate_l2(); break;
        case ComplexityLevel::L3_ANISOTROPIC: mc = generate_l3(); break;
        case ComplexityLevel::L4_TRANSIENT:   mc = generate_l4(); break;
        case ComplexityLevel::L5_EXOTIC:      mc = generate_l5(); break;
    }

    mc.case_id = static_cast<uint64_t>(cases_generated_);
    mc.seed = rng_();
    mc.level = level;
    mc.gamma_factor = gamma_;
    mc.case_name = generate_case_name(mc);

    cases_generated_++;
    return mc;
}

MaterialCase CaseGenerator::generate_l1() {
    MaterialCase mc;
    mc.description = "Simple uniform single-element material";

    // Pick a random element from the table
    const auto& table = MaterialPropertyEngine::element_table();
    std::uniform_int_distribution<size_t> dist(0, table.size() - 1);
    uint8_t Z = table[dist(rng_)].Z;

    auto props = prop_engine_.element_properties(Z);
    mc.components.push_back(props);
    mc.fractions.push_back(1.0);
    mc.effective = props;

    // Simple steady-state thermal boundary
    ThermalBoundary hot, cold;
    std::uniform_real_distribution<double> temp_dist(300.0, 1500.0);
    hot.temperature_K = temp_dist(rng_);
    hot.label = "Hot face";
    cold.temperature_K = 300.0;
    cold.label = "Cold face";
    mc.boundaries.push_back(hot);
    mc.boundaries.push_back(cold);

    mc.thermal_load.initial_temperature_K = 300.0;
    mc.thermal_load.peak_temperature_K = hot.temperature_K;
    mc.rarity_score = 0.0;
    mc.instability_index = 0.0;

    return mc;
}

MaterialCase CaseGenerator::generate_l2() {
    MaterialCase mc;
    mc.description = "Binary or ternary alloy/compound";

    const auto& table = MaterialPropertyEngine::element_table();
    std::uniform_int_distribution<size_t> dist(0, table.size() - 1);
    std::uniform_int_distribution<int> n_comp(2, 3);
    int nc = n_comp(rng_);

    std::vector<MaterialProperties> comps;
    std::vector<double> fracs;
    double frac_remain = 1.0;

    for (int i = 0; i < nc; ++i) {
        uint8_t Z = table[dist(rng_)].Z;
        comps.push_back(prop_engine_.element_properties(Z));

        if (i == nc - 1) {
            fracs.push_back(frac_remain);
        } else {
            std::uniform_real_distribution<double> fd(0.1, frac_remain - 0.1 * (nc - i - 1));
            double f = fd(rng_);
            fracs.push_back(f);
            frac_remain -= f;
        }
    }

    mc.components = comps;
    mc.fractions = fracs;
    mc.effective = prop_engine_.mix_alloy(comps, fracs, rng_);

    // Thermal boundaries
    std::uniform_real_distribution<double> temp_dist(400.0, 2000.0);
    ThermalBoundary hot, cold;
    hot.temperature_K = temp_dist(rng_);
    hot.label = "Hot face";
    cold.temperature_K = 300.0;
    cold.label = "Cold face";
    mc.boundaries.push_back(hot);
    mc.boundaries.push_back(cold);

    mc.thermal_load.initial_temperature_K = 300.0;
    mc.thermal_load.peak_temperature_K = hot.temperature_K;
    mc.rarity_score = 0.1 * nc;
    mc.instability_index = 0.05;

    return mc;
}

MaterialCase CaseGenerator::generate_l3() {
    MaterialCase mc;
    mc.description = "Anisotropic or layered material system";

    const auto& table = MaterialPropertyEngine::element_table();
    std::uniform_int_distribution<size_t> elem_dist(0, table.size() - 1);
    std::uniform_int_distribution<int> layer_count(2, 5);
    int nl = layer_count(rng_);

    std::uniform_real_distribution<double> thick_dist(0.5, 10.0);
    std::uniform_real_distribution<double> iface_dist(1.0e-5, 1.0e-3);

    for (int i = 0; i < nl; ++i) {
        LayerSpec layer;
        uint8_t Z = table[elem_dist(rng_)].Z;
        layer.material = prop_engine_.element_properties(Z);
        layer.material.anisotropy_factor = 1.0 + (rng_() % 10) * 0.1;
        layer.thickness_mm = thick_dist(rng_);
        layer.interface_resistance_m2K_W = (i > 0) ? iface_dist(rng_) : 0.0;
        mc.layers.push_back(layer);
        mc.components.push_back(layer.material);
        mc.fractions.push_back(1.0 / nl);
    }

    mc.effective = prop_engine_.mix_alloy(mc.components, mc.fractions, rng_);
    mc.effective.anisotropy_factor = 1.0 + nl * 0.3;

    // Gradient boundary
    std::uniform_real_distribution<double> temp_dist(500.0, 2500.0);
    ThermalBoundary hot, cold;
    hot.temperature_K = temp_dist(rng_);
    hot.label = "Hot surface";
    cold.temperature_K = 300.0;
    cold.label = "Cold surface";
    mc.boundaries.push_back(hot);
    mc.boundaries.push_back(cold);

    mc.thermal_load.initial_temperature_K = 300.0;
    mc.thermal_load.peak_temperature_K = hot.temperature_K;
    mc.rarity_score = 0.3 + nl * 0.05;
    mc.instability_index = 0.1;

    return mc;
}

MaterialCase CaseGenerator::generate_l4() {
    MaterialCase mc;
    mc.description = "Transient thermal loading with material variation";

    // Start with an L2 or L3 base
    std::uniform_int_distribution<int> base_type(0, 1);
    if (base_type(rng_) == 0) {
        mc = generate_l2();
    } else {
        mc = generate_l3();
    }
    mc.description = "Transient thermal loading with material variation";

    // Add transient thermal cycling
    std::uniform_real_distribution<double> rate_dist(1.0, 100.0);
    std::uniform_int_distribution<int> cycle_dist(5, 200);
    std::uniform_real_distribution<double> peak_dist(600.0, 3000.0);

    mc.thermal_load.is_transient = true;
    mc.thermal_load.heating_rate_K_s = rate_dist(rng_);
    mc.thermal_load.cooling_rate_K_s = rate_dist(rng_) * 0.8;
    mc.thermal_load.peak_temperature_K = peak_dist(rng_);
    mc.thermal_load.cycle_period_s = (mc.thermal_load.peak_temperature_K - 300.0) / mc.thermal_load.heating_rate_K_s * 2.0;
    mc.thermal_load.num_cycles = cycle_dist(rng_);

    // Add defects
    std::uniform_int_distribution<int> defect_count(0, 3);
    int nd = defect_count(rng_);
    static const char* defect_types[] = {"vacancy", "interstitial", "grain_boundary", "crack", "void"};
    std::uniform_int_distribution<int> dt(0, 4);
    std::uniform_real_distribution<double> conc(0.001, 0.05);
    std::uniform_real_distribution<double> sz(0.1, 100.0);

    for (int i = 0; i < nd; ++i) {
        DefectSpec d;
        d.type = defect_types[dt(rng_)];
        d.concentration = conc(rng_);
        d.size_nm = sz(rng_);
        mc.defects.push_back(d);
    }

    mc.rarity_score = 0.5 + nd * 0.1;
    mc.instability_index = 0.3 + mc.thermal_load.num_cycles * 0.001;

    return mc;
}

MaterialCase CaseGenerator::generate_l5() {
    MaterialCase mc;
    mc.description = "Exotic/unstable synthetic configuration with multi-factor coupling";

    // Start with complex base
    mc = generate_l4();
    mc.description = "Exotic/unstable synthetic configuration with multi-factor coupling";

    // Add synthetic component
    double exoticism = 0.5 + (rng_() % 50) * 0.01;
    auto synthetic = prop_engine_.synthetic_properties(rng_, exoticism);
    mc.components.push_back(synthetic);
    mc.fractions.push_back(0.0);
    // Rebalance fractions
    double total = 0;
    for (auto& f : mc.fractions) total += f;
    if (total > 0) {
        double new_frac = 0.1 + (rng_() % 30) * 0.01;
        double scale = (1.0 - new_frac) / total;
        for (auto& f : mc.fractions) f *= scale;
        mc.fractions.back() = new_frac;
    }

    mc.effective = prop_engine_.mix_alloy(mc.components, mc.fractions, rng_, 0.15);

    // Extreme thermal
    std::uniform_real_distribution<double> extreme_temp(1500.0, 5000.0);
    mc.thermal_load.peak_temperature_K = extreme_temp(rng_);
    mc.thermal_load.heating_rate_K_s *= 2.0;
    mc.thermal_load.num_cycles *= 2;

    // Additional defects
    DefectSpec exotic_defect;
    exotic_defect.type = "radiation_damage";
    exotic_defect.concentration = 0.01 + (rng_() % 100) * 0.001;
    exotic_defect.size_nm = 1.0 + (rng_() % 500) * 0.1;
    mc.defects.push_back(exotic_defect);

    mc.rarity_score = 0.8 + exoticism * 0.2;
    mc.instability_index = 0.6 + mc.defects.size() * 0.1;

    return mc;
}

// ============================================================================
// Experiment Runner
// ============================================================================

std::vector<ExperimentResult> ExperimentRunner::run_all(const MaterialCase& mc) const {
    std::vector<ExperimentResult> results;

    // Always run these
    results.push_back(steady_state_conduction(mc));
    results.push_back(thermal_expansion(mc));
    results.push_back(thermal_stress(mc));

    // Transient experiments if applicable
    if (mc.thermal_load.is_transient) {
        results.push_back(transient_heating(mc));
        results.push_back(transient_cooling(mc));
    }

    // Fatigue if cycling
    if (mc.thermal_load.num_cycles > 1) {
        results.push_back(fatigue_cycling(mc));
    }

    // Sensitivity sweep for complex cases
    if (static_cast<int>(mc.level) >= 3) {
        results.push_back(sensitivity_sweep(mc));
    }

    // Phase change for high temperatures
    if (mc.thermal_load.peak_temperature_K > mc.effective.melting_point_K * 0.8 &&
        mc.effective.melting_point_K > 0) {
        results.push_back(phase_change_approx(mc));
    }

    // Proxy analyses for L4+
    if (static_cast<int>(mc.level) >= 4) {
        results.push_back(oxidation_proxy(mc));
        results.push_back(crack_initiation_proxy(mc));
    }

    return results;
}

ExperimentResult ExperimentRunner::steady_state_conduction(const MaterialCase& mc) const {
    ExperimentResult r;
    r.type = ExperimentType::STEADY_STATE_CONDUCTION;
    r.experiment_name = experiment_name(r.type);

    double k = mc.effective.thermal_conductivity_W_mK;
    double dT = 0.0;
    if (mc.boundaries.size() >= 2) {
        dT = std::abs(mc.boundaries[0].temperature_K - mc.boundaries[1].temperature_K);
    }

    // q = k * dT / L  (assume L = 10mm = 0.01m)
    double L = 0.01;
    if (!mc.layers.empty()) {
        L = 0;
        for (const auto& layer : mc.layers) L += layer.thickness_mm * 0.001;
    }

    double q = (L > 0 && k > 0) ? k * dT / L : 0.0;

    // Thermal resistance
    double R_total = (k > 0 && L > 0) ? L / k : 1e10;
    for (const auto& layer : mc.layers) {
        R_total += layer.interface_resistance_m2K_W;
    }

    r.primary_value = q;
    r.primary_unit = "W/m2";
    r.primary_label = "Heat Flux";
    r.secondary_value = R_total;
    r.secondary_unit = "m2K/W";
    r.secondary_label = "Thermal Resistance";

    // Generate temperature profile series
    int n_points = 20;
    for (int i = 0; i <= n_points; ++i) {
        double x = static_cast<double>(i) / n_points;
        double T_hot = mc.boundaries.empty() ? 600.0 : mc.boundaries[0].temperature_K;
        double T_cold = mc.boundaries.size() < 2 ? 300.0 : mc.boundaries[1].temperature_K;
        double T = T_hot - (T_hot - T_cold) * x;
        r.series.push_back({x * L * 1000.0, T, ""});
    }
    r.series_x_label = "Position (mm)";
    r.series_y_label = "Temperature (K)";

    r.numerical_stability = 1.0;
    r.physical_plausibility = (q > 0 && q < 1e10) ? 1.0 : 0.5;

    return r;
}

ExperimentResult ExperimentRunner::transient_heating(const MaterialCase& mc) const {
    ExperimentResult r;
    r.type = ExperimentType::TRANSIENT_HEATING;
    r.experiment_name = experiment_name(r.type);

    double rho = mc.effective.density_kg_m3;
    double Cp = mc.effective.specific_heat_J_kgK;
    double k = mc.effective.thermal_conductivity_W_mK;
    double alpha = (rho > 0 && Cp > 0) ? k / (rho * Cp) : 1e-6;  // thermal diffusivity

    double T_init = mc.thermal_load.initial_temperature_K;
    double T_peak = mc.thermal_load.peak_temperature_K;
    double rate = mc.thermal_load.heating_rate_K_s;
    double t_heat = (rate > 0) ? (T_peak - T_init) / rate : 100.0;

    // Biot number check (assume h=100 W/m2K, L=0.01m)
    double Bi = 100.0 * 0.01 / std::max(k, 0.001);

    r.primary_value = t_heat;
    r.primary_unit = "s";
    r.primary_label = "Heating Time";
    r.secondary_value = alpha;
    r.secondary_unit = "m2/s";
    r.secondary_label = "Thermal Diffusivity";

    // Temperature vs time series
    int n = 25;
    for (int i = 0; i <= n; ++i) {
        double t = t_heat * static_cast<double>(i) / n;
        double T = T_init + rate * t;
        if (T > T_peak) T = T_peak;
        r.series.push_back({t, T, ""});
    }
    r.series_x_label = "Time (s)";
    r.series_y_label = "Temperature (K)";

    r.numerical_stability = (Bi < 0.1) ? 1.0 : 0.8;
    r.physical_plausibility = (alpha > 0 && alpha < 1.0) ? 1.0 : 0.6;
    r.notes = "Biot number: " + fmt_double(Bi, 3);

    return r;
}

ExperimentResult ExperimentRunner::transient_cooling(const MaterialCase& mc) const {
    ExperimentResult r;
    r.type = ExperimentType::TRANSIENT_COOLING;
    r.experiment_name = experiment_name(r.type);

    double rho = mc.effective.density_kg_m3;
    double Cp = mc.effective.specific_heat_J_kgK;
    double k = mc.effective.thermal_conductivity_W_mK;

    double T_peak = mc.thermal_load.peak_temperature_K;
    double T_amb = 300.0;
    double h = 100.0;  // convection coefficient W/m2K
    double L = 0.01;   // characteristic length

    // Biot number: Bi = h*L/k
    double Bi = h * L / std::max(k, 0.001);

    // Lumped capacitance: T(t) = T_amb + (T_peak - T_amb) * exp(-h*A*t / (rho*V*Cp))
    // For slab: tau = rho * Cp * L / h
    double tau = (h > 0) ? rho * Cp * L / h : 1000.0;
    double t_cool_95 = 3.0 * tau;  // 95% cooling

    r.primary_value = t_cool_95;
    r.primary_unit = "s";
    r.primary_label = "95% Cooling Time";
    r.secondary_value = tau;
    r.secondary_unit = "s";
    r.secondary_label = "Time Constant";

    int n = 25;
    for (int i = 0; i <= n; ++i) {
        double t = t_cool_95 * static_cast<double>(i) / n;
        double T = T_amb + (T_peak - T_amb) * std::exp(-t / std::max(tau, 0.001));
        r.series.push_back({t, T, ""});
    }
    r.series_x_label = "Time (s)";
    r.series_y_label = "Temperature (K)";

    r.numerical_stability = (tau > 0.01) ? 1.0 : 0.6;
    r.physical_plausibility = 1.0;
    r.notes = "Biot number: " + fmt_double(Bi, 3) +
              (Bi < 0.1 ? " (lumped capacitance valid)" : " (spatial gradients significant)");

    return r;
}

ExperimentResult ExperimentRunner::thermal_expansion(const MaterialCase& mc) const {
    ExperimentResult r;
    r.type = ExperimentType::THERMAL_EXPANSION;
    r.experiment_name = experiment_name(r.type);

    double alpha = mc.effective.thermal_expansion_1_K;
    double dT = mc.thermal_load.peak_temperature_K - mc.thermal_load.initial_temperature_K;
    double L0 = 100.0;  // mm reference length

    double dL = L0 * alpha * dT;  // mm
    double strain = alpha * dT;

    r.primary_value = dL;
    r.primary_unit = "mm";
    r.primary_label = "Linear Expansion (per 100mm)";
    r.secondary_value = strain;
    r.secondary_unit = "mm/mm";
    r.secondary_label = "Thermal Strain";

    // Expansion vs temperature
    int n = 20;
    for (int i = 0; i <= n; ++i) {
        double T = mc.thermal_load.initial_temperature_K + dT * static_cast<double>(i) / n;
        double expansion = L0 * alpha * (T - mc.thermal_load.initial_temperature_K);
        r.series.push_back({T, expansion, ""});
    }
    r.series_x_label = "Temperature (K)";
    r.series_y_label = "Expansion (mm per 100mm)";

    r.numerical_stability = 1.0;
    if (alpha < 1.0e-12) {
        r.physical_plausibility = 0.3;
        r.notes = "WARNING: Near-zero thermal expansion coefficient — data unreliable";
    } else {
        r.physical_plausibility = (strain < 0.1) ? 1.0 : 0.7;
    }

    return r;
}

ExperimentResult ExperimentRunner::thermal_stress(const MaterialCase& mc) const {
    ExperimentResult r;
    r.type = ExperimentType::THERMAL_STRESS;
    r.experiment_name = experiment_name(r.type);

    double E = mc.effective.elastic_modulus_GPa * 1000.0;  // MPa
    double alpha = mc.effective.thermal_expansion_1_K;
    double nu = mc.effective.poisson_ratio;
    double dT = mc.thermal_load.peak_temperature_K - mc.thermal_load.initial_temperature_K;

    // Constrained thermal stress: sigma = E * alpha * dT / (1 - nu)
    double sigma = E * alpha * dT / std::max(1.0 - nu, 0.01);
    double safety_factor = (sigma > 0) ? mc.effective.yield_strength_MPa / sigma : 999.0;

    r.primary_value = sigma;
    r.primary_unit = "MPa";
    r.primary_label = "Thermal Stress (constrained)";
    r.secondary_value = safety_factor;
    r.secondary_unit = "";
    r.secondary_label = "Safety Factor vs Yield";

    int n = 20;
    for (int i = 0; i <= n; ++i) {
        double T = mc.thermal_load.initial_temperature_K + dT * static_cast<double>(i) / n;
        double dT_i = T - mc.thermal_load.initial_temperature_K;
        double s = E * alpha * dT_i / std::max(1.0 - nu, 0.01);
        r.series.push_back({T, s, ""});
    }
    r.series_x_label = "Temperature (K)";
    r.series_y_label = "Stress (MPa)";

    r.numerical_stability = (std::isfinite(sigma)) ? 1.0 : 0.0;
    if (E < 1.0e-6 && alpha < 1.0e-12) {
        r.physical_plausibility = 0.3;
        r.notes = "WARNING: Near-zero modulus and expansion — stress data unreliable";
    } else {
        r.physical_plausibility = (safety_factor > 0.01) ? 1.0 : 0.5;
        r.notes = (safety_factor < 1.0) ? "WARNING: Yield exceeded" : "Within elastic range";
    }

    return r;
}

ExperimentResult ExperimentRunner::fatigue_cycling(const MaterialCase& mc) const {
    ExperimentResult r;
    r.type = ExperimentType::FATIGUE_CYCLING;
    r.experiment_name = experiment_name(r.type);

    double E = mc.effective.elastic_modulus_GPa * 1000.0;
    double alpha = mc.effective.thermal_expansion_1_K;
    double dT = mc.thermal_load.peak_temperature_K - mc.thermal_load.initial_temperature_K;
    double sigma_thermal = E * alpha * dT / std::max(1.0 - mc.effective.poisson_ratio, 0.01);

    double Se = mc.effective.fatigue_endurance_MPa;
    double b = mc.effective.fatigue_exponent;

    // Basquin's law: N = (sigma_a / Se)^(1/b) approximate cycles to failure
    double sigma_a = sigma_thermal / 2.0;  // alternating stress
    double N_fail = 1e10;
    if (Se > 0 && sigma_a > 0 && sigma_a > Se) {
        N_fail = std::pow(sigma_a / Se, 1.0 / b);
    } else if (sigma_a <= Se) {
        N_fail = 1e10;  // infinite life
    }

    double damage_per_cycle = (N_fail > 0) ? 1.0 / N_fail : 1.0;
    double total_damage = damage_per_cycle * mc.thermal_load.num_cycles;

    r.primary_value = N_fail;
    r.primary_unit = "cycles";
    r.primary_label = "Predicted Cycles to Failure";
    r.secondary_value = total_damage;
    r.secondary_unit = "";
    r.secondary_label = "Miner Cumulative Damage";

    // S-N curve
    for (int i = 1; i <= 20; ++i) {
        double N = std::pow(10.0, 1.0 + i * 0.4);
        double S = Se * std::pow(N, b);
        r.series.push_back({std::log10(N), S, ""});
    }
    r.series_x_label = "log10(Cycles)";
    r.series_y_label = "Stress Amplitude (MPa)";

    r.numerical_stability = (std::isfinite(N_fail) && N_fail > 0) ? 1.0 : 0.3;
    r.physical_plausibility = (total_damage < 100.0) ? 1.0 : 0.5;
    r.notes = (total_damage > 1.0) ? "FAILURE: Miner damage > 1.0" : "Survivable";

    return r;
}

ExperimentResult ExperimentRunner::sensitivity_sweep(const MaterialCase& mc) const {
    ExperimentResult r;
    r.type = ExperimentType::SENSITIVITY_SWEEP;
    r.experiment_name = experiment_name(r.type);

    // Sweep thermal conductivity ±50%
    double k_base = mc.effective.thermal_conductivity_W_mK;
    double dT = 0;
    if (mc.boundaries.size() >= 2)
        dT = std::abs(mc.boundaries[0].temperature_K - mc.boundaries[1].temperature_K);
    double L = 0.01;

    for (int i = -10; i <= 10; ++i) {
        double factor = 1.0 + i * 0.05;
        double k = k_base * factor;
        double q = (k > 0) ? k * dT / L : 0.0;
        r.series.push_back({factor, q, ""});
    }

    r.series_x_label = "k/k_base";
    r.series_y_label = "Heat Flux (W/m2)";

    double q_base = (k_base > 0) ? k_base * dT / L : 0.0;
    r.primary_value = q_base;
    r.primary_unit = "W/m2";
    r.primary_label = "Baseline Heat Flux";
    r.secondary_value = k_base;
    r.secondary_unit = "W/mK";
    r.secondary_label = "Baseline Conductivity";

    r.numerical_stability = 1.0;
    r.physical_plausibility = 1.0;

    return r;
}

ExperimentResult ExperimentRunner::phase_change_approx(const MaterialCase& mc) const {
    ExperimentResult r;
    r.type = ExperimentType::PHASE_CHANGE_APPROX;
    r.experiment_name = experiment_name(r.type);

    double Tm = mc.effective.melting_point_K;
    double Tb = mc.effective.boiling_point_K;
    double T_peak = mc.thermal_load.peak_temperature_K;

    std::string phase;
    if (T_peak < Tm)      phase = "Solid";
    else if (T_peak < Tb) phase = "Liquid";
    else                   phase = "Gas/Plasma";

    // Estimate latent heat (Trouton's rule: delta_Hv ~ 88 * Tb J/mol)
    double latent_heat = 88.0 * Tb;  // J/mol

    r.primary_value = Tm;
    r.primary_unit = "K";
    r.primary_label = "Melting Point";
    r.secondary_value = latent_heat;
    r.secondary_unit = "J/mol";
    r.secondary_label = "Est. Latent Heat (Trouton)";
    r.notes = "Predicted phase at T_peak=" + fmt_double(T_peak, 0) + "K: " + phase;

    // Phase diagram proxy
    r.series.push_back({Tm, 0.0, "Solid->Liquid"});
    r.series.push_back({Tb, 0.0, "Liquid->Gas"});
    r.series.push_back({T_peak, 1.0, "Operating point"});
    r.series_x_label = "Temperature (K)";
    r.series_y_label = "Phase Transition Indicator";

    r.numerical_stability = 1.0;
    r.physical_plausibility = (Tm > 0 && Tb > Tm) ? 1.0 : 0.5;

    return r;
}

ExperimentResult ExperimentRunner::oxidation_proxy(const MaterialCase& mc) const {
    ExperimentResult r;
    r.type = ExperimentType::OXIDATION_PROXY;
    r.experiment_name = experiment_name(r.type);

    // Arrhenius-like oxidation rate proxy
    // rate = A * exp(-Ea / (R * T))
    double R_gas = 8.314;
    double Ea = 50000.0 + mc.effective.melting_point_K * 20.0;  // activation energy proxy
    double T = mc.thermal_load.peak_temperature_K;
    double rate = std::exp(-Ea / (R_gas * std::max(T, 100.0)));

    // Pilling-Bedworth ratio proxy (based on density)
    double PB_ratio = mc.effective.density_kg_m3 / 3000.0;  // simplified

    r.primary_value = rate * 1e6;  // scaled
    r.primary_unit = "arb. units";
    r.primary_label = "Oxidation Rate Proxy";
    r.secondary_value = PB_ratio;
    r.secondary_unit = "";
    r.secondary_label = "Pilling-Bedworth Ratio Proxy";

    // Rate vs temperature
    for (int i = 0; i <= 20; ++i) {
        double Ti = 300.0 + i * (T - 300.0) / 20.0;
        double ri = std::exp(-Ea / (R_gas * std::max(Ti, 100.0))) * 1e6;
        r.series.push_back({Ti, ri, ""});
    }
    r.series_x_label = "Temperature (K)";
    r.series_y_label = "Oxidation Rate (arb.)";

    r.numerical_stability = 1.0;
    r.physical_plausibility = (PB_ratio > 0.5 && PB_ratio < 3.0) ? 0.9 : 0.5;
    r.notes = (PB_ratio < 1.0) ? "Oxide likely porous (non-protective)" : "Oxide may be protective";

    return r;
}

ExperimentResult ExperimentRunner::crack_initiation_proxy(const MaterialCase& mc) const {
    ExperimentResult r;
    r.type = ExperimentType::CRACK_INITIATION_PROXY;
    r.experiment_name = experiment_name(r.type);

    double E = mc.effective.elastic_modulus_GPa * 1000.0;
    double alpha = mc.effective.thermal_expansion_1_K;
    double dT = mc.thermal_load.peak_temperature_K - mc.thermal_load.initial_temperature_K;
    double sigma = E * alpha * dT / std::max(1.0 - mc.effective.poisson_ratio, 0.01);
    double Su = mc.effective.ultimate_strength_MPa;

    // Stress intensity factor proxy: K ~ sigma * sqrt(pi * a)
    // Assume initial flaw size a = 1mm
    double a = 0.001;  // meters
    double K = sigma * std::sqrt(M_PI * a);

    // Fracture toughness proxy (rough): Kc ~ 10-100 MPa*sqrt(m)
    double Kc = 20.0 + (Su / 100.0) * 10.0;

    double crack_risk = K / std::max(Kc, 1.0);

    r.primary_value = K;
    r.primary_unit = "MPa*sqrt(m)";
    r.primary_label = "Stress Intensity Factor";
    r.secondary_value = crack_risk;
    r.secondary_unit = "";
    r.secondary_label = "K/Kc Crack Risk Ratio";

    // Risk vs flaw size
    for (int i = 1; i <= 20; ++i) {
        double ai = i * 0.5e-3;  // 0.5mm to 10mm
        double Ki = sigma * std::sqrt(M_PI * ai);
        double risk = Ki / std::max(Kc, 1.0);
        r.series.push_back({ai * 1000.0, risk, ""});
    }
    r.series_x_label = "Flaw Size (mm)";
    r.series_y_label = "K/Kc Risk Ratio";

    r.numerical_stability = (std::isfinite(K)) ? 1.0 : 0.0;
    r.physical_plausibility = (crack_risk < 100.0) ? 1.0 : 0.4;
    r.notes = (crack_risk > 1.0) ? "CRITICAL: Crack propagation likely" : "Below fracture threshold";

    return r;
}

// ============================================================================
// Report Writer
// ============================================================================

std::string ReportWriter::to_markdown(const TechnicalReport& report) {
    std::ostringstream md;

    md << "# " << report.title << "\n\n";
    md << "**Report ID:** " << report.report_id << "  \n";
    md << "**Timestamp:** " << report.timestamp << "  \n";
    md << "**Complexity:** " << complexity_name(report.current_level) << "  \n";
    md << "**Report #** " << report.total_reports_so_far << "  \n\n";

    md << "## Abstract\n\n" << report.abstract_text << "\n\n";

    // Material Case
    md << "## Material System\n\n";
    md << "**Case Name:** " << report.material_case.case_name << "  \n";
    md << "**Description:** " << report.material_case.description << "  \n";
    md << "**Components:** " << report.material_case.components.size() << "  \n";
    md << "**Gamma Factor:** " << fmt_double(report.material_case.gamma_factor, 3) << "  \n";
    md << "**Rarity Score:** " << fmt_double(report.material_case.rarity_score, 3) << "  \n";
    md << "**Instability Index:** " << fmt_double(report.material_case.instability_index, 3) << "  \n\n";

    // Components table
    if (!report.material_case.components.empty()) {
        md << "### Components\n\n";
        md << "| # | Name | Formula | Wt% | Category | Density (kg/m3) | E (GPa) | k (W/mK) | Tm (K) | Confidence |\n";
        md << "|---|------|---------|-----|----------|-----------------|---------|----------|--------|------------|\n";
        for (size_t i = 0; i < report.material_case.components.size(); ++i) {
            const auto& c = report.material_case.components[i];
            double frac = (i < report.material_case.fractions.size()) ? report.material_case.fractions[i] : 0.0;
            md << "| " << i + 1
               << " | " << c.name
               << " | " << c.formula
               << " | " << fmt_double(frac * 100.0, 1)
               << " | " << c.category
               << " | " << fmt_double(c.density_kg_m3, 1)
               << " | " << fmt_double(c.elastic_modulus_GPa, 1)
               << " | " << fmt_double(c.thermal_conductivity_W_mK, 2)
               << " | " << fmt_double(c.melting_point_K, 0)
               << " | " << fmt_double(c.confidence_score, 2)
               << " |\n";
        }
        md << "\n";
    }

    // Effective properties
    md << "### Effective Properties\n\n";
    md << property_table(report.material_case.effective) << "\n";

    // Layers
    if (!report.material_case.layers.empty()) {
        md << "### Layer Structure\n\n";
        md << "| Layer | Material | Thickness (mm) | Interface R (m2K/W) | Anisotropy |\n";
        md << "|-------|----------|----------------|---------------------|------------|\n";
        for (size_t i = 0; i < report.material_case.layers.size(); ++i) {
            const auto& l = report.material_case.layers[i];
            md << "| " << i + 1
               << " | " << l.material.name
               << " | " << fmt_double(l.thickness_mm, 2)
               << " | " << fmt_sci(l.interface_resistance_m2K_W)
               << " | " << fmt_double(l.material.anisotropy_factor, 2)
               << " |\n";
        }
        md << "\n";
    }

    // Thermal load
    md << "### Thermal Loading\n\n";
    md << "| Parameter | Value |\n";
    md << "|-----------|-------|\n";
    md << "| Initial Temperature | " << fmt_double(report.material_case.thermal_load.initial_temperature_K, 1) << " K |\n";
    md << "| Peak Temperature | " << fmt_double(report.material_case.thermal_load.peak_temperature_K, 1) << " K |\n";
    if (report.material_case.thermal_load.is_transient) {
        md << "| Heating Rate | " << fmt_double(report.material_case.thermal_load.heating_rate_K_s, 2) << " K/s |\n";
        md << "| Cooling Rate | " << fmt_double(report.material_case.thermal_load.cooling_rate_K_s, 2) << " K/s |\n";
        md << "| Cycle Period | " << fmt_double(report.material_case.thermal_load.cycle_period_s, 2) << " s |\n";
        md << "| Number of Cycles | " << report.material_case.thermal_load.num_cycles << " |\n";
    }
    md << "\n";

    // Defects
    if (!report.material_case.defects.empty()) {
        md << "### Defects\n\n";
        md << "| Type | Concentration | Size (nm) |\n";
        md << "|------|---------------|-----------|\n";
        for (const auto& d : report.material_case.defects) {
            md << "| " << d.type
               << " | " << fmt_sci(d.concentration)
               << " | " << fmt_double(d.size_nm, 1) << " |\n";
        }
        md << "\n";
    }

    // Experiments
    md << "## Experiment Results\n\n";
    md << results_table(report.experiments) << "\n";

    // Detailed experiment sections
    for (const auto& exp : report.experiments) {
        md << "### " << exp.experiment_name << "\n\n";
        md << "**" << exp.primary_label << ":** " << fmt_sci(exp.primary_value)
           << " " << exp.primary_unit << "  \n";
        md << "**" << exp.secondary_label << ":** " << fmt_sci(exp.secondary_value)
           << " " << exp.secondary_unit << "  \n";
        md << "**Numerical Stability:** " << fmt_double(exp.numerical_stability, 2) << "  \n";
        md << "**Physical Plausibility:** " << fmt_double(exp.physical_plausibility, 2) << "  \n";
        if (!exp.notes.empty()) {
            md << "**Notes:** " << exp.notes << "  \n";
        }

        // Data series as table (first 10 points)
        if (!exp.series.empty()) {
            md << "\n| " << exp.series_x_label << " | " << exp.series_y_label << " |\n";
            md << "|---|---|\n";
            int limit = std::min(static_cast<int>(exp.series.size()), 10);
            for (int i = 0; i < limit; ++i) {
                md << "| " << fmt_double(exp.series[i].x, 4)
                   << " | " << fmt_sci(exp.series[i].y) << " |\n";
            }
            if (static_cast<int>(exp.series.size()) > 10) {
                md << "| ... | ... |\n";
            }
        }
        md << "\n";
    }

    // Analysis
    md << "## Analysis\n\n";
    md << "| Metric | Value |\n";
    md << "|--------|-------|\n";
    md << "| Overall Stability | " << fmt_double(report.overall_stability_score, 3) << " |\n";
    md << "| Novelty Score | " << fmt_double(report.novelty_score, 3) << " |\n";
    md << "| Thermal Response Index | " << fmt_double(report.thermal_response_index, 3) << " |\n";
    md << "| Deformation Score | " << fmt_double(report.deformation_score, 3) << " |\n\n";

    if (!report.findings.empty()) {
        md << "### Findings\n\n";
        for (const auto& f : report.findings) {
            md << "- " << f << "\n";
        }
        md << "\n";
    }

    if (!report.warnings.empty()) {
        md << "### Warnings\n\n";
        for (const auto& w : report.warnings) {
            md << "- ⚠️ " << w << "\n";
        }
        md << "\n";
    }

    md << "## Conclusion\n\n" << report.conclusion << "\n\n";

    md << "---\n*Generated by VSEPR-SIM Autonomous Report Engine v3.0.1*  \n";
    md << "*Work Order: WO-TMS-CRG-001*\n";

    return md.str();
}

std::string ReportWriter::csv_header() {
    return "report_id,case_name,level,components,gamma,rarity,instability,"
           "T_peak_K,experiments,stability,novelty,thermal_idx,deformation,"
           "warnings,conclusion_length,timestamp";
}

std::string ReportWriter::to_csv_line(const TechnicalReport& report) {
    std::ostringstream csv;
    csv << report.report_id << ","
        << "\"" << report.material_case.case_name << "\","
        << static_cast<int>(report.current_level) << ","
        << report.material_case.components.size() << ","
        << fmt_double(report.material_case.gamma_factor, 3) << ","
        << fmt_double(report.material_case.rarity_score, 3) << ","
        << fmt_double(report.material_case.instability_index, 3) << ","
        << fmt_double(report.material_case.thermal_load.peak_temperature_K, 1) << ","
        << report.experiments.size() << ","
        << fmt_double(report.overall_stability_score, 3) << ","
        << fmt_double(report.novelty_score, 3) << ","
        << fmt_double(report.thermal_response_index, 3) << ","
        << fmt_double(report.deformation_score, 3) << ","
        << report.warnings.size() << ","
        << report.conclusion.size() << ","
        << "\"" << report.timestamp << "\"";
    return csv.str();
}

std::string ReportWriter::results_table(const std::vector<ExperimentResult>& results) {
    std::ostringstream t;
    t << "| Experiment | Primary Value | Unit | Secondary Value | Unit | Stability | Plausibility |\n";
    t << "|------------|---------------|------|-----------------|------|-----------|-------------|\n";
    for (const auto& r : results) {
        t << "| " << r.experiment_name
          << " | " << fmt_sci(r.primary_value)
          << " | " << r.primary_unit
          << " | " << fmt_sci(r.secondary_value)
          << " | " << r.secondary_unit
          << " | " << fmt_double(r.numerical_stability, 2)
          << " | " << fmt_double(r.physical_plausibility, 2)
          << " |\n";
    }
    return t.str();
}

std::string ReportWriter::property_table(const MaterialProperties& props) {
    std::ostringstream t;
    t << "| Property | Value | Unit |\n";
    t << "|----------|-------|------|\n";
    t << "| Density | " << fmt_double(props.density_kg_m3, 1) << " | kg/m3 |\n";
    t << "| Elastic Modulus | " << fmt_double(props.elastic_modulus_GPa, 1) << " | GPa |\n";
    t << "| Yield Strength | " << fmt_double(props.yield_strength_MPa, 1) << " | MPa |\n";
    t << "| Ultimate Strength | " << fmt_double(props.ultimate_strength_MPa, 1) << " | MPa |\n";
    t << "| Poisson Ratio | " << fmt_double(props.poisson_ratio, 3) << " | - |\n";
    t << "| Thermal Conductivity | " << fmt_double(props.thermal_conductivity_W_mK, 2) << " | W/mK |\n";
    t << "| Specific Heat | " << fmt_double(props.specific_heat_J_kgK, 1) << " | J/kgK |\n";
    t << "| Thermal Expansion | " << fmt_sci(props.thermal_expansion_1_K) << " | 1/K |\n";
    t << "| Melting Point | " << fmt_double(props.melting_point_K, 0) << " | K |\n";
    t << "| Boiling Point | " << fmt_double(props.boiling_point_K, 0) << " | K |\n";
    t << "| Fatigue Endurance | " << fmt_double(props.fatigue_endurance_MPa, 1) << " | MPa |\n";
    t << "| Anisotropy Factor | " << fmt_double(props.anisotropy_factor, 2) << " | - |\n";
    t << "| Confidence Score | " << fmt_double(props.confidence_score, 2) << " | 0-1 |\n";
    if (props.is_synthetic) {
        t << "| **Synthetic** | Yes | - |\n";
    }
    return t.str();
}

// ============================================================================
// Autonomous Engine
// ============================================================================

AutonomousEngine::AutonomousEngine(const EngineConfig& config)
    : config_(config), case_gen_(config.base_seed) {
    case_gen_.set_escalation_thresholds(
        config_.threshold_l2, config_.threshold_l3,
        config_.threshold_l4, config_.threshold_l5);
}

ComplexityLevel AutonomousEngine::current_level() const {
    return case_gen_.current_level();
}

TechnicalReport AutonomousEngine::build_report(MaterialCase& mc) {
    TechnicalReport report;
    report.report_id = static_cast<uint64_t>(reports_generated_);
    report.timestamp = timestamp_now();
    report.material_case = mc;
    report.total_reports_so_far = reports_generated_ + 1;
    report.current_level = mc.level;

    // Run experiments
    report.experiments = experiment_runner_.run_all(mc);

    // Build title
    report.title = "TMS-" + std::to_string(report.report_id) + ": " +
                   mc.case_name + " [" + complexity_name(mc.level) + "]";

    return report;
}

void AutonomousEngine::analyze_report(TechnicalReport& report) {
    // Compute aggregate scores
    double stab_sum = 0, plaus_sum = 0;
    for (const auto& e : report.experiments) {
        stab_sum += e.numerical_stability;
        plaus_sum += e.physical_plausibility;
    }
    int ne = std::max(1, static_cast<int>(report.experiments.size()));
    report.overall_stability_score = stab_sum / ne;
    report.novelty_score = report.material_case.rarity_score;

    // Thermal response index
    double dT = report.material_case.thermal_load.peak_temperature_K -
                report.material_case.thermal_load.initial_temperature_K;
    double Tm = report.material_case.effective.melting_point_K;
    report.thermal_response_index = (Tm > 0) ? dT / Tm : dT / 1000.0;

    // Deformation score from thermal stress
    for (const auto& e : report.experiments) {
        if (e.type == ExperimentType::THERMAL_STRESS) {
            double sf = e.secondary_value;  // safety factor
            report.deformation_score = (sf > 0) ? 1.0 / sf : 10.0;
        }
    }

    // Findings
    if (report.thermal_response_index > 0.8) {
        report.findings.push_back("Material approaches or exceeds melting point under thermal load");
    }
    if (report.deformation_score > 1.0) {
        report.findings.push_back("Yield stress exceeded — plastic deformation expected");
        report.warnings.push_back("Yield exceeded at peak temperature");
    }
    if (report.overall_stability_score < 0.7) {
        report.warnings.push_back("Low numerical stability in one or more experiments");
    }

    for (const auto& e : report.experiments) {
        if (e.type == ExperimentType::FATIGUE_CYCLING && !e.notes.empty()) {
            if (e.notes.find("FAILURE") != std::string::npos) {
                report.findings.push_back("Fatigue failure predicted under thermal cycling");
                report.warnings.push_back("Fatigue life exceeded");
            }
        }
        if (e.type == ExperimentType::CRACK_INITIATION_PROXY) {
            if (e.secondary_value > 1.0) {
                report.findings.push_back("Crack propagation risk: K/Kc > 1.0");
                report.warnings.push_back("Fracture toughness exceeded");
            }
        }
    }

    if (report.material_case.effective.is_synthetic) {
        report.findings.push_back("Contains synthetic material components — reduced confidence");
    }

    if (!report.material_case.defects.empty()) {
        report.findings.push_back("Defects present: " +
            std::to_string(report.material_case.defects.size()) + " defect type(s)");
    }

    // Abstract
    {
        std::ostringstream abs;
        abs << "This report presents a " << complexity_name(report.current_level)
            << " thermal-materials digital experiment on "
            << report.material_case.case_name << ". "
            << "The system consists of " << report.material_case.components.size()
            << " component(s) subjected to thermal loading from "
            << fmt_double(report.material_case.thermal_load.initial_temperature_K, 0)
            << "K to "
            << fmt_double(report.material_case.thermal_load.peak_temperature_K, 0)
            << "K. "
            << report.experiments.size() << " digital experiment(s) were performed. "
            << "Overall numerical stability: " << fmt_double(report.overall_stability_score, 2)
            << ". Warnings: " << report.warnings.size() << ".";
        report.abstract_text = abs.str();
    }

    // Conclusion
    {
        std::ostringstream conc;
        if (report.warnings.empty()) {
            conc << "The material system " << report.material_case.case_name
                 << " demonstrates satisfactory performance under the specified "
                 << "thermal loading conditions. All experiments converged with "
                 << "acceptable stability (" << fmt_double(report.overall_stability_score, 2) << ").";
        } else {
            conc << "The material system " << report.material_case.case_name
                 << " exhibits " << report.warnings.size() << " warning(s) "
                 << "under the specified conditions. ";
            if (report.deformation_score > 1.0) {
                conc << "Plastic deformation is expected. ";
            }
            if (report.thermal_response_index > 0.8) {
                conc << "The operating temperature approaches the melting point. ";
            }
            conc << "Further investigation is recommended for operational deployment.";
        }
        report.conclusion = conc.str();
    }
}

TechnicalReport AutonomousEngine::generate_one() {
    auto mc = case_gen_.generate_next();
    auto report = build_report(mc);
    analyze_report(report);
    reports_generated_++;
    return report;
}

std::string AutonomousEngine::make_output_path(const TechnicalReport& report, const std::string& ext) {
    std::ostringstream path;
    path << config_.output_dir << "/TMS-"
         << std::setfill('0') << std::setw(6) << report.report_id
         << ext;
    return path.str();
}

void AutonomousEngine::write_report(const TechnicalReport& report) {
    std::string path = make_output_path(report, ".md");
    std::ofstream out(path);
    if (out) {
        out << ReportWriter::to_markdown(report);
    }
}

void AutonomousEngine::write_csv_entry(const TechnicalReport& report) {
    std::string path = config_.output_dir + "/summary.csv";
    bool exists = std::filesystem::exists(path);
    std::ofstream out(path, std::ios::app);
    if (out) {
        if (!exists) {
            out << ReportWriter::csv_header() << "\n";
        }
        out << ReportWriter::to_csv_line(report) << "\n";
    }
}

void AutonomousEngine::print_progress(const TechnicalReport& report) {
    std::cout << "[" << std::setw(6) << report.report_id << "] "
              << std::left << std::setw(8) << complexity_name(report.current_level)
              << " | " << std::setw(40) << report.material_case.case_name.substr(0, 40)
              << " | exp=" << report.experiments.size()
              << " stab=" << fmt_double(report.overall_stability_score, 2)
              << " warn=" << report.warnings.size()
              << "\n";
}

int AutonomousEngine::run() {
    // Create output directory
    std::filesystem::create_directories(config_.output_dir);

    if (config_.print_progress) {
        std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  VSEPR-SIM Autonomous Report Engine v3.0.1                   ║\n";
        std::cout << "║  Work Order: WO-TMS-CRG-001                                  ║\n";
        std::cout << "║  Target Reports: " << std::setw(6) << config_.target_reports
                  << "                                        ║\n";
        std::cout << "║  Output: " << std::setw(50) << config_.output_dir << " ║\n";
        std::cout << "║  Seed: " << std::setw(12) << config_.base_seed
                  << "                                        ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
        std::cout << "Escalation: L2@" << config_.threshold_l2
                  << " L3@" << config_.threshold_l3
                  << " L4@" << config_.threshold_l4
                  << " L5@" << config_.threshold_l5 << "\n\n";
    }

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < config_.target_reports; ++i) {
        auto report = generate_one();

        if (config_.write_individual) {
            write_report(report);
        }
        if (config_.write_csv_log) {
            write_csv_entry(report);
        }
        if (config_.print_progress && (i % config_.progress_interval == 0 || i == config_.target_reports - 1)) {
            print_progress(report);
        }
    }

    auto end = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    if (config_.print_progress) {
        std::cout << "\n════════════════════════════════════════════\n";
        std::cout << "  Completed: " << reports_generated_ << " reports\n";
        std::cout << "  Time:      " << fmt_double(elapsed_ms, 1) << " ms\n";
        std::cout << "  Rate:      " << fmt_double(reports_generated_ / (elapsed_ms / 1000.0), 1) << " reports/s\n";
        std::cout << "  Final Level: " << complexity_name(current_level()) << "\n";
        std::cout << "  Output:    " << config_.output_dir << "/\n";
        std::cout << "════════════════════════════════════════════\n";
    }

    return 0;
}

} // namespace report
} // namespace vsepr
