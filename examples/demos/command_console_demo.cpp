/**
 * command_console_demo.cpp
 * ------------------------
 * Minimal demo of the command console GUI with text passthrough system.
 * 
 * Philosophy: "Buttons don't scale, but parsers do."
 * 
 * This demonstrates:
 * - Command input with text parsing
 * - Command history (up/down arrows)
 * - Color-coded output
 * - Help system
 * - Integration with simulation thread
 */

#include "vis/window.hpp"
#include "vis/ui_panels.hpp"
#include "sim/sim_thread.hpp"
#include <iostream>

using namespace vsepr;

int main() {
    std::cout << "=== Command-Bus GUI Demo ===" << std::endl;
    std::cout << "Philosophy: Buttons don't scale, but parsers do." << std::endl;
    std::cout << std::endl;
    
    // Create window
    Window window(1280, 720, "VSEPR Command Console Demo");
    if (!window.initialize()) {
        std::cerr << "Failed to initialize window" << std::endl;
        return 1;
    }
    
    // Create simulation thread
    SimulationThread sim_thread;
    sim_thread.start();
    
    // Load initial molecule
    // TODO: Fix API - send_command doesn't exist
    // sim_thread.send_command(CmdLoad{"h2o.json"});
    // sim_thread.send_command(CmdSetMode{SimMode::VSEPR});
    std::cout << "Note: send_command API not yet implemented\n";
    
    
    std::cout << "Window initialized." << std::endl;
    std::cout << "Try commands like:" << std::endl;
    std::cout << "  load h2o.json" << std::endl;
    std::cout << "  mode vsepr" << std::endl;
    std::cout << "  step 10" << std::endl;
    std::cout << "  pause" << std::endl;
    std::cout << "  resume" << std::endl;
    std::cout << "  set temperature 300" << std::endl;
    std::cout << "  save output.json" << std::endl;
    std::cout << "  help" << std::endl;
    std::cout << std::endl;
    
    // Run main loop
    window.run_with_ui(sim_thread);
    
    // Cleanup
    sim_thread.stop();
    window.shutdown();
    
    std::cout << "Demo finished." << std::endl;
    
    return 0;
}
