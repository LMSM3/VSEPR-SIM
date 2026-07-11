// include/ufx_auto2/coverage_map.hpp
// UFX_AUTO_2 Phase 2 -- Coverage Map (stub)
// VSEPR-SIM v5 beta8 -> beta9
//
// CoverageMap tracks how densely each axis region has been sampled.
// Phase 2: stub implementation backed by SQLite COUNT queries.
// Phase 3+: full coverage-aware steering with weakest-region priority.
//
// The sampler asks: "which region has least coverage?"
// The map answers with a region descriptor the sampler can bias toward.

#pragma once

#include "axis_config.hpp"
#include "v4/uff/ufx_material_record.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace vsepr::ufx {

// A region key uniquely identifies an axis cell.
// Phase 2: string of the form "element_family::phase::geometry"
using RegionKey = std::string;

struct CoverageEntry {
	RegionKey key;
	int64_t   count    = 0;   // records in DB for this region
	double    density  = 0.0; // normalised 0.0 – 1.0
};

class CoverageMap {
public:
	CoverageMap() = default;

	// Load coverage counts from the material_records table.
	// db_path may be empty — in that case the map is initialised to zero.
	void load_from_db(const std::string& db_path);

	// Returns the region key with the lowest count.
	// Returns empty string if no entries loaded (Phase 2: not fatal).
	RegionKey weakest_region() const;

	// Increment in-memory count for a region key (called after each insert).
	void record_hit(const RegionKey& key);

	// Build a region key from an AxisSample.
	static RegionKey make_key(const AxisSample& sample);

	// Build a region key from an IdentityBlock (for Phase 10 scoring).
	static RegionKey make_key_from_identity(const IdentityBlock& id);

	// Return all known region keys.
	std::vector<RegionKey> all_regions() const;

	// Total distinct regions tracked.
	std::size_t region_count() const noexcept { return counts_.size(); }

	// Count for a specific region (0 if not seen).
	int64_t count_for(const RegionKey& key) const;

private:
	std::unordered_map<RegionKey, int64_t> counts_;
};

} // namespace vsepr::ufx
