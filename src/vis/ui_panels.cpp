/**
 * ui_panels.cpp
 * -------------
 * Implementation of ImGui UI panels.
 */

#include "vis/ui_panels.hpp"
#include "../../include/command_router.hpp"
#include "vis/renderer.hpp"
#include "imgui.h"
#include <GL/glew.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <variant>
#include <chrono>
#include <iostream>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace vsepr {

UIManager::UIManager()
    : selected_mode_(0)
{
    strcpy(load_file_buf_, "h2o.json");
    strcpy(save_file_buf_, "output.json");
    console_log_.push_back("[INFO] Command console ready. Type 'help' for available commands.");
}

UIManager::~UIManager() {
    // Cleanup if needed
}

void UIManager::render(SimulationThread& sim_thread, Renderer* renderer) {
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
    
    if (show_visualization_panel && renderer) {
        render_visualization_panel(renderer);
    }
    
    if (show_gpu_status_panel && renderer) {
        render_gpu_status_panel(renderer);
    }
    
    if (show_demo_window) {
        ImGui::ShowDemoWindow(&show_demo_window);
    }
}

void UIManager::render(SimulationThread& sim_thread, CommandRouter& command_router, Renderer* renderer) {
    // Store command router for use in panel methods
    command_router_ = &command_router;
    
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
    
    if (show_visualization_panel && renderer) {
        render_visualization_panel(renderer);
    }
    
    if (show_gpu_status_panel && renderer) {
        render_gpu_status_panel(renderer);
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
            if (command_router_) command_router_->submit_command("resume", CommandSource::IMGUI);
        }
    } else {
        if (ImGui::Button("Pause", ImVec2(120, 0))) {
            if (command_router_) command_router_->submit_command("pause", CommandSource::IMGUI);
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(120, 0))) {
        if (command_router_) command_router_->submit_command("reset", CommandSource::IMGUI);
    }
    
    if (ImGui::Button("Single Step", ImVec2(120, 0))) {
        if (command_router_) command_router_->submit_command("step", CommandSource::IMGUI);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Step 10", ImVec2(120, 0))) {
        if (command_router_) command_router_->submit_command("advance 10", CommandSource::IMGUI);
    }
    
    if (ImGui::Button("Step 100", ImVec2(120, 0))) {
        if (command_router_) command_router_->submit_command("advance 100", CommandSource::IMGUI);
    }
    
    ImGui::End();
}

void UIManager::render_mode_selector(SimulationThread& sim_thread) {
    ImGui::Text("Simulation Mode");
    
    const char* mode_names[] = {"Idle", "VSEPR Optimization", "General Optimization", "Molecular Dynamics", "Crystal Optimization"};
    
    if (ImGui::Combo("##mode", &selected_mode_, mode_names, IM_ARRAYSIZE(mode_names))) {
        const char* mode_strs[] = {"idle", "vsepr", "general", "md", "crystal"};
        if (command_router_ && selected_mode_ < 5) {
            std::string cmd = std::string("mode ") + mode_strs[selected_mode_];
            command_router_->submit_command(cmd, CommandSource::IMGUI);
        }
    }
    
    // Mode-specific hints
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
        selected_mode_ == 0 ? "No simulation running" :
        selected_mode_ == 1 ? "Small molecules, VSEPR rules" :
        selected_mode_ == 2 ? "General structure optimization (FIRE)" :
        selected_mode_ == 3 ? "Molecular dynamics with thermostat" :
        "Periodic crystal optimization");
}
// FIRE is not necessarily useful, primarily acts as a visualization of the internal workings of the internal vsepr model.

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
        
        if (changed && command_router_) {
            command_router_->submit_command("set fire.dt_init " + std::to_string(dt_init_), CommandSource::IMGUI);
            command_router_->submit_command("set fire.dt_max " + std::to_string(dt_max_), CommandSource::IMGUI);
            command_router_->submit_command("set fire.alpha_init " + std::to_string(alpha_init_), CommandSource::IMGUI);
            command_router_->submit_command("set fire.max_step " + std::to_string(max_step_), CommandSource::IMGUI);
            command_router_->submit_command("set fire.tol_rms_force " + std::to_string(tol_rms_force_), CommandSource::IMGUI);
            command_router_->submit_command("set fire.tol_max_force " + std::to_string(tol_max_force_), CommandSource::IMGUI);
            command_router_->submit_command("set fire.max_iterations " + std::to_string(max_iterations_), CommandSource::IMGUI);
        }
    }
    
    if (mode == SimMode::MD) {
        if (ImGui::CollapsingHeader("Molecular Dynamics", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool changed = false;
            
            changed |= ImGui::SliderFloat("Temperature", &temperature_, 50.0f, 1000.0f, "%.1f K");
            changed |= ImGui::SliderFloat("Timestep", &md_timestep_, 0.0001f, 0.01f, "%.4f ps");
            changed |= ImGui::SliderFloat("Damping", &damping_, 0.1f, 10.0f, "%.2f");
            
            if (changed && command_router_) {
                command_router_->submit_command("set md.temperature " + std::to_string(temperature_), CommandSource::IMGUI);
                command_router_->submit_command("set md.timestep " + std::to_string(md_timestep_), CommandSource::IMGUI);
                command_router_->submit_command("set md.damping " + std::to_string(damping_), CommandSource::IMGUI);
            }
        }
    }
    
    // PBC Controls (available for MD and Crystal modes)
    if (mode == SimMode::MD || mode == SimMode::CRYSTAL) {
        if (ImGui::CollapsingHeader("Periodic Boundary Conditions", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool changed = false;
            
            changed |= ImGui::Checkbox("Enable PBC", &use_pbc_);
            
            if (use_pbc_) {
                ImGui::Checkbox("Cubic Box", &pbc_cube_mode_);
                
                if (pbc_cube_mode_) {
                    if (ImGui::SliderFloat("Box Size", &box_x_, 5.0f, 100.0f, "%.1f Å")) {
                        box_y_ = box_x_;
                        box_z_ = box_x_;
                        changed = true;
                    }
                } else {
                    changed |= ImGui::SliderFloat("Box X", &box_x_, 5.0f, 100.0f, "%.1f Å");
                    changed |= ImGui::SliderFloat("Box Y", &box_y_, 5.0f, 100.0f, "%.1f Å");
                    changed |= ImGui::SliderFloat("Box Z", &box_z_, 5.0f, 100.0f, "%.1f Å");
                }
                // Remove buttons or make them optional.
                // Quick preset buttons
                ImGui::Text("Presets:");
                if (ImGui::Button("10 Å")) {
                    box_x_ = box_y_ = box_z_ = 10.0f;
                    changed = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("20 Å")) {
                    box_x_ = box_y_ = box_z_ = 20.0f;
                    changed = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("30 Å")) {
                    box_x_ = box_y_ = box_z_ = 30.0f;
                    changed = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("50 Å")) {
                    box_x_ = box_y_ = box_z_ = 50.0f;
                    changed = true;
                }
                
                // Info
                double volume = box_x_ * box_y_ * box_z_;
                ImGui::Text("Volume: %.1f Å³", volume);
            }
            
            if (changed) {
                // TODO: Re-enable PBC commands when CmdSet is implemented
                // sim_thread.send_command(CmdSet{"pbc.enabled", use_pbc_});
                // sim_thread.send_command(CmdSet{"pbc.box.x", static_cast<double>(box_x_)});
                // sim_thread.send_command(CmdSet{"pbc.box.y", static_cast<double>(box_y_)});
                // sim_thread.send_command(CmdSet{"pbc.box.z", static_cast<double>(box_z_)});
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
        if (command_router_) command_router_->submit_command(std::string("load ") + load_file_buf_, CommandSource::IMGUI);
    }
    
    ImGui::Separator();
    
    // Save
    ImGui::Text("Save Snapshot");
    ImGui::InputText("##save_file", save_file_buf_, sizeof(save_file_buf_));
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        if (command_router_) command_router_->submit_command(std::string("save ") + save_file_buf_, CommandSource::IMGUI);
    }
    
    ImGui::Separator();
    
    // Quick presets
    ImGui::Text("Quick Load");
    if (ImGui::Button("H2O")) {
        strcpy(load_file_buf_, "h2o.json");
        if (command_router_) command_router_->submit_command("load h2o.json", CommandSource::IMGUI);
    }
    ImGui::SameLine();
    if (ImGui::Button("CH4")) {
        strcpy(load_file_buf_, "ch4.json");
        if (command_router_) command_router_->submit_command("load ch4.json", CommandSource::IMGUI);
    }
    ImGui::SameLine();
    if (ImGui::Button("NH3")) {
        strcpy(load_file_buf_, "nh3.json");
        if (command_router_) command_router_->submit_command("load nh3.json", CommandSource::IMGUI);
    }
    
    ImGui::End();
}

void UIManager::render_visualization_panel(Renderer* renderer) {
    if (!renderer) return;
    
    ImGui::Begin("Visualization", &show_visualization_panel);
    
    if (ImGui::CollapsingHeader("Display Options", ImGuiTreeNodeFlags_DefaultOpen)) {
        static bool show_bonds = true;
        static bool show_box = false;
        static float atom_scale = 0.5f;  // Smaller default
        static float bond_radius = 0.15f;
        
        if (ImGui::Checkbox("Show Bonds", &show_bonds)) {
            renderer->set_show_bonds(show_bonds);
        }
        
        if (ImGui::Checkbox("Show PBC Box", &show_box)) {
            renderer->set_show_box(show_box);
        }
        
        if (ImGui::SliderFloat("Atom Scale", &atom_scale, 0.2f, 1.5f, "%.2f")) {
            renderer->set_atom_scale(atom_scale);
        }
        
        if (ImGui::SliderFloat("Bond Radius", &bond_radius, 0.05f, 0.5f, "%.2f Å")) {
            renderer->set_bond_radius(bond_radius);
        }
    }
    
    if (ImGui::CollapsingHeader("Background")) {
        static float bg_color[3] = {0.05f, 0.15f, 0.08f};  // Default GPU green
        
        if (ImGui::ColorEdit3("Color", bg_color)) {
            renderer->set_background_color(bg_color[0], bg_color[1], bg_color[2]);
        }
        
        // GPU-Accelerated Theme Presets
        ImGui::Text("GPU Themes:");
        if (ImGui::Button("Matrix Green")) {
            bg_color[0] = 0.05f; bg_color[1] = 0.15f; bg_color[2] = 0.08f;
            renderer->set_background_color(bg_color[0], bg_color[1], bg_color[2]);
        }
        ImGui::SameLine();
        if (ImGui::Button("Forest")) {
            bg_color[0] = 0.08f; bg_color[1] = 0.18f; bg_color[2] = 0.10f;
            renderer->set_background_color(bg_color[0], bg_color[1], bg_color[2]);
        }
        ImGui::SameLine();
        if (ImGui::Button("Emerald")) {
            bg_color[0] = 0.02f; bg_color[1] = 0.22f; bg_color[2] = 0.12f;
            renderer->set_background_color(bg_color[0], bg_color[1], bg_color[2]);
        }
        
        ImGui::Text("Classic:");
        if (ImGui::Button("Dark Blue")) {
            bg_color[0] = 0.1f; bg_color[1] = 0.1f; bg_color[2] = 0.15f;
            renderer->set_background_color(bg_color[0], bg_color[1], bg_color[2]);
        }
        ImGui::SameLine();
        if (ImGui::Button("Black")) {
            bg_color[0] = 0.0f; bg_color[1] = 0.0f; bg_color[2] = 0.0f;
            renderer->set_background_color(bg_color[0], bg_color[1], bg_color[2]);
        }
        ImGui::SameLine();
        if (ImGui::Button("White")) {
            bg_color[0] = 1.0f; bg_color[1] = 1.0f; bg_color[2] = 1.0f;
            renderer->set_background_color(bg_color[0], bg_color[1], bg_color[2]);
        }
    }
    
    if (ImGui::CollapsingHeader("Camera")) {
        ImGui::Text("Use mouse to control:");
        ImGui::BulletText("Left drag: Rotate");
        ImGui::BulletText("Right drag: Pan");
        ImGui::BulletText("Scroll: Zoom");
        ImGui::Separator();
        
        if (ImGui::Button("Reset Camera", ImVec2(-1, 0))) {
            renderer->camera().reset();
        }
    }
    
    ImGui::End();
}

void UIManager::render_command_console(SimulationThread& sim_thread) {
    // Terminal-style window with GPU green theme
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.08f, 0.04f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.03f, 0.10f, 0.05f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.9f, 0.3f, 0.6f));
    
    ImGui::Begin("Terminal", &show_command_console, ImGuiWindowFlags_MenuBar);
    
    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Terminal")) {
            if (ImGui::MenuItem("Clear History")) {
                console_log_.clear();
            }
            if (ImGui::MenuItem("Clear Output")) {
                command_output_.clear();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Close", "ESC")) {
                show_command_console = false;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // Calculate layout
    float window_height = ImGui::GetContentRegionAvail().y;
    float input_height = ImGui::GetFrameHeightWithSpacing() * 2.0f;
    float separator_height = 8.0f;
    float output_height = window_height * 0.45f;
    float history_height = window_height - output_height - input_height - separator_height * 2;
    
    // Terminal font style (monospace if available)
    ImGuiStyle& style = ImGui::GetStyle();
    float original_item_spacing_y = style.ItemSpacing.y;
    style.ItemSpacing.y = 1.0f;  // Tighter line spacing
    
    // ===== Command History Box =====
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::BeginChild("CommandHistory", ImVec2(0, history_height), 
                      true, ImGuiWindowFlags_HorizontalScrollbar);
    
    for (const auto& line : console_log_) {
        if (line.find("[ERROR]") == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", line.c_str());
        } else if (line.find("[INFO]") == 0) {
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", line.c_str());
        } else if (line.find(">") == 0) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", line.c_str());
        } else {
            ImGui::Text("%s", line.c_str());
        }
    }
    
    if (scroll_to_bottom_) {
        ImGui::SetScrollHereY(1.0f);
        scroll_to_bottom_ = false;
    }
    
    ImGui::EndChild();
    ImGui::PopStyleColor();
    
    // ===== Separator =====
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.2f, 0.8f, 0.2f, 0.8f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    
    // ===== Output Box (Terminal-style) =====
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::BeginChild("CommandOutput", ImVec2(0, output_height), 
                      true, ImGuiWindowFlags_HorizontalScrollbar);
    
    for (const auto& line : command_output_) {
        // Terminal color coding
        if (line.find("error:") != std::string::npos || line.find("Error") != std::string::npos || 
            line.find("ERROR") != std::string::npos) {
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", line.c_str());
        } else if (line.find("warning:") != std::string::npos || line.find("Warning") != std::string::npos ||
                   line.find("WARNING") != std::string::npos) {
            ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.0f, 1.0f), "%s", line.c_str());
        } else if (line.find("Exit code:") != std::string::npos) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", line.c_str());
        } else if (line.find("Built target") != std::string::npos || line.find("success") != std::string::npos) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", line.c_str());
        } else {
            ImGui::Text("%s", line.c_str());
        }
    }
    
    if (scroll_output_to_bottom_) {
        ImGui::SetScrollHereY(1.0f);
        scroll_output_to_bottom_ = false;
    }
    
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    
    // Restore spacing
    style.ItemSpacing.y = original_item_spacing_y;
    
    // ===== Separator =====
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.2f, 0.8f, 0.2f, 0.8f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    
    // ===== Command Input (Terminal-style) =====
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));  // Green text
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(0.1f, 0.5f, 0.1f, 0.5f));
    
    ImGui::PushItemWidth(-50);
    
    // Prompt symbol
    ImGui::Text("$");
    ImGui::SameLine();
    
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
            std::lock_guard<std::mutex> lock(console_mutex_);
            
            // Echo command to history
            console_log_.push_back("> " + command);
            scroll_to_bottom_ = true;
            
            // Clear previous output
            command_output_.clear();
            
            // Parse and execute command through CommandParser
            auto result = command_parser_.parse(command);
            
            if (std::holds_alternative<ParseSuccess>(result)) {
                auto& success = std::get<ParseSuccess>(result);
                if (command_router_) {
                    command_router_->submit_command(command, CommandSource::IMGUI);
                    command_output_.push_back("[OK] Command submitted");
                }
            } else {
                auto& error = std::get<ParseError>(result);
                command_output_.push_back("[ERROR] " + error.error_message);
                if (!error.suggestion.empty()) {
                    command_output_.push_back(error.suggestion);
                }
            }
            
            scroll_output_to_bottom_ = true;
            command_history_.add(command);
        }
        
        // Clear input and scroll
        command_input_buf_[0] = '\0';
        command_history_.reset_cursor();
        scroll_to_bottom_ = true;
        focus_command_input_ = true;
    }
    
    ImGui::PopItemWidth();
    ImGui::PopStyleColor(3);
    
    ImGui::End();
    ImGui::PopStyleColor(3);  // Restore window colors
}

void UIManager::render_command_console(SimulationThread& sim_thread, CommandRouter& command_router) {
    // Terminal-style window with GPU green theme
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.08f, 0.04f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.03f, 0.10f, 0.05f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.9f, 0.3f, 0.6f));
    
    ImGui::Begin("Terminal", &show_command_console, ImGuiWindowFlags_MenuBar);
    
    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Terminal")) {
            if (ImGui::MenuItem("Clear")) {
                command_router.clear_output_history();
            }
            if (ImGui::MenuItem("Show Help")) {
                command_router.submit_command("help", CommandSource::IMGUI);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Close", "ESC")) {
                show_command_console = false;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // Output area (scrollable)
    float input_height = ImGui::GetFrameHeightWithSpacing() * 2.0f;
    ImGui::BeginChild("ConsoleOutput", ImVec2(0, -input_height), 
                      true, ImGuiWindowFlags_HorizontalScrollbar);
    
    // Get output history from CommandRouter
    auto output_history = command_router.get_output_history();
    
    for (const auto& entry : output_history) {
        // Color code messages based on status
        ImVec4 color;
        
        switch (entry.status) {
            case ResultStatus::ERROR:
                color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
                break;
            case ResultStatus::OK:
                color = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
                break;
            case ResultStatus::INFO:
                color = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
                break;
            case ResultStatus::WARNING:
                color = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
                break;
            default:
                color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                break;
        }
        
        // Show cmd_id prefix for traceability
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%llu]", entry.cmd_id);
        ImGui::SameLine();
        ImGui::TextColored(color, "%s", entry.text.c_str());
    }
    
    // Auto-scroll to bottom if new content
    static size_t last_history_size = 0;
    if (output_history.size() != last_history_size) {
        ImGui::SetScrollHereY(1.0f);
        last_history_size = output_history.size();
    }
    
    ImGui::EndChild();
    
    // Separator
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.2f, 0.8f, 0.2f, 0.8f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    
    // Command Input
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(0.1f, 0.5f, 0.1f, 0.5f));
    
    ImGui::Text("$");
    ImGui::SameLine();
    
    ImGui::PushItemWidth(-50);
    
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
    
    ImGui::PopItemWidth();
    ImGui::PopStyleColor(3);
    
    ImGui::End();
    ImGui::PopStyleColor(3);  // Restore window colors
}

// ============================================================================
// GPU Status Panel
// ============================================================================

void UIManager::render_gpu_status_panel(Renderer* renderer) {
    (void)renderer;  // Will use for GPU stats later
    
    // Compact GPU status window with green theme
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.15f, 0.08f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.10f, 0.35f, 0.15f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.15f, 0.60f, 0.25f, 1.00f));
    
    ImGui::SetNextWindowSize(ImVec2(300, 240), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    
    if (!ImGui::Begin("GPU Accelerated", &show_gpu_status_panel, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        ImGui::PopStyleColor(3);
        return;
    }
    
    // OpenGL Status Section
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "OpenGL Status:");
    ImGui::Separator();
    
    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* renderer_name = glGetString(GL_RENDERER);
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* glsl_version = glGetString(GL_SHADING_LANGUAGE_VERSION);
    
    ImGui::Text("Vendor:");
    ImGui::SameLine(120);
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "%s", vendor ? (const char*)vendor : "Unknown");
    
    ImGui::Text("Renderer:");
    ImGui::SameLine(120);
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "%s", renderer_name ? (const char*)renderer_name : "Unknown");
    
    ImGui::Text("GL Version:");
    ImGui::SameLine(120);
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "%s", version ? (const char*)version : "Unknown");
    
    ImGui::Text("GLSL Version:");
    ImGui::SameLine(120);
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "%s", glsl_version ? (const char*)glsl_version : "Unknown");
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // GPU Capabilities
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "Capabilities:");
    ImGui::Separator();
    
    GLint max_texture_size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    ImGui::Text("Max Texture:");
    ImGui::SameLine(120);
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "%dx%d", max_texture_size, max_texture_size);
    
    GLint max_vertex_attribs = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attribs);
    ImGui::Text("Vertex Attribs:");
    ImGui::SameLine(120);
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "%d", max_vertex_attribs);
    
    // Performance indicator
    ImGui::Spacing();
    ImGui::Separator();
    float fps = ImGui::GetIO().Framerate;
    ImVec4 fps_color;
    if (fps >= 55.0f) {
        fps_color = ImVec4(0.3f, 1.0f, 0.4f, 1.0f);  // Bright green
    } else if (fps >= 30.0f) {
        fps_color = ImVec4(1.0f, 0.9f, 0.3f, 1.0f);  // Yellow
    } else {
        fps_color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // Red
    }
    
    ImGui::Text("FPS:");
    ImGui::SameLine(120);
    ImGui::TextColored(fps_color, "%.1f", fps);
    
    ImGui::End();
    ImGui::PopStyleColor(3);
}

// Removed legacy handlers - command console now uses simple ParseResult

} // namespace vsepr
