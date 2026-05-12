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

// WindowDeleter body -- requires complete Window type, so defined here.
void WindowDeleter::operator()(Window* p) const noexcept {
    delete p;
}

// ---------------------------------------------------------------------------
// ContinuousRunDisplay::open()
// ---------------------------------------------------------------------------
bool ContinuousRunDisplay::open(int width, int height) {
	cfg_.win_width  = width;
	cfg_.win_height = height;

	try {
		auto raw = new Window(width, height,
			"VSEPR-SIM  |  " + cfg_.run_label + "  [BATCH]");
		std::unique_ptr<Window, WindowDeleter> win(raw);

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
// GL loop — delegates to Window::run_batch()
// ---------------------------------------------------------------------------
void ContinuousRunDisplay::run_gl_loop() {
	// run_batch() blocks until bridge_.is_done() or the OS window is closed
	window_->run_batch(bridge_, cfg_.display_fps);

	if (cfg_.auto_close_on_done) {
		window_->close();
	}
}

} // namespace vis
} // namespace vsepr
