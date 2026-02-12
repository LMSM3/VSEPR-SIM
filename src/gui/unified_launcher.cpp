/**
 * VSEPR-Sim Unified GUI Launcher - Implementation
 * Integrates GUI, batch, and shell operations
 */

#include "gui/unified_launcher.hpp"
#include "imgui.h"
#include <cstdlib>
#include <sstream>
#include <thread>
#include <chrono>

namespace vsepr {
namespace launcher {

// ============================================================================
// UnifiedLauncher Implementation
// ============================================================================

UnifiedLauncher::UnifiedLauncher()
    : current_mode_(AppMode::MAIN_VIEWER) {
    
    viewer_ = std::make_unique<gui::ImGuiVSEPRWindow>();
    pokedex_ = std::make_unique<pokedex::ImGuiPokedexBrowser>();
    
    setupPipes();
}

UnifiedLauncher::~UnifiedLauncher() = default;

void UnifiedLauncher::setupPipes() {
    status_pipe_ = std::make_shared<gui::DataPipe<std::string>>("launcher_status");
    progress_pipe_ = std::make_shared<gui::DataPipe<double>>("launcher_progress");
    job_pipe_ = std::make_shared<gui::DataPipe<BatchJob>>("launcher_job");
    
    // Connect pipes
    gui::PipeNetwork::instance().registerPipe("launcher_status", status_pipe_);
    gui::PipeNetwork::instance().registerPipe("launcher_progress", progress_pipe_);
    gui::PipeNetwork::instance().registerPipe("launcher_job", job_pipe_);
}

void UnifiedLauncher::render() {
    renderMenuBar();
    renderQuickActions();
    
    // Main content area
    switch (current_mode_) {
        case AppMode::MAIN_VIEWER:
            renderMainViewer();
            break;
        case AppMode::POKEDEX:
            renderPokedex();
            break;
        case AppMode::BATCH_RUNNER:
            renderBatchRunner();
            break;
        case AppMode::SHELL_TERMINAL:
            renderShellTerminal();
            break;
    }
    
    renderStatusBar();
    
    // Dialogs
    if (show_new_batch_dialog_) {
        renderNewBatchDialog();
    }
}

void UnifiedLauncher::renderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open XYZ", "Ctrl+O")) { }
            if (ImGui::MenuItem("Save", "Ctrl+S")) { }
            ImGui::Separator();
            if (ImGui::MenuItem("Preferences")) { }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) { }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Main Viewer", nullptr, current_mode_ == AppMode::MAIN_VIEWER)) {
                setMode(AppMode::MAIN_VIEWER);
            }
            if (ImGui::MenuItem("Pokedex", nullptr, current_mode_ == AppMode::POKEDEX)) {
                setMode(AppMode::POKEDEX);
            }
            if (ImGui::MenuItem("Batch Runner", nullptr, current_mode_ == AppMode::BATCH_RUNNER)) {
                setMode(AppMode::BATCH_RUNNER);
            }
            if (ImGui::MenuItem("Shell Terminal", nullptr, current_mode_ == AppMode::SHELL_TERMINAL)) {
                setMode(AppMode::SHELL_TERMINAL);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Show Quick Actions", nullptr, show_quick_actions_)) {
                show_quick_actions_ = !show_quick_actions_;
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Compute")) {
            if (ImGui::MenuItem("Build Molecule")) { }
            if (ImGui::MenuItem("Optimize Geometry")) { }
            if (ImGui::MenuItem("Calculate Energy")) { }
            ImGui::Separator();
            if (ImGui::MenuItem("New Batch Job", "Ctrl+B")) {
                show_new_batch_dialog_ = true;
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("LabelMe Phase States")) {
                executeShellCommand("./scripts/labelme.sh test");
            }
            if (ImGui::MenuItem("Random Discovery")) {
                executeShellCommand("./scripts/random_discovery.sh 10");
            }
            if (ImGui::MenuItem("Chemistry Demo")) {
                executeShellCommand("./scripts/batch_chemistry_demo.sh");
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Documentation")) { }
            if (ImGui::MenuItem("About")) { }
            ImGui::EndMenu();
        }
        
        // Status in menu bar
        ImGui::Spacing();
        ImGui::SameLine(ImGui::GetWindowWidth() - 250);
        
        int active_jobs = 0;
        for (const auto& job : active_jobs_) {
            if (job.running) active_jobs++;
        }
        
        if (active_jobs > 0) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), 
                             "⚡ %d jobs running", active_jobs);
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "● Ready");
        }
        
        ImGui::EndMainMenuBar();
    }
}

void UnifiedLauncher::renderQuickActions() {
    if (!show_quick_actions_) return;
    
    ImGui::SetNextWindowPos(ImVec2(10, 40));
    ImGui::SetNextWindowSize(ImVec2(200, 0), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Quick Actions", &show_quick_actions_, 
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Common Tasks");
        ImGui::Separator();
        
        if (ImGui::Button("🔨 Build H₂O", ImVec2(-1, 0))) {
            executeShellCommand("./build/bin/vsepr build H2O --optimize");
        }
        
        if (ImGui::Button("🧪 Test LabelMe", ImVec2(-1, 0))) {
            executeShellCommand("./scripts/labelme.sh test");
        }
        
        if (ImGui::Button("📊 Batch Discovery", ImVec2(-1, 0))) {
            show_new_batch_dialog_ = true;
        }
        
        if (ImGui::Button("🎮 Open Pokedex", ImVec2(-1, 0))) {
            setMode(AppMode::POKEDEX);
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Examples");
        ImGui::Separator();
        
        if (ImGui::Button("Water (H₂O)", ImVec2(-1, 0))) {
            executeShellCommand("./build/bin/vsepr build H2O");
        }
        
        if (ImGui::Button("Ammonia (NH₃)", ImVec2(-1, 0))) {
            executeShellCommand("./build/bin/vsepr build NH3");
        }
        
        if (ImGui::Button("Methane (CH₄)", ImVec2(-1, 0))) {
            executeShellCommand("./build/bin/vsepr build CH4");
        }
    }
    ImGui::End();
}

void UnifiedLauncher::renderMainViewer() {
    if (viewer_) {
        viewer_->render();
    }
}

void UnifiedLauncher::renderPokedex() {
    if (pokedex_) {
        pokedex_->render();
    }
}

void UnifiedLauncher::renderBatchRunner() {
    ImGui::SetNextWindowPos(ImVec2(220, 40));
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x - 230, 
                                    ImGui::GetIO().DisplaySize.y - 90));
    
    if (ImGui::Begin("Batch Job Runner", nullptr, ImGuiWindowFlags_NoMove)) {
        // Header
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Batch Job Manager");
        ImGui::SameLine(ImGui::GetWindowWidth() - 200);
        
        if (ImGui::Button("+ New Batch Job", ImVec2(180, 0))) {
            show_new_batch_dialog_ = true;
        }
        
        ImGui::Separator();
        
        // Job list
        renderBatchJobList();
    }
    ImGui::End();
}

void UnifiedLauncher::renderBatchJobList() {
    if (active_jobs_.empty()) {
        ImGui::TextDisabled("No active batch jobs");
        ImGui::Spacing();
        ImGui::Text("Create a new batch job to get started:");
        ImGui::Text("  • Discovery: Find new molecules");
        ImGui::Text("  • Optimization: Optimize geometries");
        ImGui::Text("  • Thermal: Analyze thermal properties");
        return;
    }
    
    for (auto& job : active_jobs_) {
        ImGui::PushID(job.id.c_str());
        
        // Job card
        ImGui::BeginChild(job.id.c_str(), ImVec2(0, 120), true);
        
        // Header
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "%s", job.type.c_str());
        ImGui::SameLine();
        ImGui::Text("- %s", job.formula.c_str());
        
        // Progress
        ImGui::ProgressBar(job.progress, ImVec2(-1, 0), job.status.c_str());
        
        // Details
        ImGui::Text("Completed: %d / %d runs", job.completed_runs, job.total_runs);
        ImGui::Text("Output: %s", job.output_dir.c_str());
        
        // Actions
        if (job.running) {
            if (ImGui::Button("⏸ Pause")) {
                stopBatchJob(job.id);
            }
        } else {
            if (ImGui::Button("▶ Resume")) {
                // Resume logic
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("📊 View Results")) {
            // Open results
        }
        
        ImGui::SameLine();
        if (ImGui::Button("🗑 Delete")) {
            // Delete job
        }
        
        ImGui::EndChild();
        
        ImGui::PopID();
    }
}

void UnifiedLauncher::renderNewBatchDialog() {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f - 200,
                                   ImGui::GetIO().DisplaySize.y * 0.5f - 150));
    ImGui::SetNextWindowSize(ImVec2(400, 300));
    
    if (ImGui::Begin("New Batch Job", &show_new_batch_dialog_, 
                    ImGuiWindowFlags_NoResize)) {
        
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Create New Batch Job");
        ImGui::Separator();
        
        // Job type
        static int job_type = 0;
        const char* types[] = {"Discovery", "Optimization", "Thermal"};
        ImGui::Combo("Job Type", &job_type, types, 3);
        
        // Formula
        ImGui::InputText("Formula", batch_formula_, sizeof(batch_formula_));
        ImGui::SameLine();
        if (ImGui::Button("?")) {
            ImGui::SetTooltip("Example: H2O, NH3, C2H6");
        }
        
        // Number of runs
        ImGui::SliderInt("Runs", &batch_runs_, 1, 100);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Actions
        if (ImGui::Button("Start Job", ImVec2(190, 0))) {
            startBatchJob(types[job_type], batch_formula_, batch_runs_);
            show_new_batch_dialog_ = false;
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(190, 0))) {
            show_new_batch_dialog_ = false;
        }
    }
    ImGui::End();
}

void UnifiedLauncher::renderShellTerminal() {
    ImGui::SetNextWindowPos(ImVec2(220, 40));
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x - 230,
                                    ImGui::GetIO().DisplaySize.y - 90));
    
    if (ImGui::Begin("Shell Terminal", nullptr, ImGuiWindowFlags_NoMove)) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Integrated Shell");
        ImGui::Separator();
        
        // Output area
        ImGui::BeginChild("ShellOutput", ImVec2(0, -60), true);
        renderShellOutput();
        ImGui::EndChild();
        
        // Input area
        renderShellInput();
    }
    ImGui::End();
}

void UnifiedLauncher::renderShellOutput() {
    for (const auto& cmd : shell_history_) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "$ %s", cmd.command.c_str());
        ImGui::TextWrapped("%s", cmd.output.c_str());
        if (cmd.exit_code != 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), 
                             "Exit code: %d", cmd.exit_code);
        }
        ImGui::Separator();
    }
}

void UnifiedLauncher::renderShellInput() {
    static char command_buf[256] = "";
    
    ImGui::Text("Command:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-80);
    
    if (ImGui::InputText("##command", command_buf, sizeof(command_buf), 
                        ImGuiInputTextFlags_EnterReturnsTrue)) {
        executeShellCommand(command_buf);
        command_buf[0] = '\0';
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Execute")) {
        executeShellCommand(command_buf);
        command_buf[0] = '\0';
    }
}

void UnifiedLauncher::renderStatusBar() {
    ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 25));
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 25));
    
    if (ImGui::Begin("StatusBar", nullptr,
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove)) {
        
        std::string status;
        if (status_pipe_->tryGet(status)) {
            ImGui::Text("%s", status.c_str());
        } else {
            ImGui::Text("Ready");
        }
    }
    ImGui::End();
}

void UnifiedLauncher::setMode(AppMode mode) {
    current_mode_ = mode;
    
    const char* mode_names[] = {"Main Viewer", "Pokedex", "Batch Runner", "Shell Terminal"};
    status_pipe_->push(std::string("Switched to ") + mode_names[static_cast<int>(mode)]);
}

void UnifiedLauncher::startBatchJob(const std::string& type, 
                                   const std::string& formula, 
                                   int runs) {
    BatchJob job;
    job.id = "job_" + std::to_string(std::time(nullptr));
    job.type = type;
    job.formula = formula;
    job.total_runs = runs;
    job.completed_runs = 0;
    job.running = true;
    job.progress = 0.0;
    job.status = "Starting...";
    job.output_dir = "outputs/batch_" + job.id;
    
    active_jobs_.push_back(job);
    
    status_pipe_->push("Started batch job: " + type + " for " + formula);
    
    // Simulate batch execution in background
    std::thread([this, job]() mutable {
        for (int i = 0; i < job.total_runs; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            job.completed_runs = i + 1;
            job.progress = (double)job.completed_runs / job.total_runs;
            job.status = "Running " + std::to_string(job.completed_runs) + "/" + 
                        std::to_string(job.total_runs);
        }
        job.running = false;
        job.status = "Complete";
    }).detach();
}

void UnifiedLauncher::stopBatchJob(const std::string& job_id) {
    for (auto& job : active_jobs_) {
        if (job.id == job_id) {
            job.running = false;
            job.status = "Paused";
            status_pipe_->push("Paused job: " + job_id);
            break;
        }
    }
}

void UnifiedLauncher::executeShellCommand(const std::string& command) {
    ShellCommand cmd;
    cmd.command = command;
    cmd.running = true;
    
    status_pipe_->push("Executing: " + command);
    
    // Execute command
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            cmd.output += buffer;
        }
        cmd.exit_code = pclose(pipe);
        cmd.running = false;
    } else {
        cmd.output = "Failed to execute command";
        cmd.exit_code = -1;
    }
    
    shell_history_.push_back(cmd);
    last_command_ = cmd;
    
    status_pipe_->push("Command complete: exit code " + std::to_string(cmd.exit_code));
}

} // namespace launcher
} // namespace vsepr
