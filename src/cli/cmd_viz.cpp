#include "cli/cli_commands.hpp"
#include <iostream>

namespace vsepr {
namespace cli {

CommandResult cmd_viz(const std::vector<std::string>& args) {
    CommandResult result;
    
    if (args.empty()) {
        std::cerr << "Error: No input file specified\n";
        std::cerr << "Usage: vsepr viz <file.xyz>\n";
        result.exit_code = 1;
        result.message = "Missing input file";
        return result;
    }
    
    // TODO: Implement visualization
    #ifdef BUILD_VISUALIZATION
    std::cout << "Visualizing: " << args[0] << "\n";
    std::cout << "[TODO] Visualization not yet implemented\n";
    #else
    std::cout << "Error: Visualization support not compiled in\n";
    std::cout << "Rebuild with -DBUILD_VIS=ON to enable visualization\n";
    result.exit_code = 2;
    result.message = "Visualization not available";
    return result;
    #endif
    
    result.exit_code = 0;
    result.message = "Viz command executed";
    return result;
}

} // namespace cli
} // namespace vsepr
