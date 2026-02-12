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
#include <random>

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
    , rng_(std::random_device{}())
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
        std::cerr << "Failed to open file: " << filepath << "\n";
        return false;
    }
    
    // TODO: Implement JSON parsing using nlohmann/json
    std::cerr << "JSON loading not yet implemented\n";
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

// ============================================================================
// Path-Based Parameter System
// ============================================================================

// Helper to extract value from ParamValue variant
template<typename T>
static std::optional<T> get_as(const ParamValue& v) {
    if (auto* p = std::get_if<T>(&v)) return *p;
    // Try numeric conversions
    if constexpr (std::is_same_v<T, double>) {
        if (auto* p = std::get_if<int>(&v)) return static_cast<double>(*p);
    }
    if constexpr (std::is_same_v<T, int>) {
        if (auto* p = std::get_if<double>(&v)) return static_cast<int>(*p);
    }
    return std::nullopt;
}

void SimulationState::set_param(const std::string& path, const ParamValue& value) {
    // FIRE optimizer parameters
    if (path == "fire.dt_init") {
        if (auto v = get_as<double>(value)) params_.dt_init = *v;
    }
    else if (path == "fire.dt_max") {
        if (auto v = get_as<double>(value)) params_.dt_max = *v;
    }
    else if (path == "fire.alpha_init" || path == "fire.alpha") {
        if (auto v = get_as<double>(value)) params_.alpha_init = *v;
    }
    else if (path == "fire.max_step") {
        if (auto v = get_as<double>(value)) params_.max_step = *v;
    }
    else if (path == "fire.tol_rms_force") {
        if (auto v = get_as<double>(value)) params_.tol_rms_force = *v;
    }
    else if (path == "fire.tol_max_force") {
        if (auto v = get_as<double>(value)) params_.tol_max_force = *v;
    }
    else if (path == "fire.max_iterations") {
        if (auto v = get_as<int>(value)) params_.max_iterations = *v;
    }
    
    // MD parameters
    else if (path == "md.temperature") {
        if (auto v = get_as<double>(value)) params_.temperature = *v;
    }
    else if (path == "md.timestep") {
        if (auto v = get_as<double>(value)) params_.timestep = *v;
    }
    else if (path == "md.damping") {
        if (auto v = get_as<double>(value)) params_.damping = *v;
    }
    
    // PBC parameters
    else if (path == "pbc.enabled") {
        if (auto v = get_as<bool>(value)) {
            params_.use_pbc = *v;
            if (*v) {
                box_.set_dimensions(Vec3(params_.box_size[0], params_.box_size[1], params_.box_size[2]));
            } else {
                box_.set_dimensions(Vec3(0, 0, 0));
            }
        }
    }
    else if (path == "pbc.box" || path == "pbc.box.size") {
        // Set all three dimensions to same value
        if (auto v = get_as<double>(value)) {
            params_.box_size[0] = params_.box_size[1] = params_.box_size[2] = *v;
            if (params_.use_pbc) {
                box_.set_dimensions(Vec3(*v, *v, *v));
            }
        }
    }
    else if (path == "pbc.box.x") {
        if (auto v = get_as<double>(value)) {
            params_.box_size[0] = *v;
            if (params_.use_pbc) {
                box_.set_dimensions(Vec3(params_.box_size[0], params_.box_size[1], params_.box_size[2]));
            }
        }
    }
    else if (path == "pbc.box.y") {
        if (auto v = get_as<double>(value)) {
            params_.box_size[1] = *v;
            if (params_.use_pbc) {
                box_.set_dimensions(Vec3(params_.box_size[0], params_.box_size[1], params_.box_size[2]));
            }
        }
    }
    else if (path == "pbc.box.z") {
        if (auto v = get_as<double>(value)) {
            params_.box_size[2] = *v;
            if (params_.use_pbc) {
                box_.set_dimensions(Vec3(params_.box_size[0], params_.box_size[1], params_.box_size[2]));
            }
        }
    }
    
    // LJ / nonbonded parameters
    else if (path == "lj.epsilon") {
        if (auto v = get_as<double>(value)) {
            // TODO: Update energy model
            std::cout << "[SimState] Set lj.epsilon = " << *v << "\n";
        }
    }
    else if (path == "lj.sigma") {
        if (auto v = get_as<double>(value)) {
            // TODO: Update energy model
            std::cout << "[SimState] Set lj.sigma = " << *v << "\n";
        }
    }
    else if (path == "lj.cutoff") {
        if (auto v = get_as<double>(value)) {
            // TODO: Update energy model
            std::cout << "[SimState] Set lj.cutoff = " << *v << "\n";
        }
    }
    
    // Energy term enables
    else if (path == "energy.use_angles") {
        if (auto v = get_as<bool>(value)) params_.use_angles = *v;
    }
    else if (path == "energy.use_torsions") {
        if (auto v = get_as<bool>(value)) params_.use_torsions = *v;
    }
    else if (path == "energy.use_nonbonded") {
        if (auto v = get_as<bool>(value)) params_.use_nonbonded = *v;
    }
    else if (path == "energy.use_vsepr") {
        if (auto v = get_as<bool>(value)) params_.use_vsepr = *v;
    }
    
    // Unknown path
    else {
        std::cerr << "[SimState] Unknown parameter path: " << path << "\n";
    }
}

std::optional<ParamValue> SimulationState::get_param(const std::string& path) const {
    // FIRE parameters
    if (path == "fire.dt_init") return params_.dt_init;
    if (path == "fire.dt_max") return params_.dt_max;
    if (path == "fire.alpha_init") return params_.alpha_init;
    if (path == "fire.max_step") return params_.max_step;
    if (path == "fire.tol_rms_force") return params_.tol_rms_force;
    if (path == "fire.tol_max_force") return params_.tol_max_force;
    if (path == "fire.max_iterations") return params_.max_iterations;
    
    // MD parameters
    if (path == "md.temperature") return params_.temperature;
    if (path == "md.timestep") return params_.timestep;
    if (path == "md.damping") return params_.damping;
    
    // PBC parameters
    if (path == "pbc.enabled") return params_.use_pbc;
    if (path == "pbc.box.x") return params_.box_size[0];
    if (path == "pbc.box.y") return params_.box_size[1];
    if (path == "pbc.box.z") return params_.box_size[2];
    
    // Energy enables
    if (path == "energy.use_angles") return params_.use_angles;
    if (path == "energy.use_torsions") return params_.use_torsions;
    if (path == "energy.use_nonbonded") return params_.use_nonbonded;
    if (path == "energy.use_vsepr") return params_.use_vsepr;
    
    return std::nullopt;
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
    
    // Ensure masses are initialized (default to unit mass)
    if (masses_.empty()) {
        masses_.resize(N, 1.0);
    }
    
    // v(t + dt/2) = v(t) + F(t)/m * dt/2
    for (size_t i = 0; i < N; ++i) {
        velocities_[i] += (forces_[i] / masses_[i]) * dt * 0.5;
    }
    
    // x(t + dt) = x(t) + v(t + dt/2) * dt
    for (size_t i = 0; i < N; ++i) {
        coords_[i] += velocities_[i] * dt;
    }
    
    // Apply PBC if enabled
    if (params_.use_pbc && box_.enabled()) {
        box_.wrap_coords(coords_);
    }
    
    // Evaluate new forces
    evaluate_forces();
    
    // v(t + dt) = v(t + dt/2) + F(t + dt)/m * dt/2
    for (size_t i = 0; i < N; ++i) {
        velocities_[i] += (forces_[i] / masses_[i]) * dt * 0.5;
    }
}

void SimulationState::md_apply_thermostat() {
    if (params_.thermostat == "none") {
        return;  // NVE dynamics
    }
    
    const double dt = params_.timestep;
    const double target_T = params_.temperature;
    
    // Ensure masses are initialized
    if (masses_.empty()) {
        masses_.resize(coords_.size(), 1.0);  // Default unit mass
    }
    
    if (params_.thermostat == "berendsen") {
        // Berendsen weak coupling
        double KE = 0.0;
        for (size_t i = 0; i < velocities_.size(); ++i) {
            KE += 0.5 * masses_[i] * velocities_[i] * velocities_[i];
        }
        
        const size_t N_atoms = coords_.size() / 3;
        const double kb = 0.001987;  // kcal/mol/K
        const double current_T = (2.0 * KE) / (3.0 * N_atoms * kb);
        
        if (current_T > 1e-6) {
            double tau = params_.tau_thermostat;
            double lambda = std::sqrt(1.0 + (dt / tau) * (target_T / current_T - 1.0));
            
            for (auto& v : velocities_) {
                v *= lambda;
            }
        }
    }
    else if (params_.thermostat == "langevin") {
        // Simple velocity rescaling with friction (legacy compatibility)
        const double gamma = params_.damping;
        
        double KE = 0.0;
        for (size_t i = 0; i < velocities_.size(); ++i) {
            KE += 0.5 * masses_[i] * velocities_[i] * velocities_[i];
        }
        
        const size_t N_atoms = coords_.size() / 3;
        const double kb = 0.001987;  // kcal/mol/K
        const double current_T = (2.0 * KE) / (3.0 * N_atoms * kb);
        
        if (current_T > 1e-6) {
            double scale = std::sqrt(target_T / current_T);
            scale = 1.0 - gamma * dt + gamma * dt * scale;
            
            for (auto& v : velocities_) {
                v *= scale;
            }
        }
    }
    // Note: For proper Langevin, Nosé-Hoover, or v-rescale,
    // use the integrators from int/integrators.hpp
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
    
    // Add PBC information to stats map
    snap.stats["pbc_enabled"] = params_.use_pbc ? 1.0 : 0.0;
    snap.stats["box_x"] = box_.L.x;
    snap.stats["box_y"] = box_.L.y;
    snap.stats["box_z"] = box_.L.z;
    
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
    (void)filepath;  // Suppress unused parameter warning
    std::cerr << "JSON saving not yet implemented\n";
    return false;
}

// ============================================================================
// Particle Spawning
// ============================================================================

void SimulationState::spawn_particles(const CmdSpawn& cmd) {
    // Set up box dimensions
    params_.box_size[0] = cmd.box_x;
    params_.box_size[1] = cmd.box_y;
    params_.box_size[2] = cmd.box_z;
    params_.use_pbc = true;
    
    box_.set_dimensions(Vec3(cmd.box_x, cmd.box_y, cmd.box_z));
    
    // Clear existing molecule
    molecule_ = Molecule();
    coords_.clear();
    forces_.clear();
    velocities_.clear();
    
    // Determine atomic number from species
    int atomic_number = 18;  // Default: Argon
    if (cmd.species == "Ar") atomic_number = 18;
    else if (cmd.species == "Ne") atomic_number = 10;
    else if (cmd.species == "He") atomic_number = 2;
    else if (cmd.species == "Kr") atomic_number = 36;
    else if (cmd.species == "Xe") atomic_number = 54;
    else if (cmd.species == "Cu") atomic_number = 29;
    else if (cmd.species == "Au") atomic_number = 79;
    else if (cmd.species == "Ni") atomic_number = 28;
    
    // Initialize RNG with seed
    std::mt19937 rng(cmd.seed == 0 ? std::random_device{}() : cmd.seed);
    
    int n_particles = 0;
    
    if (cmd.type == SpawnType::GAS) {
        // Random gas placement with minimum distance check
        std::uniform_real_distribution<double> dist_x(0.0, cmd.box_x);
        std::uniform_real_distribution<double> dist_y(0.0, cmd.box_y);
        std::uniform_real_distribution<double> dist_z(0.0, cmd.box_z);
        
        const double min_dist = 2.0;  // Minimum distance between particles (Å)
        const int max_attempts = 1000;
        
        for (int i = 0; i < cmd.n_particles; ++i) {
            double x, y, z;
            bool valid = false;
            
            for (int attempt = 0; attempt < max_attempts && !valid; ++attempt) {
                x = dist_x(rng);
                y = dist_y(rng);
                z = dist_z(rng);
                
                // Check distance from existing particles
                valid = true;
                for (size_t j = 0; j < coords_.size() / 3; ++j) {
                    double dx = x - coords_[3*j];
                    double dy = y - coords_[3*j+1];
                    double dz = z - coords_[3*j+2];
                    
                    // Apply minimum image convention for PBC
                    if (dx > cmd.box_x/2) dx -= cmd.box_x;
                    if (dx < -cmd.box_x/2) dx += cmd.box_x;
                    if (dy > cmd.box_y/2) dy -= cmd.box_y;
                    if (dy < -cmd.box_y/2) dy += cmd.box_y;
                    if (dz > cmd.box_z/2) dz -= cmd.box_z;
                    if (dz < -cmd.box_z/2) dz += cmd.box_z;
                    
                    double r2 = dx*dx + dy*dy + dz*dz;
                    if (r2 < min_dist * min_dist) {
                        valid = false;
                        break;
                    }
                }
            }
            
            if (valid) {
                molecule_.add_atom(atomic_number, x, y, z);
                coords_.push_back(x);
                coords_.push_back(y);
                coords_.push_back(z);
                n_particles++;
            }
        }
    }
    else if (cmd.type == SpawnType::CRYSTAL || cmd.type == SpawnType::LATTICE) {
        // Crystal lattice generation
        double a = cmd.lattice_constant;
        int nx = cmd.nx, ny = cmd.ny, nz = cmd.nz;
        
        // Basis vectors for unit cell
        std::vector<std::array<double, 3>> basis;
        
        if (cmd.lattice == LatticeType::SC) {
            // Simple cubic: 1 atom per unit cell
            basis = {{0.0, 0.0, 0.0}};
        }
        else if (cmd.lattice == LatticeType::BCC) {
            // Body-centered cubic: 2 atoms per unit cell
            basis = {{0.0, 0.0, 0.0}, {0.5*a, 0.5*a, 0.5*a}};
        }
        else if (cmd.lattice == LatticeType::FCC) {
            // Face-centered cubic: 4 atoms per unit cell
            basis = {
                {0.0, 0.0, 0.0},
                {0.5*a, 0.5*a, 0.0},
                {0.5*a, 0.0, 0.5*a},
                {0.0, 0.5*a, 0.5*a}
            };
        }
        
        // Generate lattice points
        for (int ix = 0; ix < nx; ++ix) {
            for (int iy = 0; iy < ny; ++iy) {
                for (int iz = 0; iz < nz; ++iz) {
                    for (const auto& b : basis) {
                        double x = ix * a + b[0];
                        double y = iy * a + b[1];
                        double z = iz * a + b[2];
                        
                        molecule_.add_atom(atomic_number, x, y, z);
                        coords_.push_back(x);
                        coords_.push_back(y);
                        coords_.push_back(z);
                        n_particles++;
                    }
                }
            }
        }
        
        // Update box size to fit crystal
        params_.box_size[0] = nx * a;
        params_.box_size[1] = ny * a;
        params_.box_size[2] = nz * a;
        box_.set_dimensions(Vec3(params_.box_size[0], params_.box_size[1], params_.box_size[2]));
    }
    
    // Initialize forces and velocities
    forces_.resize(coords_.size(), 0.0);
    velocities_.resize(coords_.size(), 0.0);
    masses_.resize(coords_.size(), 39.948);  // Argon mass - TODO: get from atom type
    
    // Assign initial velocities for MD if temperature > 0
    if (params_.temperature > 0 && mode_ == SimMode::MD) {
        md_initialize_velocities();
    }
    
    // Reset statistics
    stats_ = SimStats{};
    stats_.iteration = 0;
    
    std::cout << "[SimState] Spawned " << n_particles << " particles of " 
              << cmd.species << " in box " << cmd.box_x << "x" << cmd.box_y << "x" << cmd.box_z << "\n";
}

// ============================================================================
// Maxwell-Boltzmann Velocity Initialization
// ============================================================================

void SimulationState::md_initialize_velocities() {
    const size_t N_atoms = coords_.size() / 3;
    const double kb = 0.001987;  // kcal/mol/K
    const double T = params_.temperature;
    
    // Ensure masses are initialized
    if (masses_.empty() || masses_.size() != coords_.size()) {
        masses_.resize(coords_.size(), 1.0);  // Default unit mass
    }
    
    std::normal_distribution<double> normal(0.0, 1.0);
    
    // Sample velocities from Maxwell-Boltzmann distribution
    // For each component: v ~ N(0, sqrt(kb*T/m))
    for (size_t i = 0; i < velocities_.size(); ++i) {
        double sigma = std::sqrt(kb * T / masses_[i]);
        velocities_[i] = normal(rng_) * sigma;
    }
    
    // Remove center of mass motion
    Vec3 com_vel(0, 0, 0);
    double total_mass = 0.0;
    
    for (size_t i = 0; i < N_atoms; ++i) {
        double m = masses_[3*i];  // All 3 components have same mass
        com_vel.x += m * velocities_[3*i + 0];
        com_vel.y += m * velocities_[3*i + 1];
        com_vel.z += m * velocities_[3*i + 2];
        total_mass += m;
    }
    
    com_vel.x /= total_mass;
    com_vel.y /= total_mass;
    com_vel.z /= total_mass;
    
    for (size_t i = 0; i < N_atoms; ++i) {
        velocities_[3*i + 0] -= com_vel.x;
        velocities_[3*i + 1] -= com_vel.y;
        velocities_[3*i + 2] -= com_vel.z;
    }
    
    // Optional: Rescale to exact target temperature
    double KE = 0.0;
    for (size_t i = 0; i < velocities_.size(); ++i) {
        KE += 0.5 * masses_[i] * velocities_[i] * velocities_[i];
    }
    
    double T_actual = (2.0 * KE) / (3.0 * N_atoms * kb);
    double scale = std::sqrt(T / T_actual);
    
    for (auto& v : velocities_) {
        v *= scale;
    }
}

} // namespace vsepr
