#pragma once
/**
 * sim_thread.hpp
 * --------------
 * Simulation thread manager.
 * Handles command queue, simulation state, and frame publishing.
 */

#include "sim_command.hpp"
#include "sim/sim_state.hpp"
#include "core/frame_buffer.hpp"
#include "pot/periodic_db.hpp"
#include "vsepr/formula_parser.hpp"
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <string>

// Forward declarations
namespace vsepr {
    class CommandRouter;
}

namespace vsepr {

/**
 * Simulation thread - runs independently from renderer.
 * 
 * Architecture:
 * - Receives CmdEnvelope from CommandRouter via lock-free queue
 * - Owns simulation state and physics
 * - Publishes CmdResult back to CommandRouter
 * - Publishes frames to double-buffered snapshot
 * - Handles all state mutations at safe points
 */
class SimulationThread {
public:
    SimulationThread();
    ~SimulationThread();
    
    // Start/stop simulation thread
    // NOTE: Must call set_command_router() before start()
    void start();
    void stop();
    bool is_running() const { return running_; }
    
    // Set the command router (must be called before start())
    void set_command_router(CommandRouter* router);
    
    // Frame output (called from renderer thread)
    FrameSnapshot get_latest_frame();
    
    // Quick status check
    bool is_paused() const { return sim_state_ ? sim_state_->is_paused() : true; }
    SimMode current_mode() const { return sim_state_ ? sim_state_->mode() : SimMode::IDLE; }
    
private:
    // Main simulation loop (runs on separate thread)
    void run();
    
    // Process pending commands from CommandRouter
    void process_commands();
    
    // Handle individual command envelope
    void handle_envelope(const struct CmdEnvelope& envelope);
    
    // Send result back to CommandRouter
    void send_result(const struct CmdResult& result);
    
    // Publish current state to frame buffer
    void publish_frame();
    
    // Helper methods for dynamic formula building
    void add_to_custom_defaults(const std::string& formula);
    std::string get_defaults_list() const;
    bool build_from_composition(const vsepr::formula::Composition& composition, 
                                const std::string& formula,
                                uint64_t cmd_id);
    
    // Thread control
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;
    std::unique_ptr<std::thread> thread_;
    
    // Communication with CommandRouter
    CommandRouter* command_router_;  // Not owned - set externally
    
    // Frame output
    FrameBuffer frame_buffer_;
    
    // Simulation state (owned by sim thread)
    std::unique_ptr<SimulationState> sim_state_;
    
    // Periodic table for formula parsing
    PeriodicTable ptable_;
    
    // Custom molecule defaults tracking
    std::vector<std::string> custom_defaults_;
    std::vector<std::string> default_molecules_ = {
        "H2O", "CH4", "NH3", "CO2", "H2S", "SF6", "PCl5", "XeF4"
    };
    
    // Pending formula build (awaiting user confirmation)
    std::string pending_formula_;
    vsepr::formula::Composition pending_composition_;
    uint64_t pending_cmd_id_;
    
    // Timing
    int frame_counter_;
};

} // namespace vsepr
