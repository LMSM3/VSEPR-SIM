/**
 * Thermal API - App-Facing Façade
 * 
 * Clean contract for thermal pathway computation and analysis
 * 
 * Contracts:
 * - Energy units: kJ/mol (kilojoules per mole)
 * - Temperature: Kelvin (K)
 * - Time: femtoseconds (fs)
 * - Activation energies: kJ/mol
 * - Gas constant R = 8.314 J/(mol·K) for Arrhenius calculations
 */

#pragma once

#include "core/error.hpp"
#include "thermal/xyzc_format.hpp"
#include "io/xyz_format.hpp"
#include <string>
#include <vector>
#include <array>

namespace vsepr {
namespace api {

// ============================================================================
// Pathway Classification
// ============================================================================

enum class ThermalPathwayType {
    PHONON_LATTICE,           // Vibrational modes
    ELECTRONIC,               // Electronic transitions
    MOLECULAR_ROTATIONAL,     // Rotational modes
    TRANSLATIONAL_KINETIC,    // Kinetic energy
    RADIATIVE_MICRO,          // Radiative transfer
    GATED_STRUCTURAL          // Activation-gated processes
};

struct PathwayInfo {
    ThermalPathwayType type;
    double coupling_strength;      // Dimensionless [0, 1]
    double activation_energy;      // kJ/mol
    double current_flow;           // Energy flow rate (kJ/mol/fs)
    std::array<double, 3> direction;  // Spatial directionality
    
    std::string type_name() const;
};

// ============================================================================
// Thermal Configuration
// ============================================================================

struct ThermalConfig {
    double temperature = 298.15;      // K (default: room temperature)
    double timestep = 1.0;            // fs
    int num_frames = 1000;            // Total simulation frames
    bool include_quantum = true;      // Include quantum corrections
    bool enable_damping = true;       // Enable energy damping
    double damping_factor = 0.95;     // Per-step energy retention [0, 1]
    
    // Pathway-specific settings
    bool enable_phonon = true;
    bool enable_electronic = true;
    bool enable_rotational = true;
    bool enable_translational = true;
    bool enable_radiative = false;     // Typically off for small molecules
    bool enable_gated = true;
    
    // Validation
    Status validate() const;
};

// ============================================================================
// Thermal Computation
// ============================================================================

/**
 * Compute thermal pathways for a molecule
 * 
 * @param molecule Input molecular geometry
 * @param config Thermal simulation parameters
 * @return Result containing pathway trajectory or error
 */
Result<thermal::ThermalTrajectory> compute_thermal_pathways(
    const io::XYZMolecule& molecule,
    const ThermalConfig& config
);

/**
 * Run thermal simulation and save to .xyzC file
 * 
 * @param molecule Input geometry
 * @param config Simulation parameters
 * @param output_file Path to output .xyzC file
 * @return Status indicating success or error
 */
Status run_thermal_simulation(
    const io::XYZMolecule& molecule,
    const ThermalConfig& config,
    const std::string& output_file
);

// ============================================================================
// Pathway Analysis
// ============================================================================

/**
 * Analyze pathway contributions at specific frame
 * 
 * @param trajectory Thermal trajectory
 * @param frame_index Frame to analyze (0-based)
 * @return Vector of pathway info for each active pathway
 */
Result<std::vector<PathwayInfo>> analyze_pathways_at_frame(
    const thermal::ThermalTrajectory& trajectory,
    int frame_index
);

/**
 * Compute time-averaged pathway contributions
 * 
 * @param trajectory Full thermal trajectory
 * @return Averaged pathway strengths over all frames
 */
Result<std::vector<PathwayInfo>> compute_average_pathways(
    const thermal::ThermalTrajectory& trajectory
);

/**
 * Find dominant pathway at each frame
 * 
 * @param trajectory Thermal trajectory
 * @return Vector mapping frame -> dominant pathway type
 */
Result<std::vector<ThermalPathwayType>> identify_dominant_pathways(
    const thermal::ThermalTrajectory& trajectory
);

// ============================================================================
// Energy Flow Analysis
// ============================================================================

struct EnergyFlowSummary {
    double total_energy;              // Total system energy (kJ/mol)
    double kinetic_energy;            // Kinetic contribution (kJ/mol)
    double potential_energy;          // Potential contribution (kJ/mol)
    double phonon_contribution;       // Phonon pathway energy (kJ/mol)
    double electronic_contribution;   // Electronic pathway energy (kJ/mol)
    double rotational_contribution;   // Rotational pathway energy (kJ/mol)
    double translational_contribution; // Translational pathway energy (kJ/mol)
    
    double temperature_estimate;      // Estimated temperature from kinetic energy (K)
    
    std::string to_string() const;
};

/**
 * Compute energy distribution at specific frame
 * 
 * @param trajectory Thermal trajectory
 * @param frame_index Frame to analyze
 * @return Energy breakdown by pathway type
 */
Result<EnergyFlowSummary> compute_energy_distribution(
    const thermal::ThermalTrajectory& trajectory,
    int frame_index
);

/**
 * Track total energy over time
 * 
 * @param trajectory Thermal trajectory
 * @return Vector of total energies (one per frame)
 */
Result<std::vector<double>> compute_energy_timeline(
    const thermal::ThermalTrajectory& trajectory
);

// ============================================================================
// Activation Analysis
// ============================================================================

struct ActivationGate {
    int atom_index_a;          // First atom involved
    int atom_index_b;          // Second atom involved
    double activation_energy;  // Barrier height (kJ/mol)
    double gate_openness;      // Current gate state [0, 1]
    bool is_open;              // Whether barrier is overcome
    
    std::string to_string() const;
};

/**
 * Identify activation gates in system
 * 
 * @param molecule Molecular geometry
 * @param temperature System temperature (K)
 * @return List of detected activation barriers
 */
Result<std::vector<ActivationGate>> identify_activation_gates(
    const io::XYZMolecule& molecule,
    double temperature
);

/**
 * Compute gate opening probability (Arrhenius factor)
 * 
 * @param activation_energy Barrier height (kJ/mol)
 * @param temperature System temperature (K)
 * @return Probability [0, 1] that gate is open
 */
double compute_gate_probability(double activation_energy, double temperature);

// ============================================================================
// Validation and Diagnostics
// ============================================================================

/**
 * Validate thermal trajectory
 * Checks:
 * - Energy conservation (within tolerance)
 * - No NaN or inf values
 * - Temperature stability
 * 
 * @param trajectory Trajectory to validate
 * @param tolerance Relative error tolerance (default: 1e-3)
 * @return Status with error if validation fails
 */
Status validate_thermal_trajectory(
    const thermal::ThermalTrajectory& trajectory,
    double tolerance = 1e-3
);

/**
 * Compute thermal stability metrics
 * 
 * @param trajectory Trajectory to analyze
 * @return Status describing stability (OK if stable, warning if drift detected)
 */
Status check_thermal_stability(
    const thermal::ThermalTrajectory& trajectory
);

/**
 * Generate human-readable summary of thermal simulation
 * 
 * @param trajectory Thermal trajectory
 * @return Multi-line summary string
 */
std::string generate_thermal_summary(
    const thermal::ThermalTrajectory& trajectory
);

// ============================================================================
// Unit Conversion Utilities
// ============================================================================

namespace units {
    // Energy conversions
    constexpr double KJ_MOL_TO_KCAL_MOL = 0.239006;
    constexpr double KCAL_MOL_TO_KJ_MOL = 4.184;
    constexpr double EV_TO_KJ_MOL = 96.485;
    constexpr double HARTREE_TO_KJ_MOL = 2625.5;
    
    // Temperature conversions
    inline double celsius_to_kelvin(double celsius) { return celsius + 273.15; }
    inline double kelvin_to_celsius(double kelvin) { return kelvin - 273.15; }
    
    // Gas constant in various units
    constexpr double R_SI = 8.314462618;           // J/(mol·K)
    constexpr double R_KCAL = 0.001987204;         // kcal/(mol·K)
    constexpr double R_ATM = 0.08205746;           // L·atm/(mol·K)
}

}} // namespace vsepr::api
