#pragma once
/**
 * ring_detector.hpp  --  Day 84 / WO-84C
 * ============================================================================
 * State-based small-cycle / SSSR-lite ring detection for organic classification.
 *
 * A legacy detector exists at src/molecule/ring_detect.hpp, but it is bound to
 * vsepr::glass::GlassMolecule.  The classify layer works on atomistic::State,
 * so WO-84C provides a dedicated, dependency-light detector here.
 *
 * What it computes from the heavy-atom (non-H) bond graph:
 *   - SSSR-lite ring set (smallest independent rings, GF(2) edge basis)
 *   - circuit rank  E - V + C  (independent cycle count, ground truth)
 *   - per-atom ring-size membership
 *   - ring-bond set (non-bridge edges) for rotatable-bond exclusion
 *   - fused-ring detection (ring pairs sharing an edge)
 *
 * Targets (WO-84C):
 *   benzene       -> 1 ring (size 6)
 *   cyclohexane   -> 1 ring (size 6)
 *   cyclopropane  -> 1 ring (size 3, strained)
 *   naphthalene   -> 2 rings (fused)
 *   linear hexane -> 0 rings
 *
 * Design rule (providers.hpp): ring data is NOT owned by VSEPR or
 * OrganicCandidate.  RingProvider (ring_provider.hpp) is the sanctioned inlet;
 * this detector is the deterministic engine behind it.
 */

#include <cstddef>
#include <utility>
#include <vector>

namespace atomistic {

struct State;

namespace classify {

// A single detected ring: heavy-atom indices forming a simple cycle.
struct DetectedRing {
	std::vector<std::size_t> atoms;
	int  size()     const { return static_cast<int>(atoms.size()); }
	bool strained() const { return size() > 0 && size() <= 4; }
};

// Full result of ring detection over a State's heavy-atom graph.
struct RingSystem {
	std::vector<DetectedRing> rings;               // SSSR-lite selection
	int circuit_rank      = 0;                      // E - V + C (independent cycles)
	int fused_ring_pairs  = 0;                      // ring pairs sharing an edge

	// Per original-atom-index ring sizes (empty for non-ring atoms).
	std::vector<std::vector<int>> atom_ring_sizes;

	// Normalized (min,max) heavy-atom ring bonds (non-bridge edges).
	std::vector<std::pair<std::size_t, std::size_t>> ring_bonds;

	int  total_rings()    const { return static_cast<int>(rings.size()); }
	int  strained_rings() const;
	bool has_fused_system() const { return fused_ring_pairs > 0; }

	// True if atom i participates in any ring.
	bool is_ring_atom(std::size_t i) const {
		return i < atom_ring_sizes.size() && !atom_ring_sizes[i].empty();
	}

	// True if the undirected bond (i,j) is a ring bond (non-bridge).
	bool is_ring_bond(std::size_t i, std::size_t j) const;
};

class RingDetector {
public:
	static constexpr int kDefaultMaxRingSize = 8;

	explicit RingDetector(int max_ring_size = kDefaultMaxRingSize)
		: max_ring_size_(max_ring_size) {}

	RingSystem detect(const State& state) const;

private:
	int max_ring_size_;
};

// Convenience free function mirroring RingDetector::detect().
RingSystem detect_ring_system(const State& state,
							  int max_ring_size = RingDetector::kDefaultMaxRingSize);

} // namespace classify
} // namespace atomistic
