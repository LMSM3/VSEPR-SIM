// GUI Main Window - Professional window manager implementation
// Integrates: WindowManager + TUI backend + Crystal objects

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "ui/WindowManager.hpp"
#include "data/Crystal.hpp"
#include <iostream>
#include <memory>
#include <vector>

using namespace vsepr::ui;
using namespace vsepr::data;

class VSEPRMainWindow {
public:
    VSEPRMainWindow() : wm_(1920, 1080) {
        // Initialize with VM0 (default microscope)
        wm_.set_viewmodel(0, 0);
        
        // Create 4 default subwindows (snapped to corners)
        win_tl_ = wm_.add_window(WindowMode::Snapped, Corner::TopLeft);
        win_tr_ = wm_.add_window(WindowMode::Snapped, Corner::TopRight);
        win_bl_ = wm_.add_window(WindowMode::Snapped, Corner::BottomLeft);
        win_br_ = wm_.add_window(WindowMode::Snapped, Corner::BottomRight);
    }
    
    bool init() {
        // Initialize GLFW
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW\n";
            return false;
        }
        
        // OpenGL 3.3 Core
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        
        // Create window
        window_ = glfwCreateWindow(1920, 1080, "VSEPR — Professional Molecular Visualization", nullptr, nullptr);
        if (!window_) {
            std::cerr << "Failed to create GLFW window\n";
            glfwTerminate();
            return false;
        }
        
        glfwMakeContextCurrent(window_);
        glfwSwapInterval(1); // VSync
        
        // Initialize GLEW
        if (glewInit() != GLEW_OK) {
            std::cerr << "Failed to initialize GLEW\n";
            return false;
        }
        
        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        
        // Style
        ImGui::StyleColorsDark();
        apply_microscope_theme();
        
        // ImGui platform/renderer bindings
        ImGui_ImplGlfw_InitForOpenGL(window_, true);
        ImGui_ImplOpenGL3_Init("#version 330");
        
        return true;
    }
    
    void run() {
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
            
            // Handle window resize
            int w, h;
            glfwGetFramebufferSize(window_, &w, &h);
            if (w != window_w_ || h != window_h_) {
                window_w_ = w;
                window_h_ = h;
                wm_.set_window_size(w, h);
            }
            
            // Start ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
            // Render UI
            render_frame();
            
            // Render ImGui
            ImGui::Render();
            glViewport(0, 0, window_w_, window_h_);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            
            glfwSwapBuffers(window_);
        }
    }
    
    void shutdown() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
    
private:
    void render_frame() {
        // Main menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open XYZ...")) { open_file_dialog(); }
                if (ImGui::MenuItem("Save XYZ")) { save_current_crystal(); }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) { glfwSetWindowShouldClose(window_, true); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::BeginMenu("ViewModel")) {
                    for (int i = 0; i < 8; ++i) {
                        char label[32];
                        snprintf(label, sizeof(label), "VM%d", i);
                        if (ImGui::MenuItem(label, nullptr, current_vm_ == i)) {
                            current_vm_ = i;
                            wm_.set_viewmodel(i, vm_tune_iterations_);
                        }
                    }
                    ImGui::Separator();
                    ImGui::SliderInt("Tune Iterations", &vm_tune_iterations_, 0, 10);
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Fullscreen TL", "F1")) { wm_.toggle_fullscreen(win_tl_); }
                if (ImGui::MenuItem("Fullscreen TR", "F2")) { wm_.toggle_fullscreen(win_tr_); }
                if (ImGui::MenuItem("Fullscreen BL", "F3")) { wm_.toggle_fullscreen(win_bl_); }
                if (ImGui::MenuItem("Fullscreen BR", "F4")) { wm_.toggle_fullscreen(win_br_); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Tools")) {
                if (ImGui::MenuItem("Annotate (xyzA)")) { annotate_current(); }
                if (ImGui::MenuItem("Supercell (xyzC)")) { show_supercell_dialog_ = true; }
                if (ImGui::MenuItem("Watch Mode", nullptr, watch_enabled_)) { toggle_watch(); }
                ImGui::Separator();
                if (ImGui::MenuItem("Formation Frequency...")) { show_formation_dialog_ = true; }
                if (ImGui::MenuItem("Baseline Generation...")) { show_baseline_dialog_ = true; }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About")) { show_about_ = true; }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        
        // Get workspace and instrument rects
        Rect workspace = wm_.workspace_rect();
        Rect instrument = wm_.instrument_rect();
        
        // Render workspace (left 65%)
        render_workspace(workspace);
        
        // Render instrument stack (right 35%)
        render_instrument_stack(instrument);
        
        // Render bottom run bar
        render_run_bar();
        
        // Dialogs
        if (show_supercell_dialog_) render_supercell_dialog();
        if (show_formation_dialog_) render_formation_dialog();
        if (show_baseline_dialog_) render_baseline_dialog();
        if (show_about_) render_about();
    }
    
    void render_workspace(Rect ws) {
        // Workspace background
        ImGui::SetNextWindowPos(ImVec2(ws.x, ws.y));
        ImGui::SetNextWindowSize(ImVec2(ws.w, ws.h));
        ImGui::Begin("Workspace", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
        
        // Render subwindows
        for (const auto& win : wm_.windows()) {
            render_subwindow(win);
        }
        
        ImGui::End();
    }
    
    void render_subwindow(const WindowState& win) {
        if (!win.visible) return;
        
        ImGui::SetNextWindowPos(ImVec2(win.rect.x, win.rect.y));
        ImGui::SetNextWindowSize(ImVec2(win.rect.w, win.rect.h));
        
        char title[64];
        snprintf(title, sizeof(title), "Subwindow %u###sub%u", win.id, win.id);
        
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
        if (win.mode == WindowMode::Fullscreen) {
            flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        }
        
        ImGui::Begin(title, nullptr, flags);
        
        // Content based on subwindow ID
        if (win.id == win_tl_) {
            render_structure_view();
        } else if (win.id == win_tr_) {
            render_property_plots();
        } else if (win.id == win_bl_) {
            render_crystal_grid();
        } else if (win.id == win_br_) {
            render_animation_player();
        }
        
        // Handle dragging
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            // TODO: Update WindowManager
        }
        
        ImGui::End();
    }
    
    void render_structure_view() {
        ImGui::Text("Structure View (Ball & Stick)");
        if (current_crystal_) {
            ImGui::Text("File: %s", current_crystal_->xyz_path.c_str());
            ImGui::Text("Atoms: %zu", current_crystal_->atoms.size());
            // TODO: Render 3D structure
        } else {
            ImGui::TextDisabled("No structure loaded");
        }
    }
    
    void render_property_plots() {
        ImGui::Text("Property Plots");
        // TODO: Energy, RDF, etc.
    }
    
    void render_crystal_grid() {
        ImGui::Text("Crystal Grid");
        // TODO: Unit cell visualization
    }
    
    void render_animation_player() {
        ImGui::Text("Animation Player");
        // TODO: Trajectory playback
    }
    
    void render_instrument_stack(Rect instr) {
        ImGui::SetNextWindowPos(ImVec2(instr.x, instr.y));
        ImGui::SetNextWindowSize(ImVec2(instr.w, instr.h));
        ImGui::Begin("Instrument Stack", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        
        // Command Panel (TUI wrapper)
        if (ImGui::CollapsingHeader("Command Panel", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Button("Build All")) { run_tui_command("1"); }
            if (ImGui::Button("Build Status")) { run_tui_command("2"); }
            if (ImGui::Button("Run CTest")) { run_tui_command("3"); }
            ImGui::Separator();
            if (ImGui::Button("Problem 1")) { run_tui_command("4"); }
            if (ImGui::Button("Problem 2")) { run_tui_command("5"); }
            if (ImGui::Button("QA Tests")) { run_tui_command("6"); }
        }
        
        // Parameters
        if (ImGui::CollapsingHeader("Parameters")) {
            ImGui::InputInt("Molecules/run", &formation_count_);
            ImGui::InputInt("Runs", &formation_runs_);
            ImGui::InputInt("Seed", &formation_seed_);
        }
        
        // Output Log
        if (ImGui::CollapsingHeader("Output Log", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::BeginChild("LogScroll", ImVec2(0, 200), true);
            ImGui::TextUnformatted(output_log_.c_str());
            if (auto_scroll_) ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
        }
        
        ImGui::End();
    }
    
    void render_run_bar() {
        float bar_h = 40.0f;
        ImGui::SetNextWindowPos(ImVec2(0, window_h_ - bar_h));
        ImGui::SetNextWindowSize(ImVec2(window_w_, bar_h));
        ImGui::Begin("Run Bar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
        
        if (ImGui::Button("Start")) { start_operation(); }
        ImGui::SameLine();
        if (ImGui::Button("Pause")) { pause_operation(); }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) { stop_operation(); }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) { reset_operation(); }
        ImGui::SameLine();
        ImGui::ProgressBar(progress_, ImVec2(-1, 0));
        
        ImGui::End();
    }
    
    void render_supercell_dialog() {
        ImGui::OpenPopup("Supercell");
        if (ImGui::BeginPopupModal("Supercell", &show_supercell_dialog_)) {
            ImGui::InputInt("a", &supercell_a_);
            ImGui::InputInt("b", &supercell_b_);
            ImGui::InputInt("c", &supercell_c_);
            if (ImGui::Button("Generate")) {
                generate_supercell(supercell_a_, supercell_b_, supercell_c_);
                show_supercell_dialog_ = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                show_supercell_dialog_ = false;
            }
            ImGui::EndPopup();
        }
    }
    
    void render_formation_dialog() {
        // TODO: Formation frequency dialog
        show_formation_dialog_ = false;
    }
    
    void render_baseline_dialog() {
        // TODO: Baseline generation dialog
        show_baseline_dialog_ = false;
    }
    
    void render_about() {
        ImGui::OpenPopup("About");
        if (ImGui::BeginPopupModal("About", &show_about_)) {
            ImGui::Text("VSEPR Professional Molecular Visualization");
            ImGui::Text("Version 2.0.0");
            ImGui::Separator();
            ImGui::Text("Window Manager: 8 ViewModels with ±10%% tuning");
            ImGui::Text("Crystal System: xyzZ → xyzA → xyzC");
            ImGui::Text("Backend: TUI (tui.py) + Python tools");
            ImGui::EndPopup();
        }
    }
    
    void apply_microscope_theme() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.FrameRounding = 3.0f;
        style.ScrollbarRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.WindowPadding = ImVec2(8, 8);
        style.FramePadding = ImVec2(4, 3);
        
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
    }
    
    void open_file_dialog() {
        // TODO: Native file dialog or ImGuiFileDialog
    }
    
    void save_current_crystal() {
        if (current_crystal_) {
            current_crystal_->save_xyz(current_crystal_->xyz_path);
        }
    }
    
    void annotate_current() {
        if (current_crystal_) {
            current_crystal_->get_bonds(); // Infer bonds
            std::string xyzA_path = current_crystal_->xyz_path;
            xyzA_path.replace(xyzA_path.find(".xyz"), 4, ".xyzA");
            current_crystal_->save_xyzA(xyzA_path);
            log("Annotated: " + xyzA_path);
        }
    }
    
    void generate_supercell(int a, int b, int c) {
        // TODO: Call atomistic-sim or construct directly
        log("Generating supercell " + std::to_string(a) + "×" + std::to_string(b) + "×" + std::to_string(c));
    }
    
    void toggle_watch() {
        watch_enabled_ = !watch_enabled_;
        // TODO: Start/stop CrystalWatcher
    }
    
    void run_tui_command(const std::string& cmd) {
        // TODO: Pipe command to tui.py subprocess
        log("Running TUI command: " + cmd);
    }
    
    void log(const std::string& msg) {
        output_log_ += msg + "\n";
        auto_scroll_ = true;
    }
    
    void start_operation() { log("Start"); }
    void pause_operation() { log("Pause"); }
    void stop_operation() { log("Stop"); }
    void reset_operation() { log("Reset"); progress_ = 0.0f; }
    
    GLFWwindow* window_ = nullptr;
    int window_w_ = 1920;
    int window_h_ = 1080;
    
    WorkspaceLayoutEngine wm_;
    uint32_t win_tl_, win_tr_, win_bl_, win_br_;
    
    int current_vm_ = 0;
    int vm_tune_iterations_ = 0;
    
    std::unique_ptr<Crystal> current_crystal_;
    bool watch_enabled_ = false;
    
    bool show_supercell_dialog_ = false;
    bool show_formation_dialog_ = false;
    bool show_baseline_dialog_ = false;
    bool show_about_ = false;
    
    int supercell_a_ = 3, supercell_b_ = 3, supercell_c_ = 3;
    int formation_count_ = 250, formation_runs_ = 10, formation_seed_ = 42;
    
    std::string output_log_;
    bool auto_scroll_ = true;
    float progress_ = 0.0f;
};

int main() {
    VSEPRMainWindow app;
    
    if (!app.init()) {
        return 1;
    }
    
    app.run();
    app.shutdown();
    
    return 0;
}
