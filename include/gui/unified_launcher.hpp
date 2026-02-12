/**
 * VSEPR-Sim Unified GUI Launcher
 * Main entry point integrating all GUI features with batch/shell
 * Version: 2.3.1
 */

#pragma once

#include "gui/imgui_integration.hpp"
#include "gui/pokedex_gui.hpp"
#include "gui/context_menu.hpp"
#include "gui/data_pipe.hpp"
#include <memory>
#include <string>
#include <vector>

namespace vsepr {
namespace launcher {

// Application mode
enum class AppMode {
    MAIN_VIEWER,     // 3D molecular viewer
    POKEDEX,         // Molecule browser
    BATCH_RUNNER,    // Batch job manager
    SHELL_TERMINAL   // Integrated terminal
};

// Batch job status
struct BatchJob {
    std::string id;
    std::string type;        // "discovery", "optimization", "thermal"
    std::string formula;
    int total_runs = 0;
    int completed_runs = 0;
    bool running = false;
    double progress = 0.0;
    std::string status;
    std::string output_dir;
};

// Shell command execution
struct ShellCommand {
    std::string command;
    std::string output;
    int exit_code = 0;
    bool running = false;
};

// Unified launcher window
class UnifiedLauncher {
public:
    UnifiedLauncher();
    ~UnifiedLauncher();
    
    // Main render
    void render();
    
    // Mode switching
    void setMode(AppMode mode);
    AppMode currentMode() const { return current_mode_; }
    
    // Batch integration
    void startBatchJob(const std::string& type, const std::string& formula, int runs);
    void stopBatchJob(const std::string& job_id);
    std::vector<BatchJob> getActiveJobs() const { return active_jobs_; }
    
    // Shell integration
    void executeShellCommand(const std::string& command);
    ShellCommand getLastCommand() const { return last_command_; }
    
    // Data pipes
    void setupPipes();
    
private:
    // Render components
    void renderMenuBar();
    void renderMainViewer();
    void renderPokedex();
    void renderBatchRunner();
    void renderShellTerminal();
    void renderStatusBar();
    void renderQuickActions();
    
    // Batch operations
    void renderBatchJobList();
    void renderBatchJobDetails(const BatchJob& job);
    void renderNewBatchDialog();
    
    // Shell operations
    void renderShellHistory();
    void renderShellInput();
    void renderShellOutput();
    
    // Components
    std::unique_ptr<gui::ImGuiVSEPRWindow> viewer_;
    std::unique_ptr<pokedex::ImGuiPokedexBrowser> pokedex_;
    
    // State
    AppMode current_mode_;
    std::vector<BatchJob> active_jobs_;
    std::vector<ShellCommand> shell_history_;
    ShellCommand last_command_;
    
    // UI state
    bool show_new_batch_dialog_ = false;
    bool show_quick_actions_ = true;
    char batch_formula_[128] = "";
    int batch_runs_ = 10;
    
    // Data pipes
    std::shared_ptr<gui::DataPipe<std::string>> status_pipe_;
    std::shared_ptr<gui::DataPipe<double>> progress_pipe_;
    std::shared_ptr<gui::DataPipe<BatchJob>> job_pipe_;
};

// Quick action buttons
struct QuickAction {
    std::string label;
    std::string icon;
    std::function<void()> action;
    bool enabled = true;
};

// Quick actions panel
class QuickActionsPanel {
public:
    QuickActionsPanel(UnifiedLauncher* launcher);
    
    void render();
    
    void addAction(const QuickAction& action);
    
private:
    UnifiedLauncher* launcher_;
    std::vector<QuickAction> actions_;
};

} // namespace launcher
} // namespace vsepr
