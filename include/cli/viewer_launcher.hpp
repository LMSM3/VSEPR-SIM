#pragma once
#include <string>

namespace vsepr {
namespace cli {

/**
 * Viewer Launcher
 * 
 * Handles launching visualization tools with appropriate flags.
 */
class ViewerLauncher {
public:
    /**
     * Launch viewer in static mode (--viz)
     * 
     * Opens viewer after simulation completes.
     * Viewer exits when user closes window.
     */
    static void launch_static(const std::string& xyz_path);
    
    /**
     * Launch viewer in watch mode (--watch)
     * 
     * Opens viewer that auto-reloads on file changes.
     * Runs in background during simulation.
     */
    static void launch_watch(const std::string& xyz_path);
    
private:
    static void launch_process(const std::string& command);
};

}} // namespace vsepr::cli
