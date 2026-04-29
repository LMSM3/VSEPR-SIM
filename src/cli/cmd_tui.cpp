/**
 * cmd_tui.cpp
 * -----------
 * Implementation of the TUI viewer command.
 *
 * WO-ID: DEV54-XYZ-SPEC-INTEGRATION
 *
 * Wires vsepr::io xyz_tui_bridge into the CLI command router so that
 * any of the four XYZ family file formats can be rendered directly in
 * the terminal via:
 *
 *   vsepr tui molecule.xyz
 *   vsepr tui dynamics.xyza --velocity
 *   vsepr tui restart.xyzc
 *   vsepr tui trajectory.xyzf --fps 15 --proj xz
 */

#include "cmd_tui.hpp"
#include "display.hpp"

// Unified XYZ I/O and TUI bridge
#include "io/xyz_tui_bridge.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>

namespace vsepr {
namespace cli {

// ============================================================================
// Help text
// ============================================================================

std::string TuiCommand::Help() const {
	return R"(
USAGE:
  vsepr tui <file> [options]

FORMATS:
  .xyz     Static geometry snapshot         (atom positions)
  .xyza    Extended frame                   (+ charge, velocity, force, energy)
  .xyzc    Checkpoint (restartable state)   (+ integrator / thermostat state)
  .xyzf    Multi-frame trajectory           (animated playback)

OPTIONS:
  --fps <N>         Animation FPS for .xyzf (default: 10)
  --proj <plane>    Projection plane: xy | xz | yz | iso  (default: xy)
  --forces          Show force arrows  [auto-on when forces present]
  --no-forces       Suppress force arrows
  --velocity        Show velocity arrows
  --no-velocity     Suppress velocity arrows
  --width <W>       Terminal column count (default: 120)
  --height <H>      Terminal row count    (default: 40)

EXAMPLES:
  vsepr tui water.xyz
  vsepr tui md_run.xyza --velocity
  vsepr tui restart.xyzc
  vsepr tui trajectory.xyzf --fps 20 --proj xz
  vsepr tui neb_path.xyzf  --no-forces --fps 5

SANITY WARNINGS:
  The reader validates observable ranges from the spec (Edition 2 §8):
	Force     > 1000 kcal/(mol·Å)  → flagged
	Charge    |q| > 5 e            → flagged
	Velocity  |v| > 1.0 Å/fs       → flagged
  Warnings are printed to stderr before rendering begins.
)";
}

// ============================================================================
// Argument parsing helpers
// ============================================================================

static std::string arg_value(const std::vector<std::string>& args,
							  const std::string& flag,
							  const std::string& default_val = "") {
	for (size_t i = 0; i + 1 < args.size(); ++i)
		if (args[i] == flag) return args[i + 1];
	return default_val;
}

static bool has_flag(const std::vector<std::string>& args, const std::string& flag) {
	return std::find(args.begin(), args.end(), flag) != args.end();
}

// ============================================================================
// Execute
// ============================================================================

int TuiCommand::Execute(const std::vector<std::string>& args) {
	// ---- require at least a file path ----
	if (args.empty()) {
		Display::Error("tui: expected a file path");
		Display::Info("Usage: vsepr tui <file.[xyz|xyza|xyzc|xyzf]> [--fps N] [--proj xy|xz|yz|iso]");
		return 1;
	}

	// First non-flag arg is the file path
	std::string path;
	for (const auto& a : args) {
		if (!a.empty() && a[0] != '-') { path = a; break; }
	}
	if (path.empty()) {
		Display::Error("tui: no file path provided");
		return 1;
	}

	// ---- parse options ----
	int  fps     = std::stoi(arg_value(args, "--fps", "10"));
	int  width   = std::stoi(arg_value(args, "--width",  "120"));
	int  height  = std::stoi(arg_value(args, "--height", "40"));
	bool forces  = !has_flag(args, "--no-forces");
	bool vel     = has_flag(args, "--velocity");

	std::string proj_str = arg_value(args, "--proj", "xy");

	// ---- build bridge config ----
	vsepr::io::BridgeTUIConfig cfg;
	cfg.term_width   = width;
	cfg.term_height  = height;
	cfg.show_forces  = forces;
	cfg.show_velocity = vel;

	// Viewport and panel layout (leave 46 cols for the diagnostics panel)
	int vp_w = std::max(30, width - 48);
	int vp_h = std::max(10, height - 6);
	cfg.viewport = {1, 1, vp_w, vp_h};
	cfg.panel    = {vp_w + 2, 1, width - vp_w - 3, vp_h};

	// Projection
	if      (proj_str == "xz")  cfg.projection = atomistic::tui::proj_xz();
	else if (proj_str == "yz")  cfg.projection = atomistic::tui::proj_yz();
	else if (proj_str == "iso") cfg.projection = vsepr::io::proj_iso();
	else                        cfg.projection = atomistic::tui::proj_xy();

	// ---- detect format and print it ----
	auto fmt = vsepr::io::detect_format_by_extension(path);
	if (fmt == vsepr::io::XYZFormat::UNKNOWN) {
		Display::Error("tui: unrecognised extension for '" + path + "'");
		Display::Info("Supported: .xyz  .xyza  .xyzc  .xyzf");
		return 1;
	}
	Display::Info(std::string("tui: loading ") + vsepr::io::format_name(fmt) + " → " + path);

	// ---- run reader + validate + render ----
	try {
		vsepr::io::ParseContext ctx;

		if (fmt == vsepr::io::XYZFormat::XYZF) {
			// Trajectory — load all frames, sanity-check, animate
			auto data = vsepr::io::read_xyzf(path, &ctx);

			for (const auto& w : ctx.warnings)
				std::cerr << "[TUI WARN] frame=" << w.frame_index
						  << " atom=" << w.atom_index
						  << " : " << w.message << "\n";
			for (const auto& e : ctx.errors)
				std::cerr << "[TUI ERR]  frame=" << e.frame_index
						  << " line=" << e.line_number
						  << " : " << e.message << "\n";

			auto sanity = vsepr::io::validate_data(data);
			for (const auto& w : sanity)
				std::cerr << "[SANITY]   frame=" << w.frame_index
						  << " atom=" << w.atom_index
						  << " : " << w.message << "\n";

			if (data.empty()) {
				Display::Error("tui: no frames parsed from '" + path + "'");
				return 1;
			}

			Display::Info("tui: " + std::to_string(data.frame_count()) + " frames at " +
						  std::to_string(fps) + " fps");

			vsepr::io::animate_xyzf(data, fps, cfg, std::cout);

		} else if (fmt == vsepr::io::XYZFormat::XYZC) {
			auto data = vsepr::io::read_xyzc(path, &ctx);
			for (const auto& e : ctx.errors)
				std::cerr << "[TUI ERR] " << e.message << "\n";

			if (data.frames.empty()) {
				Display::Error("tui: failed to read checkpoint from '" + path + "'");
				return 1;
			}

			// Annotate comment with checkpoint state for the panel
			vsepr::io::XYZFrame frame = data.frames[0];
			if (data.checkpoint) {
				const auto& ck = *data.checkpoint;
				char buf[160];
				std::snprintf(buf, sizeof(buf),
					"CHECKPOINT step=%d  t=%.1f fs  dt=%.4f fs  T_tgt=%.1f K  seed=%d",
					ck.step, ck.time, ck.dt, ck.T_target, ck.seed);
				frame.comment = buf;
				if (ck.box) {
					frame.box = ck.box;
					frame.has_force = true;  // xyzc always carries forces
					frame.has_charge = true;
					frame.has_velocity = true;
				}
			}

			auto sanity = vsepr::io::validate_frame(frame);
			for (const auto& w : sanity)
				std::cerr << "[SANITY] atom=" << w.atom_index << " : " << w.message << "\n";

			vsepr::io::display_xyz_frame(frame, cfg, std::cout);

		} else if (fmt == vsepr::io::XYZFormat::XYZA) {
			auto frame = vsepr::io::read_xyza(path, &ctx);
			for (const auto& e : ctx.errors)
				std::cerr << "[TUI ERR] " << e.message << "\n";

			// Auto-enable visualisation flags based on what the file carries
			cfg.show_forces   = cfg.show_forces  && frame.has_force;
			cfg.show_velocity = cfg.show_velocity || frame.has_velocity;

			auto sanity = vsepr::io::validate_frame(frame);
			for (const auto& w : sanity)
				std::cerr << "[SANITY] atom=" << w.atom_index << " : " << w.message << "\n";

			vsepr::io::display_xyz_frame(frame, cfg, std::cout);

		} else {  // XYZ
			auto frame = vsepr::io::read_xyz(path, &ctx);
			for (const auto& e : ctx.errors)
				std::cerr << "[TUI ERR] " << e.message << "\n";

			cfg.show_forces   = false;
			cfg.show_velocity = false;

			vsepr::io::display_xyz_frame(frame, cfg, std::cout);
		}

	} catch (const std::exception& ex) {
		Display::Error(std::string("tui: exception: ") + ex.what());
		return 2;
	}

	return 0;
}

} // namespace cli
} // namespace vsepr
