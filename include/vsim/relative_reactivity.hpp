// relative_reactivity.hpp  -  WO-85B  (Part E)
//
// Relative-reactivity inference module.
//
// A tiny, header-only inference program that takes the *derivative of species
// change* over successive simulation steps and reports it as a scalar
// "relative reactivity".  It is the analytic backing for the right-most column
// of terminal output-format B (see cmd_run_vsim.cpp).
//
// Conceptual definition
// ---------------------
// For a species with composition x (mole fraction, percent, or population)
// sampled at steps t-1 and t separated by dt:
//
//     dx/dt  ~=  (x_t - x_{t-1}) / dt            (discrete forward difference)
//
// The instantaneous *turnover* of the whole system at step t is the sum of the
// absolute per-species rates:
//
//     turnover_t = sum_i | dx_i/dt |
//
// "Relative reactivity" then normalises turnover against the largest turnover
// observed so far in the run, giving a dimensionless value in [0, 1]:
//
//     reactivity_t = turnover_t / max_{k <= t} turnover_k
//
// This makes the column comparable across steps and across species without
// depending on absolute units:  1.00 marks the most reactive moment of the run
// (typically the steepest part of a decay/formation curve), values near 0 mark
// quiescent / converged steps.
//
// Design
//   - Header-only; depends only on the standard library.
//   - Deterministic; no global state.
//   - Reusable by CLI viewers, tests, and any future analysis sink.
//
// Usage
//   #include "vsim/relative_reactivity.hpp"
//   vsim::reactivity::RelativeReactivity rr;
//   for (each step) {
//       auto frame = rr.observe({ {"CH4", ch4}, {"CO2", co2}, {"H2O", h2o} }, dt);
//       // frame.reactivity   -> [0,1] relative reactivity (right-most column)
//       // frame.turnover     -> raw sum |dx/dt|
//       // frame.dominant     -> fastest-changing species name
//   }
//
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace vsim {
namespace reactivity {

// ---------------------------------------------------------------------------
// A single named composition sample fed into the tracker.
//   name  -  species label, e.g. "CH4"
//   value -  current composition (mole fraction 0..1, percent, or raw count)
// ---------------------------------------------------------------------------
struct SpeciesSample {
	std::string name;
	double      value = 0.0;
};

// ---------------------------------------------------------------------------
// Per-species derivative record produced each observed step.
// ---------------------------------------------------------------------------
struct SpeciesRate {
	std::string name;
	double      value = 0.0;   // composition at this step
	double      rate  = 0.0;   // signed dx/dt  (negative = decaying)
	double      abs_rate = 0.0; // |dx/dt|
};

// ---------------------------------------------------------------------------
// Result of one observation step.
// ---------------------------------------------------------------------------
struct ReactivityFrame {
	std::vector<SpeciesRate> species;      // per-species derivative breakdown
	double      turnover     = 0.0;        // sum |dx/dt| across species
	double      reactivity   = 0.0;        // turnover normalised to running peak -> [0,1]
	std::string dominant;                  // name of fastest-changing species
	double      dominant_rate = 0.0;       // signed rate of the dominant species
	bool        first        = true;       // true on the very first observation (no prior)
};

// ---------------------------------------------------------------------------
// Free helper: discrete forward-difference derivative of a scalar series.
// Guards against non-positive dt.
// ---------------------------------------------------------------------------
inline double derivative(double prev, double curr, double dt) {
	if (!(dt > 0.0)) dt = 1.0;
	return (curr - prev) / dt;
}

// ---------------------------------------------------------------------------
// RelativeReactivity  -  stateful step-wise tracker.
//
// Feed it a composition snapshot each step via observe(); it retains the
// previous snapshot, computes the per-species derivative, aggregates the
// turnover, and normalises against the running peak turnover to yield the
// relative-reactivity scalar.
// ---------------------------------------------------------------------------
class RelativeReactivity {
public:
	// Observe the current composition snapshot.  dt is the step spacing used
	// for the derivative (defaults to 1 "step").  Species are matched to the
	// previous snapshot by name; species new to this step contribute a zero
	// rate for the first step they appear.
	ReactivityFrame observe(const std::vector<SpeciesSample>& snapshot, double dt = 1.0) {
		ReactivityFrame frame;
		frame.first = !have_prev_;
		frame.species.reserve(snapshot.size());

		double turnover = 0.0;
		double best_abs = -1.0;

		for (const auto& s : snapshot) {
			SpeciesRate sr;
			sr.name  = s.name;
			sr.value = s.value;

			double prev = 0.0;
			bool   found = false;
			if (have_prev_) {
				for (const auto& p : prev_) {
					if (p.name == s.name) { prev = p.value; found = true; break; }
				}
			}

			// First observation (or first sighting of this species) has no
			// derivative reference -> rate 0.
			sr.rate     = (have_prev_ && found) ? derivative(prev, s.value, dt) : 0.0;
			sr.abs_rate = std::fabs(sr.rate);
			turnover   += sr.abs_rate;

			if (sr.abs_rate > best_abs) {
				best_abs            = sr.abs_rate;
				frame.dominant      = sr.name;
				frame.dominant_rate = sr.rate;
			}
			frame.species.push_back(std::move(sr));
		}

		frame.turnover = turnover;
		peak_turnover_ = std::max(peak_turnover_, turnover);
		frame.reactivity = (peak_turnover_ > 0.0)
			? std::clamp(turnover / peak_turnover_, 0.0, 1.0)
			: 0.0;

		// Roll state forward.
		prev_.assign(snapshot.begin(), snapshot.end());
		have_prev_ = true;
		return frame;
	}

	// Largest turnover seen so far (denominator of the relative measure).
	double peak_turnover() const { return peak_turnover_; }

	// Reset the tracker to its initial (no-history) state.
	void reset() {
		prev_.clear();
		have_prev_    = false;
		peak_turnover_ = 0.0;
	}

private:
	std::vector<SpeciesSample> prev_;
	bool   have_prev_    = false;
	double peak_turnover_ = 0.0;
};

// ---------------------------------------------------------------------------
// Convenience: a single-shot relative reactivity given a peak reference.
// Useful when the caller already tracks turnover externally.
// ---------------------------------------------------------------------------
inline double relative_of(double turnover, double peak) {
	if (!(peak > 0.0)) return 0.0;
	return std::clamp(turnover / peak, 0.0, 1.0);
}

} // namespace reactivity
} // namespace vsim
