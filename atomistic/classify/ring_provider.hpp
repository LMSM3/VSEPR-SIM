#pragma once
/**
 * ring_provider.hpp  --  Day 84 / WO-84C
 * ============================================================================
 * Builds a RingProvider (providers.hpp) from an atomistic::State using the
 * deterministic RingDetector (ring_detector.hpp).
 *
 * RingProvider is the ONLY sanctioned ring-data inlet into the classify layer
 * (per providers.hpp).  OrganicClassifier consumes it through ProviderSet::ring;
 * when absent, the classifier falls back to circuit-rank ring counting.
 *
 * Usage:
 *     ProviderSet ps = ProviderSet::null();
 *     ps.ring = RingProviderBuilder{}.build(state);
 *     OrganicClassifier clf(ps);
 */

#include "atomistic/classify/providers.hpp"
#include "atomistic/classify/ring_detector.hpp"

namespace atomistic {

struct State;

namespace classify {

class RingProviderBuilder {
public:
	static constexpr int kDefaultMaxRingSize = RingDetector::kDefaultMaxRingSize;

	explicit RingProviderBuilder(int max_ring_size = kDefaultMaxRingSize)
		: max_ring_size_(max_ring_size) {}

	// Detect rings in `state` and return a RingProvider whose closures own a
	// snapshot of the per-atom ring-size table and total ring count.
	RingProvider build(const State& state) const;

	// Same detection, but also hand back the full RingSystem for callers that
	// need ring bonds (e.g. rotatable-bond exclusion) or fused-ring flags.
	RingProvider build(const State& state, RingSystem& out_system) const;

private:
	int max_ring_size_;
};

} // namespace classify
} // namespace atomistic
