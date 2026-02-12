#pragma once
/**
 * commands.hpp
 * ------------
 * Command interface for the unified CLI.
 */

#include <string>
#include <vector>

namespace vsepr {
namespace cli {

/**
 * Base interface for CLI commands.
 * Each command implements Execute() and provides help text.
 */
class Command {
public:
    virtual ~Command() = default;
    
    // Execute the command with given arguments
    virtual int Execute(const std::vector<std::string>& args) = 0;
    
    // Get command name
    virtual std::string Name() const = 0;
    
    // Get brief description
    virtual std::string Description() const = 0;
    
    // Get detailed help text
    virtual std::string Help() const = 0;
};

}} // namespace vsepr::cli
