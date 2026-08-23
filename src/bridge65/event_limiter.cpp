/**
 * event_limiter.cpp
 * =================
 * WO-BRIDGE-65 high-rate event limiter/sampler implementation.
 *
 * WO-BRIDGE-65  |  v5.1.13
 */

#include "vsim/bridge65/event_limiter.hpp"
#include <algorithm>
#include <chrono>

namespace vsim::bridge65 {

// ----------------------------------------------------------------------------
// Construction
// ----------------------------------------------------------------------------

EventLimiter::EventLimiter(EventLimiterConfig cfg)
	: cfg_(std::move(cfg)) {}

// ----------------------------------------------------------------------------
// Window management
// ----------------------------------------------------------------------------

void EventLimiter::begin_window() {
	window_start_  = std::chrono::steady_clock::now();
	window_events_ = 0;
}

void EventLimiter::end_window(std::uint64_t step, double /*time_s*/) {
	using namespace std::chrono;
	auto now     = steady_clock::now();
	double dt_s  = duration<double>(now - window_start_).count();
	if (dt_s < 1e-9) dt_s = 1e-9;

	state_.event_rate  = static_cast<double>(window_events_) / dt_s;
	state_.active      = cfg_.enabled &&
		(window_events_ > cfg_.max_console_events_per_step ||
		 state_.event_rate > static_cast<double>(cfg_.max_console_events_per_second));
	(void)step;
}

// ----------------------------------------------------------------------------
// Per-step reset
// ----------------------------------------------------------------------------

void EventLimiter::reset_step() {
	state_.total_events    = 0;
	state_.printed_events  = 0;
	state_.collisions      = 0;
	state_.bond_created    = 0;
	state_.bond_broken     = 0;
	state_.energy_anomaly  = 0;
	state_.force_spike     = 0;
	state_.mean_dE_eV      = 0.0;
	state_.max_dE_eV       = 0.0;
	state_.mean_force_eV_A = 0.0;
	state_.max_force_eV_A  = 0.0;
	step_print_count_      = 0;
}

// ----------------------------------------------------------------------------
// Observe / accumulate
// ----------------------------------------------------------------------------

void EventLimiter::accumulate_stats(const AtomEvent& ev) {
	++state_.total_events;
	++window_events_;

	double adE  = std::abs(ev.energy.delta_eV);
	double aF   = ev.force.magnitude_eV_A;
	double n    = static_cast<double>(state_.total_events);

	state_.mean_dE_eV      += (adE - state_.mean_dE_eV)      / n;
	state_.mean_force_eV_A += (aF  - state_.mean_force_eV_A) / n;
	if (adE > state_.max_dE_eV)      state_.max_dE_eV      = adE;
	if (aF  > state_.max_force_eV_A) state_.max_force_eV_A = aF;

	switch (ev.type) {
		case AtomEventType::Collision:     ++state_.collisions;    break;
		case AtomEventType::BondCreated:   ++state_.bond_created;  break;
		case AtomEventType::BondBroken:    ++state_.bond_broken;   break;
		case AtomEventType::EnergyAnomaly: ++state_.energy_anomaly; break;
		case AtomEventType::ForceSpike:    ++state_.force_spike;   break;
		default: break;
	}
}

void EventLimiter::observe(const AtomEvent& ev) {
	accumulate_stats(ev);
}

// ----------------------------------------------------------------------------
// Console print decision
// ----------------------------------------------------------------------------

bool EventLimiter::is_always_print(const AtomEvent& ev) const {
	for (auto t : cfg_.always_print) {
		if (ev.type == t) return true;
	}
	return false;
}

bool EventLimiter::should_print(const AtomEvent& ev) {
	if (!cfg_.enabled) {
		++step_print_count_;
		++state_.printed_events;
		return true;
	}

	// Always-print types bypass the limiter
	if (is_always_print(ev)) {
		++step_print_count_;
		++state_.printed_events;
		return true;
	}

	// Hard per-step cap
	if (step_print_count_ >= cfg_.max_console_events_per_step) {
		return false;
	}

	// When active, apply sampling
	if (state_.active) {
		// Simple xorshift32-derived Bernoulli
		uint32_t r = next_rand();
		double   p = static_cast<double>(r) / 4294967296.0;
		if (p > cfg_.sample_rate) {
			return false;
		}
	}

	++step_print_count_;
	++state_.printed_events;
	return true;
}

// ----------------------------------------------------------------------------
// Lightweight PRNG (xorshift32)
// ----------------------------------------------------------------------------

uint32_t EventLimiter::next_rand() {
	rng_state_ ^= rng_state_ << 13;
	rng_state_ ^= rng_state_ >> 17;
	rng_state_ ^= rng_state_ << 5;
	return rng_state_;
}

} // namespace vsim::bridge65
