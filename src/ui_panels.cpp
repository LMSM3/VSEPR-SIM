/**
 * ui_panels.cpp
 * -------------
 * Implementation of ImGui UI panels.
 */

#include "vis/ui_panels.hpp"
#include "command_router.hpp"
#include "imgui.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <variant>

namespace vsepr {

UIManager::UIManager()
    : selected_mode_(0)
{
    strcpy(load_file_buf_, "h2o.json");
    strcpy(save_file_buf_, "output.json");
}

void UIManager::render(SimulationThread& sim_thread) {
    if (show_command_console) {
        render_command_console(sim_thread);
    }
    
    if (show_control_panel) {
        render_control_panel(sim_thread);
    }
    
    if (show_parameters_panel) {
        render_parameters_panel(sim_thread);
    }
    
    if (show_diagnostics_panel) {
        FrameSnapshot frame = sim_thread.get_latest_frame();
        render_diagnostics_panel(frame);
    }
    
    if (show_io_panel) {
        render_io_panel(sim_thread);
    }
    
    if (show_demo_window) {
        ImGui::ShowDemoWindow(&show_demo_window);
    }
}

void UIManager::render(SimulationThread& sim_thread, CommandRouter& command_router) {
    if (show_command_console) {
        render_command_console(sim_thread, command_router);
    }
    
    if (show_control_panel) {
        render_control_panel(sim_thread);
    }
    
    if (show_parameters_panel) {
        render_parameters_panel(sim_thread);
    }
    
    if (show_diagnostics_panel) {
        FrameSnapshot frame = sim_thread.get_latest_frame();
        render_diagnostics_panel(frame);
    }
    
    if (show_io_panel) {
        render_io_panel(sim_thread);
    }
    
    if (show_demo_window) {
        ImGui::ShowDemoWindow(&show_demo_window);
    }
}

void UIManager::render_control_panel(SimulationThread& sim_thread) {
    ImGui::Begin("Simulation Control", &show_control_panel);
    
    // Mode selector
    render_mode_selector(sim_thread);
    
    ImGui::Separator();
    
    // Run/Pause/Step controls
    bool is_paused = sim_thread.is_paused();
    SimMode mode = sim_thread.current_mode();
    
    if (is_paused || mode == SimMode::IDLE) {
        if (ImGui::Button("Run", ImVec2(120, 0))) {
            sim_thread.send_command(CmdResume{});
        }
    } else {
        if (ImGui::Button("Pause", ImVec2(120, 0))) {
            sim_thread.send_command(CmdPause{});
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(120, 0))) {
        sim_thread.send_command(CmdReset{"default", 0});
    }
    
    if (ImGui::Button("Single Step", ImVec2(120, 0))) {
        sim_thread.send_command(CmdSingleStep{1});
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Step 10", ImVec2(120, 0))) {
        sim_thread.send_command(CmdSingleStep{10});
    }
    
    if (ImGui::Button("Step 100", ImVec2(120, 0))) {
        sim_thread.send_command(CmdSingleStep{100});
    }
    
    ImGui::End();
}

void UIManager::render_mode_selector(SimulationThread& sim_thread) {
    ImGui::Text("Simulation Mode");
    
    const char* mode_names[] = {"Idle", "VSEPR Optimization", "General Optimization", "Molecular Dynamics", "Crystal Optimization"};
    
    if (ImGui::Combo("##mode", &selected_mode_, mode_names, IM_ARRAYSIZE(mode_names))) {
        SimMode mode = static_cast<SimMode>(selected_mode_);
        sim_thread.send_command(CmdSetMode{mode});
    }
    
    // Mode-specific hints
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
        selected_mode_ == 0 ? "No simulation running" :
        selected_mode_ == 1 ? "Small molecules, VSEPR rules" :
        selected_mode_ == 2 ? "General structure optimization (FIRE)" :
        selected_mode_ == 3 ? "Molecular dynamics with thermostat" :
        "Periodic crystal optimization");
}

void UIManager::render_parameters_panel(SimulationThread& sim_thread) {
    ImGui::Begin("Parameters", &show_parameters_panel);
    
    SimMode mode = sim_thread.current_mode();
    
    if (ImGui::CollapsingHeader("Optimizer (FIRE)", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool changed = false;
        
        changed |= ImGui::SliderFloat("Initial dt", &dt_init_, 0.01f, 0.5f, "%.3f");
        changed |= ImGui::SliderFloat("Max dt", &dt_max_, 0.1f, 2.0f, "%.2f");
        changed |= ImGui::SliderFloat("Alpha", &alpha_init_, 0.01f, 0.5f, "%.3f");
        changed |= ImGui::SliderFloat("Max Step", &max_step_, 0.05f, 0.5f, "%.3f Å");
        
        ImGui::Separator();
        
        changed |= ImGui::SliderFloat("RMS Force Tol", &tol_rms_force_, 1e-5f, 1e-1f, "%.1e", ImGuiSliderFlags_Logarithmic);
        changed |= ImGui::SliderFloat("Max Force Tol", &tol_max_force_, 1e-5f, 1e-1f, "%.1e", ImGuiSliderFlags_Logarithmic);
        changed |= ImGui::SliderInt("Max Iterations", &max_iterations_, 100, 10000);
        
        if (changed) {
            CmdSetParams params;
            params.dt_init = dt_init_;
            params.dt_max = dt_max_;
            params.alpha_init = alpha_init_;
            params.max_step = max_step_;
            params.tol_rms_force = tol_rms_force_;
            params.tol_max_force = tol_max_force_;
            params.max_iterations = max_iterations_;
            sim_thread.send_command(params);
        }
    }
    
    if (mode == SimMode::MD) {
        if (ImGui::CollapsingHeader("Molecular Dynamics", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool changed = false;
            
            changed |= ImGui::SliderFloat("Temperature", &temperature_, 50.0f, 1000.0f, "%.1f K");
            changed |= ImGui::SliderFloat("Timestep", &md_timestep_, 0.0001f, 0.01f, "%.4f ps");
            changed |= ImGui::SliderFloat("Damping", &damping_, 0.1f, 10.0f, "%.2f");
            
            if (changed) {
                CmdSetParams params;
                params.temperature = temperature_;
                params.timestep = md_timestep_;
                params.damping = damping_;
                sim_thread.send_command(params);
            }
        }
    }
    
    ImGui::End();
}

void UIManager::render_diagnostics_panel(const FrameSnapshot& frame) {
    ImGui::Begin("Diagnostics", &show_diagnostics_panel);
    
    // System info
    if (ImGui::CollapsingHeader("System", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Atoms: %zu", frame.positions.size());
        ImGui::Text("Bonds: %zu", frame.bonds.size());
        ImGui::Text("Iteration: %d", frame.iteration);
    }
    
    // Energy
    if (ImGui::CollapsingHeader("Energy", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Total: %.4f kcal/mol", frame.energy);
        
        // Add a simple energy plot if we had history
        // For now just show the value
        ImGui::ProgressBar(0.0f, ImVec2(-1, 0), "");
    }
    
    // Forces
    if (ImGui::CollapsingHeader("Forces", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("RMS Force: %.6f kcal/mol/Å", frame.rms_force);
        ImGui::Text("Max Force: %.6f kcal/mol/Å", frame.max_force);
        
        // Convergence indicator
        float rms_threshold = 1e-3f;
        float progress = std::min(1.0f, static_cast<float>(frame.rms_force / rms_threshold));
        
        if (frame.rms_force < rms_threshold) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "CONVERGED");
        } else {
            ImGui::ProgressBar(1.0f - progress, ImVec2(-1, 0), "");
        }
    }
    
    // Status
    if (ImGui::CollapsingHeader("Status", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("%s", frame.status_message.c_str());
    }
    
    ImGui::End();
}

void UIManager::render_io_panel(SimulationThread& sim_thread) {
    ImGui::Begin("I/O", &show_io_panel);
    
    // Load
    ImGui::Text("Load Molecule");
    ImGui::InputText("##load_file", load_file_buf_, sizeof(load_file_buf_));
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        sim_thread.send_command(CmdLoad{load_file_buf_});
    }
    
    ImGui::Separator();
    
    // Save
    ImGui::Text("Save Snapshot");
    ImGui::InputText("##save_file", save_file_buf_, sizeof(save_file_buf_));
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        sim_thread.send_command(CmdSaveSnapshot{save_file_buf_});
    }
    
    ImGui::Separator();
    
    // Quick presets
    ImGui::Text("Quick Load");
    if (ImGui::Button("H2O")) {
        strcpy(load_file_buf_, "h2o.json");
        sim_thread.send_command(CmdLoad{"h2o.json"});
    }
    ImGui::SameLine();
    if (ImGui::Button("CH4")) {
        strcpy(load_file_buf_, "ch4.json");
        sim_thread.send_command(CmdLoad{"ch4.json"});
    }
    ImGui::SameLine();
    if (ImGui::Button("NH3")) {
        strcpy(load_file_buf_, "nh3.json");
        sim_thread.send_command(CmdLoad{"nh3.json"});
    }
    
    ImGui::End();
}

void UIManager::render_command_console(SimulationThread& sim_thread) {
    ImGui::Begin("Command Console", &show_command_console, ImGuiWindowFlags_MenuBar);
    
    // Menu bar for help and options
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Show Commands")) {
                console_log_.push_back("=== Available Commands ===");
                console_log_.push_back(command_parser_.get_help());
                scroll_to_bottom_ = true;
            }
            if (ImGui::MenuItem("Clear Console")) {
                console_log_.clear();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // Console output area (scrollable)
    ImGui::BeginChild("ConsoleOutput", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), 
                      true, ImGuiWindowFlags_HorizontalScrollbar);
    
    for (const auto& line : console_log_) {
        // Color code messages
        if (line.find("[ERROR]") == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", line.c_str());
        } else if (line.find("[OK]") == 0) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", line.c_str());
        } else if (line.find("[INFO]") == 0) {
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", line.c_str());
        } else if (line.find("===") == 0) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "%s", line.c_str());
        } else if (line.find(">") == 0) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", line.c_str());
        } else {
            ImGui::TextWrapped("%s", line.c_str());
        }
    }
    
    if (scroll_to_bottom_) {
        ImGui::SetScrollHereY(1.0f);
        scroll_to_bottom_ = false;
    }
    
    ImGui::EndChild();
    
    // Command input area
    ImGui::PushItemWidth(-1);
    
    // Handle up/down arrow for history
    if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive()) {
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow))) {
            auto prev = command_history_.previous();
            if (prev) {
                strncpy(command_input_buf_, prev->c_str(), sizeof(command_input_buf_) - 1);
                command_input_buf_[sizeof(command_input_buf_) - 1] = '\0';
                focus_command_input_ = true;
            }
        } else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow))) {
            auto next = command_history_.next();
            if (next) {
                strncpy(command_input_buf_, next->c_str(), sizeof(command_input_buf_) - 1);
                command_input_buf_[sizeof(command_input_buf_) - 1] = '\0';
                focus_command_input_ = true;
            }
        }
    }
    
    // Auto-focus on command input
    if (focus_command_input_) {
        ImGui::SetKeyboardFocusHere();
        focus_command_input_ = false;
    }
    
    // Input field with Enter to submit
    if (ImGui::InputText("##CommandInput", command_input_buf_, sizeof(command_input_buf_),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string command(command_input_buf_);
        
        // Echo command
        console_log_.push_back("> " + command);
        
        // Parse and execute
        auto result = command_parser_.parse(command);
        
        if (std::holds_alternative<ParseSuccess>(result)) {
            auto& success = std::get<ParseSuccess>(result);
            
            // Send command to simulation thread
            sim_thread.send_command(success.command);
            
            // Log success
            console_log_.push_back("[OK] " + success.echo);
            
            // Add to history
            command_history_.add(command);
            
        } else {
            auto& error = std::get<ParseError>(result);
            
            // Log error
            console_log_.push_back("[ERROR] " + error.error_message);
            if (!error.suggestion.empty()) {
                console_log_.push_back("[INFO] " + error.suggestion);
            }
        }
        
        // Clear input and scroll
        command_input_buf_[0] = '\0';
        command_history_.reset_cursor();
        scroll_to_bottom_ = true;
        focus_command_input_ = true;
    }
    
    // Hint text
    if (command_input_buf_[0] == '\0' && !ImGui::IsItemActive()) {
        ImGui::SameLine();
        ImGui::TextDisabled("Type command here (or 'help')...");
    }
    
    ImGui::PopItemWidth();
    
    ImGui::End();
}

void UIManager::render_command_console(SimulationThread& sim_thread, CommandRouter& command_router) {
    ImGui::Begin("Command Console", &show_command_console, ImGuiWindowFlags_MenuBar);
    
    // Menu bar for help and options
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Show Commands")) {
                // Submit help command through router
                command_router.submit_command("help", CommandSource::IMGUI);
            }
            if (ImGui::MenuItem("Clear Console")) {
                command_router.clear_output_history();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // Console output area (scrollable)
    ImGui::BeginChild("ConsoleOutput", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), 
                      true, ImGuiWindowFlags_HorizontalScrollbar);
    
    // Get output history from CommandRouter
    auto output_history = command_router.get_output_history();
    
    for (const auto& entry : output_history) {
        // Color code messages based on status
        ImVec4 color;
        const char* prefix = "";
        
        switch (entry.status) {
            case ResultStatus::ERROR:
                color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                prefix = "[ERROR] ";
                break;
            case ResultStatus::OK:
                color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
                prefix = "[OK] ";
                break;
            case ResultStatus::INFO:
                color = ImVec4(0.6f, 0.8f, 1.0f, 1.0f);
                prefix = "";
                break;
            case ResultStatus::WARNING:
                color = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
                prefix = "[WARN] ";
                break;
            default:
                color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                break;
        }
        
        // Display cmd_id and text with color
        std::string display_text = "[" + std::to_string(entry.cmd_id) + "] " + std::string(prefix) + entry.text;
        ImGui::TextColored(color, "%s", display_text.c_str());
    }
    
    // Auto-scroll to bottom if new content
    static size_t last_history_size = 0;
    if (output_history.size() != last_history_size) {
        ImGui::SetScrollHereY(1.0f);
        last_history_size = output_history.size();
    }
    
    ImGui::EndChild();
    
    // Command input area
    ImGui::PushItemWidth(-1);
    
    // Handle up/down arrow for history
    if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive()) {
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow))) {
            auto prev = command_history_.previous();
            if (prev) {
                strncpy(command_input_buf_, prev->c_str(), sizeof(command_input_buf_) - 1);
                command_input_buf_[sizeof(command_input_buf_) - 1] = '\0';
                focus_command_input_ = true;
            }
        } else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow))) {
            auto next = command_history_.next();
            if (next) {
                strncpy(command_input_buf_, next->c_str(), sizeof(command_input_buf_) - 1);
                command_input_buf_[sizeof(command_input_buf_) - 1] = '\0';
                focus_command_input_ = true;
            }
        }
    }
    
    // Auto-focus on command input
    if (focus_command_input_) {
        ImGui::SetKeyboardFocusHere();
        focus_command_input_ = false;
    }
    
    // Input field with Enter to submit
    if (ImGui::InputText("##CommandInput", command_input_buf_, sizeof(command_input_buf_),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string command(command_input_buf_);
        
        if (!command.empty()) {
            // Submit command through CommandRouter
            command_router.submit_command(command, CommandSource::IMGUI);
            
            // Add to local history (for up/down arrow navigation)
            command_history_.add(command);
        }
        
        // Clear input
        command_input_buf_[0] = '\0';
        command_history_.reset_cursor();
        focus_command_input_ = true;
    }
    
    // Hint text
    if (command_input_buf_[0] == '\0' && !ImGui::IsItemActive()) {
        ImGui::SameLine();
        ImGui::TextDisabled("Type command here (or 'help')...");
    }
    
    ImGui::PopItemWidth();
    
    ImGui::End();
}

} // namespace vsepr
