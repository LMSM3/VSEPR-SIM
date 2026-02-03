#include "cli/cli_commands.hpp"
#include <iostream>

namespace vsepr {
namespace cli {

CommandResult cmd_version(const std::vector<std::string>& args) {
    CommandResult result;
    result.exit_code = 0;
    
    std::cout << "VSEPR Simulator version 2.0.0\n";
    std::cout << "Built with C++20 support\n";
    std::cout << "Copyright (c) 2024\n";
    
    result.message = "Version displayed";
    return result;
}

} // namespace cli
} // namespace vsepr
