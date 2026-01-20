#pragma once

#include <string>
#include <vector>

namespace vsepr {
namespace cli {

// CLI command result
struct CommandResult {
    int exit_code = 0;
    std::string message;
};

// Command function declarations
CommandResult cmd_help(const std::vector<std::string>& args);
CommandResult cmd_version(const std::vector<std::string>& args);
CommandResult cmd_build(const std::vector<std::string>& args);
CommandResult cmd_viz(const std::vector<std::string>& args);
CommandResult cmd_therm(const std::vector<std::string>& args);
CommandResult cmd_webgl(const std::vector<std::string>& args);
CommandResult cmd_stream(const std::vector<std::string>& args);

} // namespace cli
} // namespace vsepr
