#include "cli/cli_commands.hpp"
#include <iostream>

namespace vsepr {
namespace cli {

CommandResult cmd_build(const std::vector<std::string>& args) {
    CommandResult result;
    
    if (args.empty()) {
        std::cerr << "Error: No formula specified\n";
        std::cerr << "Usage: vsepr build <formula>\n";
        result.exit_code = 1;
        result.message = "Missing formula";
        return result;
    }
    
    // TODO: Implement molecular structure building
    std::cout << "Building structure for: " << args[0] << "\n";
    std::cout << "[TODO] Structure building not yet implemented\n";
    
    result.exit_code = 0;
    result.message = "Build command executed";
    return result;
}

} // namespace cli
} // namespace vsepr
