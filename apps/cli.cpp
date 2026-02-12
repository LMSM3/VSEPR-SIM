/**
 * cli.cpp
 * -------
 * Main CLI router - unified entry point for all VSEPR-Sim functionality.
 * 
 * Architecture: Single command gateway with modular subcommands
 * Pattern: vsepr <command> [subcommand] [options]
 */

#include "cli/display.hpp"
#include "cli/commands.hpp"
#include "cli/cmd_help.hpp"
#include "cli/cmd_version.hpp"
#include "cli/cmd_build.hpp"
#include "cli/cmd_viz.hpp"
#include "cli/cmd_therm.hpp"
#include <iostream>
#include <memory>
#include <map>
#include <vector>
#include <string>

using namespace vsepr;
using namespace vsepr::cli;

// Command registry
class CommandRegistry {
    std::map<std::string, std::unique_ptr<Command>> commands;
    
public:
    CommandRegistry() {
        // Register all available commands
        Register(std::make_unique<HelpCommand>());
        Register(std::make_unique<VersionCommand>());
        Register(std::make_unique<BuildCommand>());
        Register(std::make_unique<VizCommand>());
        Register(std::make_unique<ThermCommand>());
    }
    
    void Register(std::unique_ptr<Command> cmd) {
        if (cmd) {
            commands[cmd->Name()] = std::move(cmd);
        }
    }
    
    Command* Get(const std::string& name) const {
        auto it = commands.find(name);
        return (it != commands.end()) ? it->second.get() : nullptr;
    }
    
    std::vector<std::string> GetCommandNames() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : commands) {
            names.push_back(name);
        }
        return names;
    }
};

int main(int argc, char* argv[]) {
    try {
        // Create command registry
        CommandRegistry registry;
        
        // Handle no arguments - show help
        if (argc < 2) {
            HelpCommand help;
            return help.Execute({});
        }
        
        std::string cmd_name = argv[1];
        
        // Handle --viz as special global flag (shortcut for 'viz sim')
        if (cmd_name == "--viz" || cmd_name == "-viz") {
            VizCommand viz;
            std::vector<std::string> viz_args = {"sim"};
            // Add remaining args
            for (int i = 2; i < argc; ++i) {
                viz_args.push_back(argv[i]);
            }
            return viz.Execute(viz_args);
        }
        
        // Check for help flags
        if (cmd_name == "-h" || cmd_name == "--help" || cmd_name == "help") {
            HelpCommand help;
            return help.Execute({});
        }
        
        // Check for version flags
        if (cmd_name == "-v" || cmd_name == "--version" || cmd_name == "version") {
            VersionCommand version;
            return version.Execute({});
        }
        
        // Look up command
        Command* cmd = registry.Get(cmd_name);
        if (!cmd) {
            Display::Error("Unknown command: " + cmd_name);
            Display::Info("Run 'vsepr help' for usage information");
            return 1;
        }
        
        // Check if help was requested for this command
        if (argc >= 3) {
            std::string next_arg = argv[2];
            if (next_arg == "-h" || next_arg == "--help") {
                Display::Header(cmd_name + " command");
                Display::BlankLine();
                std::cout << cmd->Help();
                Display::BlankLine();
                return 0;
            }
        }
        
        // Collect remaining arguments
        std::vector<std::string> args;
        for (int i = 2; i < argc; ++i) {
            args.push_back(argv[i]);
        }
        
        // Execute command
        return cmd->Execute(args);
        
    } catch (const std::exception& e) {
        Display::Error("Fatal error: " + std::string(e.what()));
        return 1;
    }
}
