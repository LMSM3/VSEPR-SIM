#pragma once
/**
 * cmd_tui.hpp
 * -----------
 * Terminal UI (TUI) viewer command for the XYZ file family.
 *
 * WO-ID: DEV54-XYZ-SPEC-INTEGRATION
 *
 * Dispatches all four XYZ formats into the ANSI terminal renderer via
 * the xyz_tui_bridge translation layer:
 *
 *   .xyz   → static geometry preview
 *   .xyza  → forces / velocity arrows + metadata panel
 *   .xyzc  → checkpoint state + atom preview
 *   .xyzf  → animated multi-frame trajectory
 *
 * Usage:
 *   vsepr tui <file.[xyz|xyza|xyzc|xyzf]> [options]
 *
 * Options:
 *   --fps <N>          Animation frames per second (default 10, .xyzf only)
 *   --proj <xy|xz|yz|iso>  Projection plane (default xy)
 *   --forces           Show force arrows  (default on for .xyza / .xyzc)
 *   --no-forces        Suppress force arrows
 *   --velocity         Show velocity arrows
 *   --width <W>        Terminal width hint (default 120)
 *   --height <H>       Terminal height hint (default 40)
 */

#include "commands.hpp"

namespace vsepr {
namespace cli {

class TuiCommand : public Command {
public:
	int Execute(const std::vector<std::string>& args) override;
	std::string Name()        const override { return "tui"; }
	std::string Description() const override {
		return "Terminal XYZ viewer (.xyz/.xyza/.xyzc/.xyzf)";
	}
	std::string Help() const override;
};

}} // namespace vsepr::cli
