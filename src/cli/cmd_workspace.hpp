#pragma once
/**
 * cmd_workspace.hpp — Qt Molecular Workstation subcommand
 *
 * Usage:  vsper workspace [<script.vsim>]
 *
 * Launches the WorkspaceWindow Qt host. If a .vsim script path is supplied
 * it is auto-loaded into the workspace on startup. Without Qt (build config
 * without BUILD_DESKTOP) the command prints an informative message and exits.
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
        return "Launch the Qt Molecular Workstation GUI";
    }

    std::string Help() const override {
        return R"(
USAGE:
  vsper workspace [<script.vsim>]

DESCRIPTION:
  Open the VSEPR Molecular Workstation Qt application.
  Provides an object tree, tabbed central views, a properties panel,
  and a command console in one unified window.

  Show directives declared in the .vsim script are honoured
  when [visual.workspace] enabled = true.

ARGUMENTS:
  <script.vsim>   Optional .vsim script to auto-load on startup.

EXAMPLES:
  vsper workspace
  vsper workspace scripts/gallery/calibration_htgr.vsim
  vsper workspace scripts/gallery/room_reactor.vsim
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
        std::cout << "[workspace] Qt workspace not compiled into this binary.\n"
                  << "  Rebuild with BUILD_DESKTOP=ON to enable.\n"
                  << "  Alternatively run the standalone vsper_workspace executable.\n";
        return 1;
#endif
    }
};

} // namespace cli
} // namespace vsepr
