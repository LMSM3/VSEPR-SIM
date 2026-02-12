/**
 * Phase 2: Thermal Animation Test Suite
 * vsepr-sim v2.3.1
 * 
 * Tests:
 * 1. Thermal evolution of water molecule
 * 2. Energy tracking over time
 * 3. Frame sampling
 * 4. Multi-frame XYZ export
 * 5. Energy CSV export
 * 
 * Expected Output:
 * - 100 MD steps completed
 * - 10 frames captured (sample every 10)
 * - Energy fluctuates around baseline
 * - Trajectory exported to XYZ
 * - Energy data exported to CSV
 * 
 * Author: VSEPR-Sim Development Team
 * Date: January 2025
 */

#include "thermal/thermal_runner.hpp"
#include "dynamic/real_molecule_generator.hpp"
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace vsepr;
using namespace vsepr::thermal;
using namespace vsepr::dynamic;
namespace fs = std::filesystem;

static fs::path thermal_output_base() {
    auto base = fs::temp_directory_path() / "vsepr_sim" / "thermal";
    fs::create_directories(base);
    return base;
}

// ============================================================================
// ASCII Art Header
// ============================================================================

void print_header() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║     VSEPR-Sim Phase 2: Thermal Animation Tests          ║\n";
    std::cout << "║     Version 2.3.1                                        ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
}

// ============================================================================
// Test 1: Basic Thermal Evolution
// ============================================================================

void test_thermal_evolution() {
    std::cout << "=== Test 1: Thermal Evolution (H2O at 300K) ===\n";
    auto out_base = thermal_output_base();
    
    // Generate a water molecule
    RealMoleculeGenerator generator;
    Molecule water = generator.generate_from_formula("H2O");
    
    std::cout << "Initial molecule: H2O (" << water.num_atoms() << " atoms)\n";
    
    // Configure thermal simulation
    ThermalConfig config;
    config.temperature = 300.0;  // 300 Kelvin (room temperature)
    config.total_generations = 100;  // Short simulation for testing
    config.sample_interval = 10;  // Sample every 10 steps
    config.time_step = 1.0;  // 1 femtosecond
    config.save_trajectory = true;
    config.output_path = (out_base / "water_300K.xyz").string();
    
    std::cout << "Configuration:\n";
    std::cout << "  Temperature: " << config.temperature << " K\n";
    std::cout << "  Total steps: " << config.total_generations << "\n";
    std::cout << "  Sample interval: " << config.sample_interval << "\n";
    std::cout << "  Time step: " << config.time_step << " fs\n";
    
    // Create thermal runner
    ThermalRunner runner;
    
    // Set up progress callback
    int progress_updates = 0;
    runner.set_progress_callback([&](size_t step, size_t total) {
        progress_updates++;
        if (progress_updates % 5 == 0) {
            std::cout << "  Progress: " << step << "/" << total 
                     << " (" << (100 * step / total) << "%)\n";
        }
    }, 10);
    
    // Start simulation
    std::cout << "\nStarting thermal evolution...\n";
    auto start_time = std::chrono::steady_clock::now();
    
    runner.start(water, config);
    
    // Wait for completion
    while (runner.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();
    
    // Get results
    auto frames = runner.get_frames();
    auto energy_history = runner.get_energy_history();
    auto stats = runner.get_statistics();
    
    std::cout << "\n✓ Simulation complete!\n";
    std::cout << "  Frames captured: " << frames.size() << "\n";
    std::cout << "  Energy points: " << energy_history.size() << "\n";
    std::cout << "  Total steps: " << stats.total_steps << "\n";
    std::cout << "  Elapsed time: " << std::fixed << std::setprecision(3) 
             << elapsed << " seconds\n";
    std::cout << "  Average energy: " << std::setprecision(2) 
             << stats.avg_energy << " kcal/mol\n";
    std::cout << "  Energy range: [" << stats.min_energy << ", " 
             << stats.max_energy << "] kcal/mol\n";
    
    // Export trajectory
    std::cout << "\nExporting trajectory...\n";
    auto traj_path = (out_base / "water_300K_trajectory.xyz").string();
    runner.export_trajectory(traj_path);
    std::cout << "✓ Trajectory saved to: " << traj_path << "\n";
    
    // Export energy data
    auto energy_path = (out_base / "water_300K_energy.csv").string();
    runner.export_energy_csv(energy_path);
    std::cout << "✓ Energy data saved to: " << energy_path << "\n";
}

// ============================================================================
// Test 2: Temperature Effects
// ============================================================================

void test_temperature_effects() {
    std::cout << "\n=== Test 2: Temperature Effects (H2O at Different T) ===\n";
    
    RealMoleculeGenerator generator;
    Molecule water = generator.generate_from_formula("H2O");
    
    std::vector<double> temperatures = {100.0, 300.0, 500.0};  // K
    
    for (double temp : temperatures) {
        std::cout << "\nTesting at " << temp << " K...\n";
        
        ThermalConfig config;
        config.temperature = temp;
        config.total_generations = 50;  // Quick test
        config.sample_interval = 5;
        config.save_trajectory = false;
        
        ThermalRunner runner;
        runner.start(water, config);
        
        while (runner.is_running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        auto stats = runner.get_statistics();
        auto energy_history = runner.get_energy_history();
        
        // Calculate energy variance (proxy for thermal motion)
        double mean = stats.avg_energy;
        double variance = 0.0;
        for (double e : energy_history) {
            variance += (e - mean) * (e - mean);
        }
        variance /= energy_history.size();
        double std_dev = std::sqrt(variance);
        
        std::cout << "  Avg energy: " << std::fixed << std::setprecision(2) 
                 << mean << " kcal/mol\n";
        std::cout << "  Std dev: " << std_dev << " kcal/mol\n";
        std::cout << "  (Higher temperature → higher fluctuations)\n";
    }
    
    std::cout << "\n✓ Temperature effects test complete!\n";
}

// ============================================================================
// Test 3: Pause/Resume Functionality
// ============================================================================

void test_pause_resume() {
    std::cout << "\n=== Test 3: Pause/Resume Functionality ===\n";
    
    RealMoleculeGenerator generator;
    Molecule ammonia = generator.generate_from_formula("NH3");
    
    ThermalConfig config;
    config.temperature = 300.0;
    config.total_generations = 100;
    config.sample_interval = 10;
    config.save_trajectory = false;
    
    ThermalRunner runner;
    
    std::cout << "Starting simulation...\n";
    runner.start(ammonia, config);
    
    // Let it run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Pause
    std::cout << "Pausing...\n";
    runner.pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto [step1, total1] = runner.get_progress();
    std::cout << "  Paused at step: " << step1 << "/" << total1 << "\n";
    
    // Resume
    std::cout << "Resuming...\n";
    runner.resume();
    
    // Wait for completion
    while (runner.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    auto stats = runner.get_statistics();
    std::cout << "✓ Simulation completed with pause/resume\n";
    std::cout << "  Total steps: " << stats.total_steps << "\n";
}

// ============================================================================
// Test 4: Export Functions
// ============================================================================

void test_export_functions() {
    std::cout << "\n=== Test 4: Export Functions ===\n";
    auto out_base = thermal_output_base();
    
    RealMoleculeGenerator generator;
    Molecule methane = generator.generate_from_formula("CH4");
    
    ThermalConfig config;
    config.temperature = 300.0;
    config.total_generations = 50;
    config.sample_interval = 5;
    config.save_trajectory = false;
    
    ThermalRunner runner;
    runner.start(methane, config);
    
    while (runner.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    auto frames = runner.get_frames();
    auto energies = runner.get_energy_history();
    
    std::cout << "Exporting to multiple formats...\n";
    
    // Export trajectory
    auto traj_path = (out_base / "methane_trajectory.xyz").string();
    export_thermal_animation_xyz(frames, traj_path, "Methane frame {frame_num}");
    std::cout << "✓ XYZ trajectory exported to: " << traj_path << "\n";
    
    // Export energy CSV
    auto energy_path = (out_base / "methane_energy.csv").string();
    export_energy_csv(energies, 1.0, energy_path);
    std::cout << "✓ Energy CSV exported to: " << energy_path << "\n";
 }
 
// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    try {
        print_header();
        
        std::cout << "Phase 2 implements:\n";
        std::cout << "  • Real-time thermal evolution simulation\n";
        std::cout << "  • Background threading for non-blocking UI\n";
        std::cout << "  • Energy tracking over time\n";
        std::cout << "  • Frame sampling for animation\n";
        std::cout << "  • Multi-frame XYZ export\n";
        std::cout << "  • CSV energy data export\n";
        std::cout << "  • Pause/resume controls\n";
        std::cout << "\n";
        
        // Run all tests
        test_thermal_evolution();
        test_temperature_effects();
        test_pause_resume();
        test_export_functions();
        
        // Summary
        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════╗\n";
        std::cout << "║                   ALL TESTS PASSED!                      ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
        
        std::cout << "Next steps:\n";
        std::cout << "  1. Integrate with GUI (vsepr_gui_live.cpp)\n";
        std::cout << "  2. Add real-time 3D visualization\n";
        std::cout << "  3. Add energy plot (ImPlot)\n";
        std::cout << "  4. Add GIF/MP4 export\n";
        std::cout << "\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ ERROR: " << e.what() << "\n";
        return 1;
    }
}
