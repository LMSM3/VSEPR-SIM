/**
 * Automated Discovery System
 * 
 * Generates and tests multiple molecular combinations:
 * - Weighted random sampling from element space
 * - HGST scoring to reject near-impossibilities
 * - Thermal stability analysis
 * - Automated optimization and validation
 */

#pragma once

#include "pot/periodic_db.hpp"
#include "sim/molecule.hpp"
#include "hgst/hgst_matrix.hpp"
#include "thermal/thermal_model.hpp"
#include <vector>
#include <string>
#include <map>
#include <functional>

namespace vsepr {
namespace discovery {

// Discovery parameters
struct DiscoveryParams {
    int num_combinations;       // Number of molecules to generate (default: 100)
    double hgst_threshold;      // Minimum HGST score to accept (default: 0.3)
    double thermal_max_T;       // Maximum allowed temperature (K, default: 1000.0)
    bool enable_thermal;        // Run thermal analysis
    bool enable_optimization;   // Optimize each candidate
    int max_atoms;              // Maximum atoms per molecule (default: 20)
    std::string weights_file;   // Element weights JSON
    std::string output_dir;     // Output directory for results
    
    DiscoveryParams()
        : num_combinations(100), hgst_threshold(0.3), thermal_max_T(1000.0),
          enable_thermal(true), enable_optimization(true), max_atoms(20),
          weights_file("data/element_weights.json"), output_dir("discovery_results") {}
};

// Candidate molecule with scores
struct CandidateMolecule {
    std::string formula;
    Molecule molecule;
    
    // HGST scores
    hgst::StateVector hgst_state;
    double hgst_score;          // Overall HGST viability score
    
    // Thermal properties
    double final_temperature;
    double thermal_stability;   // 0-1 score
    
    // Optimization results
    bool optimized;
    double final_energy;
    bool converged;
    
    // Chemistry assessment
    bool is_viable;             // Passed all filters
    std::string reject_reason;  // Why rejected (if applicable)
    
    CandidateMolecule() : hgst_score(0.0), final_temperature(0.0),
                          thermal_stability(0.0), optimized(false),
                          final_energy(0.0), converged(false),
                          is_viable(true) {}
};

// Discovery engine
class DiscoveryEngine {
public:
    explicit DiscoveryEngine(const PeriodicTable& ptable);
    
    // Run discovery with parameters
    std::vector<CandidateMolecule> run(const DiscoveryParams& params);
    
    // Individual steps
    std::string generate_candidate_formula(const std::map<std::string, double>& weights);
    CandidateMolecule evaluate_candidate(const std::string& formula, const DiscoveryParams& params);
    
    // HGST scoring
    double score_with_hgst(const Molecule& mol, hgst::StateVector& state);
    
    // Thermal stability test
    double test_thermal_stability(Molecule& mol, double target_T, double max_T);
    
    // Filters
    bool passes_hgst_filter(double score, double threshold) const;
    bool passes_thermal_filter(double temperature, double max_allowed) const;
    bool passes_size_filter(const Molecule& mol, int max_atoms) const;
    
    // Export results
    void export_results(const std::vector<CandidateMolecule>& candidates,
                       const std::string& filename, const std::string& format = "csv");
    
    // Progress callback
    using ProgressCallback = std::function<void(int current, int total, const std::string& status)>;
    void set_progress_callback(ProgressCallback callback) { progress_callback_ = callback; }
    
private:
    const PeriodicTable& ptable_;
    hgst::HGSTMatrix hgst_matrix_;
    thermal::ThermalModel thermal_model_;
    ProgressCallback progress_callback_;
    
    // Compute HGST state vector from molecule
    hgst::StateVector compute_hgst_state(const Molecule& mol);
    
    // Compute individual HGST components
    double compute_donor_confidence(const Molecule& mol);
    double compute_geometry_score(const Molecule& mol);
    double compute_steric_penalty(const Molecule& mol);
    double compute_agostic_propensity(const Molecule& mol);
    double compute_ox_plausibility(const Molecule& mol);
};

}} // namespace vsepr::discovery
