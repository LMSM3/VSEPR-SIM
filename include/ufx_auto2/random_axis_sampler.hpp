// include/ufx_auto2/random_axis_sampler.hpp
// UFX_AUTO_2 Phase 2 -- Random Axis Sampler
// VSEPR-SIM v5 beta8 -> beta9
//
// RandomAxisSampler draws AxisSample records from an AxisConfig.
// Uses a seeded std::mt19937_64 for reproducibility.
// Each call to next() advances the internal state and increments
// the sample index.

#pragma once

#include "axis_config.hpp"
#include <cstdint>
#include <random>

namespace vsepr::ufx {

class RandomAxisSampler {
public:
	explicit RandomAxisSampler(const AxisConfig& config, uint64_t seed = 0);

	// Draw the next sample. Increments internal sample counter.
	AxisSample next();

	// Reset the RNG to initial seed and zero the counter.
	void reset(uint64_t seed);

	uint64_t seed()         const noexcept { return seed_; }
	int      sample_count() const noexcept { return count_; }

private:
	const AxisConfig&      config_;
	uint64_t               seed_  = 0;
	int                    count_ = 0;
	std::mt19937_64        rng_;

	// Discrete draw: uniformly pick one entry from a DiscreteAxis.
	// Returns empty string if axis has no values.
	std::string draw_discrete_(const DiscreteAxis& axis);

	// Continuous draw: uniform real in [axis.lo, axis.hi].
	double      draw_continuous_(const ContinuousAxis& axis);

	// Integer draw: uniform integer in [round(lo), round(hi)].
	int         draw_int_(const ContinuousAxis& axis);
};

} // namespace vsepr::ufx
