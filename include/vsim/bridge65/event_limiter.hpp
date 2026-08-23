#pragma once
/**
 * event_limiter.hpp
 * =================
 * High-rate event limiter and sampler for the WO-BRIDGE-65 bridge.
 *
 * When event rate R = N_events / dt_wall exceeds R_max:
 *   - Console output is sampled (sample_rate) or suppressed.
 *   - Aggregate counts are tracked per step.
 *   - Full JSONL archive is preserved when configured.
 *   - "Always-print" event types (bond_broken, energy_anomaly, force_spike)
 *     bypass the rate limiter.
 *
 * Rule: Console output may be sampled. Artifact output must remain
 *       scientifically useful.
 *
 * WO-BRIDGE-65  |  v5.1.13
 */

#pragma once
#include "atom_event.hpp"
#include <chrono>
#include <cstddef>
#include <vector>

namespace vsim::bridge65 {

// ============================================================================
// Configuration
// ============================================================================

struct EventLimiterConfig {
	bool        enabled                      = true;
	std::size_t max_console_events_per_step  = 25;
	std::size_t max_console_events_per_second = 250;
	double      sample_rate                  = 0.05;   // Fraction kept when limited
	bool        aggregate_when_limited       = true;
	bool        write_full_jsonl_when_limited = true;

	// Event types that always reach the console even during limiting
	// (indices into AtomEventType)
	std::vector<AtomEventType> always_print = {
		AtomEventType::BondBroken,
		AtomEventType::EnergyAnomaly,
		AtomEventType::ForceSpike,
	};
};

// ============================================================================
// Per-step aggregate state
// ============================================================================

struct EventLimiterState {
	bool        active          = false;
	std::size_t total_events    = 0;
	std::size_t printed_events  = 0;
	std::size_t collisions      = 0;
	std::size_t bond_created    = 0;
	std::size_t bond_broken     = 0;
	std::size_t energy_anomaly  = 0;
	std::size_t force_spike     = 0;
	double      mean_dE_eV      = 0.0;
	double      max_dE_eV       = 0.0;
	double      mean_force_eV_A = 0.0;
	double      max_force_eV_A  = 0.0;
	double      event_rate      = 0.0;  // R_event (events/s wall time)
};

// ============================================================================
// EventLimiter
// ============================================================================

class EventLimiter {
public:
	explicit EventLimiter(EventLimiterConfig cfg = {});

	// Observe a new event and update internal counters.
	void observe(const AtomEvent& ev);

	// Returns true if this event should be printed to console this step.
	bool should_print(const AtomEvent& ev);

	// Reset per-step counters (call at start of each MD step).
	void reset_step();

	// Snapshot of current aggregate state.
	EventLimiterState state() const { return state_; }

	// Wall-clock measurement — call at start of limiter window.
	void begin_window();

	// Compute R_event and update active flag — call at end of step.
	void end_window(std::uint64_t step, double time_s);

private:
	bool is_always_print(const AtomEvent& ev) const;
	void accumulate_stats(const AtomEvent& ev);

	EventLimiterConfig cfg_;
	EventLimiterState  state_;
	std::size_t        step_print_count_ = 0;

	// Wall-time tracking
	std::chrono::steady_clock::time_point window_start_;
	std::size_t                            window_events_ = 0;

	// Pseudo-random sampler (deterministic seed per run)
	uint32_t rng_state_ = 0x4B4F5244u; // "KORD"
	uint32_t next_rand();
};

} // namespace vsim::bridge65
