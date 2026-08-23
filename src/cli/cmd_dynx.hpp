#pragma once
/**
 * src/cli/cmd_dynx.hpp
 * ======================
 * WO-VSIM-DYNX-V1-B  |  Phase 9  |  v5.1.x
 *
 * CLI sub-commands:
 *   vsepr dynx inspect  <file.dynx>   — print metadata header
 *   vsepr dynx validate <file.dynx>   — structural validation
 */

#include "cli/commands.hpp"
#include <string>
#include <vector>

namespace vsepr {
namespace cli {

class DynxCommand : public Command {
public:
	std::string Name()        const override { return "dynx"; }
	std::string Description() const override {
		return "Inspect and validate .dynx session archive files (WO-VSIM-DYNX-V1-B)";
	}
	std::string Help()  const override;
	int         Execute(const std::vector<std::string>& args) override;

private:
	int cmd_inspect (const std::vector<std::string>& args);
	int cmd_validate(const std::vector<std::string>& args);
};

} // namespace cli
} // namespace vsepr
