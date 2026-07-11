// src/v4/uff/uff_live_relay.cpp
// Formation Engine v4.1.0 -- UFX live ANSI shell relay implementation

#include "uff_live_relay.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdio>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

namespace vsepr::uff {

// constexpr static storage definitions
constexpr const char* UFFLiveRelay::k_frames[UFFLiveRelay::k_spinner_frames];

// ---------------------------------------------------------------------------
// ANSI escape sequences
// ---------------------------------------------------------------------------
namespace ansi {
	constexpr const char* reset      = "\033[0m";
	constexpr const char* bold       = "\033[1m";
	constexpr const char* dim        = "\033[2m";
	constexpr const char* red        = "\033[31m";
	constexpr const char* green      = "\033[32m";
	constexpr const char* yellow     = "\033[33m";
	constexpr const char* cyan       = "\033[36m";
	constexpr const char* white      = "\033[37m";
	constexpr const char* bright_red    = "\033[91m";
	constexpr const char* bright_green  = "\033[92m";
	constexpr const char* bright_yellow = "\033[93m";
	constexpr const char* bright_cyan   = "\033[96m";
	constexpr const char* bright_white  = "\033[97m";

	// Move cursor up N lines
	inline std::string up(int n) {
		return "\033[" + std::to_string(n) + "A";
	}
	// Erase entire line (cursor stays)
	constexpr const char* erase_line = "\033[2K\r";
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

UFFLiveRelay::UFFLiveRelay(int total_entries)
	: total_entries_(total_entries)
{
	ansi_enable_();
}

UFFLiveRelay::~UFFLiveRelay() {
	stop();
}

// ---------------------------------------------------------------------------
// ansi_enable_ -- enable VT processing on Windows
// ---------------------------------------------------------------------------

void UFFLiveRelay::ansi_enable_() {
#ifdef _WIN32
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	if (h != INVALID_HANDLE_VALUE) {
		DWORD mode = 0;
		if (GetConsoleMode(h, &mode))
			SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	}
#endif
}

// ---------------------------------------------------------------------------
// bar_ -- filled progress bar string
// ---------------------------------------------------------------------------

std::string UFFLiveRelay::bar_(int filled, int total, int width) {
	if (total <= 0) total = 1;
	const int f = std::clamp((filled * width) / total, 0, width);
	std::string s;
	s.reserve(static_cast<std::size_t>(width + 2));
	for (int i = 0; i < f;      ++i) s += "\xe2\x96\x88"; // █ filled block
	for (int i = f; i < width; ++i) s += "\xe2\x96\x91"; // ░ light shade
	return s;
}

// ---------------------------------------------------------------------------
// confidence_tag_ -- short coloured label
// ---------------------------------------------------------------------------

std::string UFFLiveRelay::confidence_tag_(ParamConfidence c) {
	switch (c) {
		case ParamConfidence::Published:
			return std::string(ansi::bright_green) + "published" + ansi::reset;
		case ParamConfidence::Derived:
			return std::string(ansi::bright_cyan)  + "derived  " + ansi::reset;
		case ParamConfidence::Estimated:
			return std::string(ansi::bright_yellow)+ "estimated" + ansi::reset;
		case ParamConfidence::Missing:
			return std::string(ansi::bright_red)   + "MISSING  " + ansi::reset;
	}
	return "unknown";
}

// ---------------------------------------------------------------------------
// truncate_
// ---------------------------------------------------------------------------

std::string UFFLiveRelay::truncate_(const std::string& s, int max_len) {
	if (static_cast<int>(s.size()) <= max_len) return s;
	return s.substr(0, static_cast<std::size_t>(max_len - 1)) + "…";
}

// ---------------------------------------------------------------------------
// start / stop
// ---------------------------------------------------------------------------

void UFFLiveRelay::start() {
	{
		std::lock_guard<std::mutex> lk(mutex_);
		if (dashboard_active_) return;
		// Print blank lines to claim the dashboard area.
		for (int i = 0; i < k_dash_lines; ++i) std::cout << '\n';
		dashboard_active_ = true;
		running_.store(true);
	}
	spinner_thread_ = std::thread(&UFFLiveRelay::spinner_loop_, this);
}

void UFFLiveRelay::stop() {
	running_.store(false);
	if (spinner_thread_.joinable()) spinner_thread_.join();
	{
		std::lock_guard<std::mutex> lk(mutex_);
		if (!dashboard_active_) return;
		redraw_();
		// Print final summary line below the frozen dashboard.
		std::cout << '\n'
				  << ansi::bold << ansi::bright_white
				  << "[UFX] " << ansi::reset
				  << ansi::bright_green
				  << "Table population complete. "
				  << loaded_entries_ << " / " << total_entries_
				  << " entries loaded."
				  << ansi::reset << '\n';
		dashboard_active_ = false;
	}
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void UFFLiveRelay::on_stage(RunStage stage) {
	std::lock_guard<std::mutex> lk(mutex_);
	stage_ = stage;
}

void UFFLiveRelay::on_entry(const UFFEntry& entry) {
	std::lock_guard<std::mutex> lk(mutex_);
	++loaded_entries_;
	last_atom_type_  = entry.atom_type;
	last_element_    = entry.element;
	last_geometry_   = entry.geometry_tag;
	last_confidence_ = confidence_tag_(entry.confidence);
}

void UFFLiveRelay::on_batch_result(const BatchResult& result) {
	std::lock_guard<std::mutex> lk(mutex_);

	std::ostringstream line;
	if (result.passed) {
		line << ansi::bright_green << "✓ PASS" << ansi::reset;
	} else {
		line << ansi::bright_red   << "✗ FAIL" << ansi::reset;
	}
	line << "  Batch #" << result.batch_number
		 << "  (" << result.valid << "/" << result.total << " valid)";

	if (!result.errors.empty()) {
		line << '\n';
		for (const auto& err : result.errors) {
			line << ansi::bright_red
				 << "         ⚠ " << truncate_(err, 60)
				 << ansi::reset << '\n';
		}
	}

	batch_status_line_ = line.str();
	// Force an immediate redraw so batch result appears without waiting for
	// the next spinner tick.
	redraw_();
}

// ---------------------------------------------------------------------------
// spinner_loop_ -- background thread, ticks every 120 ms
// ---------------------------------------------------------------------------

void UFFLiveRelay::spinner_loop_() {
	while (running_.load()) {
		{
			std::lock_guard<std::mutex> lk(mutex_);
			spinner_idx_ = (spinner_idx_ + 1) % k_spinner_frames;
			redraw_();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(120));
	}
}

// ---------------------------------------------------------------------------
// redraw_ -- must be called under mutex_
// ---------------------------------------------------------------------------

void UFFLiveRelay::redraw_() {
	if (!dashboard_active_) return;

	// Move cursor up to the top of the dashboard.
	std::cout << ansi::up(k_dash_lines);

	const std::string spin = k_frames[spinner_idx_];
	const std::string stage_str{to_string(stage_)};

	// --- Line 1: header ---
	std::cout << ansi::erase_line
			  << ansi::bold << ansi::bright_cyan
			  << "╔══════════════════════════════════════════════════════╗"
			  << ansi::reset << '\n';

	// --- Line 2: title + spinner ---
	std::cout << ansi::erase_line
			  << ansi::bold << ansi::bright_cyan << "║ " << ansi::reset
			  << ansi::bold << ansi::bright_white
			  << "[UFX] " << spin << "  Building UFF runtime table"
			  << ansi::reset;
	// pad to column 54
	std::cout << std::string(std::max(0, 24 - static_cast<int>(stage_str.size())), ' ')
			  << ansi::bright_cyan << " ║" << ansi::reset << '\n';

	// --- Line 3: stage ---
	std::cout << ansi::erase_line
			  << ansi::bright_cyan << "║ " << ansi::reset
			  << ansi::dim << "Stage  : " << ansi::reset
			  << ansi::bright_white << std::left << std::setw(40) << truncate_(stage_str, 40)
			  << ansi::reset
			  << ansi::bright_cyan << " ║" << ansi::reset << '\n';

	// --- Line 4: last entry ---
	const std::string entry_label =
		last_atom_type_.empty()
			? "(waiting)"
			: (last_atom_type_ + "  " + last_element_ + "  " + last_geometry_
			   + "  " + last_confidence_);
	std::cout << ansi::erase_line
			  << ansi::bright_cyan << "║ " << ansi::reset
			  << ansi::dim << "Entry  : " << ansi::reset
			  << truncate_(entry_label, 41)
			  << ansi::bright_cyan << " ║" << ansi::reset << '\n';

	// --- Line 5: progress bar ---
	const std::string bar = bar_(loaded_entries_, total_entries_);
	std::ostringstream count;
	count << loaded_entries_ << " / " << total_entries_;
	std::cout << ansi::erase_line
			  << ansi::bright_cyan << "║ " << ansi::reset
			  << ansi::dim << "Table  : " << ansi::reset
			  << ansi::bright_green << bar << ansi::reset
			  << "  " << ansi::bright_white << std::left << std::setw(12) << count.str()
			  << ansi::reset
			  << ansi::bright_cyan << "║" << ansi::reset << '\n';

	// --- Line 6: batch status ---
	// May contain embedded newlines (error lines); print as-is but keep the
	// frame by always emitting erase_line at the start.
	const std::string bstat = batch_status_line_.empty()
		? ansi::dim + std::string("(no batch results yet)") + ansi::reset
		: batch_status_line_;
	std::cout << ansi::erase_line
			  << ansi::bright_cyan << "║ " << ansi::reset
			  << ansi::dim << "Batch  : " << ansi::reset
			  << bstat << '\n';

	// --- Line 7: footer ---
	std::cout << ansi::erase_line
			  << ansi::bold << ansi::bright_cyan
			  << "╚══════════════════════════════════════════════════════╝"
			  << ansi::reset << '\n';

	std::cout.flush();
}

} // namespace vsepr::uff
