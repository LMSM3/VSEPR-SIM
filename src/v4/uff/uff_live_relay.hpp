// src/v4/uff/uff_live_relay.hpp
// Formation Engine v4.1.0 -- UFX live ANSI shell relay
//
// Owns a 6-line in-place ANSI dashboard that is redrawn on every event:
//
//   ╔══════════════════════════════════════════════════════╗
//   ║ [UFX]  Building UFF runtime table                    ║
//   ║ Stage  : Loading UFF table                           ║
//   ║ Entry  : Os6+6  (osmium, octahedral)  published      ║
//   ║ Table  : ████████████░░░░░░░░░░  63 / 126           ║
//   ║ Batch  : #6  PASS  (13/13 valid)                     ║
//   ╚══════════════════════════════════════════════════════╝
//
// Thread-safety: all public methods are mutex-guarded; safe to call from
// a loading thread while the spinner ticks from a timer thread.
//
// ANSI VT100 sequences are used throughout.  On Windows the relay calls
// EnableVirtualTerminalProcessing on stdout at construction.

#pragma once

#include "uff_table.hpp"
#include "run_stage.hpp"
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>

namespace vsepr::uff {

// One per-batch validation result emitted by UFFBatchValidator.
struct BatchResult {
	int         batch_number   = 0;   // 1-based
	int         total          = 0;
	int         valid          = 0;
	bool        passed         = false;
	std::vector<std::string> errors;  // field-level error messages
};

// ---------------------------------------------------------------------------
// UFFLiveRelay
// ---------------------------------------------------------------------------

class UFFLiveRelay {
public:
	// total_entries: the total number of entries to be loaded (for the bar).
	explicit UFFLiveRelay(int total_entries);
	~UFFLiveRelay();

	// Lifecycle
	void start();   // reserve dashboard lines, start spinner thread
	void stop();    // freeze dashboard, print final summary below

	// Events (call from loading / validation code)
	void on_stage(RunStage stage);
	void on_entry(const UFFEntry& entry);           // per-entry during load
	void on_batch_result(const BatchResult& result); // per batch from validator

private:
	// ANSI helpers
	static void ansi_enable_();
	static std::string bar_(int filled, int total, int width = 22);
	static std::string confidence_tag_(ParamConfidence c);
	static std::string truncate_(const std::string& s, int max_len);

	void redraw_();          // called under mutex_; does the actual write
	void spinner_loop_();    // background thread

	// State
	mutable std::mutex    mutex_;
	std::atomic<bool>     running_{false};
	std::thread           spinner_thread_;

	int                   total_entries_;
	int                   loaded_entries_  = 0;
	RunStage              stage_           = RunStage::LoadingUFF;

	std::string           last_atom_type_;
	std::string           last_element_;
	std::string           last_geometry_;
	std::string           last_confidence_;

	std::string           batch_status_line_;

	static constexpr int  k_spinner_frames = 4;
	static constexpr const char* k_frames[k_spinner_frames] = {"◐","◓","◑","◒"};
	int                   spinner_idx_ = 0;

	static constexpr int  k_dash_lines = 7;  // lines owned by the dashboard
	bool                  dashboard_active_ = false;
};

} // namespace vsepr::uff
