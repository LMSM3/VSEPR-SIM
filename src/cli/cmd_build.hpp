#pragma once
/**
 * cmd_build.hpp
 * -------------
 * Build command - create molecules from chemical formulas.
 */

#include "commands.hpp"

namespace vsepr {
namespace cli {

class BuildCommand : public Command {
public:
    int Execute(const std::vector<std::string>& args) override;
    std::string Name() const override;
    std::string Description() const override;
    std::string Help() const override;
};

}} // namespace vsepr::cli
