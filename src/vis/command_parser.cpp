/**
 * command_parser.cpp
 * ------------------
 * Simple command parser implementation.
 */

#include "command_parser.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace vsepr {

CommandParser::CommandParser() {
    command_help_["help"] = "help - Show this help";
    command_help_["build"] = "build FORMULA - Build molecule from chemical formula (e.g., H2O, CH4, NH3)";
    command_help_["yes"] = "yes/y - Confirm building new molecule";
    command_help_["no"] = "no/n - Cancel building new molecule";
    command_help_["run"] = "run - Resume simulation";
    command_help_["pause"] = "pause - Pause simulation";
    command_help_["resume"] = "resume - Resume simulation";
    command_help_["step"] = "step [N] - Step N times (default 1)";
    command_help_["reset"] = "reset - Reset to initial state";
    command_help_["load"] = "load FILE - Load molecule";
    command_help_["save"] = "save FILE - Save snapshot";
    command_help_["mode"] = "mode (vsepr|optimize|md|idle) - Set simulation mode";
    command_help_["set"] = "set KEY VALUE - Set parameter";
    
    for (const auto& [cmd, _] : command_help_) {
        command_list_.push_back(cmd);
    }
    std::sort(command_list_.begin(), command_list_.end());
}

ParsedCommand CommandParser::tokenize(const std::string& command_line) const {
    ParsedCommand result;
    std::istringstream iss(command_line);
    std::string token;
    
    if (!(iss >> token)) return result;
    result.verb = token;
    
    while (iss >> token) {
        if (token.substr(0, 2) == "--") {
            std::string key = token.substr(2);
            std::string next;
            auto pos = iss.tellg();
            if (iss >> next && next.substr(0, 2) != "--") {
                result.flags[key] = next;
            } else {
                result.switches.push_back(key);
                if (pos != -1) iss.seekg(pos);
            }
        } else {
            result.args.push_back(token);
        }
    }
    return result;
}

ParseResult CommandParser::parse(const std::string& command_line) {
    auto trimmed = command_line;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    
    if (trimmed.empty()) {
        return make_error("Empty command", "Type 'help' for available commands");
    }
    
    auto parsed = tokenize(trimmed);
    
    // Handle yes/no/y/n as build commands for confirmation flow
    if (parsed.verb == "yes" || parsed.verb == "y" || 
        parsed.verb == "no" || parsed.verb == "n") {
        return make_success(CmdBuild{parsed.verb}, "Build: " + parsed.verb);
    }
    
    if (parsed.verb == "help" || parsed.verb == "?") return parse_help(parsed);
    if (parsed.verb == "build") return parse_build(parsed);
    if (parsed.verb == "run" || parsed.verb == "resume") return parse_resume(parsed);
    if (parsed.verb == "pause") return parse_pause(parsed);
    if (parsed.verb == "step") return parse_step(parsed);
    if (parsed.verb == "reset") return parse_reset(parsed);
    if (parsed.verb == "load") return parse_load(parsed);
    if (parsed.verb == "save") return parse_save(parsed);
    if (parsed.verb == "mode") return parse_set_mode(parsed);
    if (parsed.verb == "set") return parse_set(parsed);
    
    // If it looks like a chemical formula, treat it as build command
    if (!parsed.verb.empty() && std::isupper(parsed.verb[0])) {
        return make_success(CmdBuild{parsed.verb}, "Build: " + parsed.verb);
    }
    
    return make_error("Unknown command: " + parsed.verb, "Type 'help' for commands");
}

std::string CommandParser::get_help(const std::string& command) const {
    if (command.empty()) {
        std::ostringstream oss;
        oss << "Available commands:\n";
        for (const auto& cmd : command_list_) {
            oss << "  " << command_help_.at(cmd) << "\n";
        }
        return oss.str();
    }
    auto it = command_help_.find(command);
    return (it != command_help_.end()) ? it->second : "Unknown command";
}

ParseResult CommandParser::parse_help(const ParsedCommand& p) {
    return make_error(get_help(), "");
}

ParseResult CommandParser::parse_run(const ParsedCommand& p) {
    return make_success(CmdResume{}, "Running simulation");
}

ParseResult CommandParser::parse_pause(const ParsedCommand& p) {
    return make_success(CmdPause{}, "Paused");
}

ParseResult CommandParser::parse_resume(const ParsedCommand& p) {
    return make_success(CmdResume{}, "Resumed");
}

ParseResult CommandParser::parse_build(const ParsedCommand& p) {
    if (p.args.empty()) return make_error("Missing formula", "Usage: build <formula>");
    
    std::string formula = p.args[0];
    int charge = 0;
    int seed = 0;
    GeometryGuess guess = GeometryGuess::VSEPR;
    
    // Parse optional flags
    auto charge_it = p.flags.find("charge");
    if (charge_it != p.flags.end()) {
        if (auto c = parse_int(charge_it->second)) charge = *c;
    }
    
    auto seed_it = p.flags.find("seed");
    if (seed_it != p.flags.end()) {
        if (auto s = parse_int(seed_it->second)) seed = *s;
    }
    
    return make_success(CmdBuild{formula, guess, charge, seed}, "Building " + formula);
}

ParseResult CommandParser::parse_step(const ParsedCommand& p) {
    int n = 1;
    if (!p.args.empty()) {
        auto val = parse_int(p.args[0]);
        if (val && *val > 0) n = *val;
    }
    return make_success(CmdSingleStep{n}, "Stepping " + std::to_string(n));
}

ParseResult CommandParser::parse_reset(const ParsedCommand& p) {
    return make_success(CmdReset{"default", 0}, "Reset");
}

ParseResult CommandParser::parse_load(const ParsedCommand& p) {
    if (p.args.empty()) return make_error("Missing filename");
    return make_success(CmdLoad{p.args[0]}, "Loading " + p.args[0]);
}

ParseResult CommandParser::parse_save(const ParsedCommand& p) {
    if (p.args.empty()) return make_error("Missing filename");
    bool snapshot = std::find(p.switches.begin(), p.switches.end(), "snapshot") != p.switches.end();
    return make_success(CmdSave{p.args[0], snapshot}, "Saving to " + p.args[0]);
}

ParseResult CommandParser::parse_set_mode(const ParsedCommand& p) {
    if (p.args.empty()) return make_error("Missing mode");
    auto mode = parse_mode(p.args[0]);
    if (!mode) return make_error("Invalid mode: " + p.args[0]);
    return make_success(CmdSetMode{*mode}, "Mode: " + p.args[0]);
}

ParseResult CommandParser::parse_set(const ParsedCommand& p) {
    if (p.args.size() < 2) return make_error("Usage: set KEY VALUE");
    
    std::string path = p.args[0];
    std::string value_str = p.args[1];
    
    ParamValue value;
    std::string val_lower = value_str;
    std::transform(val_lower.begin(), val_lower.end(), val_lower.begin(), ::tolower);
    
    if (val_lower == "true" || val_lower == "on") {
        value = true;
    } else if (val_lower == "false" || val_lower == "off") {
        value = false;
    } else if (auto d = parse_double(value_str)) {
        value = *d;
    } else if (auto i = parse_int(value_str)) {
        value = *i;
    } else {
        value = value_str;
    }
    
    return make_success(CmdSet{path, value}, "Set " + path);
}

// Helper functions
std::optional<double> CommandParser::parse_double(const std::string& s) const {
    try {
        size_t pos;
        double val = std::stod(s, &pos);
        if (pos == s.length()) return val;
    } catch (...) {}
    return std::nullopt;
}

std::optional<int> CommandParser::parse_int(const std::string& s) const {
    try {
        size_t pos;
        int val = std::stoi(s, &pos);
        if (pos == s.length()) return val;
    } catch (...) {}
    return std::nullopt;
}

std::optional<SimMode> CommandParser::parse_mode(const std::string& s) const {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "idle") return SimMode::IDLE;
    if (lower == "vsepr") return SimMode::VSEPR;
    if (lower == "optimize") return SimMode::OPTIMIZE;
    if (lower == "md") return SimMode::MD;
    if (lower == "crystal") return SimMode::CRYSTAL;
    return std::nullopt;
}

ParseError CommandParser::make_error(const std::string& msg, const std::string& suggestion) const {
    return ParseError{msg, suggestion};
}

ParseSuccess CommandParser::make_success(SimCommand cmd, const std::string& echo) const {
    return ParseSuccess{cmd, echo};
}

// Command history
CommandHistory::CommandHistory(size_t max_size) : max_size_(max_size), cursor_(-1) {}

void CommandHistory::add(const std::string& command) {
    if (history_.empty() || history_.back() != command) {
        history_.push_back(command);
        if (history_.size() > max_size_) {
            history_.erase(history_.begin());
        }
    }
    reset_cursor();
}

std::optional<std::string> CommandHistory::previous() {
    if (history_.empty()) return std::nullopt;
    if (cursor_ == -1) cursor_ = history_.size();
    if (cursor_ > 0) {
        cursor_--;
        return history_[cursor_];
    }
    return std::nullopt;
}

std::optional<std::string> CommandHistory::next() {
    if (cursor_ == -1 || cursor_ >= (int)history_.size() - 1) {
        cursor_ = -1;
        return std::nullopt;
    }
    cursor_++;
    return history_[cursor_];
}

void CommandHistory::reset_cursor() {
    cursor_ = -1;
}

} // namespace vsepr
