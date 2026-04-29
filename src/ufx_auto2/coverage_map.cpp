// src/ufx_auto2/coverage_map.cpp
// UFX_AUTO_2 Phase 2 -- Coverage Map Implementation

#include "ufx_auto2/coverage_map.hpp"
#include <sqlite3.h>
#include <algorithm>
#include <limits>

namespace vsepr::ufx {

// ============================================================================
// make_key
// ============================================================================

RegionKey CoverageMap::make_key(const AxisSample& s) {
	// Phase 2 key: element_family :: phase :: geometry
	// Deliberately coarse — one cell per (family, phase, geometry) triple.
	// This keeps the coverage map manageable at Phase 2 scale.
	return s.element_family + "::" + s.phase + "::" + s.geometry;
}

// ============================================================================
// load_from_db
// ============================================================================

void CoverageMap::load_from_db(const std::string& db_path) {
	counts_.clear();
	if (db_path.empty()) return;

	sqlite3* db = nullptr;
	if (sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return;
	}

	// Query generation_axes for axis_config to rebuild region counts.
	// Phase 2: generation_axes.axis_config stores a plain text region key
	// in the run_id field (populated by auto2_randomfill).
	const char* sql =
		"SELECT run_id, COUNT(*) AS cnt "
		"FROM generation_axes "
		"GROUP BY run_id;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			const unsigned char* key_txt = sqlite3_column_text(stmt, 0);
			int64_t cnt                  = sqlite3_column_int64(stmt, 1);
			if (key_txt) {
				counts_[reinterpret_cast<const char*>(key_txt)] += cnt;
			}
		}
		sqlite3_finalize(stmt);
	}

	sqlite3_close(db);
}

// ============================================================================
// weakest_region
// ============================================================================

RegionKey CoverageMap::weakest_region() const {
	if (counts_.empty()) return "";

	RegionKey best;
	int64_t   min_count = std::numeric_limits<int64_t>::max();

	for (const auto& [key, cnt] : counts_) {
		if (cnt < min_count) {
			min_count = cnt;
			best      = key;
		}
	}
	return best;
}

// ============================================================================
// record_hit
// ============================================================================

void CoverageMap::record_hit(const RegionKey& key) {
	counts_[key]++;
}

// ============================================================================
// count_for
// ============================================================================

int64_t CoverageMap::count_for(const RegionKey& key) const {
	auto it = counts_.find(key);
	return (it != counts_.end()) ? it->second : 0;
}

// ============================================================================
// make_key_from_identity
// ============================================================================

RegionKey CoverageMap::make_key_from_identity(const IdentityBlock& id) {
	// Use geometry_tag and phase from identity block
	// element_family is not stored directly in IdentityBlock;
	// use first element as proxy for the key
	std::string elem = id.elements.empty() ? "?" : id.elements[0];
	return elem + "::" + id.phase + "::" + id.geometry_tag;
}

// ============================================================================
// all_regions
// ============================================================================

std::vector<RegionKey> CoverageMap::all_regions() const {
	std::vector<RegionKey> out;
	out.reserve(counts_.size());
	for (const auto& kv : counts_)
		out.push_back(kv.first);
	return out;
}

} // namespace vsepr::ufx
