/**
 * cmd_viz.cpp - Interactive visualization command implementation
 */

#include "cmd_viz.hpp"
#include "cli/display.hpp"

// Check if visualization is available
#ifdef BUILD_VISUALIZATION
#include "vis/window.hpp"
#include "sim/sim_thread.hpp"
#include "sim/molecule.hpp"
#include "command_router.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#endif

namespace vsepr {
namespace cli {

// Helper to create initial test molecule
#ifdef BUILD_VISUALIZATION
static Molecule create_initial_molecule(const std::string& type) {
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
    else if (type == "empty" || type == "none") {
        // Start with empty molecule
        return mol;
    }
    else {
        // Default to empty (user will build via commands)
        return mol;
    }
    
    return mol;
}
#endif

int VizCommand::Execute(const std::vector<std::string>& args) {
#ifndef BUILD_VISUALIZATION
    Display::Error("Visualization not enabled in this build");
    Display::Info("Rebuild with: ./build.sh --viz");
    return 1;
#else
    // Parse arguments
    int width = 1280;
    int height = 720;
    bool enable_stdin = true;
    bool auto_demo = false;
    std::string initial_molecule = "empty";
    std::string xyz_file = "";
    
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        
        if (arg == "--help" || arg == "-h") {
            Display::Info(VizCommand::Help());
            return 0;
        }
        else if (arg == "sim") {
            // Main mode - just continue
            continue;
        }
        else if (arg.find(".xyz") != std::string::npos) {
            // XYZ file argument
            xyz_file = arg;
        }
        else if (arg == "--width" && i + 1 < args.size()) {
            width = std::stoi(args[++i]);
        }
        else if (arg == "--height" && i + 1 < args.size()) {
            height = std::stoi(args[++i]);
        }
        else if (arg == "--no-stdin") {
            enable_stdin = false;
        }
        else if (arg == "--demo" || arg == "--auto") {
            auto_demo = true;
            enable_stdin = false;  // Disable stdin in demo mode
        }
        else if (arg == "--initial" && i + 1 < args.size()) {
            initial_molecule = args[++i];
        }
        else if (arg == "-h" || arg == "--help") {
            std::cout << Help();
            return 0;
        }
    }
    
    // Display banner
    Display::Header(auto_demo ? "VSEPR Automatic Demo Mode" : "VSEPR Interactive Visualization Session");
    Display::BlankLine();
    Display::Info("Window size: " + std::to_string(width) + "x" + std::to_string(height));
    if (auto_demo) {
        Display::Info("Mode: AUTOMATIC DEMO - Cycling through molecules");
        Display::Info("Showcasing batch visualization workflow");
    } else {
        Display::Info("STDIN commands: " + std::string(enable_stdin ? "enabled" : "disabled"));
        Display::Info("Initial molecule: " + (initial_molecule == "empty" ? "none (use 'build' command)" : initial_molecule));
    }
    Display::BlankLine();
    
    // Create initial molecule
    Molecule mol;
    
    // Load from XYZ file if provided
    if (!xyz_file.empty()) {
        std::ifstream file(xyz_file);
        if (file.is_open()) {
            int num_atoms;
            std::string comment_line;
            file >> num_atoms;
            std::getline(file, comment_line); // Read rest of first line
            std::getline(file, comment_line); // Read comment line
            
            for (int i = 0; i < num_atoms; ++i) {
                std::string symbol;
                double x, y, z;
                file >> symbol >> x >> y >> z;
                
                // Convert symbol to atomic number (simple lookup)
                int Z = 0;
                if (symbol == "H") Z = 1;
                else if (symbol == "He") Z = 2;
                else if (symbol == "C") Z = 6;
                else if (symbol == "N") Z = 7;
                else if (symbol == "O") Z = 8;
                else if (symbol == "F") Z = 9;
                else if (symbol == "S") Z = 16;
                else if (symbol == "Cl") Z = 17;
                else if (symbol == "Br") Z = 35;
                else if (symbol == "I") Z = 53;
                else if (symbol == "P") Z = 15;
                else if (symbol == "Xe") Z = 54;
                // Add more as needed
                
                if (Z > 0) {
                    mol.add_atom(Z, x, y, z);
                }
            }
            file.close();
            
            // Note: XYZ files don't contain bond information
            // Bonds will be generated by the simulation if needed
            
            initial_molecule = xyz_file;
        }
    }
    
    if (mol.num_atoms() == 0) {
        mol = create_initial_molecule(initial_molecule);
    }
    
    if (mol.num_atoms() > 0) {
        Display::Success("Loaded " + std::to_string(mol.num_atoms()) + " atoms, " + 
                        std::to_string(mol.num_bonds()) + " bonds");
    } else {
        Display::Info("Starting with empty system - use 'build <formula>' to create molecules");
    }
    Display::BlankLine();
    
    // Create simulation thread
    SimulationThread sim_thread;
    
    // Create CommandRouter and connect
    CommandRouter command_router(sim_thread);
    sim_thread.set_command_router(&command_router);
    
    // Register output callback
    command_router.register_output_callback([](const OutputEntry& output) {
        const char* prefix = "";
        switch (output.status) {
            case ResultStatus::INFO:    prefix = "[INFO] "; break;
            case ResultStatus::OK:      prefix = "[OK] "; break;
            case ResultStatus::ERROR:   prefix = "[ERROR] "; break;
            case ResultStatus::WARNING: prefix = "[WARN] "; break;
        }
        std::cout << prefix << output.text << std::endl;
    });
    
    // Initialize simulation
    sim_thread.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Initialize with molecule if provided
    if (mol.num_atoms() > 0) {
        // Submit init command
        std::string init_cmd = "init";
        for (size_t i = 0; i < mol.num_atoms(); ++i) {
            init_cmd += " " + std::to_string(mol.atoms[i].Z);
            init_cmd += " " + std::to_string(mol.coords[3*i]);
            init_cmd += " " + std::to_string(mol.coords[3*i+1]);
            init_cmd += " " + std::to_string(mol.coords[3*i+2]);
        }
        command_router.submit_command(init_cmd, CommandSource::INTERNAL);
        
        // Set VSEPR mode
        command_router.submit_command("mode vsepr", CommandSource::INTERNAL);
    }
    
    // Start STDIN reader if enabled
    std::unique_ptr<StdinReader> stdin_reader;
    if (enable_stdin && !auto_demo) {
        stdin_reader = std::make_unique<StdinReader>(command_router);
        stdin_reader->set_prompt("vsepr-viz> ");
        stdin_reader->start();
        
        Display::Success("Command interface ready!");
        Display::Info("Type 'help' for available commands");
        Display::Info("Type 'exit' or 'quit' to close");
        Display::BlankLine();
    }
    
    // Start automatic demo if requested
    if (auto_demo) {
        Display::Success("Starting automatic demo...");
        Display::Info("Watch as molecules are built and optimized automatically");
        Display::BlankLine();
        
        // Submit demo commands
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        command_router.submit_command("build H2O", CommandSource::INTERNAL);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        command_router.submit_command("mode vsepr", CommandSource::INTERNAL);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        command_router.submit_command("build CH4", CommandSource::INTERNAL);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        command_router.submit_command("build NH3", CommandSource::INTERNAL);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        command_router.submit_command("build CO2", CommandSource::INTERNAL);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        command_router.submit_command("build H2S", CommandSource::INTERNAL);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        command_router.submit_command("build SF6", CommandSource::INTERNAL);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        command_router.submit_command("build PCl5", CommandSource::INTERNAL);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        command_router.submit_command("build XeF4", CommandSource::INTERNAL);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        Display::Success("Demo sequence queued - molecules will appear automatically!");
        Display::Info("Close the window when finished");
        Display::BlankLine();
    }
    
    // Create visualization window
    std::string title = "VSEPR Interactive - " + 
                       (initial_molecule == "empty" ? "Ready" : initial_molecule);
    Window window(width, height, title);
    
    if (!window.initialize()) {
        Display::Error("Failed to initialize OpenGL window");
        return 1;
    }
    
    // Set camera
    window.camera().set_target(Vec3(0, 0, 0));
    window.camera().zoom(-3.0);
    
    Display::Header("Controls");
    std::cout << "  Mouse Left:   Rotate camera\n";
    std::cout << "  Mouse Right:  Pan camera\n";
    std::cout << "  Scroll:       Zoom\n";
    std::cout << "  R:            Reset camera\n";
    std::cout << "  ESC:          Exit\n";
    std::cout << "  UI Panels:    Control simulation parameters\n";
    std::cout << "  Console:      Type commands (terminal or ImGui)\n";
    Display::BlankLine();
    
    Display::Header("Batch Workflow Example");
    std::cout << "  > build H2O\n";
    std::cout << "  > optimize\n";
    std::cout << "  > energy\n";
    std::cout << "  > save water.xyz\n";
    std::cout << "  > build CH4\n";
    std::cout << "  > optimize\n";
    std::cout << "  ... (continues with live visualization)\n";
    Display::BlankLine();
    
    Display::Success("Starting visualization...");
    Display::BlankLine();
    
    // Run main render loop with UI
    window.run_with_ui(sim_thread, command_router);
    
    // Cleanup
    Display::Info("Shutting down...");
    
    if (stdin_reader) {
        stdin_reader->stop();
    }
    
    command_router.submit_command("shutdown", CommandSource::INTERNAL);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    sim_thread.stop();
    
    Display::Success("Session closed");
    return 0;
#endif
}

} // namespace cli
} // namespace vsepr
