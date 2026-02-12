#include "demo/platform_terminal.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

using namespace vsepr::demo;

void print_usage(const char* program_name) {
    std::cout << "\n";
    std::cout << "VSEPR-Sim Demo Launcher\n";
    std::cout << "========================\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << program_name << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  (no args)          Launch interactive meso-build\n";
    std::cout << "  --vsepr            Launch meso-build with demo molecule\n";
    std::cout << "  --build            Launch interactive meso-build\n";
    std::cout << "  --sim              Launch meso-sim for quick simulation\n";
    std::cout << "  --align            Launch meso-align for structure alignment\n";
    std::cout << "  --discover         Launch meso-discover for reaction discovery\n";
    std::cout << "  --relax            Launch meso-relax for FIRE minimization\n";
    std::cout << "  --terminal         Open terminal in current directory\n";
    std::cout << "  --help, -h         Show this help message\n\n";
    std::cout << "Platform: " << PlatformTerminal::platform_name(PlatformTerminal::detect_platform()) << "\n\n";
    std::cout << "Examples:\n";
    std::cout << "  demo                   # Start interactive builder\n";
    std::cout << "  demo --vsepr           # Build ethane interactively\n";
    std::cout << "  demo --sim             # Run quick MD simulation\n";
    std::cout << "  demo --discover        # Launch reaction discovery\n\n";
}

int main(int argc, char* argv[]) {
    // Parse command line
    std::string mode = "build";  // default
    
    if (argc > 1) {
        std::string arg = argv[1];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--vsepr") {
            mode = "vsepr";
        } else if (arg == "--build") {
            mode = "build";
        } else if (arg == "--sim") {
            mode = "sim";
        } else if (arg == "--align") {
            mode = "align";
        } else if (arg == "--discover") {
            mode = "discover";
        } else if (arg == "--relax") {
            mode = "relax";
        } else if (arg == "--terminal") {
            mode = "terminal";
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Detect platform
    Platform platform = PlatformTerminal::detect_platform();
    std::cout << "Detected platform: " << PlatformTerminal::platform_name(platform) << "\n";
    
    // Build command based on mode
    std::string command;
    std::string title;
    
    if (mode == "terminal") {
        std::cout << "Launching terminal...\n";
        if (!PlatformTerminal::launch_terminal(".")) {
            std::cerr << "Failed to launch terminal\n";
            return 1;
        }
        return 0;
    } else if (mode == "build") {
        title = "VSEPR-Sim: Interactive Builder";
        command = "meso-build";
    } else if (mode == "vsepr") {
        title = "VSEPR-Sim: Demo Molecule";
        // Launch meso-build with ethane demo
        command = "echo 'Building ethane (C2H6) demo...' && meso-build";
    } else if (mode == "sim") {
        title = "VSEPR-Sim: MD Simulation";
        command = "meso-sim --help && echo 'Ready for simulation...'";
    } else if (mode == "align") {
        title = "VSEPR-Sim: Structure Alignment";
        command = "meso-align --help && echo 'Ready for alignment...'";
    } else if (mode == "discover") {
        title = "VSEPR-Sim: Reaction Discovery";
        command = "meso-discover --help && echo 'Ready for discovery...'";
    } else if (mode == "relax") {
        title = "VSEPR-Sim: FIRE Minimization";
        command = "meso-relax --help && echo 'Ready for minimization...'";
    }
    
    // Launch command in new terminal
    std::cout << "Launching: " << title << "\n";
    std::cout << "Command: " << command << "\n\n";
    
    bool success = PlatformTerminal::launch_command_titled(title, command, false);
    
    if (success) {
        std::cout << "✓ Demo launched successfully!\n";
        std::cout << "  A new terminal window should open with " << mode << " mode.\n";
        return 0;
    } else {
        std::cerr << "✗ Failed to launch demo\n";
        std::cerr << "  Platform: " << PlatformTerminal::platform_name(platform) << "\n";
        std::cerr << "  Try running the command directly: " << command << "\n";
        return 1;
    }
}
