/**
 * bio_report_engine.cpp
 * ---------------------
 * Organic / biochemical report-generation engine implementation.
 * Work Order: WO-BIO-CRG-003
 *
 * Implements: CompoundPropertyEngine, BioCaseGenerator, BioExperimentRunner,
 *             BioReportWriter, BioAutonomousEngine
 *
 * Compound data sources:
 *   PubChem, Merck Index, phytochemistry literature, CRC Handbook
 *   All values are curated estimates suitable for deterministic simulation.
 *   Confidence scores reflect data quality (1.0 = well-characterised,
 *   0.5 = estimated from class averages).
 */

#include "core/bio_report_engine.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <map>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace vsepr {
namespace report {
namespace bio {

// ============================================================================
// String Helpers (local)
// ============================================================================

static std::string fmt_d(double v, int prec = 4) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << v;
    return ss.str();
}

static std::string fmt_s(double v, int prec = 3) {
    std::ostringstream ss;
    ss << std::scientific << std::setprecision(prec) << v;
    return ss.str();
}

static std::string ts_now() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// ============================================================================
// Enum name helpers
// ============================================================================

const char* domain_name(SystemDomain d) {
    switch (d) {
        case SystemDomain::FLORAL_PIGMENT:   return "Floral-Pigment";
        case SystemDomain::FLORAL_VOLATILE:  return "Floral-Volatile";
        case SystemDomain::LEAF_SEASONAL:    return "Leaf-Seasonal";
        case SystemDomain::LEAF_STRUCTURE:   return "Leaf-Structure";
        case SystemDomain::DEVELOPMENTAL:    return "Developmental";
    }
    return "Unknown";
}

const char* family_name(CompoundFamily f) {
    switch (f) {
        case CompoundFamily::ANTHOCYANIN:        return "Anthocyanin";
        case CompoundFamily::CHALCONE:           return "Chalcone";
        case CompoundFamily::AURONE:             return "Aurone";
        case CompoundFamily::FLAVONOID:          return "Flavonoid";
        case CompoundFamily::CHLOROPHYLL_A:      return "Chlorophyll a";
        case CompoundFamily::CHLOROPHYLL_B:      return "Chlorophyll b";
        case CompoundFamily::CAROTENOID:         return "Carotenoid";
        case CompoundFamily::TERPENOID:          return "Terpenoid";
        case CompoundFamily::BENZENOID:          return "Benzenoid";
        case CompoundFamily::PHENYLPROPANOID:    return "Phenylpropanoid";
        case CompoundFamily::ALDEHYDE:           return "Aldehyde";
        case CompoundFamily::AMINO_ACID:         return "Amino Acid";
        case CompoundFamily::FATTY_ACID:         return "Fatty Acid";
        case CompoundFamily::SUGAR:              return "Sugar";
        case CompoundFamily::GLYCOSIDE_VOLATILE: return "Glycoside-Bound Volatile";
        case CompoundFamily::PHENOLIC:           return "Phenolic";
    }
    return "Unknown";
}

const char* bio_experiment_name(BioExperimentType type) {
    switch (type) {
        case BioExperimentType::PIGMENT_ABSORPTION_SPECTRUM:   return "Pigment Absorption Spectrum";
        case BioExperimentType::VOLATILE_RELEASE_KINETICS:     return "Volatile Release Kinetics";
        case BioExperimentType::DEVELOPMENTAL_STAGING:         return "Developmental Staging";
        case BioExperimentType::UV_REFLECTANCE_PATTERN:        return "UV Reflectance Pattern";
        case BioExperimentType::SEPARATION_EFFICIENCY:         return "Separation Efficiency";
        case BioExperimentType::SEASONAL_PIGMENT_TRANSITION:   return "Seasonal Pigment Transition";
        case BioExperimentType::SCENT_COMPOSITION_PROFILE:     return "Scent Composition Profile";
        case BioExperimentType::PATHWAY_CONSTRAINT_ANALYSIS:   return "Pathway Constraint Analysis";
    }
    return "Unknown";
}

const char* stage_name(DevelopmentalStage s) {
    switch (s) {
        case DevelopmentalStage::BUD:            return "Bud";
        case DevelopmentalStage::EARLY_OPENING:  return "Early Opening";
        case DevelopmentalStage::FULL_BLOOM:     return "Full Bloom";
        case DevelopmentalStage::SENESCENCE:     return "Senescence";
    }
    return "Unknown";
}

const char* season_name(Season s) {
    switch (s) {
        case Season::SPRING: return "Spring";
        case Season::SUMMER: return "Summer";
        case Season::AUTUMN: return "Autumn";
        case Season::WINTER: return "Winter";
    }
    return "Unknown";
}

// ============================================================================
// Compound Property Engine — Curated Compound Table
// ============================================================================
//
// Each record:
//   name, formula, family, role,
//   molar_mass (g/mol), density (kg/m3), solubility (g/L @25C), logP,
//   lambda_max (nm), absorptivity (L/mol/cm), uv_active, uv_lambda (nm),
//   vapor_pressure (Pa @25C), is_volatile,
//   formation_rate (1/day), degradation_rate (1/day), decomposition_K (K),
//   confidence (0-1)
//
// Sources: PubChem, Merck Index, phytochemistry literature, CRC Handbook.

const std::vector<CompoundPropertyEngine::CompoundRecord>&
CompoundPropertyEngine::compound_table() {
    static const std::vector<CompoundRecord> table = {
        // ---- PIGMENTS ----
        {"Cyanidin-3-glucoside", "C21H21O11+", CompoundFamily::ANTHOCYANIN, "pigment",
         449.4, 1500.0, 25.0, 0.4, 520.0, 26900.0, true, 280.0,
         0.0, false, 0.15, 0.08, 473.0, 0.90},

        {"Pelargonidin-3-glucoside", "C21H21O10+", CompoundFamily::ANTHOCYANIN, "pigment",
         433.4, 1480.0, 20.0, 0.6, 503.0, 24800.0, true, 275.0,
         0.0, false, 0.14, 0.09, 468.0, 0.88},

        {"Delphinidin-3-glucoside", "C21H21O12+", CompoundFamily::ANTHOCYANIN, "pigment",
         465.4, 1520.0, 30.0, 0.2, 543.0, 29000.0, true, 278.0,
         0.0, false, 0.12, 0.10, 478.0, 0.85},

        {"Butein", "C15H12O5", CompoundFamily::CHALCONE, "pigment",
         272.3, 1350.0, 1.5, 1.8, 390.0, 22000.0, true, 320.0,
         0.0, false, 0.10, 0.06, 498.0, 0.80},

        {"Isoliquiritigenin", "C15H12O4", CompoundFamily::CHALCONE, "pigment",
         256.3, 1310.0, 0.8, 2.3, 372.0, 20500.0, true, 315.0,
         0.0, false, 0.09, 0.05, 508.0, 0.78},

        {"Sulfuretin", "C15H10O5", CompoundFamily::AURONE, "pigment",
         270.2, 1400.0, 0.5, 1.9, 403.0, 23500.0, true, 330.0,
         0.0, false, 0.08, 0.07, 510.0, 0.75},

        {"Kaempferol", "C15H10O6", CompoundFamily::FLAVONOID, "pigment",
         286.2, 1430.0, 0.1, 1.9, 366.0, 19100.0, true, 265.0,
         0.0, false, 0.12, 0.04, 550.0, 0.92},

        {"Quercetin", "C15H10O7", CompoundFamily::FLAVONOID, "pigment",
         302.2, 1460.0, 0.06, 1.5, 370.0, 20000.0, true, 256.0,
         0.0, false, 0.11, 0.03, 588.0, 0.95},

        // ---- CHLOROPHYLLS ----
        {"Chlorophyll a", "C55H72MgN4O5", CompoundFamily::CHLOROPHYLL_A, "pigment",
         893.5, 1100.0, 0.0, 6.5, 662.0, 111700.0, true, 430.0,
         0.0, false, 0.20, 0.05, 423.0, 0.95},

        {"Chlorophyll b", "C55H70MgN4O6", CompoundFamily::CHLOROPHYLL_B, "pigment",
         907.5, 1100.0, 0.0, 5.8, 644.0, 51800.0, true, 455.0,
         0.0, false, 0.25, 0.06, 418.0, 0.93},

        // ---- CAROTENOIDS ----
        {"beta-Carotene", "C40H56", CompoundFamily::CAROTENOID, "pigment",
         536.9, 1000.0, 0.0, 15.5, 450.0, 139500.0, false, 0.0,
         0.0, false, 0.10, 0.03, 448.0, 0.95},

        {"Lutein", "C40H56O2", CompoundFamily::CAROTENOID, "pigment",
         568.9, 1050.0, 0.0, 7.9, 445.0, 145000.0, false, 0.0,
         0.0, false, 0.09, 0.04, 443.0, 0.90},

        {"Zeaxanthin", "C40H56O2", CompoundFamily::CAROTENOID, "pigment",
         568.9, 1060.0, 0.0, 8.1, 449.0, 141000.0, false, 0.0,
         0.0, false, 0.08, 0.04, 445.0, 0.88},

        // ---- TERPENOID VOLATILES ----
        {"Linalool", "C10H18O", CompoundFamily::TERPENOID, "volatile",
         154.3, 868.0, 1.6, 2.97, 0.0, 0.0, false, 0.0,
         21.3, true, 0.30, 0.15, 471.0, 0.95},

        {"Geraniol", "C10H18O", CompoundFamily::TERPENOID, "volatile",
         154.3, 889.0, 0.7, 3.47, 0.0, 0.0, false, 0.0,
         4.0, true, 0.25, 0.12, 503.0, 0.92},

        {"beta-Ocimene", "C10H16", CompoundFamily::TERPENOID, "volatile",
         136.2, 800.0, 0.0, 4.3, 0.0, 0.0, false, 0.0,
         267.0, true, 0.40, 0.20, 373.0, 0.88},

        {"Farnesene", "C15H24", CompoundFamily::TERPENOID, "volatile",
         204.4, 841.0, 0.0, 6.1, 0.0, 0.0, false, 0.0,
         0.8, true, 0.18, 0.10, 523.0, 0.82},

        // ---- BENZENOID / PHENYLPROPANOID VOLATILES ----
        {"Methyl benzoate", "C8H8O2", CompoundFamily::BENZENOID, "volatile",
         136.2, 1094.0, 2.1, 2.12, 0.0, 0.0, false, 0.0,
         53.0, true, 0.20, 0.10, 472.0, 0.90},

        {"Benzyl alcohol", "C7H8O", CompoundFamily::BENZENOID, "volatile",
         108.1, 1045.0, 40.0, 1.1, 0.0, 0.0, false, 0.0,
         12.0, true, 0.22, 0.11, 478.0, 0.92},

        {"Eugenol", "C10H12O2", CompoundFamily::PHENYLPROPANOID, "volatile",
         164.2, 1066.0, 2.5, 2.27, 0.0, 0.0, true, 282.0,
         3.2, true, 0.15, 0.08, 527.0, 0.90},

        {"Isoeugenol", "C10H12O2", CompoundFamily::PHENYLPROPANOID, "volatile",
         164.2, 1088.0, 1.0, 2.58, 0.0, 0.0, true, 260.0,
         1.1, true, 0.12, 0.07, 539.0, 0.85},

        // ---- ALDEHYDES ----
        {"Hexanal", "C6H12O", CompoundFamily::ALDEHYDE, "volatile",
         100.2, 834.0, 5.6, 1.78, 0.0, 0.0, false, 0.0,
         1200.0, true, 0.50, 0.30, 404.0, 0.88},

        {"(E)-2-Hexenal", "C6H10O", CompoundFamily::ALDEHYDE, "volatile",
         98.1, 846.0, 3.2, 1.58, 0.0, 0.0, false, 0.0,
         530.0, true, 0.45, 0.25, 420.0, 0.85},

        // ---- PRECURSORS AND SUPPORT ----
        {"Phenylalanine", "C9H11NO2", CompoundFamily::AMINO_ACID, "precursor",
         165.2, 1290.0, 27.0, -1.38, 257.0, 195.0, true, 257.0,
         0.0, false, 0.60, 0.10, 556.0, 0.95},

        {"Sucrose", "C12H22O11", CompoundFamily::SUGAR, "structural",
         342.3, 1587.0, 2000.0, -3.7, 0.0, 0.0, false, 0.0,
         0.0, false, 0.80, 0.02, 459.0, 0.98},

        {"Palmitic acid", "C16H32O2", CompoundFamily::FATTY_ACID, "structural",
         256.4, 853.0, 0.007, 7.17, 0.0, 0.0, false, 0.0,
         0.0, false, 0.30, 0.01, 624.0, 0.95},

        {"Geranyl glucoside", "C16H28O6", CompoundFamily::GLYCOSIDE_VOLATILE, "precursor",
         316.4, 1200.0, 80.0, -0.5, 0.0, 0.0, false, 0.0,
         0.0, false, 0.10, 0.03, 480.0, 0.70},

        {"Chlorogenic acid", "C16H18O9", CompoundFamily::PHENOLIC, "structural",
         354.3, 1430.0, 40.0, -0.36, 326.0, 19000.0, true, 326.0,
         0.0, false, 0.20, 0.05, 483.0, 0.92},

        {"Caffeic acid", "C9H8O4", CompoundFamily::PHENOLIC, "structural",
         180.2, 1380.0, 1.0, 1.15, 323.0, 18400.0, true, 323.0,
         0.0, false, 0.18, 0.04, 498.0, 0.90},
    };
    return table;
}

// ============================================================================
// Compound Property Engine — Implementation
// ============================================================================

CompoundPropertyEngine::CompoundPropertyEngine() {}

const CompoundPropertyEngine::CompoundRecord*
CompoundPropertyEngine::find_compound(const std::string& name) const {
    for (const auto& rec : compound_table()) {
        if (rec.name == name) return &rec;
    }
    return nullptr;
}

CompoundProperties CompoundPropertyEngine::compound_properties(const std::string& name) const {
    CompoundProperties props;
    const auto* rec = find_compound(name);
    if (!rec) return props;

    props.name = rec->name;
    props.formula = rec->formula;
    props.family = rec->family;
    props.role = rec->role;
    props.molar_mass_g_mol = rec->molar_mass;
    props.density_kg_m3 = rec->density;
    props.solubility_water_g_L = rec->solubility;
    props.log_P = rec->log_P;
    props.lambda_max_nm = rec->lambda_max;
    props.molar_absorptivity = rec->absorptivity;
    props.uv_active = rec->uv_active;
    props.uv_lambda_nm = rec->uv_lambda;
    props.vapor_pressure_Pa_25C = rec->vapor_pressure;
    props.is_volatile = rec->is_volatile;
    props.formation_rate_k = rec->formation_k;
    props.degradation_rate_k = rec->degradation_k;
    props.decomposition_K = rec->decomposition_K;
    props.confidence_score = rec->confidence;
    props.is_glycoside_bound = (rec->family == CompoundFamily::GLYCOSIDE_VOLATILE);

    return props;
}

std::vector<CompoundProperties>
CompoundPropertyEngine::compounds_by_family(CompoundFamily family) const {
    std::vector<CompoundProperties> result;
    for (const auto& rec : compound_table()) {
        if (rec.family == family) {
            result.push_back(compound_properties(rec.name));
        }
    }
    return result;
}

std::vector<CompoundProperties>
CompoundPropertyEngine::compounds_by_role(const std::string& role) const {
    std::vector<CompoundProperties> result;
    for (const auto& rec : compound_table()) {
        if (rec.role == role) {
            result.push_back(compound_properties(rec.name));
        }
    }
    return result;
}

std::vector<CompoundProperties>
CompoundPropertyEngine::floral_pigment_set(std::mt19937_64& rng) const {
    // Select 3-6 pigment compounds representing a plausible floral palette
    auto pigments = compounds_by_role("pigment");
    // Remove chlorophylls and carotenoids (leaf pigments, not floral display)
    std::erase_if(pigments, [](const CompoundProperties& p) {
        return p.family == CompoundFamily::CHLOROPHYLL_A ||
               p.family == CompoundFamily::CHLOROPHYLL_B ||
               p.family == CompoundFamily::CAROTENOID;
    });

    std::shuffle(pigments.begin(), pigments.end(), rng);
    int n = std::min(static_cast<int>(pigments.size()),
                     3 + static_cast<int>(rng() % 4));
    pigments.resize(n);
    return pigments;
}

std::vector<CompoundProperties>
CompoundPropertyEngine::floral_volatile_set(std::mt19937_64& rng) const {
    auto volatiles = compounds_by_role("volatile");
    std::shuffle(volatiles.begin(), volatiles.end(), rng);
    int n = std::min(static_cast<int>(volatiles.size()),
                     3 + static_cast<int>(rng() % 4));
    volatiles.resize(n);
    return volatiles;
}

std::vector<CompoundProperties>
CompoundPropertyEngine::leaf_seasonal_set(Season season, std::mt19937_64& rng) const {
    std::vector<CompoundProperties> result;

    // Season-dependent dominant pigment
    switch (season) {
        case Season::SPRING:
            for (auto& c : compounds_by_family(CompoundFamily::CHLOROPHYLL_B)) result.push_back(c);
            for (auto& c : compounds_by_family(CompoundFamily::CHLOROPHYLL_A)) result.push_back(c);
            break;
        case Season::SUMMER:
            for (auto& c : compounds_by_family(CompoundFamily::CHLOROPHYLL_A)) result.push_back(c);
            for (auto& c : compounds_by_family(CompoundFamily::CHLOROPHYLL_B)) result.push_back(c);
            break;
        case Season::AUTUMN:
            for (auto& c : compounds_by_family(CompoundFamily::CAROTENOID)) result.push_back(c);
            break;
        case Season::WINTER: {
            // Anthocyanin remnants + phenolics
            auto anth = compounds_by_family(CompoundFamily::ANTHOCYANIN);
            if (!anth.empty()) {
                std::shuffle(anth.begin(), anth.end(), rng);
                result.push_back(anth[0]);
            }
            for (auto& c : compounds_by_family(CompoundFamily::PHENOLIC)) result.push_back(c);
            break;
        }
    }

    // Always add structural support
    auto precursors = compounds_by_role("precursor");
    if (!precursors.empty()) result.push_back(precursors[0]);
    auto structural = compounds_by_role("structural");
    for (auto& c : structural) {
        if (c.family == CompoundFamily::SUGAR) { result.push_back(c); break; }
    }

    return result;
}

// ============================================================================
// Bio Experiment Runner
// ============================================================================

std::vector<ExperimentResult> BioExperimentRunner::run_all(const BioCase& bc) const {
    std::vector<ExperimentResult> results;

    // Always run absorption spectrum if pigments present
    bool has_pigments = false;
    bool has_volatiles = false;
    bool has_uv = false;
    for (const auto& c : bc.compounds) {
        if (c.role == "pigment" && c.lambda_max_nm > 0) has_pigments = true;
        if (c.is_volatile) has_volatiles = true;
        if (c.uv_active) has_uv = true;
    }

    if (has_pigments) results.push_back(pigment_absorption_spectrum(bc));
    if (has_volatiles) results.push_back(volatile_release_kinetics(bc));
    if (has_uv) results.push_back(uv_reflectance_pattern(bc));

    // Domain-specific
    if (bc.domain == SystemDomain::DEVELOPMENTAL ||
        bc.domain == SystemDomain::FLORAL_PIGMENT) {
        results.push_back(developmental_staging(bc));
    }

    if (bc.domain == SystemDomain::LEAF_SEASONAL) {
        results.push_back(seasonal_pigment_transition(bc));
    }

    if (bc.domain == SystemDomain::FLORAL_VOLATILE) {
        results.push_back(scent_composition_profile(bc));
    }

    // Always run separation and pathway analysis
    results.push_back(separation_efficiency(bc));
    results.push_back(pathway_constraint_analysis(bc));

    return results;
}

ExperimentResult BioExperimentRunner::pigment_absorption_spectrum(const BioCase& bc) const {
    ExperimentResult r;
    r.type = ExperimentType::STEADY_STATE_CONDUCTION; // reuse enum slot for bio
    r.experiment_name = bio_experiment_name(BioExperimentType::PIGMENT_ABSORPTION_SPECTRUM);

    // Build composite absorption spectrum 200-700nm
    // Beer-Lambert: A(λ) = Σ εi(λ) * ci * l
    // Model each compound as Gaussian around lambda_max
    double path_length_cm = 1.0;
    double max_abs = 0.0;
    double peak_lambda = 0.0;

    for (int wl = 200; wl <= 700; wl += 5) {
        double A_total = 0.0;
        for (size_t i = 0; i < bc.compounds.size(); ++i) {
            const auto& c = bc.compounds[i];
            if (c.lambda_max_nm <= 0 || c.molar_absorptivity <= 0) continue;

            double conc_M = (i < bc.concentrations_mM.size())
                            ? bc.concentrations_mM[i] * 1e-3 : 0.01;

            // Gaussian absorption band: ε(λ) = ε_max * exp(-((λ - λ_max)/σ)^2)
            double sigma = 30.0; // nm half-width
            double dl = static_cast<double>(wl) - c.lambda_max_nm;
            double eps = c.molar_absorptivity * std::exp(-(dl * dl) / (2.0 * sigma * sigma));

            // UV band (if present)
            if (c.uv_active && c.uv_lambda_nm > 0) {
                double dl_uv = static_cast<double>(wl) - c.uv_lambda_nm;
                double sigma_uv = 20.0;
                eps += c.molar_absorptivity * 0.4 *
                       std::exp(-(dl_uv * dl_uv) / (2.0 * sigma_uv * sigma_uv));
            }

            A_total += eps * conc_M * path_length_cm;
        }

        r.series.push_back({static_cast<double>(wl), A_total, ""});

        if (A_total > max_abs) {
            max_abs = A_total;
            peak_lambda = static_cast<double>(wl);
        }
    }

    r.series_x_label = "Wavelength (nm)";
    r.series_y_label = "Absorbance (AU)";

    r.primary_value = peak_lambda;
    r.primary_unit = "nm";
    r.primary_label = "Peak Absorption Wavelength";
    r.secondary_value = max_abs;
    r.secondary_unit = "AU";
    r.secondary_label = "Peak Absorbance";

    r.numerical_stability = 1.0;
    r.physical_plausibility = (max_abs > 0 && max_abs < 50.0) ? 1.0 : 0.6;
    r.notes = std::to_string(bc.compounds.size()) + " compound(s) in mixture";

    return r;
}

ExperimentResult BioExperimentRunner::volatile_release_kinetics(const BioCase& bc) const {
    ExperimentResult r;
    r.type = ExperimentType::TRANSIENT_HEATING; // reuse slot
    r.experiment_name = bio_experiment_name(BioExperimentType::VOLATILE_RELEASE_KINETICS);

    // Model volatile emission over 24 hours
    // V(t) = Σ Vi_max * (1 - exp(-ki * t)) * exp(-kd * t)
    // where ki = formation/release rate, kd = degradation/dissipation rate
    double total_peak_emission = 0.0;
    double peak_time_h = 0.0;
    double max_total = 0.0;

    for (int ti = 0; ti <= 48; ++ti) {
        double t = ti * 0.5; // hours, 0-24
        double V_total = 0.0;

        for (size_t i = 0; i < bc.compounds.size(); ++i) {
            const auto& c = bc.compounds[i];
            if (!c.is_volatile) continue;

            double conc = (i < bc.concentrations_mM.size()) ? bc.concentrations_mM[i] : 0.1;
            double kr = c.formation_rate_k * 24.0; // convert /day to /hour factor
            double kd = c.degradation_rate_k * 24.0;

            // Release rate proportional to vapor pressure
            double vp_factor = std::log10(std::max(c.vapor_pressure_Pa_25C, 0.01) + 1.0);

            double V = conc * vp_factor *
                       (1.0 - std::exp(-kr * t / 24.0)) *
                       std::exp(-kd * t / 48.0);

            // Temperature modulation
            double T_factor = std::exp(0.05 * (bc.organism.temperature_K - 298.15));
            V *= T_factor;

            // Glycoside delay
            if (c.is_glycoside_bound) {
                double release_lag = 2.0; // hours before enzymatic release
                V *= std::max(0.0, 1.0 - std::exp(-(t - release_lag)));
            }

            V_total += std::max(0.0, V);
        }

        r.series.push_back({t, V_total, ""});

        if (V_total > max_total) {
            max_total = V_total;
            peak_time_h = t;
        }
        total_peak_emission += V_total * 0.5; // trapezoidal integration
    }

    r.series_x_label = "Time (hours)";
    r.series_y_label = "Total Volatile Emission (arb.)";

    r.primary_value = peak_time_h;
    r.primary_unit = "hours";
    r.primary_label = "Peak Emission Time";
    r.secondary_value = max_total;
    r.secondary_unit = "arb.";
    r.secondary_label = "Peak Emission Rate";

    r.numerical_stability = 1.0;
    r.physical_plausibility = (max_total > 0) ? 1.0 : 0.5;
    r.notes = "T=" + fmt_d(bc.organism.temperature_K, 1) + "K, " +
              "stage=" + stage_name(bc.organism.stage);

    return r;
}

ExperimentResult BioExperimentRunner::developmental_staging(const BioCase& bc) const {
    ExperimentResult r;
    r.type = ExperimentType::THERMAL_EXPANSION; // reuse slot
    r.experiment_name = bio_experiment_name(BioExperimentType::DEVELOPMENTAL_STAGING);

    // Model compound abundance across 4 developmental stages
    // Each compound has stage-dependent expression modifiers
    static const double stage_modifiers[4][6] = {
        // Bud:          pigment, volatile, precursor, structural, phenolic, glycoside
        {0.15, 0.05, 0.90, 0.80, 0.60, 0.90},
        // Early opening:
        {0.50, 0.30, 0.60, 0.70, 0.50, 0.70},
        // Full bloom:
        {1.00, 1.00, 0.30, 0.50, 0.40, 0.30},
        // Senescence:
        {0.60, 0.40, 0.10, 0.30, 0.80, 0.10},
    };

    auto role_index = [](const std::string& role) -> int {
        if (role == "pigment") return 0;
        if (role == "volatile") return 1;
        if (role == "precursor") return 2;
        if (role == "structural") return 3;
        return 4; // phenolic/other
    };

    // Output: compound abundance at current stage vs full bloom
    double current_total = 0.0;
    double bloom_total = 0.0;
    int stage_idx = static_cast<int>(bc.organism.stage);

    for (size_t i = 0; i < bc.compounds.size(); ++i) {
        const auto& c = bc.compounds[i];
        double conc = (i < bc.concentrations_mM.size()) ? bc.concentrations_mM[i] : 0.1;
        int ri = role_index(c.role);
        if (c.is_glycoside_bound) ri = 5;

        double abundance = conc * stage_modifiers[stage_idx][ri];
        double bloom_abundance = conc * stage_modifiers[2][ri]; // full bloom reference
        current_total += abundance;
        bloom_total += bloom_abundance;
    }

    // Series: all 4 stages, total compound index
    for (int s = 0; s < 4; ++s) {
        double total = 0.0;
        for (size_t i = 0; i < bc.compounds.size(); ++i) {
            const auto& c = bc.compounds[i];
            double conc = (i < bc.concentrations_mM.size()) ? bc.concentrations_mM[i] : 0.1;
            int ri = role_index(c.role);
            if (c.is_glycoside_bound) ri = 5;
            total += conc * stage_modifiers[s][ri];
        }
        r.series.push_back({static_cast<double>(s), total,
                            stage_name(static_cast<DevelopmentalStage>(s))});
    }

    r.series_x_label = "Developmental Stage";
    r.series_y_label = "Total Chemical Index (mM-equiv)";

    double ratio = (bloom_total > 0) ? current_total / bloom_total : 0.0;
    r.primary_value = ratio;
    r.primary_unit = "";
    r.primary_label = "Stage/Bloom Abundance Ratio";
    r.secondary_value = current_total;
    r.secondary_unit = "mM-equiv";
    r.secondary_label = "Current Stage Total Index";

    r.numerical_stability = 1.0;
    r.physical_plausibility = 1.0;
    r.notes = "Current stage: " + std::string(stage_name(bc.organism.stage));

    return r;
}

ExperimentResult BioExperimentRunner::uv_reflectance_pattern(const BioCase& bc) const {
    ExperimentResult r;
    r.type = ExperimentType::SENSITIVITY_SWEEP; // reuse slot
    r.experiment_name = bio_experiment_name(BioExperimentType::UV_REFLECTANCE_PATTERN);

    // Model UV absorption vs reflectance across petal surface
    // Radial position 0 (centre) to 1 (tip)
    // Flavonoids/phenolics concentrated at centre → UV-absorbing bullseye
    double total_uv_contrast = 0.0;

    for (int pi = 0; pi <= 20; ++pi) {
        double pos = pi / 20.0; // 0 = centre, 1 = tip

        // Centre-heavy UV absorber distribution (Gaussian, centre at 0.2)
        double absorber_density = std::exp(-((pos - 0.2) * (pos - 0.2)) / 0.08);

        double A_uv = 0.0;
        for (size_t i = 0; i < bc.compounds.size(); ++i) {
            const auto& c = bc.compounds[i];
            if (!c.uv_active) continue;
            double conc = (i < bc.concentrations_mM.size()) ? bc.concentrations_mM[i] : 0.05;
            A_uv += c.molar_absorptivity * 0.4 * conc * 1e-3 * absorber_density;
        }

        // Reflectance = 1 - absorptance (simplified)
        double R_uv = std::max(0.0, 1.0 - A_uv * 0.001);

        r.series.push_back({pos, R_uv, ""});
        total_uv_contrast += std::abs(R_uv - 0.5);
    }

    r.series_x_label = "Radial Position (0=centre, 1=tip)";
    r.series_y_label = "UV Reflectance (0-1)";

    r.primary_value = total_uv_contrast / 21.0;
    r.primary_unit = "";
    r.primary_label = "Mean UV Contrast Index";
    r.secondary_value = r.series.empty() ? 0.0 :
                        r.series[0].y - r.series.back().y;
    r.secondary_unit = "";
    r.secondary_label = "Centre-to-Tip Reflectance Difference";

    r.numerical_stability = 1.0;
    r.physical_plausibility = 1.0;
    r.notes = "Simplified radial UV model, 21-point profile";

    return r;
}

ExperimentResult BioExperimentRunner::separation_efficiency(const BioCase& bc) const {
    ExperimentResult r;
    r.type = ExperimentType::PHASE_CHANGE_APPROX; // reuse slot
    r.experiment_name = bio_experiment_name(BioExperimentType::SEPARATION_EFFICIENCY);

    // Model extraction: polar (aqueous) vs nonpolar (organic) partition
    // Compounds partition by logP: positive → organic, negative → aqueous
    double aqueous_mass = 0.0, organic_mass = 0.0, lost_mass = 0.0;
    double total_mass = 0.0;

    for (size_t i = 0; i < bc.compounds.size(); ++i) {
        const auto& c = bc.compounds[i];
        double conc = (i < bc.concentrations_mM.size()) ? bc.concentrations_mM[i] : 0.1;
        double mass = conc * c.molar_mass_g_mol * 1e-3; // mg in 1L

        // Partition coefficient from logP
        double Kow = std::pow(10.0, c.log_P);
        double f_organic = Kow / (1.0 + Kow);
        double f_aqueous = 1.0 / (1.0 + Kow);

        // Degradation loss during extraction (proportional to degradation rate)
        double f_loss = 1.0 - std::exp(-c.degradation_rate_k * 0.5); // 0.5 day extraction

        double recovered = mass * (1.0 - f_loss);
        aqueous_mass += recovered * f_aqueous;
        organic_mass += recovered * f_organic;
        lost_mass += mass * f_loss;
        total_mass += mass;

        r.series.push_back({c.log_P, recovered / std::max(mass, 1e-12), c.name});
    }

    r.series_x_label = "logP";
    r.series_y_label = "Recovery Fraction";

    double recovery = (total_mass > 0) ? (aqueous_mass + organic_mass) / total_mass : 0.0;
    r.primary_value = recovery * 100.0;
    r.primary_unit = "%";
    r.primary_label = "Total Recovery";
    r.secondary_value = (total_mass > 0) ? organic_mass / total_mass * 100.0 : 0.0;
    r.secondary_unit = "%";
    r.secondary_label = "Organic Phase Fraction";

    r.numerical_stability = 1.0;
    r.physical_plausibility = (recovery > 0.1 && recovery <= 1.0) ? 1.0 : 0.6;
    r.notes = fmt_d(total_mass, 2) + " mg total, " +
              fmt_d(lost_mass / std::max(total_mass, 1e-12) * 100.0, 1) + "% degradation loss";

    return r;
}

ExperimentResult BioExperimentRunner::seasonal_pigment_transition(const BioCase& bc) const {
    ExperimentResult r;
    r.type = ExperimentType::TRANSIENT_COOLING; // reuse slot
    r.experiment_name = bio_experiment_name(BioExperimentType::SEASONAL_PIGMENT_TRANSITION);

    // Model pigment pool dynamics over 365 days
    // Four pools: Chl_b (spring), Chl_a (summer), Carotenoid (autumn), Anthocyanin/phenolic (winter)
    // Each follows logistic rise and exponential decay tied to day-of-year

    for (int day = 0; day <= 365; day += 5) {
        double d = static_cast<double>(day);

        // Chlorophyll b: peaks ~day 90 (spring equinox)
        double chl_b = 0.8 * std::exp(-std::pow((d - 90.0) / 50.0, 2));

        // Chlorophyll a: peaks ~day 180 (summer solstice)
        double chl_a = 1.0 * std::exp(-std::pow((d - 180.0) / 70.0, 2));

        // Carotenoid: peaks ~day 270 (autumn equinox)
        double carot = 0.6 * std::exp(-std::pow((d - 270.0) / 40.0, 2));

        // Anthocyanin/phenolic: peaks ~day 300, persists into winter
        double anth = 0.4 * std::exp(-std::pow((d - 310.0) / 35.0, 2));

        double total = chl_b + chl_a + carot + anth;
        r.series.push_back({d, total, ""});
    }

    r.series_x_label = "Day of Year";
    r.series_y_label = "Total Pigment Index (arb.)";

    // Current season's dominant pigment amplitude
    double season_peak = 0.0;
    switch (bc.organism.season) {
        case Season::SPRING: season_peak = 0.8; break;
        case Season::SUMMER: season_peak = 1.0; break;
        case Season::AUTUMN: season_peak = 0.6; break;
        case Season::WINTER: season_peak = 0.4; break;
    }

    r.primary_value = season_peak;
    r.primary_unit = "arb.";
    r.primary_label = "Dominant Pigment Amplitude";
    r.secondary_value = static_cast<double>(bc.organism.season);
    r.secondary_unit = "";
    r.secondary_label = "Current Season Index";

    r.numerical_stability = 1.0;
    r.physical_plausibility = 1.0;
    r.notes = "Season: " + std::string(season_name(bc.organism.season)) +
              ", 365-day Gaussian pigment model";

    return r;
}

ExperimentResult BioExperimentRunner::scent_composition_profile(const BioCase& bc) const {
    ExperimentResult r;
    r.type = ExperimentType::DIFFUSION_VARIATION; // reuse slot
    r.experiment_name = bio_experiment_name(BioExperimentType::SCENT_COMPOSITION_PROFILE);

    // Bar chart: volatile contribution by family at current conditions
    // Contribution proportional to concentration × vapor pressure
    double total_scent = 0.0;

    struct FamilyContrib {
        std::string name;
        double value;
    };
    std::vector<FamilyContrib> contribs;

    // Aggregate by family
    std::map<std::string, double> family_totals;

    for (size_t i = 0; i < bc.compounds.size(); ++i) {
        const auto& c = bc.compounds[i];
        if (!c.is_volatile) continue;
        double conc = (i < bc.concentrations_mM.size()) ? bc.concentrations_mM[i] : 0.1;
        double emission = conc * std::log10(std::max(c.vapor_pressure_Pa_25C, 0.01) + 1.0);

        // Temperature correction
        double T_corr = std::exp(0.05 * (bc.organism.temperature_K - 298.15));
        emission *= T_corr;

        std::string fam = family_name(c.family);
        family_totals[fam] += emission;
        total_scent += emission;
    }

    int idx = 0;
    for (const auto& [fam, val] : family_totals) {
        double pct = (total_scent > 0) ? val / total_scent * 100.0 : 0.0;
        r.series.push_back({static_cast<double>(idx), pct, fam});
        idx++;
    }

    r.series_x_label = "Volatile Family";
    r.series_y_label = "Composition (%)";

    r.primary_value = total_scent;
    r.primary_unit = "arb.";
    r.primary_label = "Total Scent Index";
    r.secondary_value = static_cast<double>(family_totals.size());
    r.secondary_unit = "";
    r.secondary_label = "Number of Volatile Families";

    r.numerical_stability = 1.0;
    r.physical_plausibility = (total_scent > 0) ? 1.0 : 0.3;
    r.notes = std::to_string(family_totals.size()) + " volatile families detected";

    return r;
}

ExperimentResult BioExperimentRunner::pathway_constraint_analysis(const BioCase& bc) const {
    ExperimentResult r;
    r.type = ExperimentType::CRACK_INITIATION_PROXY; // reuse slot
    r.experiment_name = bio_experiment_name(BioExperimentType::PATHWAY_CONSTRAINT_ANALYSIS);

    // Identify pathway bottlenecks: low formation_rate or missing precursors
    // Score each compound's biosynthetic accessibility
    double total_accessibility = 0.0;
    int constrained_count = 0;
    std::string most_constrained;
    double min_accessibility = 1e10;

    for (size_t i = 0; i < bc.compounds.size(); ++i) {
        const auto& c = bc.compounds[i];

        // Accessibility = formation_rate / (formation_rate + degradation_rate)
        double accessibility = 0.5;
        if (c.formation_rate_k + c.degradation_rate_k > 0) {
            accessibility = c.formation_rate_k /
                           (c.formation_rate_k + c.degradation_rate_k);
        }

        // Penalty for glycoside-bound (requires hydrolysis)
        if (c.is_glycoside_bound) {
            accessibility *= 0.5;
        }

        r.series.push_back({static_cast<double>(i), accessibility, c.name});
        total_accessibility += accessibility;

        if (accessibility < 0.4) constrained_count++;
        if (accessibility < min_accessibility) {
            min_accessibility = accessibility;
            most_constrained = c.name;
        }
    }

    r.series_x_label = "Compound Index";
    r.series_y_label = "Biosynthetic Accessibility (0-1)";

    int n = std::max(1, static_cast<int>(bc.compounds.size()));
    r.primary_value = total_accessibility / n;
    r.primary_unit = "";
    r.primary_label = "Mean Pathway Accessibility";
    r.secondary_value = static_cast<double>(constrained_count);
    r.secondary_unit = "";
    r.secondary_label = "Constrained Compounds";

    r.numerical_stability = 1.0;
    r.physical_plausibility = 1.0;
    r.notes = "Most constrained: " + most_constrained +
              " (accessibility=" + fmt_d(min_accessibility, 3) + ")";

    // Check for F3'5'H-like constraint (delphinidin in non-delphinidin systems)
    bool has_delphinidin = false;
    for (const auto& c : bc.compounds) {
        if (c.name.find("Delphinidin") != std::string::npos) has_delphinidin = true;
    }
    if (!has_delphinidin) {
        r.notes += "; F3'5'H pathway absent — blue pigmentation not accessible";
    }

    return r;
}

// ============================================================================
// Bio Case Generator
// ============================================================================

BioCaseGenerator::BioCaseGenerator(uint64_t base_seed)
    : rng_(base_seed), prop_engine_() {}

std::string BioCaseGenerator::generate_case_name(const BioCase& bc) {
    std::string base = bc.organism.common_name;
    if (!base.empty()) base += "-";
    base += domain_name(bc.domain);
    base += "-" + std::to_string(bc.case_id);
    return base;
}

BioCase BioCaseGenerator::generate_next() {
    // Rotate through domains
    static const SystemDomain domains[] = {
        SystemDomain::FLORAL_PIGMENT,
        SystemDomain::FLORAL_VOLATILE,
        SystemDomain::LEAF_SEASONAL,
        SystemDomain::DEVELOPMENTAL,
        SystemDomain::LEAF_STRUCTURE,
    };
    SystemDomain d = domains[cases_generated_ % 5];
    return generate_for_domain(d);
}

BioCase BioCaseGenerator::generate_for_domain(SystemDomain domain) {
    BioCase bc;
    bc.domain = domain;

    switch (domain) {
        case SystemDomain::FLORAL_PIGMENT:  bc = generate_floral_pigment(); break;
        case SystemDomain::FLORAL_VOLATILE: bc = generate_floral_volatile(); break;
        case SystemDomain::LEAF_SEASONAL:   bc = generate_leaf_seasonal(); break;
        case SystemDomain::LEAF_STRUCTURE:  bc = generate_leaf_structure(); break;
        case SystemDomain::DEVELOPMENTAL:   bc = generate_developmental(); break;
    }

    bc.case_id = static_cast<uint64_t>(cases_generated_);
    bc.seed = rng_();
    bc.domain = domain;
    bc.case_name = generate_case_name(bc);
    cases_generated_++;
    return bc;
}

BioCase BioCaseGenerator::generate_floral_pigment() {
    BioCase bc;
    bc.description = "Floral pigment palette — colour chemistry";
    bc.level = ComplexityLevel::L2_BINARY;

    static const char* flower_names[] = {
        "Dahlia", "Rosa damascena", "Tulipa gesneriana", "Petunia hybrida",
        "Chrysanthemum", "Gerbera jamesonii", "Zinnia elegans", "Cosmos bipinnatus",
        "Dianthus caryophyllus", "Helianthus annuus", "Rudbeckia hirta",
        "Delphinium consolida", "Iris germanica", "Papaver rhoeas",
    };
    int fi = static_cast<int>(rng_() % 14);
    bc.organism.common_name = flower_names[fi];
    bc.organism.tissue_type = "petal";

    int si = static_cast<int>(rng_() % 4);
    bc.organism.stage = static_cast<DevelopmentalStage>(si);

    std::uniform_real_distribution<double> temp_dist(288.0, 308.0);
    bc.organism.temperature_K = temp_dist(rng_);

    bc.compounds = prop_engine_.floral_pigment_set(rng_);

    // Assign concentrations
    std::uniform_real_distribution<double> conc_dist(0.01, 2.0);
    for (size_t i = 0; i < bc.compounds.size(); ++i) {
        bc.concentrations_mM.push_back(conc_dist(rng_));
    }

    bc.rarity_score = 0.1;
    bc.complexity_index = 0.3;
    return bc;
}

BioCase BioCaseGenerator::generate_floral_volatile() {
    BioCase bc;
    bc.description = "Floral volatile emission — scent chemistry";
    bc.level = ComplexityLevel::L2_BINARY;

    static const char* flower_names[] = {
        "Rosa damascena", "Jasminum grandiflorum", "Gardenia jasminoides",
        "Lavandula angustifolia", "Plumeria rubra", "Osmanthus fragrans",
        "Stephanotis floribunda", "Citrus sinensis", "Michelia champaca",
    };
    int fi = static_cast<int>(rng_() % 9);
    bc.organism.common_name = flower_names[fi];
    bc.organism.tissue_type = "petal";

    int si = static_cast<int>(rng_() % 4);
    bc.organism.stage = static_cast<DevelopmentalStage>(si);

    std::uniform_real_distribution<double> temp_dist(290.0, 310.0);
    bc.organism.temperature_K = temp_dist(rng_);

    bc.compounds = prop_engine_.floral_volatile_set(rng_);

    // Add a glycoside-bound precursor sometimes
    if (rng_() % 3 == 0) {
        auto gly = prop_engine_.compound_properties("Geranyl glucoside");
        if (!gly.name.empty()) bc.compounds.push_back(gly);
    }

    std::uniform_real_distribution<double> conc_dist(0.05, 1.5);
    for (size_t i = 0; i < bc.compounds.size(); ++i) {
        bc.concentrations_mM.push_back(conc_dist(rng_));
    }

    bc.rarity_score = 0.15;
    bc.complexity_index = 0.4;
    return bc;
}

BioCase BioCaseGenerator::generate_leaf_seasonal() {
    BioCase bc;
    bc.level = ComplexityLevel::L3_ANISOTROPIC;

    int si = static_cast<int>(rng_() % 4);
    Season season = static_cast<Season>(si);
    bc.organism.season = season;
    bc.description = std::string("Leaf pigment isolation — ") + season_name(season);

    static const char* tree_names[] = {
        "Acer saccharum", "Quercus rubra", "Betula pendula",
        "Fagus sylvatica", "Liquidambar styraciflua", "Ginkgo biloba",
        "Populus tremuloides", "Cornus florida", "Liriodendron tulipifera",
    };
    int ti = static_cast<int>(rng_() % 9);
    bc.organism.common_name = tree_names[ti];
    bc.organism.tissue_type = "leaf";

    // Season-dependent temperature
    switch (season) {
        case Season::SPRING: bc.organism.temperature_K = 285.0 + (rng_() % 10); break;
        case Season::SUMMER: bc.organism.temperature_K = 298.0 + (rng_() % 10); break;
        case Season::AUTUMN: bc.organism.temperature_K = 278.0 + (rng_() % 10); break;
        case Season::WINTER: bc.organism.temperature_K = 265.0 + (rng_() % 10); break;
    }

    // Season-dependent stage
    switch (season) {
        case Season::SPRING: bc.organism.stage = DevelopmentalStage::BUD; break;
        case Season::SUMMER: bc.organism.stage = DevelopmentalStage::FULL_BLOOM; break;
        case Season::AUTUMN: bc.organism.stage = DevelopmentalStage::SENESCENCE; break;
        case Season::WINTER: bc.organism.stage = DevelopmentalStage::SENESCENCE; break;
    }

    bc.compounds = prop_engine_.leaf_seasonal_set(season, rng_);

    std::uniform_real_distribution<double> conc_dist(0.1, 5.0);
    for (size_t i = 0; i < bc.compounds.size(); ++i) {
        bc.concentrations_mM.push_back(conc_dist(rng_));
    }

    bc.rarity_score = 0.05;
    bc.complexity_index = 0.5;
    return bc;
}

BioCase BioCaseGenerator::generate_leaf_structure() {
    BioCase bc;
    bc.description = "Leaf microstructure — transport and extraction targets";
    bc.level = ComplexityLevel::L3_ANISOTROPIC;

    static const char* species[] = {
        "Arabidopsis thaliana", "Nicotiana tabacum", "Zea mays",
        "Oryza sativa", "Eucalyptus globulus", "Mentha piperita",
    };
    int si = static_cast<int>(rng_() % 6);
    bc.organism.common_name = species[si];
    bc.organism.tissue_type = "leaf";

    // Mix of structural + volatile compounds
    auto structural = prop_engine_.compounds_by_role("structural");
    auto volatiles = prop_engine_.floral_volatile_set(rng_);

    for (auto& c : structural) bc.compounds.push_back(c);
    if (!volatiles.empty()) bc.compounds.push_back(volatiles[0]);

    std::uniform_real_distribution<double> conc_dist(0.5, 10.0);
    for (size_t i = 0; i < bc.compounds.size(); ++i) {
        bc.concentrations_mM.push_back(conc_dist(rng_));
    }

    bc.rarity_score = 0.2;
    bc.complexity_index = 0.6;
    return bc;
}

BioCase BioCaseGenerator::generate_developmental() {
    BioCase bc;
    bc.level = ComplexityLevel::L4_TRANSIENT;

    int si = static_cast<int>(rng_() % 4);
    bc.organism.stage = static_cast<DevelopmentalStage>(si);
    bc.description = std::string("Developmental staging — ") +
                     stage_name(bc.organism.stage);

    static const char* flowers[] = {
        "Rosa centifolia", "Paeonia lactiflora", "Nelumbo nucifera",
        "Camellia sinensis", "Magnolia grandiflora",
    };
    int fi = static_cast<int>(rng_() % 5);
    bc.organism.common_name = flowers[fi];
    bc.organism.tissue_type = "petal";

    std::uniform_real_distribution<double> temp_dist(290.0, 305.0);
    bc.organism.temperature_K = temp_dist(rng_);

    // Full chemical inventory: pigments + volatiles + precursors
    auto pigments = prop_engine_.floral_pigment_set(rng_);
    auto volatiles = prop_engine_.floral_volatile_set(rng_);
    auto precursors = prop_engine_.compounds_by_role("precursor");

    for (auto& c : pigments) bc.compounds.push_back(c);
    for (auto& c : volatiles) bc.compounds.push_back(c);
    for (auto& c : precursors) bc.compounds.push_back(c);

    std::uniform_real_distribution<double> conc_dist(0.05, 3.0);
    for (size_t i = 0; i < bc.compounds.size(); ++i) {
        bc.concentrations_mM.push_back(conc_dist(rng_));
    }

    bc.rarity_score = 0.3;
    bc.complexity_index = 0.7;
    return bc;
}

// ============================================================================
// Bio Report Writer
// ============================================================================

std::string BioReportWriter::organism_table_md(const OrganismSpec& org) {
    std::ostringstream t;
    t << "| Parameter | Value |\n";
    t << "|-----------|-------|\n";
    t << "| Species | " << org.common_name << " |\n";
    t << "| Tissue | " << org.tissue_type << " |\n";
    t << "| Developmental Stage | " << stage_name(org.stage) << " |\n";
    t << "| Season | " << season_name(org.season) << " |\n";
    t << "| Temperature | " << fmt_d(org.temperature_K, 1) << " K |\n";
    t << "| Light (PAR) | " << fmt_d(org.light_intensity_umol, 0) << " umol/m2/s |\n";
    t << "| Relative Humidity | " << fmt_d(org.relative_humidity * 100.0, 0) << "% |\n";
    return t.str();
}

std::string BioReportWriter::compound_table_md(
    const std::vector<CompoundProperties>& compounds,
    const std::vector<double>& concentrations) {
    std::ostringstream t;
    t << "| # | Name | Formula | Family | Role | M (g/mol) | logP | "
      << "lambda_max (nm) | Conc (mM) | Confidence |\n";
    t << "|---|------|---------|--------|------|-----------|------|"
      << "-----------------|-----------|------------|\n";
    for (size_t i = 0; i < compounds.size(); ++i) {
        const auto& c = compounds[i];
        double conc = (i < concentrations.size()) ? concentrations[i] : 0.0;
        t << "| " << i + 1
          << " | " << c.name
          << " | " << c.formula
          << " | " << family_name(c.family)
          << " | " << c.role
          << " | " << fmt_d(c.molar_mass_g_mol, 1)
          << " | " << fmt_d(c.log_P, 2)
          << " | " << (c.lambda_max_nm > 0 ? fmt_d(c.lambda_max_nm, 0) : "---")
          << " | " << fmt_d(conc, 3)
          << " | " << fmt_d(c.confidence_score, 2)
          << " |\n";
    }
    return t.str();
}

std::string BioReportWriter::to_markdown(const TechnicalReport& report, const BioCase& bc) {
    std::ostringstream md;

    md << "# " << report.title << "\n\n";
    md << "**Report ID:** " << report.report_id << "  \n";
    md << "**Timestamp:** " << report.timestamp << "  \n";
    md << "**Domain:** " << domain_name(bc.domain) << "  \n";
    md << "**Complexity:** " << complexity_name(report.current_level) << "  \n";
    md << "**Report #** " << report.total_reports_so_far << "  \n\n";

    md << "## Abstract\n\n" << report.abstract_text << "\n\n";

    // Organism context
    md << "## Organism and Conditions\n\n";
    md << organism_table_md(bc.organism) << "\n";

    // Chemical inventory
    md << "## Chemical Inventory\n\n";
    md << "**Compounds:** " << bc.compounds.size() << "  \n";
    md << "**Case:** " << bc.case_name << "  \n";
    md << "**Description:** " << bc.description << "  \n\n";
    md << compound_table_md(bc.compounds, bc.concentrations_mM) << "\n";

    // Experiment results summary table
    md << "## Experiment Results\n\n";
    md << ReportWriter::results_table(report.experiments) << "\n";

    // Detailed experiment sections
    for (const auto& exp : report.experiments) {
        md << "### " << exp.experiment_name << "\n\n";
        md << "**" << exp.primary_label << ":** " << fmt_s(exp.primary_value)
           << " " << exp.primary_unit << "  \n";
        md << "**" << exp.secondary_label << ":** " << fmt_s(exp.secondary_value)
           << " " << exp.secondary_unit << "  \n";
        md << "**Numerical Stability:** " << fmt_d(exp.numerical_stability, 2) << "  \n";
        md << "**Physical Plausibility:** " << fmt_d(exp.physical_plausibility, 2) << "  \n";
        if (!exp.notes.empty()) {
            md << "**Notes:** " << exp.notes << "  \n";
        }

        if (!exp.series.empty()) {
            md << "\n| " << exp.series_x_label << " | " << exp.series_y_label << " |";
            // Add label column if any series point has a non-empty label
            bool has_labels = false;
            for (const auto& p : exp.series) {
                if (!p.label.empty()) { has_labels = true; break; }
            }
            if (has_labels) md << " Label |";
            md << "\n|---|---|";
            if (has_labels) md << "---|";
            md << "\n";

            int limit = std::min(static_cast<int>(exp.series.size()), 15);
            for (int i = 0; i < limit; ++i) {
                md << "| " << fmt_d(exp.series[i].x, 4)
                   << " | " << fmt_s(exp.series[i].y);
                if (has_labels) md << " | " << exp.series[i].label;
                md << " |\n";
            }
            if (static_cast<int>(exp.series.size()) > 15) {
                md << "| ... | ... |";
                if (has_labels) md << " ... |";
                md << "\n";
            }
        }
        md << "\n";
    }

    // Analysis
    md << "## Analysis\n\n";
    md << "| Metric | Value |\n";
    md << "|--------|-------|\n";
    md << "| Overall Stability | " << fmt_d(report.overall_stability_score, 3) << " |\n";
    md << "| Novelty Score | " << fmt_d(report.novelty_score, 3) << " |\n";
    md << "| Thermal Response Index | " << fmt_d(report.thermal_response_index, 3) << " |\n";
    md << "| Deformation Score | " << fmt_d(report.deformation_score, 3) << " |\n\n";

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
            md << "- Warning: " << w << "\n";
        }
        md << "\n";
    }

    md << "## Conclusion\n\n" << report.conclusion << "\n\n";

    md << "---\n*Generated by VSEPR-SIM Bio Report Engine v4.0.0*  \n";
    md << "*Work Order: WO-BIO-CRG-003*\n";

    return md.str();
}

// ============================================================================
// Bio Autonomous Engine
// ============================================================================

BioAutonomousEngine::BioAutonomousEngine(const BioEngineConfig& config)
    : config_(config), case_gen_(config.base_seed) {}

TechnicalReport BioAutonomousEngine::build_report(BioCase& bc) {
    TechnicalReport report;
    report.report_id = static_cast<uint64_t>(reports_generated_);
    report.timestamp = ts_now();
    report.total_reports_so_far = reports_generated_ + 1;
    report.current_level = bc.level;

    // Run experiments
    report.experiments = experiment_runner_.run_all(bc);

    // Build title
    report.title = "BIO-" + std::to_string(report.report_id) + ": " +
                   bc.case_name + " [" + domain_name(bc.domain) + "]";

    return report;
}

void BioAutonomousEngine::analyze_report(TechnicalReport& report, const BioCase& bc) {
    // Stability
    double stab_sum = 0, plaus_sum = 0;
    for (const auto& e : report.experiments) {
        stab_sum += e.numerical_stability;
        plaus_sum += e.physical_plausibility;
    }
    int ne = std::max(1, static_cast<int>(report.experiments.size()));
    report.overall_stability_score = stab_sum / ne;
    report.novelty_score = bc.rarity_score;

    // Bio-specific indices
    report.thermal_response_index = bc.complexity_index;
    report.deformation_score = 0.0;

    // Separation-derived deformation score
    for (const auto& e : report.experiments) {
        if (e.experiment_name == bio_experiment_name(BioExperimentType::SEPARATION_EFFICIENCY)) {
            report.deformation_score = 100.0 - e.primary_value; // % loss
        }
    }

    // Findings
    if (bc.compounds.size() > 6) {
        report.findings.push_back("Complex mixture: " +
            std::to_string(bc.compounds.size()) + " compounds in inventory");
    }

    bool has_uv = false;
    for (const auto& c : bc.compounds) {
        if (c.uv_active) { has_uv = true; break; }
    }
    if (has_uv) {
        report.findings.push_back("UV-active compounds present — hidden optical signaling likely");
    }

    for (const auto& e : report.experiments) {
        if (e.experiment_name == bio_experiment_name(BioExperimentType::PATHWAY_CONSTRAINT_ANALYSIS)) {
            if (e.notes.find("F3'5'H") != std::string::npos) {
                report.findings.push_back("Delphinidin pathway absent — blue pigmentation not accessible");
            }
            if (e.secondary_value > 0) {
                report.findings.push_back(fmt_d(e.secondary_value, 0) +
                    " compound(s) show constrained biosynthetic accessibility");
            }
        }
    }

    if (bc.domain == SystemDomain::LEAF_SEASONAL) {
        report.findings.push_back("Seasonal regime: " +
            std::string(season_name(bc.organism.season)));
    }

    if (report.deformation_score > 20.0) {
        report.warnings.push_back("High separation loss (" +
            fmt_d(report.deformation_score, 1) + "% unrecovered)");
    }

    if (plaus_sum / ne < 0.7) {
        report.warnings.push_back("Low physical plausibility in one or more experiments");
    }

    // Abstract
    {
        std::ostringstream abs;
        abs << "This report presents a " << domain_name(bc.domain)
            << " analysis of " << bc.organism.common_name
            << " (" << bc.organism.tissue_type << "). "
            << bc.compounds.size() << " compound(s) were inventoried across "
            << report.experiments.size() << " experiment(s). "
            << "Developmental stage: " << stage_name(bc.organism.stage) << ". "
            << "Season: " << season_name(bc.organism.season) << ". "
            << "Overall stability: " << fmt_d(report.overall_stability_score, 2) << ". "
            << "Warnings: " << report.warnings.size() << ".";
        report.abstract_text = abs.str();
    }

    // Conclusion
    {
        std::ostringstream conc;
        if (report.warnings.empty()) {
            conc << "The " << bc.organism.common_name << " "
                 << bc.organism.tissue_type << " system at "
                 << stage_name(bc.organism.stage) << " stage ("
                 << season_name(bc.organism.season)
                 << ") shows a well-resolved chemical profile across "
                 << report.experiments.size() << " experiments. "
                 << "All analyses converged with acceptable stability ("
                 << fmt_d(report.overall_stability_score, 2) << ").";
        } else {
            conc << "The " << bc.organism.common_name << " system exhibits "
                 << report.warnings.size() << " warning(s). ";
            if (report.deformation_score > 20.0) {
                conc << "Significant separation loss observed. ";
            }
            conc << "Further investigation is recommended for isolation and "
                 << "characterisation workflows.";
        }
        report.conclusion = conc.str();
    }
}

TechnicalReport BioAutonomousEngine::generate_one() {
    auto bc = case_gen_.generate_for_domain(config_.domain);
    auto report = build_report(bc);
    analyze_report(report, bc);

    if (config_.write_individual) write_report(report, bc);
    if (config_.write_csv_log) write_csv_entry(report);

    reports_generated_++;
    return report;
}

std::string BioAutonomousEngine::make_output_path(const TechnicalReport& report,
                                                   const std::string& ext) {
    std::ostringstream path;
    path << config_.output_dir << "/BIO-"
         << std::setfill('0') << std::setw(6) << report.report_id
         << ext;
    return path.str();
}

void BioAutonomousEngine::write_report(const TechnicalReport& report, const BioCase& bc) {
    std::string path = make_output_path(report, ".md");
    std::ofstream out(path);
    if (out) {
        out << BioReportWriter::to_markdown(report, bc);
    }
}

void BioAutonomousEngine::write_csv_entry(const TechnicalReport& report) {
    std::string path = config_.output_dir + "/bio_summary.csv";
    bool exists = std::filesystem::exists(path);
    std::ofstream out(path, std::ios::app);
    if (out) {
        if (!exists) {
            out << ReportWriter::csv_header() << "\n";
        }
        out << ReportWriter::to_csv_line(report) << "\n";
    }
}

void BioAutonomousEngine::print_progress(const TechnicalReport& report) {
    std::cout << "[" << std::setw(6) << report.report_id << "] "
              << std::left << std::setw(20) << report.title.substr(0, 20)
              << " | exp=" << report.experiments.size()
              << " stab=" << fmt_d(report.overall_stability_score, 2)
              << " warn=" << report.warnings.size()
              << "\n";
}

int BioAutonomousEngine::run() {
    std::filesystem::create_directories(config_.output_dir);

    if (config_.print_progress) {
        std::cout << "================================================================\n";
        std::cout << "  VSEPR-SIM Bio Report Engine v4.0.0\n";
        std::cout << "  Work Order: WO-BIO-CRG-003\n";
        std::cout << "  Domain:     " << domain_name(config_.domain) << "\n";
        std::cout << "  Target:     " << config_.target_reports << " reports\n";
        std::cout << "  Seed:       " << config_.base_seed << "\n";
        std::cout << "  Output:     " << config_.output_dir << "/\n";
        std::cout << "================================================================\n\n";
    }

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < config_.target_reports; ++i) {
        auto report = generate_one();
        if (config_.print_progress &&
            (i % config_.progress_interval == 0 || i == config_.target_reports - 1)) {
            print_progress(report);
        }
    }

    auto end = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    if (config_.print_progress) {
        std::cout << "\n================================================================\n";
        std::cout << "  Completed: " << reports_generated_ << " reports\n";
        std::cout << "  Time:      " << fmt_d(elapsed_ms, 1) << " ms\n";
        std::cout << "  Rate:      " << fmt_d(reports_generated_ / (elapsed_ms / 1000.0), 1) << " reports/s\n";
        std::cout << "  Output:    " << config_.output_dir << "/\n";
        std::cout << "================================================================\n";
    }

    return 0;
}

} // namespace bio
} // namespace report
} // namespace vsepr
