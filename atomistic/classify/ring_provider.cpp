/**
 * ring_provider.cpp  --  Day 84 / WO-84C
 * ============================================================================
 * Implementation of RingProviderBuilder.  See ring_provider.hpp.
 * ============================================================================
 */

#include "atomistic/classify/ring_provider.hpp"
#include "atomistic/core/state.hpp"

#include <utility>

namespace atomistic {
namespace classify {

RingProvider RingProviderBuilder::build(const State& state) const {
	RingSystem system;
	return build(state, system);
}

RingProvider RingProviderBuilder::build(const State& state, RingSystem& out_system) const {
	out_system = RingDetector(max_ring_size_).detect(state);

	RingProvider provider;

	provider.ring_sizes_for_atom =
		[sizes = out_system.atom_ring_sizes](std::size_t i) -> std::vector<int> {
			if (i >= sizes.size()) return {};
			return sizes[i];
		};

	provider.total_ring_count =
		[total = out_system.total_rings()]() -> std::optional<int> {
			return total;
		};

	return provider;
}

} // namespace classify
} // namespace atomistic
