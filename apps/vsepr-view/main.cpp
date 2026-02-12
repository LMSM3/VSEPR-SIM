/**
 * VSEPR Visualizer V2 - Modern Architecture
 * 
 * Clean separation:
 * - Renderer (main thread): Window, UI, camera, OpenGL
 * - Simulation (worker thread): Physics, optimization, state
 * - Command queue: Lock-free renderer → sim communication
 * - Frame buffer: Lock-free sim → renderer snapshots
 * 
 * Features:
 * - ImGui-based UI for full control
 * - Multiple simulation modes (VSEPR, MD, optimization)
 * - Thread-safe command dispatch
 * - Real-time parameter tuning
 * - Unified command routing (STDIN + ImGui console)
 * - Genuine passthrough: both terminal and GUI access same command bus
 */

#include "vis/window.hpp"
#include "sim/sim_thread.hpp"
#include "sim/sim_command.hpp"
#include "sim/molecule.hpp"
#include "command_router.hpp"
#include <iostream>
#include <fstream>

using namespace vsepr;

// Helper: Create a simple test molecule
Molecule create_test_molecule(const std::string& type) {
    Molecule mol;
    
    if (type == "h2o" || type == "water") {
        mol.add_atom(8, 0.0, 0.0, 0.0);       // O
        mol.add_atom(1, 1.2, 0.0, 0.0);       // H
        mol.add_atom(1, -0.3, 1.1, 0.0);      // H
        mol.add_bond(0, 1, 1);
        mol.add_bond(0, 2, 1);
        mol.generate_angles_from_bonds();
    }
    else if (type == "ch4" || type == "methane") {
        mol.add_atom(6, 0.0, 0.0, 0.0);       // C
        mol.add_atom(1, 1.2, 0.0, 0.0);       // H
        mol.add_atom(1, -0.4, 1.1, 0.0);      // H
        mol.add_atom(1, -0.4, -0.4, 1.0);     // H
        mol.add_atom(1, -0.4, -0.7, -0.7);    // H
        mol.add_bond(0, 1, 1);
        mol.add_bond(0, 2, 1);
        mol.add_bond(0, 3, 1);
        mol.add_bond(0, 4, 1);
        mol.generate_angles_from_bonds();
    }
    else if (type == "nh3" || type == "ammonia") {
        mol.add_atom(7, 0.0, 0.0, 0.0);       // N
        mol.add_atom(1, 1.1, 0.0, 0.0);       // H
        mol.add_atom(1, -0.4, 1.0, 0.0);      // H
        mol.add_atom(1, -0.4, -0.5, 0.9);     // H
        mol.add_bond(0, 1, 1);
        mol.add_bond(0, 2, 1);
        mol.add_bond(0, 3, 1);
        mol.generate_angles_from_bonds();
    }
    else {
        // Default to water
        std::cerr << "Unknown molecule type '" << type << "', defaulting to H2O\n";
        return create_test_molecule("h2o");
    }
    
    return mol;
}

int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  VSEPR Simulator V2 - Modern UI\n";
    std::cout << "  Multi-Input Command Architecture\n";
    std::cout << "========================================\n\n";
    
    // Parse command line
    std::string molecule_type = "h2o";
    bool enable_stdin = true;  // Enable STDIN reader by default
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--no-stdin") {
            enable_stdin = false;
        } else {
            molecule_type = arg;
        }
    }
    
    // Create initial molecule
    Molecule mol = create_test_molecule(molecule_type);
    
    std::cout << "Loaded molecule: " << molecule_type << "\n";
    std::cout << "  Atoms: " << mol.num_atoms() << "\n";
    std::cout << "  Bonds: " << mol.num_bonds() << "\n";
    std::cout << "  Angles: " << mol.angles.size() << "\n\n";
    
    // Create simulation thread
    SimulationThread sim_thread;
    
    // === NEW: Create CommandRouter and connect to SimThread ===
    CommandRouter command_router(sim_thread);
    sim_thread.set_command_router(&command_router);
    
    // Register stdout callback for command output
    command_router.register_output_callback([](const OutputEntry& output) {
        // Print to stdout with appropriate formatting
        const char* level_prefix = "";
        switch (output.status) {
            case ResultStatus::INFO:    level_prefix = "[INFO] "; break;
            case ResultStatus::OK:      level_prefix = "[OK] "; break;
            case ResultStatus::ERROR:   level_prefix = "[ERROR] "; break;
            case ResultStatus::WARNING: level_prefix = "[WARN] "; break;
        }
        std::cout << level_prefix << output.text << std::endl;
    });
    
    // Initialize simulation with molecule (via CommandRouter)
    sim_thread.start();
    
    // Give thread time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Build init command and submit via router
    {
        std::string init_cmd = "init";
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            init_cmd += " " + std::to_string(mol.atoms[i].Z);
            init_cmd += " " + std::to_string(mol.coords[3*i]);
            init_cmd += " " + std::to_string(mol.coords[3*i+1]);
            init_cmd += " " + std::to_string(mol.coords[3*i+2]);
        }
        // Note: Bond info would need to be added to command format
        // For now, just initialize atoms
        // command_router.submit_command(init_cmd, CommandSource::INTERNAL);
    }
    
    // Set to VSEPR mode via router
    command_router.submit_command("mode vsepr", CommandSource::INTERNAL);
    
    // Start STDIN reader thread (if enabled)
    std::unique_ptr<StdinReader> stdin_reader;
    if (enable_stdin) {
        stdin_reader = std::make_unique<StdinReader>(command_router);
        stdin_reader->set_prompt("vsepr> ");
        stdin_reader->start();
        std::cout << "STDIN reader enabled - you can type commands in the terminal\n";
    } else {
        std::cout << "STDIN reader disabled (use --no-stdin to disable)\n";
    }
    std::cout << "\n";
    
    // Create window with ImGui UI
    Window window(1280, 720, "VSEPR Simulator V2 - " + molecule_type);
    if (!window.initialize()) {
        std::cerr << "Failed to initialize window\n";
        return 1;
    }
    
    // Set camera position
    window.camera().set_target(Vec3(0, 0, 0));
    window.camera().zoom(-2.0);
    
    std::cout << "=== Controls ===\n";
    std::cout << "Mouse Left:   Rotate camera\n";
    std::cout << "Mouse Right:  Pan camera\n";
    std::cout << "Scroll:       Zoom\n";
    std::cout << "R:            Reset camera\n";
    std::cout << "ESC:          Exit\n";
    std::cout << "UI:           Use panels to control simulation\n";
    std::cout << "Console:      Type commands in terminal or ImGui console\n";
    std::cout << "              Type 'help' for available commands\n\n";
    
    std::cout << "Starting main loop...\n\n";
    
    // Run main loop with UI and CommandRouter
    window.run_with_ui(sim_thread, command_router);
    
    // Clean shutdown
    std::cout << "\nShutting down...\n";
    
    // Stop STDIN reader first (if running)
    if (stdin_reader) {
        stdin_reader->stop();
    }
    
    // Send shutdown command via router
    command_router.submit_command("shutdown", CommandSource::INTERNAL);
    
    // Give time for shutdown command to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    sim_thread.stop();
    
    std::cout << "Done.\n\n";
    return 0;
}
