/**
 * sim_thread.cpp
 * --------------
 * Implementation of simulation thread manager.
 */

#include "sim/sim_thread.hpp"
#include <iostream>
#include <chrono>
#include <variant>

namespace vsepr {

// ============================================================================
// Construction & Lifecycle
// ============================================================================

SimulationThread::SimulationThread()
    : running_(false)
    , should_stop_(false)
    , frame_counter_(0)
{
    sim_state_ = std::make_unique<SimulationState>();
}

SimulationThread::~SimulationThread() {
    stop();
}

void SimulationThread::start() {
    if (running_) {
        return;  // Already running
    }
    
    should_stop_ = false;
    running_ = true;
    
    thread_ = std::make_unique<std::thread>(&SimulationThread::run, this);
    
    std::cout << "[SimThread] Started\n";
}

void SimulationThread::stop() {
    if (!running_) {
        return;
    }
    
    should_stop_ = true;
    
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
    
    running_ = false;
    thread_.reset();
    
    std::cout << "[SimThread] Stopped\n";
}

// ============================================================================
// Command Interface
// ============================================================================

bool SimulationThread::send_command(const SimCommand& cmd) {
    if (!running_) {
        std::cerr << "[SimThread] Cannot send command - thread not running\n";
        return false;
    }
    
    return command_queue_.try_push(cmd);
}

FrameSnapshot SimulationThread::get_latest_frame() {
    return frame_buffer_.read();
}

// ============================================================================
// Main Simulation Loop
// ============================================================================

void SimulationThread::run() {
    std::cout << "[SimThread] Main loop started\n";
    
    // Publish initial empty frame
    publish_frame();
    
    auto last_frame_time = std::chrono::steady_clock::now();
    const auto frame_interval = std::chrono::milliseconds(16);  // ~60 FPS max
    
    while (!should_stop_) {
        // 1. Drain command queue
        process_commands();
        
        // 2. Advance simulation (if running)
        if (sim_state_ && sim_state_->is_running() && !sim_state_->is_paused()) {
            sim_state_->step();
            frame_counter_++;
            
            // Publish frame periodically
            if (frame_counter_ % sim_state_->params().publish_every == 0) {
                publish_frame();
            }
            
            // Debug output every 100 steps
            if (frame_counter_ % 100 == 0) {
                std::cout << "[SimThread] Step " << frame_counter_ << " - Energy: " 
                          << sim_state_->stats().total_energy << "\n";
            }
        } else {
            // Idle - sleep a bit to avoid busy-wait
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // 3. Frame rate limiting (optional)
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_frame_time);
        if (elapsed < frame_interval) {
            std::this_thread::sleep_for(frame_interval - elapsed);
        }
        last_frame_time = now;
    }
    
    std::cout << "[SimThread] Main loop finished\n";
}

void SimulationThread::process_commands() {
    // Drain all pending commands
    while (auto cmd_opt = command_queue_.try_pop()) {
        handle_command(*cmd_opt);
    }
}

void SimulationThread::handle_command(const SimCommand& cmd) {
    std::visit([this](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        
        if constexpr (std::is_same_v<T, CmdReset>) {
            std::cout << "[SimThread] Reset: " << arg.config_id << "\n";
            if (sim_state_) {
                sim_state_->reset();
                publish_frame();
            }
        }
        else if constexpr (std::is_same_v<T, CmdLoad>) {
            std::cout << "[SimThread] Load: " << arg.filepath << "\n";
            if (sim_state_) {
                if (sim_state_->load_from_file(arg.filepath)) {
                    publish_frame();
                } else {
                    std::cerr << "[SimThread] Failed to load file\n";
                }
            }
        }
        else if constexpr (std::is_same_v<T, CmdInitMolecule>) {
            std::cout << "[SimThread] Init molecule: " << arg.atomic_numbers.size() << " atoms\n";
            if (sim_state_) {
                // Build molecule from command data
                Molecule mol;
                for (size_t i = 0; i < arg.atomic_numbers.size(); ++i) {
                    mol.add_atom(arg.atomic_numbers[i],
                                arg.coords[3*i],
                                arg.coords[3*i+1],
                                arg.coords[3*i+2]);
                }
                for (const auto& bond : arg.bonds) {
                    mol.add_bond(bond.first, bond.second, 1);
                }
                mol.generate_angles_from_bonds();
                
                sim_state_->initialize(mol);
                publish_frame();
            }
        }
        else if constexpr (std::is_same_v<T, CmdSetMode>) {
            const char* mode_names[] = {"IDLE", "VSEPR", "OPTIMIZE", "MD", "CRYSTAL"};
            int mode_idx = static_cast<int>(arg.mode);
            std::cout << "[SimThread] Set mode: " << mode_names[mode_idx] << "\n";
            if (sim_state_) {
                sim_state_->set_mode(arg.mode);
                publish_frame();
            }
        }
        else if constexpr (std::is_same_v<T, CmdSetParams>) {
            std::cout << "[SimThread] Update parameters\n";
            if (sim_state_) {
                sim_state_->update_params(arg);
            }
        }
        else if constexpr (std::is_same_v<T, CmdPause>) {
            std::cout << "[SimThread] Pause\n";
            if (sim_state_) {
                sim_state_->pause();
                publish_frame();
            }
        }
        else if constexpr (std::is_same_v<T, CmdResume>) {
            std::cout << "[SimThread] Resume\n";
            if (sim_state_) {
                std::cout << "[SimThread]   - Num atoms: " << sim_state_->molecule().num_atoms() << "\n";
                std::cout << "[SimThread]   - Current mode: " << static_cast<int>(sim_state_->mode()) << "\n";
                sim_state_->resume();
                std::cout << "[SimThread]   - Now running: " << sim_state_->is_running() << "\n";
                std::cout << "[SimThread]   - Paused: " << sim_state_->is_paused() << "\n";
                publish_frame();
            }
        }
        else if constexpr (std::is_same_v<T, CmdSingleStep>) {
            std::cout << "[SimThread] Single step: " << arg.n_steps << "\n";
            if (sim_state_) {
                sim_state_->advance(arg.n_steps);
                publish_frame();
            }
        }
        else if constexpr (std::is_same_v<T, CmdSaveSnapshot>) {
            std::cout << "[SimThread] Save snapshot: " << arg.filepath << "\n";
            if (sim_state_) {
                sim_state_->save_to_file(arg.filepath);
            }
        }
        else if constexpr (std::is_same_v<T, CmdShutdown>) {
            std::cout << "[SimThread] Shutdown requested\n";
            should_stop_ = true;
        }
    }, cmd);
}

void SimulationThread::publish_frame() {
    if (sim_state_) {
        FrameSnapshot snap = sim_state_->get_snapshot();
        frame_buffer_.write(snap);
    }
}

} // namespace vsepr
