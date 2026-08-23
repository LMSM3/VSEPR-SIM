#pragma once
/**
 * cmd_ui.hpp
 * ----------
 * CLI command: vsepr ui <sub-command> [args]
 *
 * Sub-commands (communicate over local TCP to a running desktop instance):
 *   showroom              Enter showroom / admin debug mode
 *   open-all              Raise every dock panel
 *   open-panel <name>     Raise the named dock panel (partial, case-insensitive)
 *   open-vsim  <path>     Open a .vsim script in the editor
 *   open-dynx  <path>     Open a .dynx replay archive
 *   open-xyz   <path>     Import an .xyz / .xyzFull file
 *   ping                  Check whether a desktop instance is listening
 *
 * Default IPC port: 47215  (override: VSEPR_IPC_PORT env var)
 */
#include "commands.hpp"
#include <string>
#include <vector>

namespace vsepr {
namespace cli {

class UiCommand : public Command
{
public:
    int         Execute(const std::vector<std::string>& args) override;
    std::string Name()        const override { return "ui"; }
    std::string Description() const override {
        return "Control the VSEPR desktop UI from the command line";
    }
    std::string Help() const override;

private:
    bool sendIpc(const std::string& json) const;
    static int ipcPort();
};

}} // namespace vsepr::cli