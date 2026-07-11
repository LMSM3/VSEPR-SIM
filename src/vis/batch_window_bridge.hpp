#pragma once
/**
 * batch_window_bridge.hpp -- Lock-free frame feed for passive window display
 * ==========================================================================
 * VSEPR-SIM  |  branch: v5.0.0-beta.7-step-attempt
 *
 * Connects any continuous-run producer (metal_gen batch loop, database
 * generator, report pipeline) to a live window WITHOUT blocking either side.
 *
 * Design contract
 * ---------------
 *   Producer thread  →  push_frame()       (never blocks; drops frame if display busy)
 *   Display thread   →  latest_frame()     (always returns immediately)
 *   Status text      →  push_status()      (atomic string swap)
 *
 * Thread model
 * ------------
 *   One producer, one consumer (the window's render loop).
 *   Uses std::atomic<std::shared_ptr<T>> for the frame slot — C++20 guaranteed
 *   lock-free on all supported platforms (x86-64 / ARM64).
 *   Status text uses a mutex only on the write side (infrequent).
 *
 * Usage
 * -----
 *   // Producer side (batch loop thread):
 *   BatchWindowBridge bridge;
 *   bridge.push_frame(frame, "NiTi_B2");
 *   bridge.push_status({ .run_label="NiTi_B2", .frame_index=42, .artifacts_done=5 });
 *
 *   // Consumer side (window render loop — called ~5 fps):
 *   if (auto f = bridge.latest_frame()) {
 *       window.update(*f);
 *       window.set_overlay_text(bridge.status_text());
 *   }
 */

#include "../../src/io/xyz_unified.hpp"   // XYZFrame
#include "viz_config.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <sstream>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace vsepr {
namespace vis {

// ---------------------------------------------------------------------------
// BatchStatus — human-readable run state pushed by the producer
// ---------------------------------------------------------------------------
struct BatchStatus {
	std::string  run_label;          // e.g. material tag, report ID
	int          frame_index   = 0;  // simulation frame or supercell index
	int          total_frames  = 0;  // 0 = unknown
	int          artifacts_done = 0; // files written so far
	int          artifacts_total = 0;
	double       elapsed_s     = 0.0; // wall-clock since batch start
	std::string  current_op;          // e.g. "writing .xyza", "building supercell"

	std::string to_overlay_text() const {
		std::ostringstream s;
		s << "[BATCH] " << run_label;
		if (total_frames > 0)
			s << "  frame " << frame_index << "/" << total_frames;
		else
			s << "  frame " << frame_index;
		if (artifacts_total > 0)
			s << "  artifacts " << artifacts_done << "/" << artifacts_total;
		if (!current_op.empty())
			s << "  | " << current_op;
		if (elapsed_s > 0.0) {
			int mins  = static_cast<int>(elapsed_s) / 60;
			int secs  = static_cast<int>(elapsed_s) % 60;
			s << "  (" << mins << "m" << secs << "s)";
		}
		return s.str();
	}
};

// ---------------------------------------------------------------------------
// BatchWindowBridge
// ---------------------------------------------------------------------------
class BatchWindowBridge {
public:
	BatchWindowBridge() = default;

	// -----------------------------------------------------------------------
	// Producer API — call from batch loop thread
	// -----------------------------------------------------------------------

	/**
	 * Push a new frame from the producer.
	 * Never blocks.  If the display hasn't consumed the previous frame yet,
	 * the old frame is silently replaced (drop-oldest policy).
	 *
	 * @param frame   The XYZFrame to display (copied into a shared_ptr).
	 * @param label   Optional label written into the status run_label.
	 */
	void push_frame(const io::XYZFrame& frame, const std::string& label = "") {
		auto ptr = std::make_shared<io::XYZFrame>(frame);
		frame_slot_.store(ptr);                    // C++20 atomic<shared_ptr>
		++frames_pushed_;

		if (!label.empty()) {
			std::lock_guard<std::mutex> lk(status_mtx_);
			status_.run_label = label;
			status_.frame_index = static_cast<int>(frames_pushed_.load());
		}
	}

	/**
	 * Push a full BatchStatus record.
	 */
	void push_status(const BatchStatus& s) {
		std::lock_guard<std::mutex> lk(status_mtx_);
		status_ = s;
	}

	/**
	 * Convenience: update only the progress counters (cheap path).
	 */
	void push_progress(int frame_index, int total_frames,
					   int artifacts_done, int artifacts_total,
					   const std::string& current_op = "") {
		std::lock_guard<std::mutex> lk(status_mtx_);
		status_.frame_index     = frame_index;
		status_.total_frames    = total_frames;
		status_.artifacts_done  = artifacts_done;
		status_.artifacts_total = artifacts_total;
		if (!current_op.empty()) status_.current_op = current_op;
		status_.elapsed_s = elapsed_seconds();
	}

	/** Mark the batch run as started (resets elapsed clock). */
	void start() {
		start_time_ = std::chrono::steady_clock::now();
		frames_pushed_.store(0);
		done_.store(false);
	}

	/** Signal that the batch run has finished. */
	void finish() { done_.store(true); }

	bool is_done() const { return done_.load(); }

	double elapsed_seconds() const {
		auto now = std::chrono::steady_clock::now();
		return std::chrono::duration<double>(now - start_time_).count();
	}

	// -----------------------------------------------------------------------
	// Consumer API — call from window render loop thread
	// -----------------------------------------------------------------------

	/**
	 * Returns the latest available frame, or nullptr if none pushed yet.
	 * Never blocks.
	 */
	std::shared_ptr<io::XYZFrame> latest_frame() const {
		return frame_slot_.load();
	}

	/**
	 * Returns a formatted overlay string for the window's status bar.
	 */
	std::string status_text() const {
		std::lock_guard<std::mutex> lk(status_mtx_);
		return status_.to_overlay_text();
	}

	BatchStatus status_snapshot() const {
		std::lock_guard<std::mutex> lk(status_mtx_);
		return status_;
	}

	std::uint64_t frames_pushed_count() const { return frames_pushed_.load(); }

	// -----------------------------------------------------------------------
	// Display-side throttle helper
	// -----------------------------------------------------------------------

	/**
	 * Returns true if enough time has elapsed since the last display update
	 * for the given target_fps (default 5).
	 * The consumer calls this to decide whether to call latest_frame() at all.
	 */
	bool display_tick(float target_fps = 5.0f) {
		auto now = std::chrono::steady_clock::now();
		double elapsed = std::chrono::duration<double>(now - last_display_).count();
		if (elapsed >= (1.0 / target_fps)) {
			last_display_ = now;
			return true;
		}
		return false;
	}

private:
	// Lock-free latest-frame slot (C++20 atomic<shared_ptr>)
	mutable std::atomic<std::shared_ptr<io::XYZFrame>> frame_slot_;
	std::atomic<std::uint64_t>                         frames_pushed_{0};

	// Status (written infrequently by producer, read by display thread)
	mutable std::mutex status_mtx_;
	BatchStatus        status_;

	// Lifecycle
	std::atomic<bool>                              done_{false};
	std::chrono::steady_clock::time_point          start_time_;
	std::chrono::steady_clock::time_point          last_display_;
};

} // namespace vis
} // namespace vsepr
