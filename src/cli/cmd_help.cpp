#include "cli/cli_commands.hpp"
#include <iostream>

namespace vsepr {
namespace cli {

CommandResult cmd_help(const std::vector<std::string>& args) {
    CommandResult result;
    result.exit_code = 0;
    
    std::cout << "VSEPR Simulator - Command Line Interface\n\n";
    std::cout << "Available commands:\n";
    std::cout << "  help     - Show this help message\n";
    std::cout << "  version  - Show version information\n";
    std::cout << "  build    - Build molecular structure from formula\n";
    std::cout << "  viz      - Visualize molecular structure\n";
    std::cout << "  therm    - Thermal pathway analysis\n";
    std::cout << "  webgl    - Export to WebGL format\n";
    std::cout << "  stream   - Stream molecular dynamics\n";
    std::cout << "\nUse: vsepr <command> [options]\n";
    
    result.message = "Help displayed";
    return result;
}

} // namespace cli
} // namespace vsepr
