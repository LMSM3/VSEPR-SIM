/**
 * Thermal Animation Runner Header
 * vsepr-sim v2.3.1
 * 
 * Features:
 * - Real-time thermal evolution simulation
 * - Background thread processing
 * - Energy tracking over time
 * - Frame sampling for animation
 * - Temperature-dependent molecular dynamics
 * - Pause/resume/stop controls
 * 
 * Integration:
 * - Works with existing Molecule class
 * - Provides frames for GUI rendering
 * - Energy history for plotting
 * - Thread-safe state access
 * 
 * Author: VSEPR-Sim Development Team
 * Date: January 2025
 * License: MIT
 */

#pragma once

#include "sim/molecule.hpp"
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

namespace vsepr {
namespace thermal {

// ============================================================================
// Thermal Evolution Configuration
// ============================================================================

struct ThermalConfig {
    double temperature;        // Temperature in Kelvin
    int total_generations;     // Total MD steps to simulate
    int sample_interval;       // Sample every N generations
    double time_step;          // Time step in femtoseconds (default: 1.0 fs)
    bool save_trajectory;      // Save full trajectory to disk
    std::string output_path;   // Output path for trajectory file
    
    ThermalConfig()
        : temperature(300.0)
        , total_generations(10000)
        , sample_interval(10)
        , time_step(1.0)
        , save_trajectory(false)
        , output_path("output/thermal/trajectory.xyz") {}
};

// ============================================================================
// Thermal Statistics
// ============================================================================

struct ThermalStatistics {
    size_t total_steps;           // Total MD steps completed
    size_t frames_captured;       // Number of frames sampled
    double avg_energy;            // Average energy (kcal/mol)
    double min_energy;            // Minimum energy encountered
    double max_energy;            // Maximum energy encountered
    double temperature_actual;    // Actual temperature from kinetic energy
    double elapsed_time_seconds;  // Real-time elapsed
    
    ThermalStatistics()
        : total_steps(0)
        , frames_captured(0)
        , avg_energy(0.0)
        , min_energy(1e100)
        , max_energy(-1e100)
        , temperature_actual(0.0)
        , elapsed_time_seconds(0.0) {}
};

// ============================================================================
// ThermalRunner: Real-Time Thermal Evolution Simulator
// ============================================================================

class ThermalRunner {
public:
    // Callback types
    using ProgressCallback = std::function<void(size_t step, size_t total)>;
    using FrameCallback = std::function<void(const Molecule& frame)>;
    
    ThermalRunner();
    ~ThermalRunner();
    
    // ========================================================================
    // Control Methods
    // ========================================================================
    
    /**
     * Start thermal evolution simulation
     * @param initial_molecule Starting molecular structure
     * @param config Simulation parameters
     */
    void start(const Molecule& initial_molecule, const ThermalConfig& config);
    
    /**
     * Stop simulation and wait for thread to finish
     */
    void stop();
    
    /**
     * Pause simulation (can be resumed)
     */
    void pause();
    
    /**
     * Resume paused simulation
     */
    void resume();
    
    /**
     * Check if simulation is currently running
     */
    bool is_running() const { return running_.load(); }
    
    /**
     * Check if simulation is paused
     */
    bool is_paused() const { return paused_.load(); }
    
    // ========================================================================
    // Data Access (Thread-Safe)
    // ========================================================================
    
    /**
     * Get current frame for real-time visualization
     * @return Current molecular structure
     */
    Molecule get_current_frame() const;
    
    /**
     * Get all sampled frames (for export)
     * @return Vector of molecular snapshots
     */
    std::vector<Molecule> get_frames() const;
    
    /**
     * Get energy history for plotting
     * @return Vector of energy values (kcal/mol)
     */
    std::vector<double> get_energy_history() const;
    
    /**
     * Get current simulation progress
     * @return Pair of (current_step, total_steps)
     */
    std::pair<size_t, size_t> get_progress() const;
    
    /**
     * Get simulation statistics
     */
    ThermalStatistics get_statistics() const;
    
    // ========================================================================
    // Callbacks
    // ========================================================================
    
    /**
     * Set progress callback (called every N steps)
     */
    void set_progress_callback(ProgressCallback callback, int interval = 100);
    
    /**
     * Set frame callback (called when new frame is captured)
     */
    void set_frame_callback(FrameCallback callback);
    
    // ========================================================================
    // Export Methods
    // ========================================================================
    
    /**
     * Export trajectory to multi-frame XYZ file
     * @param path Output file path
     */
    void export_trajectory(const std::string& path) const;
    
    /**
     * Export energy history to CSV
     * @param path Output file path
     */
    void export_energy_csv(const std::string& path) const;
    
private:
    // Thread function
    void run_simulation();
    
    // MD evolution step
    Molecule evolve_one_step(const Molecule& mol, double temperature, double dt);
    
    // Calculate molecular energy (placeholder for now)
    double calculate_energy(const Molecule& mol) const;
    
    // Calculate temperature from kinetic energy
    double calculate_temperature(const Molecule& mol) const;
    
    // Apply Langevin thermostat
    void apply_thermostat(Molecule& mol, double target_temp, double dt);
    
    // Thread state
    std::thread simulation_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> stop_requested_{false};
    
    // Simulation data (protected by mutex)
    mutable std::mutex data_mutex_;
    std::vector<Molecule> frames_;
    std::vector<double> energy_history_;
    Molecule current_frame_;
    ThermalConfig config_;
    ThermalStatistics stats_;
    
    // Progress tracking
    std::atomic<size_t> current_step_{0};
    std::atomic<size_t> total_steps_{0};
    
    // Callbacks
    ProgressCallback progress_callback_;
    FrameCallback frame_callback_;
    int progress_callback_interval_{100};
};

// ============================================================================
// Animation Export Utilities
// ============================================================================

/**
 * Export thermal animation to multi-frame XYZ file
 * @param frames Vector of molecular snapshots
 * @param output_path Output file path
 * @param comment_template Comment line template (can include frame number)
 */
void export_thermal_animation_xyz(
    const std::vector<Molecule>& frames,
    const std::string& output_path,
    const std::string& comment_template = "Frame {frame_num}"
);

/**
 * Export energy vs time data to CSV
 * @param energy_history Vector of energies (kcal/mol)
 * @param time_step Time step in fs
 * @param output_path Output file path
 */
void export_energy_csv(
    const std::vector<double>& energy_history,
    double time_step,
    const std::string& output_path
);

} // namespace thermal
} // namespace vsepr
