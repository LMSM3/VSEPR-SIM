/**
 * bio_report_engine.hpp
 * ---------------------
 * Organic / biochemical report-generation engine for VSEPR-SIM.
 * Work Order: WO-BIO-CRG-003
 *
 * Extends the core report engine (WO-TMS-CRG-001) to handle macro organic
 * chemistry: floral pigments, leaf metabolites, volatile terpenoids,
 * UV-active flavonoids, developmental staging, and separation efficiency.
 *
 * Architecture:
 *   CompoundPropertyEngine  → curated tables of bio-organic molecules
 *   BioCaseGenerator        → floral / leaf / seasonal case assembly
 *   BioExperimentRunner     → pigment absorption, volatile kinetics, UV response, ...
 *   ReportWriter            → reuses core Markdown/CSV (domain-aware)
 *   BioAutonomousEngine     → orchestrator (mirrors AutonomousEngine for TMS)
 *
 * Reuses from core report_engine.hpp:
 *   ExperimentResult, DataPoint, TechnicalReport, ReportWriter, EngineConfig
 *
 * Anti-black-box: all compound records, kinetic parameters, and absorption
 * coefficients are explicit, inspectable, and deterministic.
 */

#pragma once

#include "core/report_engine.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <random>

namespace vsepr {
namespace report {
namespace bio {

// ============================================================================
// System Domain (selects TMS vs BIO pipelines)
// ============================================================================

enum class SystemDomain : int {
    FLORAL_PIGMENT   = 1,  // Flower colour and pigment chemistry
    FLORAL_VOLATILE  = 2,  // Scent and volatile release
    LEAF_SEASONAL    = 3,  // Seasonal pigment isolation (Chl a/b, carotenoid, anthocyanin)
    LEAF_STRUCTURE   = 4,  // Leaf microstructure transport/extraction
    DEVELOPMENTAL    = 5,  // Bud → bloom → senescence staging
};

const char* domain_name(SystemDomain d);

// ============================================================================
// Bio-Organic Compound Properties
// ============================================================================

enum class CompoundFamily : int {
    ANTHOCYANIN,
    CHALCONE,
    AURONE,
    FLAVONOID,
    CHLOROPHYLL_A,
    CHLOROPHYLL_B,
    CAROTENOID,
    TERPENOID,
    BENZENOID,
    PHENYLPROPANOID,
    ALDEHYDE,
    AMINO_ACID,
    FATTY_ACID,
    SUGAR,
    GLYCOSIDE_VOLATILE,
    PHENOLIC,
};

const char* family_name(CompoundFamily f);

struct CompoundProperties {
    // Identity
    std::string name;
    std::string formula;
    CompoundFamily family            = CompoundFamily::FLAVONOID;
    std::string role;                // "pigment", "volatile", "precursor", "structural"

    // Physical
    double molar_mass_g_mol          = 0.0;
    double density_kg_m3             = 0.0;
    double solubility_water_g_L      = 0.0;   // aqueous solubility at 25°C
    double log_P                     = 0.0;   // octanol-water partition (hydrophobicity)
    double boiling_point_K           = 0.0;
    double decomposition_K           = 0.0;   // thermal decomposition onset

    // Optical
    double lambda_max_nm             = 0.0;   // peak absorption wavelength
    double molar_absorptivity        = 0.0;   // L/(mol·cm) at lambda_max
    bool   uv_active                 = false;  // absorbs in 200-400nm UV range
    double uv_lambda_nm              = 0.0;   // UV absorption peak if active

    // Volatile
    double vapor_pressure_Pa_25C     = 0.0;   // vapor pressure at 25°C
    double henry_constant            = 0.0;   // Henry's law constant (Pa·m3/mol)
    bool   is_volatile               = false;
    bool   is_glycoside_bound        = false;  // requires hydrolysis for release

    // Kinetic
    double formation_rate_k          = 0.0;   // pseudo-first-order formation (1/day)
    double degradation_rate_k        = 0.0;   // pseudo-first-order degradation (1/day)
    double enzymatic_release_rate    = 0.0;   // glycoside hydrolysis rate (1/day)

    // Confidence
    double confidence_score          = 1.0;   // 0-1
};

// ============================================================================
// Bio Experiment Types
// ============================================================================

enum class BioExperimentType : int {
    PIGMENT_ABSORPTION_SPECTRUM,     // UV-Vis absorption profile
    VOLATILE_RELEASE_KINETICS,       // time-dependent volatile emission
    DEVELOPMENTAL_STAGING,           // compound abundance vs developmental stage
    UV_REFLECTANCE_PATTERN,          // UV-absorbing/reflecting petal regions
    SEPARATION_EFFICIENCY,           // extraction yield and selectivity
    SEASONAL_PIGMENT_TRANSITION,     // Chl→Carotenoid→Anthocyanin over season
    SCENT_COMPOSITION_PROFILE,       // volatile family distribution at bloom
    PATHWAY_CONSTRAINT_ANALYSIS,     // biosynthetic pathway bottlenecks
};

const char* bio_experiment_name(BioExperimentType type);

// ============================================================================
// Biological System Case
// ============================================================================

enum class DevelopmentalStage : int {
    BUD            = 0,
    EARLY_OPENING  = 1,
    FULL_BLOOM     = 2,
    SENESCENCE     = 3,
};

const char* stage_name(DevelopmentalStage s);

enum class Season : int {
    SPRING = 0,
    SUMMER = 1,
    AUTUMN = 2,
    WINTER = 3,
};

const char* season_name(Season s);

struct OrganismSpec {
    std::string common_name;         // "Dahlia", "Rosa damascena"
    std::string tissue_type;         // "petal", "leaf", "trichome"
    DevelopmentalStage stage         = DevelopmentalStage::FULL_BLOOM;
    Season season                    = Season::SUMMER;
    double temperature_K             = 298.15; // ambient
    double light_intensity_umol      = 500.0;  // PAR µmol/m2/s
    double relative_humidity         = 0.6;    // 0-1
};

struct BioCase {
    // Identity
    uint64_t case_id                 = 0;
    uint64_t seed                    = 0;
    SystemDomain domain              = SystemDomain::FLORAL_PIGMENT;
    ComplexityLevel level            = ComplexityLevel::L1_SIMPLE;
    std::string case_name;
    std::string description;

    // Organism context
    OrganismSpec organism;

    // Chemical inventory
    std::vector<CompoundProperties> compounds;
    std::vector<double> concentrations_mM;     // millimolar

    // Progression
    double gamma_factor              = 1.0;
    double rarity_score              = 0.0;
    double complexity_index          = 0.0;
};

// ============================================================================
// Compound Property Engine
// ============================================================================

class CompoundPropertyEngine {
public:
    CompoundPropertyEngine();

    // Curated compound table (anti-black-box: public, inspectable)
    struct CompoundRecord {
        const char* name;
        const char* formula;
        CompoundFamily family;
        const char* role;
        double molar_mass;
        double density;
        double solubility;
        double log_P;
        double lambda_max;
        double absorptivity;
        bool uv_active;
        double uv_lambda;
        double vapor_pressure;
        bool is_volatile;
        double formation_k;
        double degradation_k;
        double decomposition_K;
        double confidence;
    };

    static const std::vector<CompoundRecord>& compound_table();

    // Lookup
    CompoundProperties compound_properties(const std::string& name) const;
    std::vector<CompoundProperties> compounds_by_family(CompoundFamily family) const;
    std::vector<CompoundProperties> compounds_by_role(const std::string& role) const;

    // Generate mixed tissue composition
    std::vector<CompoundProperties> floral_pigment_set(std::mt19937_64& rng) const;
    std::vector<CompoundProperties> floral_volatile_set(std::mt19937_64& rng) const;
    std::vector<CompoundProperties> leaf_seasonal_set(Season season, std::mt19937_64& rng) const;

private:
    const CompoundRecord* find_compound(const std::string& name) const;
};

// ============================================================================
// Bio Experiment Runner
// ============================================================================

class BioExperimentRunner {
public:
    // Run all applicable experiments for a bio case
    std::vector<ExperimentResult> run_all(const BioCase& bc) const;

    // Individual experiments
    ExperimentResult pigment_absorption_spectrum(const BioCase& bc) const;
    ExperimentResult volatile_release_kinetics(const BioCase& bc) const;
    ExperimentResult developmental_staging(const BioCase& bc) const;
    ExperimentResult uv_reflectance_pattern(const BioCase& bc) const;
    ExperimentResult separation_efficiency(const BioCase& bc) const;
    ExperimentResult seasonal_pigment_transition(const BioCase& bc) const;
    ExperimentResult scent_composition_profile(const BioCase& bc) const;
    ExperimentResult pathway_constraint_analysis(const BioCase& bc) const;
};

// ============================================================================
// Bio Case Generator
// ============================================================================

class BioCaseGenerator {
public:
    explicit BioCaseGenerator(uint64_t base_seed = 42);

    BioCase generate_next();
    BioCase generate_for_domain(SystemDomain domain);

    int cases_generated() const { return cases_generated_; }

private:
    BioCase generate_floral_pigment();
    BioCase generate_floral_volatile();
    BioCase generate_leaf_seasonal();
    BioCase generate_leaf_structure();
    BioCase generate_developmental();

    std::string generate_case_name(const BioCase& bc);

    std::mt19937_64 rng_;
    CompoundPropertyEngine prop_engine_;
    int cases_generated_ = 0;
};

// ============================================================================
// Bio Report Writer (extends core ReportWriter with bio-domain tables)
// ============================================================================

class BioReportWriter {
public:
    static std::string to_markdown(const TechnicalReport& report, const BioCase& bc);
    static std::string compound_table_md(const std::vector<CompoundProperties>& compounds,
                                         const std::vector<double>& concentrations);
    static std::string organism_table_md(const OrganismSpec& org);
};

// ============================================================================
// Bio Autonomous Engine
// ============================================================================

struct BioEngineConfig {
    uint64_t base_seed        = 42;
    int target_reports        = 100;
    std::string output_dir    = "reports";
    bool write_individual     = true;
    bool write_csv_log        = true;
    bool print_progress       = true;
    int progress_interval     = 10;
    SystemDomain domain       = SystemDomain::FLORAL_PIGMENT; // default; rotates if ALL
};

class BioAutonomousEngine {
public:
    explicit BioAutonomousEngine(const BioEngineConfig& config = BioEngineConfig{});

    int run();
    TechnicalReport generate_one();
    int reports_generated() const { return reports_generated_; }

private:
    BioEngineConfig config_;
    BioCaseGenerator case_gen_;
    BioExperimentRunner experiment_runner_;
    int reports_generated_ = 0;

    TechnicalReport build_report(BioCase& bc);
    void analyze_report(TechnicalReport& report, const BioCase& bc);
    void write_report(const TechnicalReport& report, const BioCase& bc);
    void write_csv_entry(const TechnicalReport& report);
    void print_progress(const TechnicalReport& report);
    std::string make_output_path(const TechnicalReport& report, const std::string& ext);
};

} // namespace bio
} // namespace report
} // namespace vsepr
