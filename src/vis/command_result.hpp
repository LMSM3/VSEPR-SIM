#pragma once
/**
 * command_result.hpp
 * ------------------
 * Command execution result system.
 * 
 * Architecture: Commands can produce multiple actions across different subsystems
 * - SimCommand: Sent to simulation thread
 * - UIAction: Handled by UI manager (show/hide panels, display info)
 * - Multiple actions from a single command (e.g., "run --steps 500" = set param + resume + schedule stop)
 */

#include "../sim/sim_command.hpp"
#include <string>
#include <vector>
#include <variant>

namespace vsepr {

// ============================================================================
// UI Actions (handled by UIManager, NOT sent to sim thread)
// ============================================================================

enum class PanelAction {
    SHOW,
    HIDE,
    TOGGLE
};

struct UIShowPanel {
    std::string panel_name;  // "diagnostics", "parameters", "console", etc.
    PanelAction action;
};

struct UIDisplayHelp {
    std::string help_text;
};

struct UIDisplayInfo {
    std::string message;
    bool is_error = false;
};

using UIAction = std::variant<
    UIShowPanel,
    UIDisplayHelp,
    UIDisplayInfo
>;

// ============================================================================
// Compound Action Result
// ============================================================================

struct CommandResult {
    std::vector<SimCommand> sim_commands;  // Commands for simulation thread
    std::vector<UIAction> ui_actions;      // Actions for UI manager
    std::vector<std::string> echo_lines;   // Console output messages
    bool success = true;
    
    // Convenience constructors
    static CommandResult error(const std::string& message, const std::string& suggestion = "");
    static CommandResult success(const std::string& message);
    static CommandResult sim_command(SimCommand cmd, const std::string& echo);
    static CommandResult ui_action(UIAction action, const std::string& echo = "");
    
    // Builder pattern for multi-action results
    CommandResult& add_sim(SimCommand cmd);
    CommandResult& add_ui(UIAction action);
    CommandResult& add_echo(const std::string& line);
};

} // namespace vsepr
