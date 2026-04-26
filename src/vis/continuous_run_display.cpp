/**
 * continuous_run_display.cpp -- GL wiring for passive batch display
 * =================================================================
 * VSEPR-SIM  |  branch: v5.0.0-beta.7-step-attempt
 *
 * Implements the GL-dependent parts of ContinuousRunDisplay that were kept
 * out of the header to avoid pulling GLFW/OpenGL into every TU.
 */

#include "vis/continuous_run_display.hpp"
#include "vis/window.hpp"          // full Window definition with run_batch()

#include <iostream>
#include <chrono>
#include <thread>

namespace vsepr {
namespace vis {

// ---------------------------------------------------------------------------
// ContinuousRunDisplay::open()
// ---------------------------------------------------------------------------
bool ContinuousRunDisplay::open(int width, int height) {
	cfg_.win_width  = width;
	cfg_.win_height = height;

	try {
		auto win = std::make_unique<Window>(
			width, height,
			"VSEPR-SIM  |  " + cfg_.run_label + "  [BATCH]");

		if (!win->initialize()) {
			std::cerr << "[ContinuousRunDisplay] GL window init failed — "
						 "using console fallback.\n";
			return false;
		}

		window_ = std::move(win);
		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "[ContinuousRunDisplay] Exception during GL open: "
				  << e.what() << " — using console fallback.\n";
		return false;
	}
}

// ---------------------------------------------------------------------------
// ContinuousRunDisplay::run_event_loop()
// ---------------------------------------------------------------------------
void ContinuousRunDisplay::run_event_loop() {
	if (window_) {
		run_gl_loop();
	} else {
		run_console_loop();
	}
}

// ---------------------------------------------------------------------------
// GL loop — delegates to Window::run_batch()
// ---------------------------------------------------------------------------
void ContinuousRunDisplay::run_gl_loop() {
	// run_batch() blocks until bridge_.is_done() or the OS window is closed
	window_->run_batch(bridge_, cfg_.display_fps);

	if (cfg_.auto_close_on_done) {
		window_->close();
	}
}

// ---------------------------------------------------------------------------
// Console fallback loop
// ---------------------------------------------------------------------------
void ContinuousRunDisplay::run_console_loop() {
	using clock = std::chrono::steady_clock;
	const double tick_s = 1.0 / std::max(cfg_.display_fps, 0.5f);

	while (!close_requested_.load()) {
		auto t0 = clock::now();

		console_bar_.print(bridge_.status_snapshot());

		if (bridge_.is_done()) {
			console_bar_.done(bridge_.status_snapshot());
			break;
		}

		double elapsed = std::chrono::duration<double>(clock::now() - t0).count();
		double sleep_s = tick_s - elapsed;
		if (sleep_s > 0.0)
			std::this_thread::sleep_for(std::chrono::duration<double>(sleep_s));
	}
}

} // namespace vis
} // namespace vsepr
