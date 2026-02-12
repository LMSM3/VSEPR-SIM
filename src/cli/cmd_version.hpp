#pragma once
/**
 * cmd_version.hpp
 * ---------------
 * Version command - display version and build information.
 */

#include "commands.hpp"

namespace vsepr {
namespace cli {

class VersionCommand : public Command {
public:
    int Execute(const std::vector<std::string>& args) override;
    std::string Name() const override;
    std::string Description() const override;
    std::string Help() const override;
};

}} // namespace vsepr::cli
