#include "cli/cli_commands.hpp"
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        vsepr::cli::cmd_help({});
        return 1;
    }
    
    std::string command = argv[1];
    std::vector<std::string> args;
    
    // Collect remaining arguments
    for (int i = 2; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    
    vsepr::cli::CommandResult result;
    
    // Route to appropriate command
    if (command == "help" || command == "-h" || command == "--help") {
        result = vsepr::cli::cmd_help(args);
    } else if (command == "version" || command == "-v" || command == "--version") {
        result = vsepr::cli::cmd_version(args);
    } else if (command == "build") {
        result = vsepr::cli::cmd_build(args);
    } else if (command == "viz") {
        result = vsepr::cli::cmd_viz(args);
    } else if (command == "therm") {
        result = vsepr::cli::cmd_therm(args);
    } else if (command == "webgl") {
        result = vsepr::cli::cmd_webgl(args);
    } else if (command == "stream") {
        result = vsepr::cli::cmd_stream(args);
    } else {
        std::cerr << "Unknown command: " << command << "\n";
        std::cerr << "Use 'vsepr help' for available commands\n";
        return 1;
    }
    
    return result.exit_code;
}
