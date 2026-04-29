/**
 * report_engine.hpp
 * -----------------
 * *** LEGACY — DO NOT EXTEND ***
 *
 * Autonomous report generation is no longer a supported architectural pattern
 * in VSEPR-SIM.  Any reporting cadence or snapshot behavior that was previously
 * handled by AutonomousEngine should instead be encoded by the user in one or
 * more .vsim scripts and interpreted by VsimRuntime.
 *
 * Doctrine (beta-7 and beyond):
 *   Reporting is script-declared, not runtime-autonomous.
 *   If you want periodic snapshots, write a [while] loop with an [export]
 *   section in your .vsim script.  The runtime will honour it.
 *   There is no background engine that decides when to report on your behalf.
 *
 * This file is retained for reference and compile compatibility only.
 * AutonomousEngine will not receive new features.
 *
 * Original work order: WO-TMS-CRG-001
 * Deprecation recorded: WO-56C (beta-7 philosophical overhaul)
 */

#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <random>
#include <functional>

namespace vsepr {
namespace report {

// ============================================================================
// Complexity Level
// ============================================================================

enum class ComplexityLevel : int {
    L1_SIMPLE       = 1,  // Single element, uniform
    L2_BINARY       = 2,  // Binary/ternary alloys
    L3_ANISOTROPIC  = 3,  // Graded, layered, anisotropic
    L4_TRANSIENT    = 4,  // Transient thermal + material variation
    L5_EXOTIC       = 5   // Rare/unstable, multi-factor coupling
};

const char* complexity_name(ComplexityLevel level);

// ============================================================================
// Material Properties (per-component)
// ============================================================================

struct MaterialProperties {
    // Identity
    std::string name;
    std::string formula;
    std::string category;          // "element", "alloy", "compound", "synthetic"

    // Mechanical
    double density_kg_m3       = 0.0;
    double elastic_modulus_GPa = 0.0;
    double yield_strength_MPa  = 0.0;
    double ultimate_strength_MPa = 0.0;
    double poisson_ratio       = 0.3;

    // Thermal
    double thermal_conductivity_W_mK = 0.0;
    double specific_heat_J_kgK       = 0.0;
    double thermal_expansion_1_K     = 0.0;
    double melting_point_K           = 0.0;
    double boiling_point_K           = 0.0;

    // Fatigue
    double fatigue_endurance_MPa     = 0.0;
    double fatigue_exponent          = -0.1;

    // Modifiers
    double anisotropy_factor         = 1.0;  // 1.0 = isotropic
    double uncertainty_factor        = 0.0;  // 0-1 confidence band
    double confidence_score          = 1.0;  // 0-1

    // Classification
    bool is_synthetic                = false;
    uint8_t primary_Z                = 0;    // Primary element Z (if single)
};

// ============================================================================
// Material Case (what we're experimenting on)
// ============================================================================

struct LayerSpec {
    MaterialProperties material;
    double thickness_mm = 1.0;
    double interface_resistance_m2K_W = 0.0;
};

struct ThermalBoundary {
    double temperature_K  = 300.0;
    double heat_flux_W_m2 = 0.0;
    bool is_insulated     = false;
    std::string label;
};

struct ThermalLoad {
    double initial_temperature_K = 300.0;
    double peak_temperature_K    = 600.0;
    double cooling_rate_K_s      = 0.0;
    double heating_rate_K_s      = 0.0;
    double cycle_period_s        = 0.0;
    int    num_cycles            = 1;
    bool   is_transient          = false;
};

struct DefectSpec {
    std::string type;        // "vacancy", "interstitial", "grain_boundary", "crack", "void"
    double concentration;    // fraction or count
    double size_nm;          // characteristic size
};

struct MaterialCase {
    // Identity
    uint64_t case_id         = 0;
    uint64_t seed            = 0;
    ComplexityLevel level    = ComplexityLevel::L1_SIMPLE;
    std::string case_name;
    std::string description;

    // Material system
    std::vector<MaterialProperties> components;
    std::vector<double> fractions;             // weight fractions (sum to 1.0)
    std::vector<LayerSpec> layers;             // for layered materials

    // Effective (mixed) properties
    MaterialProperties effective;

    // Boundary conditions
    std::vector<ThermalBoundary> boundaries;
    ThermalLoad thermal_load;

    // Defects
    std::vector<DefectSpec> defects;

    // Progression metrics
    double gamma_factor       = 1.0;
    double rarity_score       = 0.0;
    double instability_index  = 0.0;
};

// ============================================================================
// Experiment Types
// ============================================================================

enum class ExperimentType : int {
    STEADY_STATE_CONDUCTION,
    TRANSIENT_HEATING,
    TRANSIENT_COOLING,
    THERMAL_EXPANSION,
    THERMAL_STRESS,
    FATIGUE_CYCLING,
    SENSITIVITY_SWEEP,
    PHASE_CHANGE_APPROX,
    OXIDATION_PROXY,
    CRACK_INITIATION_PROXY,
    DIFFUSION_VARIATION,
};

const char* experiment_name(ExperimentType type);

// ============================================================================
// Experiment Results
// ============================================================================

struct DataPoint {
    double x = 0.0;
    double y = 0.0;
    std::string label;
};

struct ExperimentResult {
    ExperimentType type;
    std::string experiment_name;

    // Scalar outputs
    double primary_value     = 0.0;
    std::string primary_unit;
    std::string primary_label;

    double secondary_value   = 0.0;
    std::string secondary_unit;
    std::string secondary_label;

    // Series data (for plots)
    std::vector<DataPoint> series;
    std::string series_x_label;
    std::string series_y_label;

    // Diagnostic
    double numerical_stability = 1.0;  // 0-1
    double physical_plausibility = 1.0; // 0-1
    std::string notes;
    bool converged = true;
};

// ============================================================================
// Technical Report
// ============================================================================

struct TechnicalReport {
    // Header
    uint64_t report_id       = 0;
    std::string title;
    std::string abstract_text;
    std::string timestamp;

    // Case
    MaterialCase material_case;

    // Results
    std::vector<ExperimentResult> experiments;

    // Analysis
    double overall_stability_score = 0.0;
    double novelty_score           = 0.0;
    double thermal_response_index  = 0.0;
    double deformation_score       = 0.0;
    std::vector<std::string> findings;
    std::vector<std::string> warnings;
    std::string conclusion;

    // Metadata
    int total_reports_so_far = 0;
    ComplexityLevel current_level = ComplexityLevel::L1_SIMPLE;
};

// ============================================================================
// Material Property Engine
// ============================================================================

class MaterialPropertyEngine {
public:
    MaterialPropertyEngine();

    // Generate properties for known elements (Z=1-118)
    MaterialProperties element_properties(uint8_t Z) const;

    // Generate random synthetic material
    MaterialProperties synthetic_properties(std::mt19937_64& rng, double exoticism = 0.0) const;

    // Alloy mixing (rule of mixtures + random perturbation)
    MaterialProperties mix_alloy(
        const std::vector<MaterialProperties>& components,
        const std::vector<double>& fractions,
        std::mt19937_64& rng,
        double perturbation = 0.05
    ) const;

    // Perturb existing properties (uncertainty modeling)
    MaterialProperties perturb(const MaterialProperties& base, std::mt19937_64& rng, double sigma = 0.1) const;

    // Internal element property tables (public for inspection — anti-black-box)
    struct ElementRecord {
        uint8_t Z;
        const char* symbol;
        const char* name;
        double density;
        double elastic_modulus;
        double yield_strength;
        double ult_strength;
        double thermal_cond;
        double specific_heat;
        double thermal_exp;
        double melting_point;
        double boiling_point;
    };
    static const std::vector<ElementRecord>& element_table();

private:
    const ElementRecord* find_element(uint8_t Z) const;
};

// ============================================================================
// Case Generator
// ============================================================================

class CaseGenerator {
public:
    explicit CaseGenerator(uint64_t base_seed = 42);

    // Generate next case with automatic complexity escalation
    MaterialCase generate_next();

    // Generate at specific complexity level
    MaterialCase generate_at_level(ComplexityLevel level);

    // Progression state
    int cases_generated() const { return cases_generated_; }
    ComplexityLevel current_level() const { return current_level_; }
    double current_gamma() const { return gamma_; }

    // Escalation thresholds (configurable)
    void set_escalation_thresholds(int l2, int l3, int l4, int l5);

private:
    MaterialCase generate_l1();
    MaterialCase generate_l2();
    MaterialCase generate_l3();
    MaterialCase generate_l4();
    MaterialCase generate_l5();

    void escalate_if_needed();
    std::string generate_case_name(const MaterialCase& c);

    std::mt19937_64 rng_;
    MaterialPropertyEngine prop_engine_;
    int cases_generated_     = 0;
    ComplexityLevel current_level_ = ComplexityLevel::L1_SIMPLE;
    double gamma_            = 1.0;
    double rarity_accum_     = 0.0;

    int threshold_l2_ = 50;
    int threshold_l3_ = 150;
    int threshold_l4_ = 400;
    int threshold_l5_ = 700;
};

// ============================================================================
// Experiment Runner
// ============================================================================

class ExperimentRunner {
public:
    // Run all applicable experiments on a material case
    std::vector<ExperimentResult> run_all(const MaterialCase& mc) const;

    // Individual experiments
    ExperimentResult steady_state_conduction(const MaterialCase& mc) const;
    ExperimentResult transient_heating(const MaterialCase& mc) const;
    ExperimentResult transient_cooling(const MaterialCase& mc) const;
    ExperimentResult thermal_expansion(const MaterialCase& mc) const;
    ExperimentResult thermal_stress(const MaterialCase& mc) const;
    ExperimentResult fatigue_cycling(const MaterialCase& mc) const;
    ExperimentResult sensitivity_sweep(const MaterialCase& mc) const;
    ExperimentResult phase_change_approx(const MaterialCase& mc) const;
    ExperimentResult oxidation_proxy(const MaterialCase& mc) const;
    ExperimentResult crack_initiation_proxy(const MaterialCase& mc) const;
};

// ============================================================================
// Report Writer
// ============================================================================

class ReportWriter {
public:
    // Write report to Markdown string
    static std::string to_markdown(const TechnicalReport& report);

    // Write report to CSV summary line
    static std::string to_csv_line(const TechnicalReport& report);

    // CSV header
    static std::string csv_header();

    // Write ASCII table of experiment results
    static std::string results_table(const std::vector<ExperimentResult>& results);

    // Property summary table
    static std::string property_table(const MaterialProperties& props);
};

// ============================================================================
// Autonomous Engine
// ============================================================================

struct EngineConfig {
    uint64_t base_seed       = 42;
    int target_reports       = 1000;
    std::string output_dir   = "reports";
    bool write_individual    = true;   // Write per-report .md files
    bool write_csv_log       = true;   // Write cumulative CSV
    bool print_progress      = true;   // Print to stdout
    int progress_interval    = 50;     // Print every N reports

    // Escalation thresholds
    int threshold_l2 = 50;
    int threshold_l3 = 150;
    int threshold_l4 = 400;
    int threshold_l5 = 700;
};

// LEGACY: AutonomousEngine is deprecated. Encode reporting behavior in .vsim scripts.
class [[deprecated("AutonomousEngine is LEGACY. Declare reporting behavior in .vsim scripts interpreted by VsimRuntime.")]] AutonomousEngine {
public:
    explicit AutonomousEngine(const EngineConfig& config = EngineConfig{});

    // Run the full generation loop
    // Run the full generation loop
    int run();

    // Generate a single report
    TechnicalReport generate_one();

    // Statistics
    int reports_generated() const { return reports_generated_; }
    ComplexityLevel current_level() const;

private:
    EngineConfig config_;
    CaseGenerator case_gen_;
    ExperimentRunner experiment_runner_;
    int reports_generated_ = 0;

    TechnicalReport build_report(MaterialCase& mc);
    void analyze_report(TechnicalReport& report);
    void write_report(const TechnicalReport& report);
    void write_csv_entry(const TechnicalReport& report);
    void print_progress(const TechnicalReport& report);
    std::string make_output_path(const TechnicalReport& report, const std::string& ext);
};

} // namespace report
} // namespace vsepr
