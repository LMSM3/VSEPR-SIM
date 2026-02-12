#pragma once
/**
 * ui_panels.hpp
 * -------------
 * ImGui UI panels for simulation control and visualization.
 */

#include "../sim/sim_thread.hpp"
#include "../sim/sim_command.hpp"
#include "vis/command_parser.hpp"
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>

namespace vsepr {

// Forward declaration
class Renderer;
class CommandRouter;

/**
 * UI Manager for ImGui panels.
 * Provides control panels for simulation parameters, mode selection,
 * diagnostics, and I/O.
 */
class UIManager {
public:
    UIManager();
    ~UIManager();
    
    // Render all UI panels (call once per frame)
    // Old interface (without CommandRouter)
    void render(SimulationThread& sim_thread, Renderer* renderer = nullptr);
    
    // New interface (with CommandRouter)
    void render(SimulationThread& sim_thread, CommandRouter& command_router, Renderer* renderer = nullptr);
    
    // Individual panel renderers
    void render_control_panel(SimulationThread& sim_thread);
    void render_parameters_panel(SimulationThread& sim_thread);
    void render_diagnostics_panel(const FrameSnapshot& frame);
    void render_io_panel(SimulationThread& sim_thread);
    void render_visualization_panel(Renderer* renderer);
    void render_mode_selector(SimulationThread& sim_thread);
    
    // Old command console (without CommandRouter)
    void render_command_console(SimulationThread& sim_thread);
    
    // New command console (with CommandRouter)
    void render_command_console(SimulationThread& sim_thread, CommandRouter& command_router);
    
    // GPU/OpenGL status panel
    void render_gpu_status_panel(Renderer* renderer);
    
    // Settings - Console-first UI: only command console shown by default
    bool show_control_panel = false;
    bool show_parameters_panel = false;
    bool show_diagnostics_panel = false;
    bool show_io_panel = false;
    bool show_visualization_panel = false;
    bool show_gpu_status_panel = true;  // GPU status visible by default
    bool show_demo_window = false;
    bool show_command_console = false;  // Start hidden, toggle with ~
    
private:
    // Command router for sending commands (not owned)
    CommandRouter* command_router_ = nullptr;
    // UI state
    int selected_mode_ = 0;
    
    // Parameter controls (local UI state, synced to sim)
    float dt_init_ = 0.1f;
    float dt_max_ = 1.0f;
    float alpha_init_ = 0.1f;
    float max_step_ = 0.2f;
    float tol_rms_force_ = 1e-3f;
    float tol_max_force_ = 1e-3f;
    int max_iterations_ = 1000;
    
    float temperature_ = 300.0f;
    float md_timestep_ = 0.001f;
    float damping_ = 1.0f;
    
    // PBC parameters
    bool use_pbc_ = false;
    float box_x_ = 20.0f;
    float box_y_ = 20.0f;
    float box_z_ = 20.0f;
    bool pbc_cube_mode_ = true;
    
    // I/O state
    char load_file_buf_[256] = "h2o.json";
    char save_file_buf_[256] = "output.json";
    
    // Command console state
    CommandParser command_parser_;
    CommandHistory command_history_;
    char command_input_buf_[512] = "";
    std::vector<std::string> console_log_;
    std::vector<std::string> command_output_;  // Separate buffer for last command output
    bool scroll_to_bottom_ = false;
    bool scroll_output_to_bottom_ = false;
    bool focus_command_input_ = false;
    std::mutex console_mutex_;
};

} // namespace vsepr
