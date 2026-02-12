#pragma once
/**
 * sim_state.hpp
 * -------------
 * Unified simulation state machine supporting multiple modes:
 * - VSEPR geometry optimization
 * - General structure optimization
 * - Molecular dynamics (MD)
 * - Crystal/periodic optimization
 */

#include "molecule.hpp"
#include "pot/energy_model.hpp"
#include "sim/optimizer.hpp"
#include "sim_command.hpp"
#include "core/frame_snapshot.hpp"
#include <random>
#include "box/pbc.hpp"
#include <vector>
#include <string>
#include <memory>

namespace vsepr {

// ============================================================================
// Simulation Parameters (unified across modes)
// ============================================================================

struct SimParams {
    // Optimizer (FIRE) params
    double dt_init = 0.1;
    double dt_max = 1.0;
    double alpha_init = 0.1;
    double max_step = 0.2;
    double tol_rms_force = 1e-3;
    double tol_max_force = 1e-3;
    int max_iterations = 1000;
    
    // MD params
    double temperature = 300.0;     // K
    double timestep = 0.001;        // ps
    double damping = 1.0;           // friction coefficient (ps^-1)
    std::string thermostat = "berendsen";  // "none", "berendsen", "langevin", "nose-hoover", "v-rescale"
    double tau_thermostat = 0.1;    // Thermostat coupling time (ps)
    
    // Energy model params
    double bond_k = 300.0;
    double angle_scale = 0.1;
    bool use_angles = true;
    bool use_torsions = false;
    bool use_nonbonded = true;
    bool use_vsepr = false;
    
    // Periodic boundary conditions
    bool use_pbc = false;
    double box_size[3] = {10.0, 10.0, 10.0};
    
    // Visualization
    int publish_every = 2;          // Publish frame every N steps
    int print_every = 10;           // Print stats every N steps
};

// ============================================================================
// Simulation Statistics
// ============================================================================

struct SimStats {
    int iteration = 0;
    double total_energy = 0.0;
    double kinetic_energy = 0.0;
    double potential_energy = 0.0;
    double temperature = 0.0;
    double rms_force = 0.0;
    double max_force = 0.0;
    double pressure = 0.0;
    
    // Component breakdown
    double bond_energy = 0.0;
    double angle_energy = 0.0;
    double torsion_energy = 0.0;
    double nonbonded_energy = 0.0;
    double vsepr_energy = 0.0;
    
    // FIRE state
    double fire_dt = 0.0;
    double fire_alpha = 0.0;
    int fire_n_positive = 0;
};

// ============================================================================
// Simulation State
// ============================================================================

class SimulationState {
public:
    SimulationState();
    
    // Initialize from molecule
    void initialize(const Molecule& mol);
    
    // Load from file
    bool load_from_file(const std::string& filepath);
    
    // Reset to initial state
    void reset();
    
    // Get current mode
    SimMode mode() const { return mode_; }
    void set_mode(SimMode mode);
    
    // Control
    bool is_running() const { return running_; }
    bool is_paused() const { return paused_; }
    void pause() { paused_ = true; }
    void resume() { paused_ = false; running_ = true; }
    void stop() { running_ = false; paused_ = false; }
    
    // Parameters
    SimParams& params() { return params_; }
    const SimParams& params() const { return params_; }
    
    // Path-based parameter setting (replaces update_params)
    void set_param(const std::string& path, const ParamValue& value);
    std::optional<ParamValue> get_param(const std::string& path) const;
    
    // Single simulation step (mode-dependent)
    void step();
    
    // Multi-step advance
    void advance(int n_steps);
    
    // Get current state for visualization
    FrameSnapshot get_snapshot() const;
    
    // Statistics
    const SimStats& stats() const { return stats_; }
    
    // Save current state to file
    bool save_to_file(const std::string& filepath) const;
    
    // Spawn particles for PBC systems
    void spawn_particles(const CmdSpawn& cmd);
    
    // Access to molecule (read-only for thread safety)
    const Molecule& molecule() const { return molecule_; }
    
    // Access to periodic box
    const BoxOrtho& box() const { return box_; }
    
    // Check if converged (for optimization modes)
    bool is_converged() const;
    
private:
    // Mode-specific step implementations
    void step_vsepr();
    void step_optimize();
    void step_md();
    void step_crystal();
    
    // Energy/force evaluation
    void evaluate_forces();
    void compute_statistics();
    
    // FIRE optimizer state
    void fire_update_velocity();
    void fire_update_timestep();
    void fire_velocity_verlet_step();
    
    // MD integrator
    void md_velocity_verlet_step();
    void md_apply_thermostat();
    void md_initialize_velocities();  // Maxwell-Boltzmann distribution
    
    // State
    SimMode mode_;
    bool running_;
    bool paused_;
    
    // Molecule and coordinates
    Molecule molecule_;
    std::vector<double> coords_;        // Current positions [x1,y1,z1, x2,y2,z2, ...]
    std::vector<double> velocities_;    // Current velocities (for MD)
    std::vector<double> forces_;        // Current forces
    std::vector<double> masses_;        // Atomic masses [m1,m1,m1, m2,m2,m2, ...] (3N)
    std::vector<double> coords_init_;   // Initial coordinates (for reset)
    
    // Energy model
    std::unique_ptr<EnergyModel> energy_model_;
    
    // Periodic box
    BoxOrtho box_;
    
    // Parameters and statistics
    SimParams params_;
    SimStats stats_;
    
    // FIRE optimizer state
    double fire_dt_;
    double fire_alpha_;
    int fire_n_positive_;
    
    // MD state
    double md_time_;                    // Current simulation time (ps)
    
    // Random number generator
    std::mt19937 rng_;                  // For stochastic initialization
};

} // namespace vsepr
