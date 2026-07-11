#pragma once
/**
 * verlet_list.hpp — ERB-Aware Verlet Neighbor List
 * =================================================
 *
 * A Verlet neighbor list designed specifically for the Environment-Responsive
 * Bead (ERB) framework. The critical difference from a naive geometric NL:
 *
 *   Each neighbor entry carries η_j (the environment state of the neighbor).
 *
 * This means modulation factors g_k(η_i, η_j) = 1 + γ_k · η̄ can be
 * evaluated directly from NL entries during the force/energy pass —
 * no O(N²) η scan required.
 *
 * Architecture:
 *
 *   VerletEntry   — {j, r_ij, eta_j, [dx,dy,dz]}
 *   VerletList    — per-bead neighbor lists + rebuild logic
 *   VerletBuilder — constructs VerletList from positions + η array
 *
 * Rebuild trigger:
 *   Maximum displacement since last build > r_skin / 2.
 *   Caller owns the displacement tracking; call needs_rebuild() each step.
 *
 * Skin distance:
 *   r_cutoff_effective = r_cutoff + r_skin
 *   Pairs within r_cutoff_effective are stored; only pairs within r_cutoff
 *   are used in force evaluation. r_skin buys steps between rebuilds.
 *
 * ERB integration:
 *   After a rebuild, VerletEntry.eta_j is snapshotted from the current η
 *   array. The η snapshot is valid until the next rebuild. This is correct
 *   because η evolves slowly compared to positions (ERB design rule).
 *
 * Reference: environment_coupling.hpp — fill_pair_modulation()
 *            energy_decomposition.hpp — PairInteraction
 *
 * WO-56C  |  v5.0.0-beta.7
 */

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <vector>

namespace vsepr::nl {

// ============================================================================
// VerletEntry — one neighbor record
// ============================================================================

/**
 * A single neighbor entry in the Verlet list for bead i.
 *
 * Stores:
 *   j        — neighbor index
 *   r_ij     — distance at build time (Å) — used for rebuild threshold only
 *   eta_j    — environment state of bead j at build time (ERB η snapshot)
 *   dx,dy,dz — displacement vector r_j - r_i at build time (Å)
 *
 * During force evaluation, the caller recomputes r_ij from current positions.
 * The stored r_ij is only used in needs_rebuild() displacement check.
 */
struct VerletEntry {
	uint32_t j       = 0;
	float    r_ij    = 0.0f;   // Å — distance at build time
	float    eta_j   = 0.0f;   // ERB η snapshot of neighbor j
	float    dx      = 0.0f;   // Å — r_j.x - r_i.x at build time
	float    dy      = 0.0f;
	float    dz      = 0.0f;
};

// ============================================================================
// VerletParams — configuration
// ============================================================================

struct VerletParams {
	float r_cutoff = 12.0f;   // Å — interaction cutoff (same as LJ/Coulomb)
	float r_skin   = 2.0f;    // Å — skin distance; r_cutoff + r_skin = build cutoff
	float eta_skin = 0.1f;    // η change threshold — triggers rebuild if |Δη_i| > eta_skin
							   // (density-dependent rebuild: dense systems rebuild more often)

	float build_cutoff() const { return r_cutoff + r_skin; }
	float rebuild_disp_threshold() const { return r_skin * 0.5f; }
};

// ============================================================================
// VerletList — the neighbor list for all beads
// ============================================================================

class VerletList {
public:
	VerletList() = default;

	explicit VerletList(int n_beads, const VerletParams& params = {})
		: params_(params), n_beads_(n_beads)
	{
		neighbors_.resize(n_beads);
		ref_positions_.resize(3 * n_beads, 0.0f);
		ref_eta_.resize(n_beads, 0.0f);
		max_disp_sq_ = 0.0f;
	}

	// -----------------------------------------------------------------------
	// Build — (re)construct list from current positions and η array
	// -----------------------------------------------------------------------

	/**
	 * Build the neighbor list.
	 *
	 * @param coords   [3*N] flat array: x0,y0,z0, x1,y1,z1, ...  (Å)
	 * @param eta      [N]   environment state per bead (ERB η)
	 * @param n_beads  Number of beads
	 */
	void build(const float* coords, const float* eta, int n_beads) {
		n_beads_ = n_beads;
		neighbors_.assign(n_beads, {});
		ref_positions_.assign(coords, coords + 3 * n_beads);
		ref_eta_.assign(eta, eta + n_beads);
		max_disp_sq_ = 0.0f;
		n_builds_++;

		float rc2 = params_.build_cutoff() * params_.build_cutoff();

		for (int i = 0; i < n_beads; ++i) {
			float xi = coords[3*i],     yi = coords[3*i+1], zi = coords[3*i+2];
			auto& list = neighbors_[i];
			list.clear();

			for (int j = i + 1; j < n_beads; ++j) {
				float dx = coords[3*j]   - xi;
				float dy = coords[3*j+1] - yi;
				float dz = coords[3*j+2] - zi;
				float r2 = dx*dx + dy*dy + dz*dz;

				if (r2 < rc2) {
					float r = std::sqrt(r2);
					// Add i→j
					list.push_back({
						static_cast<uint32_t>(j), r, eta[j], dx, dy, dz
					});
					// Add j→i (symmetric)
					neighbors_[j].push_back({
						static_cast<uint32_t>(i), r, eta[i], -dx, -dy, -dz
					});
				}
			}
		}
	}

	// -----------------------------------------------------------------------
	// needs_rebuild — displacement + η drift check
	// -----------------------------------------------------------------------

	/**
	 * Check whether the list needs rebuilding.
	 *
	 * Two triggers:
	 *   1. Any bead has moved more than r_skin/2 since last build.
	 *   2. Any bead's η has drifted more than eta_skin (density change).
	 *
	 * @param coords  Current positions [3*N]
	 * @param eta     Current η array [N]
	 * @return        true if rebuild is required
	 */
	bool needs_rebuild(const float* coords, const float* eta) const {
		float disp_thresh2 = params_.rebuild_disp_threshold() *
							 params_.rebuild_disp_threshold();

		for (int i = 0; i < n_beads_; ++i) {
			// Position displacement check
			float ddx = coords[3*i]   - ref_positions_[3*i];
			float ddy = coords[3*i+1] - ref_positions_[3*i+1];
			float ddz = coords[3*i+2] - ref_positions_[3*i+2];
			if (ddx*ddx + ddy*ddy + ddz*ddz > disp_thresh2) return true;

			// η drift check (density-dependent rebuild trigger)
			float deta = std::abs(eta[i] - ref_eta_[i]);
			if (deta > params_.eta_skin) return true;
		}
		return false;
	}

	// -----------------------------------------------------------------------
	// Accessors
	// -----------------------------------------------------------------------

	const std::vector<VerletEntry>& neighbors(int i) const {
		return neighbors_[static_cast<size_t>(i)];
	}

	int  n_beads()  const { return n_beads_; }
	int  n_builds() const { return n_builds_; }
	const VerletParams& params() const { return params_; }

	/**
	 * Total neighbor pairs stored (double-counted — both i→j and j→i).
	 */
	size_t total_entries() const {
		size_t n = 0;
		for (const auto& v : neighbors_) n += v.size();
		return n;
	}

	/**
	 * Average neighbors per bead — useful for diagnostics / density reporting.
	 */
	float avg_neighbors() const {
		if (n_beads_ == 0) return 0.0f;
		return static_cast<float>(total_entries()) / static_cast<float>(n_beads_);
	}

	// -----------------------------------------------------------------------
	// ERB pair iteration helper
	// -----------------------------------------------------------------------

	/**
	 * Iterate all unique pairs (i, entry) for ERB force/energy evaluation.
	 *
	 * The callback receives:
	 *   i       — bead index
	 *   entry   — VerletEntry with j, r_ij (at build time), eta_j, dx,dy,dz
	 *   eta_i   — η of bead i (from the η array passed to most recent build)
	 *
	 * Note: this iterates i→j only (not the symmetric j→i entry) to avoid
	 * double-counting in energy evaluation. Force evaluation should use the
	 * full list (both directions) via neighbors(i) directly.
	 *
	 * @param fn  Callback: fn(int i, const VerletEntry& e, float eta_i)
	 */
	template<typename Fn>
	void for_each_pair(Fn&& fn) const {
		for (int i = 0; i < n_beads_; ++i) {
			float eta_i = ref_eta_[static_cast<size_t>(i)];
			for (const auto& e : neighbors_[static_cast<size_t>(i)]) {
				if (static_cast<int>(e.j) > i) {  // unique pairs only
					fn(i, e, eta_i);
				}
			}
		}
	}

private:
	VerletParams params_;
	int          n_beads_   = 0;
	int          n_builds_  = 0;
	float        max_disp_sq_ = 0.0f;

	std::vector<std::vector<VerletEntry>> neighbors_;
	std::vector<float>                    ref_positions_;  // [3*N] snapshot
	std::vector<float>                    ref_eta_;        // [N] η snapshot
};

// ============================================================================
// Convenience: ERB modulation factor from a VerletEntry
// ============================================================================

/**
 * Compute the ERB modulation factor g_k for a pair from NL data.
 *
 *   g_k = 1 + γ_k · η̄,    η̄ = 0.5·(η_i + entry.eta_j)
 *
 * This is the direct NL integration point for environment_coupling.hpp.
 * No O(N²) lookup — η_j is already in the entry.
 *
 * @param gamma_k   Channel modulation parameter (gamma_steric, gamma_elec, or gamma_disp)
 * @param eta_i     η of bead i (from caller or NL ref_eta_)
 * @param entry     VerletEntry (carries eta_j)
 * @return          Modulation factor (clamped > 0)
 */
inline float erb_modulation(float gamma_k, float eta_i, const VerletEntry& entry) {
	float eta_bar = 0.5f * (eta_i + entry.eta_j);
	float g = 1.0f + gamma_k * eta_bar;
	return (g > 0.0f) ? g : 1e-10f;
}

} // namespace vsepr::nl
