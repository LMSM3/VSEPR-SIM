#pragma once
/**
 * cmd_workspace.hpp - archived workspace compatibility subcommand
 *
 * Usage:  vsper workspace [<script.vsim>]
 *
 * The former Qt workspace is archived. Interactive work is handled by the
 * supported fixed-timestep live viewer.
 *
 * WO-VSIM-VIS-OVERHAUL-01
 */

#include "cli/commands.hpp"
#include <string>
#include <vector>
#include <iostream>

namespace vsepr {
namespace cli {

class WorkspaceCommand : public Command {
public:
    std::string Name() const override { return "workspace"; }
    std::string Description() const override {
        return "Show the supported live viewer route";
    }

    std::string Help() const override {
        return R"(
USAGE:
  vsepr workspace [<artifact>]

DESCRIPTION:
  The former Qt workspace is archived. Use the supported live viewer instead.

ARGUMENTS:
  <artifact>      Optional artifact to pass to vsepr-view.

EXAMPLES:
  vsepr-view
  vsepr-view --artifact output.xyz
)";
    }

    int Execute(const std::vector<std::string>& args) override {
#if defined(VSIM_WORKSPACE_AVAILABLE)
        // Resolved at link time when vsim_workspace is linked into this binary.
        extern int workspace_main(int, char**);

        // Re-synthesise an argv array for Qt (QApplication wants argc/argv).
        std::vector<std::string> fake_argv_s = { "vsper" };
        for (const auto& a : args) fake_argv_s.push_back(a);

        std::vector<char*> fake_argv;
        fake_argv.reserve(fake_argv_s.size());
        for (auto& s : fake_argv_s) fake_argv.push_back(s.data());

        int fake_argc = static_cast<int>(fake_argv.size());
        return workspace_main(fake_argc, fake_argv.data());
#else
        (void)args;
        std::cout << "[workspace] The Qt workspace is archived.\n"
                  << "  Use vsepr-view for supported interactive inspection.\n";
        return 1;
#endif
    }
};

} // namespace cli
} // namespace vsepr
