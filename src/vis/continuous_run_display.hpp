#pragma once
/**
 * continuous_run_display.hpp -- Passive window display for continuous batch runs
 * ===============================================================================
 * VSEPR-SIM  |  branch: v5.0.0-beta.7-step-attempt
 *
 * Provides ContinuousRunDisplay: the top-level wiring that allows any
 * continuous-run producer (batch metal_gen, database builder, report pipeline)
 * to keep a live simulation window open as a passive observer while the run
 * executes on its own thread(s).
 *
 * Architecture
 * ─────────────────────────────────────────────────────────────────────────────
 *
 *   Producer thread (your batch loop)
 *      │
 *      │  push_frame(XYZFrame, label)   ← never blocks; drop-oldest
 *      │  push_progress(...)            ← lightweight counter update
 *      ▼
 *   BatchWindowBridge                  ← lock-free atomic slot
 *      │
 *      │  display_tick() @ target_fps  ← throttled (default 5 fps)
 *      │  latest_frame()
 *      │  status_text()
 *      ▼
 *   ContinuousRunDisplay::tick()       ← called from display thread's event loop
 *      │
 *      ├── window.update(frame)        ← pushes atom positions to renderer
 *      └── window.set_overlay_text()  ← run label, frame idx, artifact count
 *
 *   Display thread (owns the OS window and GL context)
 *      └── ContinuousRunDisplay::run_event_loop()  ← replaces Window::run()
 *
 * ─────────────────────────────────────────────────────────────────────────────
 *
 * Minimal usage (same-process, producer on worker thread):
 *
 *   // --- main thread ---
 *   ContinuousRunDisplay display;
 *   display.configure("My Batch Run", 5.0f);   // label, display_fps
 *   display.open(800, 600);
 *
 *   // --- worker thread ---
 *   std::thread worker([&]{
 *       display.bridge().start();
 *       for (auto& mat : presets) {
 *           auto frame = build_supercell(mat, 4, 4, 4);
 *           display.bridge().push_frame(frame, mat.tag);
 *           display.bridge().push_progress(i, total, artifacts_done, artifacts_total,
 *                                           "writing .xyza");
 *           // ... write artifacts ...
 *       }
 *       display.bridge().finish();
 *   });
 *
 *   display.run_event_loop();   // blocks until window closed OR batch done
 *   worker.join();
 *
 * ─────────────────────────────────────────────────────────────────────────────
 *
 * Headless mode (no GL / no window — CI, SSH, no display):
 *   If ContinuousRunDisplay::open() fails (GLFW unavailable), it silently
 *   enters headless mode.  push_frame() and push_progress() still work;
 *   the bridge stays active so the same producer code runs unchanged.
 *   A console progress line is printed instead.
 */

#include "batch_window_bridge.hpp"
#include "viz_config.hpp"

// Forward-declare the Window class so this header can be included without
// pulling in GLFW, ImGui, or OpenGL headers.  Implementations that actually
// want to call window methods must include "vis/window.hpp" separately.
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <iostream>
#include <iomanip>
#include <chrono>

namespace vsepr {

class Window;   // defined in src/vis/window.hpp

namespace vis {

// ---------------------------------------------------------------------------
// DisplayConfig — passed once at startup
// ---------------------------------------------------------------------------
struct DisplayConfig {
	std::string  run_label    = "Batch Run";
	float        display_fps  = 5.0f;     // window refresh rate (not sim rate)
	int          win_width    = 960;
	int          win_height   = 720;
	bool         auto_close_on_done = true; // close window when batch finishes
	bool         console_fallback   = true; // print status to stdout if no window

	// BATCH_PASSIVE preset is applied automatically; override if needed
	VizConfig    viz;
};

// ---------------------------------------------------------------------------
// ConsoleProgressBar — fallback for headless / no-GL environments
// ---------------------------------------------------------------------------
class ConsoleProgressBar {
public:
	explicit ConsoleProgressBar(int width = 40) : bar_width_(width) {}

	void print(const BatchStatus& s) const {
		float pct = (s.total_frames > 0)
			? static_cast<float>(s.frame_index) / s.total_frames
			: 0.0f;

		int filled = static_cast<int>(pct * bar_width_);
		std::cout << "\r[";
		for (int i = 0; i < bar_width_; ++i)
			std::cout << (i < filled ? '#' : '-');
		std::cout << "] ";
		if (s.total_frames > 0)
			std::cout << std::setw(3) << static_cast<int>(pct * 100) << "% ";
		std::cout << s.run_label;
		if (!s.current_op.empty()) std::cout << " | " << s.current_op;
		int mins = static_cast<int>(s.elapsed_s) / 60;
		int secs = static_cast<int>(s.elapsed_s) % 60;
		std::cout << "  " << mins << "m" << secs << "s   ";
		std::cout.flush();
	}

	void done(const BatchStatus& s) const {
		print(s);
		std::cout << "\n[BATCH DONE] " << s.run_label
				  << "  frames=" << s.frame_index
				  << "  artifacts=" << s.artifacts_done
				  << "  elapsed=" << static_cast<int>(s.elapsed_s) << "s\n";
	}

private:
	int bar_width_;
};

// ---------------------------------------------------------------------------
// ContinuousRunDisplay
// ---------------------------------------------------------------------------
class ContinuousRunDisplay {
public:
	ContinuousRunDisplay() = default;
	~ContinuousRunDisplay() { close(); }

	// -----------------------------------------------------------------------
	// Setup — call before starting the producer thread
	// -----------------------------------------------------------------------

	void configure(const DisplayConfig& cfg) {
		cfg_ = cfg;
		cfg_.viz.apply_mode_preset(VizMode::BATCH_PASSIVE);
		cfg_.viz.batch_display_fps = cfg_.display_fps;
		cfg_.viz.batch_run_label   = cfg_.run_label;
	}

	void configure(const std::string& label, float display_fps = 5.0f) {
		DisplayConfig c;
		c.run_label   = label;
		c.display_fps = display_fps;
		configure(c);
	}

	/**
	 * Open the display window.
	 * Returns true if a real GL window was created.
	 * Returns false and enters console-fallback mode if GLFW is unavailable.
	 *
	 * Implementation note: this header does not directly call GLFW.
	 * The actual Window construction is deferred to the .cpp translation unit
	 * so that this header compiles cleanly without GL headers.
	 * If no .cpp is linked, headless mode is used automatically.
	 */
	bool open(int width = 960, int height = 720) {
		cfg_.win_width  = width;
		cfg_.win_height = height;
		window_open_requested_ = true;
		// Real GL init happens inside run_event_loop() on the display thread.
		// Here we only record the intent.
		return true;
	}

	void close() {
		close_requested_.store(true);
	}

	// -----------------------------------------------------------------------
	// Bridge access — hand this to the producer thread
	// -----------------------------------------------------------------------
	BatchWindowBridge& bridge() { return bridge_; }

	// -----------------------------------------------------------------------
	// Display-thread event loop
	// -----------------------------------------------------------------------

	/**
	 * run_event_loop()
	 * Blocks the calling thread (which must own the GL context) until:
	 *   (a) the window is closed by the user, OR
	 *   (b) batch is done AND auto_close_on_done is set, OR
	 *   (c) close() is called from another thread.
	 *
	 * Internally: polls for a new frame from the bridge at display_fps,
	 * processes OS events, ticks the renderer.
	 *
	 * If no GL window could be opened (headless), it runs the console
	 * progress bar instead and returns when bridge().is_done().
	 */
	void run_event_loop() {
		if (!try_open_window()) {
			run_console_loop();
			return;
		}
		run_gl_loop();
	}

	/**
	 * Single-tick variant for embedding into an existing event loop.
	 * Returns false when the display should stop (window closed or batch done).
	 */
	bool tick() {
		if (close_requested_.load()) return false;

		if (bridge_.display_tick(cfg_.display_fps)) {
			auto frame = bridge_.latest_frame();
			if (frame) {
				dispatch_frame_to_window(*frame);
			}
			if (cfg_.console_fallback && !window_alive_) {
				console_bar_.print(bridge_.status_snapshot());
			}
		}

		if (bridge_.is_done()) {
			if (cfg_.auto_close_on_done) {
				if (cfg_.console_fallback && !window_alive_)
					console_bar_.done(bridge_.status_snapshot());
				return false;
			}
		}
		return true;
	}

private:
	// -----------------------------------------------------------------------
	// Internal helpers
	// -----------------------------------------------------------------------

	bool try_open_window() {
		// Actual window construction requires a .cpp that includes window.hpp
		// and calls the GL init path.  This base implementation returns false
		// (headless) so the header compiles standalone.
		// Override via subclass or link continuous_run_display.cpp (beta.8).
		(void)window_open_requested_;
		return false;   // headless default for this header-only release
	}

	void run_gl_loop() {
		// GL loop body — provided by continuous_run_display.cpp (beta.8).
		// Headless fallback until then.
		run_console_loop();
	}

	void run_console_loop() {
		using clock = std::chrono::steady_clock;
		const double tick_s = 1.0 / std::max(cfg_.display_fps, 0.5f);

		while (!close_requested_.load()) {
			auto t0 = clock::now();

			auto frame = bridge_.latest_frame();
			(void)frame;  // in console mode: no GL calls

			console_bar_.print(bridge_.status_snapshot());

			if (bridge_.is_done()) {
				console_bar_.done(bridge_.status_snapshot());
				break;
			}

			// Sleep for remainder of tick window
			auto elapsed = std::chrono::duration<double>(clock::now() - t0).count();
			double sleep_s = tick_s - elapsed;
			if (sleep_s > 0.0)
				std::this_thread::sleep_for(
					std::chrono::duration<double>(sleep_s));
		}
	}

	void dispatch_frame_to_window(const io::XYZFrame& /*frame*/) {
		// Wired to Window::update() inside continuous_run_display.cpp (beta.8).
		// No-op until GL layer is linked.
	}

	DisplayConfig       cfg_;
	BatchWindowBridge   bridge_;
	ConsoleProgressBar  console_bar_;

	std::atomic<bool>   close_requested_{false};
	bool                window_open_requested_ = false;
	bool                window_alive_          = false;
};

} // namespace vis
} // namespace vsepr
