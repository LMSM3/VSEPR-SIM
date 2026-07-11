// src/ufx_auto2/random_axis_sampler.cpp
// UFX_AUTO_2 Phase 2 -- Random Axis Sampler Implementation

#include "ufx_auto2/random_axis_sampler.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace vsepr::ufx {

// ============================================================================
// AxisConfig::default_phase2
// ============================================================================

AxisConfig AxisConfig::default_phase2() {
	AxisConfig cfg;

	cfg.element_family = {
		"element_family",
		{
			"alkali", "alkaline_earth", "transition_metal",
			"post_transition", "metalloid", "nonmetal",
			"halogen", "noble_gas", "lanthanide", "actinide"
		}
	};

	cfg.element = {
		"element",
		{
			// Period 1
			"H",
			// Period 2
			"Li","Be","B","C","N","O","F","Ne",
			// Period 3
			"Na","Mg","Al","Si","P","S","Cl","Ar",
			// Period 4
			"K","Ca","Ti","V","Cr","Mn","Fe","Co","Ni","Cu","Zn",
			"Ga","Ge","As","Se","Br",
			// Period 5
			"Zr","Mo","Pd","Ag","Cd","Sn","I",
			// Period 6
			"Cs","Ba","La","Ce","Pr","Nd","Gd","Tb","Dy",
			"W","Re","Os","Ir","Pt","Au","Hg","Pb","Bi",
			// Period 7
			"Th","U","Pu"
		}
	};

	cfg.geometry = {
		"geometry",
		{
			"linear",
			"trigonal_planar",
			"tetrahedral",
			"square_planar",
			"trigonal_bipyramidal",
			"octahedral",
			"pentagonal_bipyramidal"
		}
	};

	cfg.phase = {
		"phase",
		{ "gas", "liquid", "solid", "powder", "molten_salt" }
	};

	cfg.target_property = {
		"target_property",
		{ "force_field", "thermo", "crystal", "transport", "mechanical" }
	};

	cfg.oxidation_state = { "oxidation_state", -4.0,  8.0 };
	cfg.coordination    = { "coordination",      1.0, 12.0 };
	cfg.temperature_K   = { "temperature_K",    50.0, 2500.0 };
	cfg.pressure_atm    = { "pressure_atm",   0.001,  1000.0 };

	return cfg;
}

bool AxisConfig::valid() const noexcept {
	if (element_family.empty())    return false;
	if (element.empty())           return false;
	if (geometry.empty())          return false;
	if (phase.empty())             return false;
	if (target_property.empty())   return false;
	if (temperature_K.lo >= temperature_K.hi) return false;
	if (pressure_atm.lo  >= pressure_atm.hi)  return false;
	return true;
}

// ============================================================================
// RandomAxisSampler
// ============================================================================

RandomAxisSampler::RandomAxisSampler(const AxisConfig& config, uint64_t seed)
	: config_(config)
	, seed_(seed)
	, count_(0)
	, rng_(seed)
{
	if (!config.valid()) {
		throw std::invalid_argument("RandomAxisSampler: AxisConfig is invalid");
	}
}

void RandomAxisSampler::reset(uint64_t seed) {
	seed_  = seed;
	count_ = 0;
	rng_.seed(seed);
}

AxisSample RandomAxisSampler::next() {
	AxisSample s;
	s.element_family   = draw_discrete_(config_.element_family);
	s.element          = draw_discrete_(config_.element);
	s.oxidation_state  = draw_int_(config_.oxidation_state);
	s.coordination     = draw_int_(config_.coordination);
	s.geometry         = draw_discrete_(config_.geometry);
	s.phase            = draw_discrete_(config_.phase);
	s.temperature_K    = draw_continuous_(config_.temperature_K);
	s.pressure_atm     = draw_continuous_(config_.pressure_atm);
	s.target_property  = draw_discrete_(config_.target_property);
	s.seed_used        = seed_;
	s.sample_index     = count_++;
	return s;
}

std::string RandomAxisSampler::draw_discrete_(const DiscreteAxis& axis) {
	if (axis.values.empty()) return "";
	std::uniform_int_distribution<std::size_t> dist(0, axis.values.size() - 1);
	return axis.values[dist(rng_)];
}

double RandomAxisSampler::draw_continuous_(const ContinuousAxis& axis) {
	std::uniform_real_distribution<double> dist(axis.lo, axis.hi);
	return dist(rng_);
}

int RandomAxisSampler::draw_int_(const ContinuousAxis& axis) {
	int lo = static_cast<int>(std::round(axis.lo));
	int hi = static_cast<int>(std::round(axis.hi));
	if (lo > hi) std::swap(lo, hi);
	std::uniform_int_distribution<int> dist(lo, hi);
	return dist(rng_);
}

} // namespace vsepr::ufx
