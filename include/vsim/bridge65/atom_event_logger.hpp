#pragma once
/**
 * atom_event_logger.hpp
 * =====================
 * Per-atom event logger for the WO-BRIDGE-65 classical MD / empirical bridge.
 *
 * Responsibilities:
 *   - Accept AtomEvent records from the MD integration loop.
 *   - Decide console visibility via EventLimiter.
 *   - Write JSONL records to the event archive.
 *   - Flush per-step aggregate summaries to TSV.
 *   - Emit [EVENT_LIMITER] console lines when rate limiting activates.
 *
 * Console format:
 *   [ATOM_EVENT] step=1240 t=1.240e-12 id=42 sym=Fe event=collision
 *     E_k=2.14e-2 eV E_u=-4.81e-1 eV dE=8.20e-3 eV
 *     |F|=1.92e+1 eV/A partner=77 r=2.18 A gate=pass
 *
 * WO-BRIDGE-65  |  v5.1.13
 */

#include "atom_event.hpp"
#include "event_limiter.hpp"
#include <fstream>
#include <string>

namespace vsim::bridge65 {

// ============================================================================
// Logger configuration (mirrors [bridge65.atom_events] VSIM block)
// ============================================================================

struct AtomEventLoggerConfig {
	bool        enabled              = true;
	bool        console              = true;
	bool        jsonl                = true;
	bool        per_atom_energy      = true;
	bool        collision_events     = true;
	bool        bond_events          = true;
	bool        unbond_events        = true;
	bool        force_spike_events   = true;
	bool        thermal_spike_events = true;
	bool        energy_anomaly_events = true;
	std::string run_id               = "run_0001";
	std::string jsonl_path           = "events/atom_events.jsonl";
	std::string tsv_path             = "events/event_summary.tsv";
	EventLimiterConfig limiter;
};

// ============================================================================
// AtomEventLogger
// ============================================================================

class AtomEventLogger {
public:
	explicit AtomEventLogger(AtomEventLoggerConfig cfg = {});
	~AtomEventLogger();

	// Disable copy; allow move.
	AtomEventLogger(const AtomEventLogger&)            = delete;
	AtomEventLogger& operator=(const AtomEventLogger&) = delete;
	AtomEventLogger(AtomEventLogger&&)                 = default;

	// Submit one event from the MD loop.
	void log(const AtomEvent& ev);

	// Call at the start of each MD step to reset per-step counters.
	void begin_step(std::uint64_t step, double time_s);

	// Call at the end of each MD step.
	// Flushes aggregate TSV row and optional [EVENT_LIMITER] console line.
	void end_step(std::uint64_t step, double time_s);

	// Force flush all open file handles.
	void flush();

private:
	// Routing helpers
	bool accept_event(const AtomEvent& ev) const;

	// Output writers
	void write_jsonl(const AtomEvent& ev);
	void print_console(const AtomEvent& ev);
	void write_tsv_row(std::uint64_t step, double time_s,
					   const EventLimiterState& st);
	void print_limiter_summary(std::uint64_t step,
							   const EventLimiterState& st);

	AtomEventLoggerConfig cfg_;
	EventLimiter          limiter_;
	std::ofstream         jsonl_;
	std::ofstream         tsv_;
	bool                  tsv_header_written_ = false;
	uint64_t              current_step_       = 0;
	double                current_time_s_     = 0.0;
};

} // namespace vsim::bridge65
