#pragma once
/**
 * cmd_therm.hpp
 * -------------
 * Thermal properties analysis command.
 */

#include "commands.hpp"

namespace vsepr {
namespace cli {

class ThermCommand : public Command {
public:
    int Execute(const std::vector<std::string>& args) override;
    std::string Name() const override;
    std::string Description() const override;
    std::string Help() const override;
};

}} // namespace vsepr::cli
