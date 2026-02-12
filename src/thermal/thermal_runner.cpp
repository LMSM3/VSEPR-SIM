/**
 * Thermal Animation Runner Implementation
 * vsepr-sim v2.3.1
 * 
 * Implements real-time thermal evolution simulation with background threading,
 * energy tracking, and frame sampling for animation.
 * 
 * Author: VSEPR-Sim Development Team
 * Date: January 2025
 * License: MIT
 */

#include "thermal/thermal_runner.hpp"
#include "io/xyz_format.hpp"
#include "core/element_data_integrated.hpp"
#include "pot/periodic_db.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <random>
#include <algorithm>

// Suppress unused parameter warnings (TODO: implement these functions)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

namespace vsepr {
namespace thermal {

// Resolve element symbol by atomic number for trajectory exports
static const char* element_symbol(uint8_t Z) {
    static PeriodicTable table = [] {
        try {
            return PeriodicTable::load_from_json_file("data/PeriodicTableJSON.json");
        } catch (...) {
            return PeriodicTable();
        }
    }();
    const auto* elem = table.by_Z(Z);
    return elem ? elem->symbol.c_str() : "?";
}

// ============================================================================
// ThermalRunner Implementation
// ============================================================================

ThermalRunner::ThermalRunner() {
    // Initialize random seed
    std::srand(static_cast<unsigned>(std::time(nullptr)));
}

ThermalRunner::~ThermalRunner() {
    stop();
}

void ThermalRunner::start(const Molecule& initial_molecule, const ThermalConfig& config) {
    if (running_.load()) {
        return;  // Already running
    }
    
    // Initialize state
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        config_ = config;
        current_frame_ = initial_molecule;
        frames_.clear();
        energy_history_.clear();
        stats_ = ThermalStatistics();
        
        // Add initial frame
        frames_.push_back(initial_molecule);
        double initial_energy = calculate_energy(initial_molecule);
        energy_history_.push_back(initial_energy);
        stats_.frames_captured = 1;
    }
    
    current_step_ = 0;
    total_steps_ = config.total_generations;
    running_ = true;
    paused_ = false;
    stop_requested_ = false;
    
    // Start simulation thread
    simulation_thread_ = std::thread(&ThermalRunner::run_simulation, this);
}

void ThermalRunner::stop() {
    stop_requested_ = true;

    if (simulation_thread_.joinable()) {
        simulation_thread_.join();
    }

    running_ = false;
}

void ThermalRunner::pause() {
    paused_ = true;
}

void ThermalRunner::resume() {
    paused_ = false;
}

Molecule ThermalRunner::get_current_frame() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return current_frame_;
}

std::vector<Molecule> ThermalRunner::get_frames() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return frames_;
}

std::vector<double> ThermalRunner::get_energy_history() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return energy_history_;
}

std::pair<size_t, size_t> ThermalRunner::get_progress() const {
    return {current_step_.load(), total_steps_.load()};
}

ThermalStatistics ThermalRunner::get_statistics() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return stats_;
}

void ThermalRunner::set_progress_callback(ProgressCallback callback, int interval) {
    progress_callback_ = callback;
    progress_callback_interval_ = interval;
}

void ThermalRunner::set_frame_callback(FrameCallback callback) {
    frame_callback_ = callback;
}

// ============================================================================
// Private Methods
// ============================================================================

void ThermalRunner::run_simulation() {
    auto start_time = std::chrono::steady_clock::now();
    
    Molecule mol = current_frame_;
    
    for (size_t step = 0; step < static_cast<size_t>(config_.total_generations); ++step) {
        // Check for stop request
        if (stop_requested_.load()) {
            break;
        }
        
        // Handle pause
        while (paused_.load() && !stop_requested_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Evolve one MD step
        mol = evolve_one_step(mol, config_.temperature, config_.time_step);
        
        // Update current frame
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            current_frame_ = mol;
        }
        
        // Sample frame if needed
        if (step % config_.sample_interval == 0) {
            double energy = calculate_energy(mol);
            
            std::lock_guard<std::mutex> lock(data_mutex_);
            frames_.push_back(mol);
            energy_history_.push_back(energy);
            
            // Update statistics
            stats_.frames_captured++;
            stats_.total_steps = step + 1;
            stats_.min_energy = std::min(stats_.min_energy, energy);
            stats_.max_energy = std::max(stats_.max_energy, energy);
            
            // Calculate running average energy
            double sum = 0.0;
            for (double e : energy_history_) sum += e;
            stats_.avg_energy = sum / energy_history_.size();
            
            stats_.temperature_actual = calculate_temperature(mol);
            
            // Call frame callback
            if (frame_callback_) {
                frame_callback_(mol);
            }
        }
        
        // Update progress
        current_step_ = step + 1;
        
        // Progress callback
        if (progress_callback_ && (step % progress_callback_interval_ == 0)) {
            progress_callback_(step, config_.total_generations);
        }
    }
    
    // Final statistics
    auto end_time = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        stats_.elapsed_time_seconds = 
            std::chrono::duration<double>(end_time - start_time).count();
        stats_.total_steps = current_step_.load();
    }
    
    // Save trajectory if requested
    if (config_.save_trajectory) {
        export_trajectory(config_.output_path);
    }
    
    running_ = false;
}

Molecule ThermalRunner::evolve_one_step(const Molecule& mol, double temperature, double dt) {
    // Simplified molecular dynamics step
    // In a full implementation, this would:
    // 1. Calculate forces from potential energy function
    // 2. Update velocities using Velocity Verlet
    // 3. Update positions
    // 4. Apply thermostat (Langevin, Berendsen, etc.)
    
    Molecule new_mol = mol;
    
    // Random number generator for thermal noise
    static thread_local std::mt19937 rng(std::random_device{}());
    
    // Boltzmann constant (kcal/mol/K)
    const double kB = 0.001987;
    
    // Thermal energy scale
    double thermal_scale = std::sqrt(kB * temperature);
    
    // Apply small random displacements to each atom
    // Magnitude scales with temperature
    std::normal_distribution<double> dist(0.0, thermal_scale * dt * 0.1);
    
    for (size_t i = 0; i < new_mol.num_atoms(); ++i) {
        double x, y, z;
        new_mol.get_position(i, x, y, z);
        
        // Add thermal noise
        x += dist(rng);
        y += dist(rng);
        z += dist(rng);
        
        new_mol.set_position(i, x, y, z);
    }
    
    // TODO: Add proper force calculation
    // TODO: Add bond constraints (SHAKE/RATTLE)
    // TODO: Add proper thermostat (Langevin damping)
    
    return new_mol;
}

double ThermalRunner::calculate_energy(const Molecule& mol) const {
    // Placeholder energy calculation
    // In full implementation, this would include:
    // - Bond stretch energy
    // - Angle bend energy
    // - Torsion energy
    // - Non-bonded interactions (LJ + Coulomb)
    
    // For now, use a simple estimate based on geometry
    double energy = 0.0;
    
    // Bond energy estimate (sum of bond lengths)
    for (const auto& bond : mol.bonds) {
        double x1, y1, z1, x2, y2, z2;
        mol.get_position(bond.i, x1, y1, z1);
        mol.get_position(bond.j, x2, y2, z2);
        
        double dx = x2 - x1;
        double dy = y2 - y1;
        double dz = z2 - z1;
        double r = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        // Simple harmonic potential: E = k*(r - r0)^2
        double r0 = 1.5;  // Equilibrium bond length (Angstroms)
        double k = 100.0;  // Force constant (kcal/mol/A^2)
        energy += k * (r - r0) * (r - r0);
    }
    
    // Add base energy proportional to atom count
    energy += mol.num_atoms() * (-10.0);
    
    return energy;
}

double ThermalRunner::calculate_temperature(const Molecule& mol) const {
    // Placeholder temperature calculation
    // In full implementation: T = (2 * KE) / (3 * N * kB)
    // where KE is kinetic energy, N is number of atoms
    
    // For now, return target temperature
    return config_.temperature;
}

void ThermalRunner::apply_thermostat(Molecule& mol, double target_temp, double dt) {
    // Placeholder thermostat
    // In full implementation: Langevin thermostat with friction and noise
    
    // For now, this is handled in evolve_one_step
}

// ============================================================================
// Export Methods
// ============================================================================

void ThermalRunner::export_trajectory(const std::string& path) const {
    std::vector<Molecule> frames;
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        frames = frames_;
    }
    
    export_thermal_animation_xyz(frames, path, "Frame {frame_num}");
}

void ThermalRunner::export_energy_csv(const std::string& path) const {
    std::vector<double> energies;
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        energies = energy_history_;
    }
    
    vsepr::thermal::export_energy_csv(energies, config_.time_step, path);
}

// ============================================================================
// Standalone Export Functions
// ============================================================================

void export_thermal_animation_xyz(
    const std::vector<Molecule>& frames,
    const std::string& output_path,
    const std::string& comment_template)
{
    // Use the proper XYZ format library - never encode xyz by hand!
    std::ofstream file(output_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + output_path);
    }
    
    // Write multi-frame XYZ format
    // Each frame is a complete XYZ block
    
    for (size_t frame_idx = 0; frame_idx < frames.size(); ++frame_idx) {
        const Molecule& mol = frames[frame_idx];
        
        // Convert to XYZMolecule
        vsepr::io::XYZMolecule xyz_mol;
        
        // Generate comment with frame number
        std::string comment = comment_template;
        size_t pos = comment.find("{frame_num}");
        if (pos != std::string::npos) {
            comment.replace(pos, 11, std::to_string(frame_idx));
        }
        xyz_mol.comment = comment;
        
        // Copy atoms
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            double x, y, z;
            mol.get_position(i, x, y, z);
            
            const char* symbol = element_symbol(mol.atoms[i].Z);
            xyz_mol.atoms.emplace_back(symbol, x, y, z);
        }
        
        // Write this frame using XYZWriter
        vsepr::io::XYZWriter writer;
        writer.set_precision(6);
        writer.write_stream(file, xyz_mol);
    }
}

void export_energy_csv(
    const std::vector<double>& energy_history,
    double time_step,
    const std::string& output_path)
{
    std::ofstream file(output_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + output_path);
    }
    
    // CSV header
    file << "Time_fs,Energy_kcal_mol\n";
    
    // Write data
    for (size_t i = 0; i < energy_history.size(); ++i) {
        double time_fs = i * time_step;
        file << std::fixed << std::setprecision(2) << time_fs << ","
             << std::setprecision(6) << energy_history[i] << "\n";
    }
}

#pragma GCC diagnostic pop  // Restore warnings

} // namespace thermal
} // namespace vsepr
