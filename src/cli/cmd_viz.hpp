#pragma once
/**
 * cmd_viz.hpp - Interactive visualization command
 * 
 * Usage: vsepr --viz sim
 * 
 * Creates an active OpenGL window with real-time updates.
 * Perfect for batch tasks: run commands via stdin and watch live updates.
 * 
 * Example workflow:
 *   ./vsepr --viz sim
 *   > build H2O
 *   > optimize
 *   > build CH4
 *   > optimize
 *   ... (window updates in real-time)
 */

#include "cli/commands.hpp"
#include <string>
#include <vector>

namespace vsepr {
namespace cli {

class VizCommand : public Command {
public:
    std::string Name() const override { return "viz"; }
    std::string Description() const override { 
        return "Launch interactive visualization session";
    }
    
    std::string Help() const override {
        return R"(
USAGE:
  vsepr --viz sim [options]

DESCRIPTION:
  Launch interactive visualization window with command interface.
  Perfect for batch geometry optimization tasks with real-time visual feedback.

OPTIONS:
  --width <W>       Window width (default: 1280)
  --height <H>      Window height (default: 720)
  --no-stdin        Disable command input from terminal
  --initial <MOL>   Start with molecule (h2o, ch4, nh3, etc.)
  --demo, --auto    Automatic demo mode (cycles through molecules)

WORKFLOW:
  1. Launch: vsepr --viz sim
  2. Window opens showing 3D view
  3. Type commands in terminal (or ImGui console)
  4. See results update in real-time
  5. Perfect for:
     - 100+ molecule geometry optimizations
     - Parameter tuning with visual feedback
     - Batch MD simulations
     - Interactive exploration

EXAMPLES:
  # Basic launch
  vsepr --viz sim

  # Automatic demo (rendering test & workflow showcase)
  vsepr --viz sim --demo

  # Start with specific molecule
  vsepr --viz sim --initial ch4

  # Larger window, no terminal input
  vsepr --viz sim --width 1920 --height 1080 --no-stdin

BATCH WORKFLOW:
  # Run multiple optimizations
  ./vsepr --viz sim << EOF
  build H2O
  optimize
  save water.xyz
  build CH4
  optimize
  save methane.xyz
  build NH3
  optimize
  save ammonia.xyz
  EOF

AVAILABLE COMMANDS (in session):
  build <formula>       - Build molecule from formula
  optimize              - Run geometry optimization
  mode <type>           - Set simulation mode (vsepr/optimize/md)
  set <param> <value>   - Set parameter
  advance <N>           - Run N simulation steps
  save <file>           - Save current geometry
  energy                - Show energy breakdown
  summary               - Show system summary
  help                  - Show available commands
  exit/quit             - Exit visualization
)";
    }
    
    int Execute(const std::vector<std::string>& args) override;
};

} // namespace cli
} // namespace vsepr
