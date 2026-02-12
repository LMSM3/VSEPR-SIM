/**
 * sim_state.cpp
 * -------------
 * Implementation of unified simulation state machine.
 */

#include "sim/sim_state.hpp"
#include "core/json_schema.hpp"
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace vsepr {

// ============================================================================
// Construction & Initialization
// ============================================================================

SimulationState::SimulationState()
    : mode_(SimMode::IDLE)
    , running_(false)
    , paused_(false)
    , fire_dt_(0.0)
    , fire_alpha_(0.0)
    , fire_n_positive_(0)
    , md_time_(0.0)
{
}

void SimulationState::initialize(const Molecule& mol) {
    molecule_ = mol;
    coords_ = mol.coords;
    coords_init_ = mol.coords;
    
    velocities_.resize(coords_.size(), 0.0);
    forces_.resize(coords_.size(), 0.0);
    
    // Create energy model
    energy_model_ = std::make_unique<EnergyModel>(
        molecule_,
        params_.bond_k,
        params_.use_angles,
        params_.use_nonbonded,
        NonbondedParams(),
        params_.use_torsions,
        params_.use_vsepr,
        params_.angle_scale
    );
    
    // Reset statistics
    stats_ = SimStats();
    
    // Initialize FIRE state
    fire_dt_ = params_.dt_init;
    fire_alpha_ = params_.alpha_init;
    fire_n_positive_ = 0;
    
    // Evaluate initial forces
    evaluate_forces();
    compute_statistics();
    
    // Start running by default (but paused)
    running_ = true;
    paused_ = true;
    
    std::cout << "[SimState] Initialized with " << mol.num_atoms() << " atoms\n";
}

bool SimulationState::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[SimState] Failed to open file: " << filepath << "\n";
        return false;
    }
    
    // TODO: Implement JSON parsing using nlohmann/json
    std::cerr << "[SimState] WARNING: JSON loading not yet implemented\n";
    std::cerr << "[SimState] Use CmdInitMolecule or load via visualizer startup instead\n";
    return false;
}

void SimulationState::reset() {
    coords_ = coords_init_;
    std::fill(velocities_.begin(), velocities_.end(), 0.0);
    std::fill(forces_.begin(), forces_.end(), 0.0);
    
    stats_ = SimStats();
    fire_dt_ = params_.dt_init;
    fire_alpha_ = params_.alpha_init;
    fire_n_positive_ = 0;
    md_time_ = 0.0;
    
    // Keep running flag, but pause
    paused_ = true;
    
    evaluate_forces();
    compute_statistics();
    
    std::cout << "[SimState] Reset to initial coordinates\n";
}

// ============================================================================
// Mode Control
// ============================================================================

void SimulationState::set_mode(SimMode mode) {
    if (mode_ != mode) {
        mode_ = mode;
        
        // Reset velocities when switching modes
        std::fill(velocities_.begin(), velocities_.end(), 0.0);
        
        // Mode-specific initialization
        if (mode == SimMode::MD) {
            // Initialize Maxwell-Boltzmann velocities
            // TODO: Proper random velocity initialization
        } else if (mode == SimMode::VSEPR || mode == SimMode::OPTIMIZE) {
            fire_dt_ = params_.dt_init;
            fire_alpha_ = params_.alpha_init;
            fire_n_positive_ = 0;
        }
    }
}

void SimulationState::update_params(const CmdSetParams& cmd) {
    if (cmd.dt_init) params_.dt_init = *cmd.dt_init;
    if (cmd.dt_max) params_.dt_max = *cmd.dt_max;
    if (cmd.alpha_init) params_.alpha_init = *cmd.alpha_init;
    if (cmd.max_step) params_.max_step = *cmd.max_step;
    if (cmd.tol_rms_force) params_.tol_rms_force = *cmd.tol_rms_force;
    if (cmd.tol_max_force) params_.tol_max_force = *cmd.tol_max_force;
    if (cmd.max_iterations) params_.max_iterations = *cmd.max_iterations;
    if (cmd.temperature) params_.temperature = *cmd.temperature;
    if (cmd.timestep) params_.timestep = *cmd.timestep;
    if (cmd.damping) params_.damping = *cmd.damping;
}

// ============================================================================
// Simulation Steps
// ============================================================================

void SimulationState::step() {
    if (!running_ || paused_) {
        return;
    }
    
    switch (mode_) {
        case SimMode::VSEPR:
            step_vsepr();
            break;
        case SimMode::OPTIMIZE:
            step_optimize();
            break;
        case SimMode::MD:
            step_md();
            break;
        case SimMode::CRYSTAL:
            step_crystal();
            break;
        case SimMode::IDLE:
        default:
            break;
    }
    
    stats_.iteration++;
    compute_statistics();
}

void SimulationState::advance(int n_steps) {
    for (int i = 0; i < n_steps; ++i) {
        step();
        
        // Check convergence for optimization modes
        if (mode_ != SimMode::MD && is_converged()) {
            stop();
            break;
        }
        
        // Check iteration limit
        if (stats_.iteration >= params_.max_iterations) {
            stop();
            break;
        }
    }
}

// ============================================================================
// Mode-Specific Steps
// ============================================================================

void SimulationState::step_vsepr() {
    // VSEPR uses FIRE with constraints
    fire_velocity_verlet_step();
    fire_update_velocity();
    fire_update_timestep();
}

void SimulationState::step_optimize() {
    // General optimization uses FIRE
    fire_velocity_verlet_step();
    fire_update_velocity();
    fire_update_timestep();
}

void SimulationState::step_md() {
    // MD uses velocity Verlet + thermostat
    md_velocity_verlet_step();
    md_apply_thermostat();
    md_time_ += params_.timestep;
}

void SimulationState::step_crystal() {
    // Crystal optimization (placeholder - similar to optimize)
    fire_velocity_verlet_step();
    fire_update_velocity();
    fire_update_timestep();
}

// ============================================================================
// FIRE Implementation
// ============================================================================

void SimulationState::fire_velocity_verlet_step() {
    const size_t N = coords_.size();
    
    // v(t + dt/2) = v(t) + F(t) * dt/2
    for (size_t i = 0; i < N; ++i) {
        velocities_[i] += forces_[i] * fire_dt_ * 0.5;
    }
    
    // x(t + dt) = x(t) + v(t + dt/2) * dt
    std::vector<double> displacement(N);
    for (size_t i = 0; i < N; ++i) {
        displacement[i] = velocities_[i] * fire_dt_;
    }
    
    // Clamp displacements
    const size_t N_atoms = N / 3;
    for (size_t i = 0; i < N_atoms; ++i) {
        double dx = displacement[3*i + 0];
        double dy = displacement[3*i + 1];
        double dz = displacement[3*i + 2];
        double d = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        if (d > params_.max_step) {
            double scale = params_.max_step / d;
            displacement[3*i + 0] *= scale;
            displacement[3*i + 1] *= scale;
            displacement[3*i + 2] *= scale;
        }
    }
    
    // Apply displacement
    for (size_t i = 0; i < N; ++i) {
        coords_[i] += displacement[i];
    }
    
    // Evaluate new forces
    evaluate_forces();
    
    // v(t + dt) = v(t + dt/2) + F(t + dt) * dt/2
    for (size_t i = 0; i < N; ++i) {
        velocities_[i] += forces_[i] * fire_dt_ * 0.5;
    }
}

void SimulationState::fire_update_velocity() {
    const size_t N = velocities_.size();
    
    // Compute power: P = F · v
    double power = 0.0;
    for (size_t i = 0; i < N; ++i) {
        power += forces_[i] * velocities_[i];
    }
    
    // Velocity mixing: v = (1 - alpha)*v + alpha*|v|*F/|F|
    double v_norm = 0.0, f_norm = 0.0;
    for (size_t i = 0; i < N; ++i) {
        v_norm += velocities_[i] * velocities_[i];
        f_norm += forces_[i] * forces_[i];
    }
    v_norm = std::sqrt(v_norm);
    f_norm = std::sqrt(f_norm);
    
    if (f_norm > 1e-12) {
        double scale = fire_alpha_ * v_norm / f_norm;
        for (size_t i = 0; i < N; ++i) {
            velocities_[i] = (1.0 - fire_alpha_) * velocities_[i] + scale * forces_[i];
        }
    }
    
    // Store power state for statistics
    stats_.fire_n_positive = fire_n_positive_;
}

void SimulationState::fire_update_timestep() {
    const size_t N = velocities_.size();
    
    // Compute power
    double power = 0.0;
    for (size_t i = 0; i < N; ++i) {
        power += forces_[i] * velocities_[i];
    }
    
    if (power > 0.0) {
        fire_n_positive_++;
        if (fire_n_positive_ > 5) {  // N_min = 5
            fire_dt_ = std::min(fire_dt_ * 1.1, params_.dt_max);
            fire_alpha_ *= 0.99;
        }
    } else {
        // Reset on uphill
        std::fill(velocities_.begin(), velocities_.end(), 0.0);
        fire_dt_ *= 0.5;
        fire_alpha_ = params_.alpha_init;
        fire_n_positive_ = 0;
    }
    
    stats_.fire_dt = fire_dt_;
    stats_.fire_alpha = fire_alpha_;
}

// ============================================================================
// MD Implementation
// ============================================================================

void SimulationState::md_velocity_verlet_step() {
    const size_t N = coords_.size();
    const double dt = params_.timestep;
    
    // v(t + dt/2) = v(t) + F(t) * dt/2
    for (size_t i = 0; i < N; ++i) {
        velocities_[i] += forces_[i] * dt * 0.5;
    }
    
    // x(t + dt) = x(t) + v(t + dt/2) * dt
    for (size_t i = 0; i < N; ++i) {
        coords_[i] += velocities_[i] * dt;
    }
    
    // Apply PBC if enabled
    if (params_.use_pbc) {
        const size_t N_atoms = N / 3;
        for (size_t i = 0; i < N_atoms; ++i) {
            for (int d = 0; d < 3; ++d) {
                double& x = coords_[3*i + d];
                double box = params_.box_size[d];
                x = x - box * std::floor(x / box);
            }
        }
    }
    
    // Evaluate new forces
    evaluate_forces();
    
    // v(t + dt) = v(t + dt/2) + F(t + dt) * dt/2
    for (size_t i = 0; i < N; ++i) {
        velocities_[i] += forces_[i] * dt * 0.5;
    }
}

void SimulationState::md_apply_thermostat() {
    // Simple Langevin thermostat (velocity rescaling + friction)
    const double gamma = params_.damping;
    const double dt = params_.timestep;
    const double target_T = params_.temperature;
    
    // Compute current kinetic energy and temperature
    double KE = 0.0;
    for (size_t i = 0; i < velocities_.size(); ++i) {
        KE += 0.5 * velocities_[i] * velocities_[i];
    }
    
    const size_t N_atoms = coords_.size() / 3;
    const double kb = 0.001987;  // kcal/mol/K
    const double current_T = (2.0 * KE) / (3.0 * N_atoms * kb);
    
    // Velocity rescaling factor
    if (current_T > 1e-6) {
        double scale = std::sqrt(target_T / current_T);
        // Mix with damping
        scale = 1.0 - gamma * dt + gamma * dt * scale;
        
        for (auto& v : velocities_) {
            v *= scale;
        }
    }
}

// ============================================================================
// Energy & Statistics
// ============================================================================

void SimulationState::evaluate_forces() {
    if (!energy_model_) {
        return;
    }
    
    std::vector<double> gradient(coords_.size());
    double energy = energy_model_->evaluate_energy_gradient(coords_, gradient);
    
    // Convert gradient to forces (F = -grad)
    for (size_t i = 0; i < gradient.size(); ++i) {
        forces_[i] = -gradient[i];
    }
    
    stats_.potential_energy = energy;
}

void SimulationState::compute_statistics() {
    if (!energy_model_) {
        return;
    }
    
    // Get detailed energy breakdown
    EnergyResult result = energy_model_->evaluate_detailed(coords_);
    
    stats_.total_energy = result.total_energy;
    stats_.bond_energy = result.bond_energy;
    stats_.angle_energy = result.angle_energy;
    stats_.torsion_energy = result.torsion_energy;
    stats_.nonbonded_energy = result.nonbonded_energy;
    stats_.potential_energy = result.total_energy;
    
    // Kinetic energy
    stats_.kinetic_energy = 0.0;
    for (double v : velocities_) {
        stats_.kinetic_energy += 0.5 * v * v;
    }
    
    // Temperature (for MD)
    const size_t N_atoms = coords_.size() / 3;
    const double kb = 0.001987;  // kcal/mol/K
    if (N_atoms > 0) {
        stats_.temperature = (2.0 * stats_.kinetic_energy) / (3.0 * N_atoms * kb);
    }
    
    // Force metrics
    stats_.rms_force = 0.0;
    stats_.max_force = 0.0;
    for (double f : forces_) {
        stats_.rms_force += f * f;
        stats_.max_force = std::max(stats_.max_force, std::abs(f));
    }
    stats_.rms_force = std::sqrt(stats_.rms_force / forces_.size());
}

// ============================================================================
// Snapshot Generation
// ============================================================================

FrameSnapshot SimulationState::get_snapshot() const {
    FrameSnapshot snap;
    
    const size_t N_atoms = coords_.size() / 3;
    for (size_t i = 0; i < N_atoms; ++i) {
        snap.positions.push_back(Vec3(coords_[3*i], coords_[3*i+1], coords_[3*i+2]));
        snap.atomic_numbers.push_back(molecule_.atoms[i].Z);
    }
    
    for (const auto& bond : molecule_.bonds) {
        snap.bonds.push_back({bond.i, bond.j});
    }
    
    snap.iteration = stats_.iteration;
    snap.energy = stats_.total_energy;
    snap.rms_force = stats_.rms_force;
    snap.max_force = stats_.max_force;
    
    // Build status message
    char status[256];
    const char* mode_str = "IDLE";
    switch (mode_) {
        case SimMode::VSEPR: mode_str = "VSEPR"; break;
        case SimMode::OPTIMIZE: mode_str = "OPTIMIZE"; break;
        case SimMode::MD: mode_str = "MD"; break;
        case SimMode::CRYSTAL: mode_str = "CRYSTAL"; break;
        default: break;
    }
    
    if (mode_ == SimMode::MD) {
        snprintf(status, sizeof(status),
                 "%s | Iter %d | E=%.2f | T=%.1f K | t=%.3f ps",
                 mode_str, stats_.iteration, stats_.total_energy, 
                 stats_.temperature, md_time_);
    } else {
        snprintf(status, sizeof(status),
                 "%s | Iter %d | E=%.2f | RMS=%.4f | dt=%.3f",
                 mode_str, stats_.iteration, stats_.total_energy,
                 stats_.rms_force, stats_.fire_dt);
    }
    
    snap.status_message = status;
    
    return snap;
}

// ============================================================================
// Convergence & I/O
// ============================================================================

bool SimulationState::is_converged() const {
    return stats_.rms_force < params_.tol_rms_force && 
           stats_.max_force < params_.tol_max_force;
}

bool SimulationState::save_to_file(const std::string& filepath) const {
    // TODO: Implement JSON serialization
    std::cerr << "JSON saving not yet implemented\n";
    return false;
}

} // namespace vsepr
