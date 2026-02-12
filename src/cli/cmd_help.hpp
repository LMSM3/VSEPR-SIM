#pragma once
/**
 * cmd_help.hpp
 * -----------
 * Help command - display usage and available commands.
 */

#include "commands.hpp"

namespace vsepr {
namespace cli {

class HelpCommand : public Command {
public:
    int Execute(const std::vector<std::string>& args) override;
    std::string Name() const override;
    std::string Description() const override;
    std::string Help() const override;
};

}} // namespace vsepr::cli
