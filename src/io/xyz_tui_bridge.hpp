#pragma once
/**
 * xyz_tui_bridge.hpp -- Universal XYZ → TUI translation layer
 * =============================================================
 * VSEPR-SIM  |  WO-ID: DEV54-XYZ-SPEC-INTEGRATION
 *
 * This is the architectural connector described in the WO:
 *
 *   .xyz / .xyza / .xyzc / .xyzf
 *           ↓
 *    Unified Reader  (xyz_reader.hpp)
 *           ↓
 *        XYZData
 *           ↓
 *    xyz_tui_bridge    ← YOU ARE HERE
 *           ↓
 *    TUISnapshot / FrameRenderer<XYZFrame>
 *           ↓
 *    FrameBuffer → ANSI → Terminal stdout
 *
 * Three main entry points:
 *
 *   1. frame_to_snapshot(XYZFrame)
 *      Converts a single parsed frame into a TUISnapshot for
 *      manual use with CrystalTUI or any custom renderer.
 *
 *   2. make_xyz_renderer(mode)
 *      Returns a configured FrameRenderer<XYZFrame> with atom,
 *      force, velocity, math-panel, and footer layers pre-attached.
 *      Supports all four visual modes (.xyz/.xyza/.xyzc/.xyzf).
 *
 *   3. animate_xyzf(data, fps)
 *      Full terminal animation loop for .xyzf trajectories.
 *      Blocks until 'q' or EOF.
 *
 * Projection:
 *   3D atom positions (Å, Cartesian) → 2D terminal grid via
 *   projection.hpp Projection2D (XY by default; configurable).
 *
 * Design:
 *   - No raw terminal cursor manipulation outside enter/exit helpers
 *   - Deterministic: same XYZFrame → same rendered frame
 *   - Zero coupling to the atomistic MD engine or any simulation type
 */

#include "xyz_unified.hpp"
#include "xyz_reader.hpp"
#include "xyz_writer.hpp"
#include "atomistic/tui/crystal_tui.hpp"
#include "atomistic/tui/projection.hpp"
#include "atomistic/tui/element_render.hpp"
#include "atomistic/tui/frame_renderer.hpp"

#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <functional>

namespace vsepr {
namespace io {

// ============================================================================
// proj_iso() — bridge-local helper (projection.hpp has project_iso() only)
// Returns a Projection2D that approximates isometric view via XY axes.
// For true 3D iso, the renderer layer can call project_iso() directly.
// ============================================================================

inline atomistic::tui::Projection2D proj_iso() {
    // Isometric approximation: use XZ plane (rotated) as closest standard proxy
    return atomistic::tui::proj_xz();
}

// ============================================================================
// BridgeConfig — visual settings for the translation layer
// ============================================================================

struct BridgeTUIConfig {
	int  term_width    = 120;
	int  term_height   = 40;

	// Viewport for atom projection
	atomistic::tui::Rect viewport = {1, 1, 72, 34};

	// Math panel
	atomistic::tui::Rect panel    = {74, 1, 44, 34};

	atomistic::tui::Projection2D projection = atomistic::tui::proj_xy();  // default: XY slice

	bool show_forces   = true;
	bool show_velocity = false;   // enable for .xyza files
	bool show_box      = true;
	bool show_footer   = true;

	// Colour scale for force arrows
	bool   force_auto_scale = true;
	double force_max_manual = 100.0;  // kcal/(mol·Å)

	// Colour scale for velocity arrows
	bool   vel_auto_scale   = true;
	double vel_max_manual   = 0.1;    // Å/fs
};

// ============================================================================
// 1. frame_to_snapshot — XYZFrame → atomistic::tui::TUISnapshot
// ============================================================================
//
// Converts the unified AtomRecord vector into the TUISnapshot struct used by
// CrystalTUI and any downstream renderer.  This is a lightweight value
// conversion: no physics, no neighbour lists, no MD state.

inline atomistic::tui::TUISnapshot frame_to_snapshot(const XYZFrame& frame) {
	atomistic::tui::TUISnapshot snap;

	snap.step = frame.frame_index;
	snap.dt   = 0.0;     // not available from XYZ
	snap.dt_eff = 0.0;

	if (frame.energy)      snap.U_total = *frame.energy;
	if (frame.temperature) snap.T       = *frame.temperature;

	// Box → lattice metrics
	if (frame.box) {
		snap.a = frame.box->ax;
		snap.b = frame.box->ay;
		snap.c = frame.box->az;
		snap.volume = frame.box->volume();
	}

	snap.positions.reserve(frame.atoms.size());
	snap.types.reserve(frame.atoms.size());
	snap.forces.reserve(frame.atoms.size());

	double frms2 = 0.0, fmax = 0.0;
	double ke    = 0.0;

	for (const auto& a : frame.atoms) {
		// positions (using atomistic::Vec3 from crystal_tui.hpp)
		atomistic::Vec3 pos;
		pos.x = a.x; pos.y = a.y; pos.z = a.z;
		snap.positions.push_back(pos);
		snap.types.push_back(static_cast<uint32_t>(a.Z));

		// forces
		atomistic::Vec3 fvec{};
		if (a.f) { fvec.x = a.f->x; fvec.y = a.f->y; fvec.z = a.f->z; }
		snap.forces.push_back(fvec);
		double fi = std::sqrt(fvec.x*fvec.x + fvec.y*fvec.y + fvec.z*fvec.z);
		frms2 += fi * fi;
		if (fi > fmax) fmax = fi;

		// kinetic energy estimate from velocity (½ m v²)
		// m ≈ Z (crude; real masses not stored here)
		if (a.v) {
			double v2 = a.v->x*a.v->x + a.v->y*a.v->y + a.v->z*a.v->z;
			ke += 0.5 * a.Z * v2;
		}
	}

	int N = static_cast<int>(frame.atoms.size());
	snap.Frms = (N > 0) ? std::sqrt(frms2 / N) : 0.0;
	snap.Fmax = fmax;
	snap.KE   = ke;

	// Temperature from KE if not in comment
	if (!frame.temperature && ke > 0.0 && N > 0)
		snap.T = (2.0 * ke) / (3.0 * N * 0.0019872041);  // kB in kcal/(mol·K)

	return snap;
}

// ============================================================================
// 2. make_xyz_renderer — pre-configured FrameRenderer<XYZFrame>
// ============================================================================
//
// Returns a FrameRenderer with all standard layers attached.
// Caller can add additional layers before calling display() or animate_xyzf().

inline atomistic::tui::FrameRenderer<XYZFrame>
make_xyz_renderer(const BridgeTUIConfig& cfg = {})
{
	using namespace atomistic::tui;

	RendererConfig rcfg;
	rcfg.width  = cfg.term_width;
	rcfg.height = cfg.term_height;
	rcfg.hide_cursor   = true;
	rcfg.clear_on_open = true;

	FrameRenderer<XYZFrame> renderer(rcfg);

	// ---- Layer 0: border + title ----
	renderer.add_border_layer({60, 60, 80});
	renderer.add_title_layer("VSEPR-SIM :: XYZ Viewer", {80, 220, 255});

	// ---- Layer 1: atom projection ----
	Rect vp  = cfg.viewport;
	Projection2D proj = cfg.projection;
	bool show_forces  = cfg.show_forces;
	bool show_vel     = cfg.show_velocity;
	bool force_auto   = cfg.force_auto_scale;
	double force_max  = cfg.force_max_manual;
	bool vel_auto     = cfg.vel_auto_scale;
	double vel_max    = cfg.vel_max_manual;

	renderer.add_layer([vp, proj, show_forces, show_vel,
						force_auto, force_max, vel_auto, vel_max]
					   (FrameBuffer& fb, const XYZFrame& frame)
	{
		if (frame.atoms.empty()) return;

		// Build world bounding box from frame
		Bounds3D bounds;
		for (const auto& a : frame.atoms)
			bounds.expand({a.x, a.y, a.z});
		bounds.pad(1.5);

		// Compute force/velocity scales
		double fmax_val = force_max;
		double vmax_val = vel_max;
		if (force_auto || vel_auto) {
			for (const auto& a : frame.atoms) {
				if (force_auto && a.f) {
					double fm = std::sqrt(a.f->x*a.f->x + a.f->y*a.f->y + a.f->z*a.f->z);
					if (fm > fmax_val) fmax_val = fm;
				}
				if (vel_auto && a.v) {
					double vm = std::sqrt(a.v->x*a.v->x + a.v->y*a.v->y + a.v->z*a.v->z);
					if (vm > vmax_val) vmax_val = vm;
				}
			}
		}
		if (fmax_val < 1e-20) fmax_val = 1.0;
		if (vmax_val < 1e-20) vmax_val = 0.02;

		// --- Draw atoms ---
		for (size_t i = 0; i < frame.atoms.size(); ++i) {
			const auto& a = frame.atoms[i];
			atomistic::Vec3 p3{a.x, a.y, a.z};
			ScreenPoint sp = project(p3, proj, bounds, vp);
			if (!in_viewport(sp, vp)) continue;

			Colour col = colour_for_element(a.Z);
			char   gly = glyph_for_element(a.Z);
			fb.put(sp.col, sp.row, gly, col, true);
		}

		// --- Draw force arrows ---
		if (show_forces) {
			for (size_t i = 0; i < frame.atoms.size(); ++i) {
				const auto& a = frame.atoms[i];
				if (!a.f) continue;
				double fmag = std::sqrt(a.f->x*a.f->x + a.f->y*a.f->y + a.f->z*a.f->z);
				if (fmag < 1e-6) continue;

				atomistic::Vec3 p3{a.x, a.y, a.z};
				ScreenPoint sp = project(p3, proj, bounds, vp);

				// Arrow direction from force horizontal/vertical components
				double fh = (proj.horiz == Axis::X) ? a.f->x
						  : (proj.horiz == Axis::Y) ? a.f->y : a.f->z;
				double fv = (proj.vert  == Axis::X) ? a.f->x
						  : (proj.vert  == Axis::Y) ? a.f->y : a.f->z;

				int ax = sp.col + (fh > 0.3 ? 1 : (fh < -0.3 ? -1 : 0));
				int ay = sp.row + (fv > 0.3 ? (proj.invert_vert ? -1 : 1)
											: (fv < -0.3 ? (proj.invert_vert ? 1 : -1) : 0));
				Colour fc = force_gradient(fmag, fmax_val);
				char arrow_chars[8][2] = {">","/","^","\\","<","/","v","\\"};
				double angle = std::atan2(fv, fh) * 180.0 / 3.14159265;
				if (angle < 0) angle += 360.0;
				int ai = static_cast<int>((angle + 22.5) / 45.0) % 8;
				fb.set_if_empty(ax, ay, arrow_chars[ai][0], fc);
			}
		}

		// --- Draw velocity arrows ---
		if (show_vel) {
			for (size_t i = 0; i < frame.atoms.size(); ++i) {
				const auto& a = frame.atoms[i];
				if (!a.v) continue;
				double vmag = std::sqrt(a.v->x*a.v->x + a.v->y*a.v->y + a.v->z*a.v->z);
				if (vmag < 1e-8) continue;

				atomistic::Vec3 p3{a.x, a.y, a.z};
				ScreenPoint sp = project(p3, proj, bounds, vp);
				double vh = (proj.horiz == Axis::X) ? a.v->x
						  : (proj.horiz == Axis::Y) ? a.v->y : a.v->z;
				int ax = sp.col + (vh > 0 ? 1 : -1);
				Colour vc = velocity_colour(vmag / vmax_val);
				fb.set_if_empty(ax, sp.row, '~', vc);
			}
		}
	});

	// ---- Layer 2: axis labels inside viewport ----
	renderer.add_layer([vp, proj](FrameBuffer& fb, const XYZFrame&) {
		Colour dim{80, 80, 100};
		auto axis_label = [](Axis ax) -> const char* {
			return ax == Axis::X ? "x" : ax == Axis::Y ? "y" : "z";
		};
		std::string hlabel = std::string("  ") + axis_label(proj.horiz) + " →";
		std::string vlabel = std::string("↑ ") + axis_label(proj.vert);
		fb.put_string(vp.x + 2, vp.y + vp.h - 2, hlabel, dim);
		fb.put_string(vp.x + 2, vp.y + 2,        vlabel, dim);
	});

	// ---- Layer 3: math/diagnostics panel ----
	Rect panel = cfg.panel;
	renderer.add_layer([panel](FrameBuffer& fb, const XYZFrame& frame) {
		int x0 = panel.x + 2;
		int y  = panel.y + 1;
		Colour hdr{80, 220, 255};
		Colour val{200, 200, 200};
		Colour dim{120, 120, 120};
		Colour hot{255, 100, 60};
		Colour grn{80, 230, 80};

		// Title
		fb.put_string(x0, y++, "=== FRAME INFO ===", hdr, true);
		++y;

		// Frame index
		fb.put_string(x0, y++, "Frame  " + std::to_string(frame.frame_index), dim);
		fb.put_string(x0, y++, "N      " + std::to_string(frame.N), val);

		++y;
		fb.put_string(x0, y++, "Energy (kcal/mol)", hdr);
		if (frame.energy) {
			fb.put_string(x0, y++, "  E = " + [&]{
				char buf[32];
				std::snprintf(buf, sizeof(buf), "%.4f", *frame.energy);
				return std::string(buf);
			}(), val);
		} else {
			fb.put_string(x0, y++, "  E = (not available)", dim);
		}

		++y;
		fb.put_string(x0, y++, "Temperature", hdr);
		if (frame.temperature) {
			char tbuf[32];
			std::snprintf(tbuf, sizeof(tbuf), "  T = %.1f K", *frame.temperature);
			Colour tc = (*frame.temperature > 500) ? hot : val;
			fb.put_string(x0, y++, tbuf, tc);
		} else {
			fb.put_string(x0, y++, "  T = (not available)", dim);
		}

		++y;
		fb.put_string(x0, y++, "Extended Fields", hdr);
		fb.put_string(x0, y++,
			std::string("  charge   ") + (frame.has_charge   ? "[yes]" : "[no] "), 
			frame.has_charge ? grn : dim);
		fb.put_string(x0, y++,
			std::string("  velocity ") + (frame.has_velocity ? "[yes]" : "[no] "),
			frame.has_velocity ? grn : dim);
		fb.put_string(x0, y++,
			std::string("  force    ") + (frame.has_force    ? "[yes]" : "[no] "),
			frame.has_force ? grn : dim);
		fb.put_string(x0, y++,
			std::string("  energy   ") + (frame.has_energy_col ? "[yes]" : "[no] "),
			frame.has_energy_col ? grn : dim);

		++y;
		if (frame.box) {
			fb.put_string(x0, y++, "Box (Å)", hdr);
			char bbuf[64];
			std::snprintf(bbuf, sizeof(bbuf), "  %.2f × %.2f × %.2f",
						  frame.box->ax, frame.box->ay, frame.box->az);
			fb.put_string(x0, y++, bbuf, val);
			char vbuf[32];
			std::snprintf(vbuf, sizeof(vbuf), "  V = %.1f Å³", frame.box->volume());
			fb.put_string(x0, y++, vbuf, dim);
		}

		// Force diagnostics if present
		if (frame.has_force) {
			++y;
			fb.put_string(x0, y++, "Forces", hdr);
			double frms2 = 0, fmax = 0;
			for (const auto& a : frame.atoms) {
				if (!a.f) continue;
				double fm = std::sqrt(a.f->x*a.f->x + a.f->y*a.f->y + a.f->z*a.f->z);
				frms2 += fm * fm;
				if (fm > fmax) fmax = fm;
			}
			int N = static_cast<int>(frame.atoms.size());
			double frms = (N > 0) ? std::sqrt(frms2 / N) : 0.0;
			char fbuf[48];
			std::snprintf(fbuf, sizeof(fbuf), "  Frms = %.2e  Fmax = %.2e", frms, fmax);
			Colour fc = (frms < 0.01) ? grn : val;
			fb.put_string(x0, y++, fbuf, fc);
		}
	});

	// ---- Layer 4: footer (controls / source) ----
	if (cfg.show_footer) {
		renderer.add_footer_layer(" [q] quit  |  [→] next  |  [←] prev  |  VSEPR-SIM XYZ Viewer",
								   {80, 80, 80});
	}

	return renderer;
}

// ============================================================================
// 3. animate_xyzf — terminal animation loop for .xyzf trajectories
// ============================================================================
//
// Drives a FrameRenderer<XYZFrame> through a vector of frames at target_fps.
// This is the "now your terminal actually shows trajectories" path from the WO.
//
// Controls:
//   q / Q / ESC  → quit
//   (frame pacing via sleep; no interactive key-polling — add platform
//    layer if needed; the renderer remains keyboard-agnostic here)

inline void animate_xyzf(const XYZData& data,
						   int target_fps = 10,
						   const BridgeTUIConfig& cfg = {},
						   std::ostream& out = std::cout)
{
	if (data.frames.empty()) {
		out << "[xyz_tui_bridge] No frames to animate.\n";
		return;
	}

	auto renderer = make_xyz_renderer(cfg);
	const long frame_ms = 1000 / std::max(1, target_fps);

	// Hide cursor and clear screen on entry
	out << "\033[?25l\033[2J" << std::flush;

	for (const auto& frame : data.frames) {
		using clock = std::chrono::steady_clock;
		auto t0 = clock::now();

		renderer.display(frame, out);

		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
						   clock::now() - t0).count();
		long sleep = frame_ms - elapsed;
		if (sleep > 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
	}

	// Restore cursor and clear
	out << "\033[?25h\033[0m" << std::flush;
}

// ============================================================================
// 4. Static preview — single-frame display (.xyz / .xyza / .xyzc)
// ============================================================================

inline void display_xyz_frame(const XYZFrame& frame,
								const BridgeTUIConfig& cfg = {},
								std::ostream& out = std::cout)
{
	auto renderer = make_xyz_renderer(cfg);
	out << "\033[?25l\033[2J" << std::flush;
	renderer.display(frame, out);
	out << "\033[?25h\033[0m\n" << std::flush;
}

// ============================================================================
// 5. from_path — load + display in one call
// ============================================================================
//
// Detects format, loads, and either previews or animates.
// Intended for "tui file.xyz" / "tui file.xyzf" CLI dispatch.

inline void tui_from_path(const std::string& path,
							int fps = 10,
							const BridgeTUIConfig& cfg = {},
							std::ostream& out = std::cout)
{
	XYZFormat fmt = detect_format_by_extension(path);
	switch (fmt) {
		case XYZFormat::XYZ: {
			auto frame = read_xyz(path);
			display_xyz_frame(frame, cfg, out);
			break;
		}
		case XYZFormat::XYZA: {
			auto frame = read_xyza(path);
			BridgeTUIConfig c = cfg;
			c.show_velocity = frame.has_velocity;
			c.show_forces   = frame.has_force;
			display_xyz_frame(frame, c, out);
			break;
		}
		case XYZFormat::XYZC: {
			auto data  = read_xyzc(path);
			if (!data.frames.empty()) {
				// Show checkpoint metadata in comment override
				if (data.checkpoint) {
					auto& ck = *data.checkpoint;
					char buf[128];
					std::snprintf(buf, sizeof(buf),
						"CHECKPOINT step=%d t=%.1f fs dt=%.4f fs T_tgt=%.1f K",
						ck.step, ck.time, ck.dt, ck.T_target);
					XYZFrame copy = data.frames[0];
					copy.comment  = buf;
					display_xyz_frame(copy, cfg, out);
				} else {
					display_xyz_frame(data.frames[0], cfg, out);
				}
			}
			break;
		}
		case XYZFormat::XYZF: {
			auto data = read_xyzf(path);
			animate_xyzf(data, fps, cfg, out);
			break;
		}
		default:
			out << "[xyz_tui_bridge] Unrecognised format for: " << path << '\n';
			break;
	}
}

} // namespace io
} // namespace vsepr
