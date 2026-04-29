#pragma once
/**
 * frame_renderer.hpp -- Universal terminal frame renderer
 * ========================================================
 * VSEPR-SIM
 *
 * FrameRenderer<TState> decouples the rendering pipeline from any specific
 * simulation state type.  Users register typed "layer" functions that each
 * receive the current state and write into the shared FrameBuffer.
 *
 * Pipeline:
 *
 *   TState (AtomisticState, BeadDynState, ThermalPipeState, ...)
 *       \u2193
 *   FrameRenderer<TState>::render(state)
 *       \u2193
 *   [Layer 0](fb, state)    -- e.g. draw border
 *   [Layer 1](fb, state)    -- e.g. project atoms
 *   [Layer 2](fb, state)    -- e.g. overlay forces
 *   [Layer N](fb, state)    -- e.g. diagnostics panel
 *       \u2193
 *   FrameBuffer::flush(out)
 *       \u2193
 *   Terminal stdout
 *
 * Usage example:
 *
 *   using namespace atomistic::tui;
 *
 *   FrameRenderer<MyState> renderer(120, 40);
 *   renderer.add_border_layer({Colour{60,60,80}});
 *   renderer.add_title_layer("VSEPR-SIM :: My Module", {80,220,255});
 *   renderer.add_layer([](FrameBuffer& fb, const MyState& s) {
 *       // draw atoms, beads, field values ...
 *   });
 *   renderer.display(state);  // renders + flushes to stdout
 *
 * Design decisions:
 *   - Layer order is draw order (first layer is drawn first = background)
 *   - Each layer receives the SAME FrameBuffer, so later layers overwrite earlier ones
 *   - set_if_empty() is available for layers that should not stomp others
 *   - The FrameRenderer itself is stateless between frames (FrameBuffer is cleared on each render)
 */

#include "crystal_tui.hpp"   // FrameBuffer, Colour, Cell, RESET, BOLD
#include <functional>
#include <string>
#include <vector>
#include <iostream>
#include <cstdio>
#include <chrono>
#include <thread>

namespace atomistic {
namespace tui {

// ============================================================================
// Layer type alias
// ============================================================================

template<typename TState>
using Layer = std::function<void(FrameBuffer&, const TState&)>;

// ============================================================================
// RendererConfig
// ============================================================================

struct RendererConfig {
	int  width  = 120;
	int  height = 40;
	bool hide_cursor   = true;
	bool clear_on_open = true;
};

// ============================================================================
// FrameRenderer<TState>
// ============================================================================

template<typename TState>
class FrameRenderer {
public:

	// --------------------------------------------------------
	// Construction
	// --------------------------------------------------------

	explicit FrameRenderer(int width = 120, int height = 40)
		: cfg_{width, height}
		, fb_(width, height)
	{}

	explicit FrameRenderer(const RendererConfig& cfg)
		: cfg_(cfg)
		, fb_(cfg.width, cfg.height)
	{}

	// --------------------------------------------------------
	// Layer registration
	// --------------------------------------------------------

	/** Append a rendering layer.  Layers execute in registration order. */
	void add_layer(Layer<TState> fn) {
		layers_.push_back(std::move(fn));
	}

	/** Remove all layers. */
	void clear_layers() { layers_.clear(); }

	/** Number of registered layers. */
	int layer_count() const { return static_cast<int>(layers_.size()); }

	// --------------------------------------------------------
	// Pre-built convenience layers
	// --------------------------------------------------------

	/** Add a simple box border around the whole viewport. */
	void add_border_layer(Colour col = {60, 60, 80}) {
		int w = cfg_.width, h = cfg_.height;
		add_layer([w, h, col](FrameBuffer& fb, const TState&) {
			fb.box(0, 0, w, h, col);
		});
	}

	/** Add a centred title string on row 0. */
	void add_title_layer(std::string title,
						  Colour col  = {80, 220, 255},
						  bool  bold  = true)
	{
		int w = cfg_.width;
		add_layer([title, col, bold, w](FrameBuffer& fb, const TState&) {
			int x = std::max(0, (w - static_cast<int>(title.size())) / 2);
			fb.put_string(x, 0, title, col, bold);
		});
	}

	/** Add a footer string on the last row. */
	void add_footer_layer(std::string footer,
						   Colour col = {100, 100, 100})
	{
		int h = cfg_.height;
		add_layer([footer, col, h](FrameBuffer& fb, const TState&) {
			fb.put_string(1, h - 1, footer, col);
		});
	}

	/**
	 * Add a horizontal bar panel at a fixed position.
	 * The value01_fn extracts a [0,1] value from TState each frame.
	 * The colour_fn maps that value to a Colour each frame.
	 */
	void add_bar_layer(int x, int y, int width,
						std::function<double(const TState&)>  value01_fn,
						std::function<Colour(double)>          colour_fn,
						std::string label = "",
						char fill = '#', char empty = '.')
	{
		add_layer([x, y, width, value01_fn, colour_fn, label, fill, empty]
				  (FrameBuffer& fb, const TState& s)
		{
			double v = value01_fn(s);
			Colour col = colour_fn(v);
			if (!label.empty())
				fb.put_string(x, y, label, {160, 160, 160});
			int bx = x + static_cast<int>(label.size()) + 1;
			fb.draw_bar(bx, y, width, v, col, fill, empty);
		});
	}

	// --------------------------------------------------------
	// Render
	// --------------------------------------------------------

	/**
	 * Render all layers into the FrameBuffer and return the ANSI string.
	 * Does not flush to any output.
	 */
	std::string render(const TState& state) {
		fb_ = FrameBuffer(cfg_.width, cfg_.height);
		for (auto& layer : layers_)
			layer(fb_, state);
		return fb_.render();
	}

	/**
	 * Render and flush to any ostream (defaults to std::cout).
	 * Issues cursor-home (\033[H) before the frame.  No clear-screen --
	 * the caller controls that to avoid flicker.
	 */
	void display(const TState& state, std::ostream& out = std::cout) {
		out << render(state);
		out.flush();
	}

	/**
	 * Full-screen display with cursor hide/show and clear-on-open.
	 * Suitable for use inside an animation loop.
	 */
	void display_fullscreen(const TState& state, std::ostream& out = std::cout) {
		if (cfg_.hide_cursor)   out << "\033[?25l";
		if (cfg_.clear_on_open) out << "\033[2J";
		display(state, out);
	}

	/** Restore terminal (show cursor). Call when leaving animation loop. */
	void restore_terminal(std::ostream& out = std::cout) {
		out << "\033[?25h" << "\033[0m";
		out.flush();
	}

	// --------------------------------------------------------
	// Accessors
	// --------------------------------------------------------

	const RendererConfig& config() const { return cfg_; }
	FrameBuffer&          framebuffer()  { return fb_; }

private:
	RendererConfig          cfg_;
	FrameBuffer             fb_;
	std::vector<Layer<TState>> layers_;
};

// ============================================================================
// AnimationLoop helper
// ============================================================================
//
// Thin RAII wrapper that manages the terminal state around a
// FrameRenderer-driven animation loop.
//
// Usage:
//
//   FrameRenderer<MyState> renderer(120, 40);
//   // ... add layers ...
//
//   AnimationLoop<MyState> loop(renderer, /*fps=*/10);
//   while (simulation_running) {
//       advance_simulation(state);
//       if (!loop.tick(state)) break;   // returns false if 'q' pressed
//   }
//
// NOTE: key-polling is not implemented here (platform-specific).
// The caller drives the loop and calls tick() each frame.
// tick() handles render + frame-rate pacing via sleep.

template<typename TState>
class AnimationLoop {
public:
	explicit AnimationLoop(FrameRenderer<TState>& renderer, int target_fps = 10)
		: renderer_(renderer)
		, frame_ms_(1000 / (target_fps > 0 ? target_fps : 10))
	{
		// Hide cursor and clear screen on entry
		std::cout << "\033[?25l\033[2J" << std::flush;
	}

	~AnimationLoop() {
		renderer_.restore_terminal();
	}

	/**
	 * Render one frame and sleep to maintain the target frame rate.
	 * Returns true to continue, false if the user should exit.
	 * (Key-checking is left to the caller; this just paces the render.)
	 */
	bool tick(const TState& state, std::ostream& out = std::cout) {
		using clock = std::chrono::steady_clock;
		auto t0 = clock::now();

		renderer_.display(state, out);
		++frame_count_;

		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
						   clock::now() - t0).count();
		long sleep = frame_ms_ - elapsed;
		if (sleep > 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(sleep));

		return true;
	}

	uint64_t frame_count() const { return frame_count_; }

private:
	FrameRenderer<TState>& renderer_;
	long     frame_ms_;
	uint64_t frame_count_ = 0;
};

} // namespace tui
} // namespace atomistic
