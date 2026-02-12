#pragma once
/**
 * command_parser.hpp
 * ------------------
 * Simple command parser for simulation control.
 * Parses text commands and converts to SimCommand variants.
 */

#include "../sim/sim_command.hpp"
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <variant>

namespace vsepr {

// ============================================================================
// Parsed Command (internal)
// ============================================================================

struct ParsedCommand {
    std::string verb;
    std::vector<std::string> args;
    std::unordered_map<std::string, std::string> flags;
    std::vector<std::string> switches;
};

// ============================================================================
// Parse Result
// ============================================================================

struct ParseSuccess {
    SimCommand command;
    std::string echo;
};

struct ParseError {
    std::string error_message;
    std::string suggestion;
};

using ParseResult = std::variant<ParseSuccess, ParseError>;

// ============================================================================
// Command Parser
// ============================================================================

class CommandParser {
public:
    CommandParser();
    
    ParseResult parse(const std::string& command_line);
    std::string get_help(const std::string& command = "") const;
    
private:
    ParsedCommand tokenize(const std::string& command_line) const;
    
    // Individual parsers
    ParseResult parse_run(const ParsedCommand& p);
    ParseResult parse_pause(const ParsedCommand& p);
    ParseResult parse_resume(const ParsedCommand& p);
    ParseResult parse_build(const ParsedCommand& p);
    ParseResult parse_step(const ParsedCommand& p);
    ParseResult parse_reset(const ParsedCommand& p);
    ParseResult parse_load(const ParsedCommand& p);
    ParseResult parse_save(const ParsedCommand& p);
    ParseResult parse_set_mode(const ParsedCommand& p);
    ParseResult parse_set(const ParsedCommand& p);
    ParseResult parse_help(const ParsedCommand& p);
    
    // Helpers
    std::optional<double> parse_double(const std::string& s) const;
    std::optional<int> parse_int(const std::string& s) const;
    std::optional<SimMode> parse_mode(const std::string& s) const;
    
    ParseError make_error(const std::string& msg, const std::string& suggestion = "") const;
    ParseSuccess make_success(SimCommand cmd, const std::string& echo) const;
    
    std::unordered_map<std::string, std::string> command_help_;
    std::vector<std::string> command_list_;
};

// ============================================================================
// Command History
// ============================================================================

class CommandHistory {
public:
    CommandHistory(size_t max_size = 1000);
    
    void add(const std::string& command);
    std::optional<std::string> previous();
    std::optional<std::string> next();
    void reset_cursor();
    
    const std::vector<std::string>& get_all() const { return history_; }
    
private:
    std::vector<std::string> history_;
    size_t max_size_;
    int cursor_;  // -1 means at the end (new input)
};

} // namespace vsepr
