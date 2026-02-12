/**
 * command_parser.cpp (Minimal Implementation)
 * ------------------
 * Minimal command parser for visualization mode.
 * Supports basic commands needed for --viz sim mode.
 */

#include "vis/command_parser.hpp"
#include <sstream>
#include <algorithm>

namespace vsepr {

CommandParser::CommandParser() {
    // Register basic commands
    command_help_["run"] = "run - Resume simulation";
    command_help_["pause"] = "pause - Pause simulation";
    command_help_["resume"] = "resume - Resume simulation";  
    command_help_["step"] = "step [N] - Step simulation N times";
    command_help_["advance"] = "advance N - Advance N steps";
    command_help_["reset"] = "reset - Reset to initial state";
    command_help_["load"] = "load FILE - Load molecule from file";
    command_help_["save"] = "save FILE - Save current snapshot";
    command_help_["mode"] = "mode MODE - Set simulation mode (vsepr|optimize|md)";
    command_help_["set"] = "set KEY VALUE - Set parameter";
    command_help_["help"] = "help [COMMAND] - Show help";
    command_help_["exit"] = "exit - Exit program";
    command_help_["quit"] = "quit - Exit program";
    
    for (const auto& [cmd, _] : command_help_) {
        command_list_.push_back(cmd);
    }
    std::sort(command_list_.begin(), command_list_.end());
}

ParsedCommand CommandParser::tokenize(const std::string& command_line) const {
    ParsedCommand result;
    
    std::istringstream iss(command_line);
    std::string token;
    
    // First token is the verb
    if (!(iss >> token)) {
        return result;  // Empty command
    }
    result.verb = token;
    
    // Parse remaining tokens as args
    while (iss >> token) {
        if (token.substr(0, 2) == "--") {
            // Flag: --key value
            std::string key = token.substr(2);
            std::string next;
            if (iss >> next && next.substr(0, 2) != "--") {
                result.flags[key] = next;
            } else {
                result.switches.push_back(key);
            }
        } else {
            result.args.push_back(token);
        }
    }
    
    return result;
}

ParseResult CommandParser::parse(const std::string& command_line) {
    // Trim whitespace
    auto trimmed = command_line;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    
    if (trimmed.empty()) {
        return make_error("Empty command", "Type 'help' for available commands");
    }
    
    // Tokenize
    auto parsed = tokenize(trimmed);
    
    // Dispatch to specific parser
    if (parsed.verb == "run") return parse_run(parsed);
    if (parsed.verb == "pause") return parse_pause(parsed);
    if (parsed.verb == "resume") return parse_resume(parsed);
    if (parsed.verb == "step") return parse_step(parsed);
    if (parsed.verb == "advance") {
        int steps = parsed.args.empty() ? 1 : std::stoi(parsed.args[0]);
        return make_success(CmdSingleStep{steps}, "Advancing " + std::to_string(steps) + " steps");
    }
    if (parsed.verb == "reset") return parse_reset(parsed);
    if (parsed.verb == "load") return parse_load(parsed);
    if (parsed.verb == "save") return parse_save(parsed);
    if (parsed.verb == "mode") return parse_set_mode(parsed);
    if (parsed.verb == "set") return parse_set(parsed);
    if (parsed.verb == "help") return parse_help(parsed);
    if (parsed.verb == "exit" || parsed.verb == "quit") {
        return make_success(CmdShutdown{}, "Exiting");
    }
    
    // Handle yes/no/y/n as build commands for confirmation flow
    if (parsed.verb == "yes" || parsed.verb == "y" || 
        parsed.verb == "no" || parsed.verb == "n") {
        return make_success(CmdBuild{parsed.verb}, "Build: " + parsed.verb);
    }
    
    // Handle build commands (formulas)
    if (parsed.verb == "build") {
        if (parsed.args.empty()) {
            return make_error("Missing formula", "Usage: build <formula>");
        }
        return make_success(CmdBuild{parsed.args[0]}, "Build: " + parsed.args[0]);
    }
    
    // If it looks like a chemical formula, treat it as build command
    // (uppercase letter at start suggests element symbol)
    if (!parsed.verb.empty() && std::isupper(parsed.verb[0])) {
        return make_success(CmdBuild{parsed.verb}, "Build: " + parsed.verb);
    }
    
    // Unsupported command - provide helpful message
    if (parsed.verb == "optimize" || parsed.verb == "minimize") {
        return make_error("Command '" + parsed.verb + "' not yet implemented in viz mode", 
                         "These commands will be available in future releases");
    }
    
    return make_error("Unknown command: " + parsed.verb, "Type 'help' for available commands");
}

std::string CommandParser::get_help(const std::string& command) const {
    if (command.empty()) {
        std::ostringstream oss;
        oss << "Available commands:\n";
        oss << "  build FORMULA - Build molecule from chemical formula (e.g., H2O, CH4, NH3)\n";
        oss << "    Or just type the formula directly: H2O, CH4, SF6, etc.\n";
        oss << "    When prompted to create new molecule, type: yes/no or y/n\n";
        oss << "  help - Show this help\n";
        oss << "  load FILE - Load molecule\n";
        oss << "  mode (vsepr|optimize|md|idle) - Set simulation mode\n";
        oss << "  pause - Pause simulation\n";
        oss << "  reset - Reset to initial state\n";
        oss << "  resume - Resume simulation\n";
        oss << "  run - Resume simulation\n";
        oss << "  save FILE - Save snapshot\n";
        oss << "  set KEY VALUE - Set parameter\n";
        oss << "  step [N] - Step N times (default 1)\n";
        return oss.str();
    } else {
        auto it = command_help_.find(command);
        if (it != command_help_.end()) {
            return it->second;
        } else {
            return "Unknown command: " + command;
        }
    }
}

// ============================================================================
// Individual Command Parsers
// ============================================================================

ParseResult CommandParser::parse_run(const ParsedCommand&) {
    return make_success(CmdResume{}, "Resuming simulation");
}

ParseResult CommandParser::parse_pause(const ParsedCommand&) {
    return make_success(CmdPause{}, "Pausing simulation");
}

ParseResult CommandParser::parse_resume(const ParsedCommand&) {
    return make_success(CmdResume{}, "Resuming simulation");
}

ParseResult CommandParser::parse_step(const ParsedCommand& parsed) {
    int steps = 1;
    if (!parsed.args.empty()) {
        steps = std::stoi(parsed.args[0]);
    }
    return make_success(CmdSingleStep{steps}, "Stepping " + std::to_string(steps) + " times");
}

ParseResult CommandParser::parse_reset(const ParsedCommand& parsed) {
    std::string config = parsed.args.empty() ? "default" : parsed.args[0];
    int seed = 0;
    auto it = parsed.flags.find("seed");
    if (it != parsed.flags.end()) {
        seed = std::stoi(it->second);
    }
    return make_success(CmdReset{config, seed}, "Resetting to: " + config);
}

ParseResult CommandParser::parse_load(const ParsedCommand& parsed) {
    if (parsed.args.empty()) {
        return make_error("Missing filename", "Usage: load FILE");
    }
    return make_success(CmdLoad{parsed.args[0]}, "Loading: " + parsed.args[0]);
}

ParseResult CommandParser::parse_save(const ParsedCommand& parsed) {
    if (parsed.args.empty()) {
        return make_error("Missing filename", "Usage: save FILE");
    }
    return make_success(CmdSave{parsed.args[0], true}, "Saving to: " + parsed.args[0]);
}

ParseResult CommandParser::parse_set_mode(const ParsedCommand& parsed) {
    if (parsed.args.empty()) {
        return make_error("Missing mode", "Usage: mode (vsepr|optimize|md|crystal|idle)");
    }
    
    auto mode = parse_mode(parsed.args[0]);
    if (!mode) {
        return make_error("Invalid mode: " + parsed.args[0], 
                          "Valid modes: vsepr, optimize, md, crystal, idle");
    }
    
    return make_success(CmdSetMode{*mode}, "Setting mode to: " + parsed.args[0]);
}

ParseResult CommandParser::parse_set(const ParsedCommand& parsed) {
    if (parsed.args.size() < 2) {
        return make_error("Missing parameter", "Usage: set KEY VALUE");
    }
    
    std::string key = parsed.args[0];
    std::string value = parsed.args[1];
    
    // Use CmdSet - SimulationState will handle the parameter setting
    return make_success(CmdSet{key, value}, "Set " + key + " = " + value);
}

ParseResult CommandParser::parse_help(const ParsedCommand& parsed) {
    std::string topic;
    if (!parsed.args.empty()) {
        topic = parsed.args[0];
    }
    return make_error(get_help(topic), "");  // Use error for help text display
}

// ============================================================================
// Helper Functions
// ============================================================================

ParseSuccess CommandParser::make_success(SimCommand cmd, const std::string& msg) const {
    ParseSuccess success;
    success.command = cmd;
    success.echo = msg;
    return success;
}

ParseError CommandParser::make_error(const std::string& error, const std::string& hint) const {
    ParseError err;
    err.error_message = error;
    err.suggestion = hint;
    return err;
}

std::optional<SimMode> CommandParser::parse_mode(const std::string& str) const {
    if (str == "idle") return SimMode::IDLE;
    if (str == "vsepr") return SimMode::VSEPR;
    if (str == "optimize") return SimMode::OPTIMIZE;
    if (str == "md") return SimMode::MD;
    if (str == "crystal") return SimMode::CRYSTAL;
    return std::nullopt;
}

std::optional<int> CommandParser::parse_int(const std::string& str) const {
    try {
        return std::stoi(str);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<double> CommandParser::parse_double(const std::string& str) const {
    try {
        return std::stod(str);
    } catch (...) {
        return std::nullopt;
    }
}

// ============================================================================
// Command History
// ============================================================================

void CommandHistory::add(const std::string& cmd) {
    if (cmd.empty()) return;
    
    // Remove duplicates
    auto it = std::find(history_.begin(), history_.end(), cmd);
    if (it != history_.end()) {
        history_.erase(it);
    }
    
    history_.push_back(cmd);
    
    // Limit size
    if (history_.size() > max_size_) {
        history_.erase(history_.begin());
    }
    
    cursor_ = -1;
}

std::optional<std::string> CommandHistory::previous() {
    if (history_.empty()) return std::nullopt;
    
    if (cursor_ == -1) {
        cursor_ = history_.size() - 1;
    } else if (cursor_ > 0) {
        --cursor_;
    }
    
    return history_[cursor_];
}

std::optional<std::string> CommandHistory::next() {
    if (history_.empty() || cursor_ == -1) return std::nullopt;
    
    if (cursor_ < static_cast<int>(history_.size()) - 1) {
        ++cursor_;
    } else {
        cursor_ = -1;
        return std::nullopt;
    }
    
    return history_[cursor_];
}

void CommandHistory::reset_cursor() {
    cursor_ = -1;
}

} // namespace vsepr
