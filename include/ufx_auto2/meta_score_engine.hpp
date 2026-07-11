// include/ufx_auto2/meta_score_engine.hpp
// UFX_AUTO_2 Phase 10 -- Meta-Score Engine + Coverage Scheduler
// VSEPR-SIM v5 beta9
//
// MetaScoreEngine computes MetaScoreBlock fields for fully-filled records
// and steers the RandomAxisSampler toward under-covered regions.
//
// Composite score model (UFX_continual_2.tex §19):
//   S = 0.25*identity + 0.20*local + 0.20*reference + 0.15*web + 0.10*repeat + 0.10*macro
//
// Phase 10 populates S_macro for the first time.
//
// Sampling priority function:
//   p(region) = (1 - coverage)^2 * (1 + macro_usefulness_avg)
//             * (1 - extrapolation_risk) * (1 + weirdness_avg * 0.2)
//
// property_values rows written (block_tier=9, block_name='meta'):
//   weirdness, coverage, confidence, novelty, local_consistency,
//   source_conflict, macro_usefulness, simulation_stability,
//   extrapolation_risk, composite_score

#pragma once

#include "v4/uff/ufx_material_record.hpp"
#include "ufx_auto2/coverage_map.hpp"
#include "ufx_auto2/axis_config.hpp"

#include <string>
#include <map>

struct sqlite3;

namespace vsepr::ufx {

// ============================================================================
// Per-region meta statistics (extended CoverageMap overlay)
// ============================================================================

struct RegionMeta {
	double coverage_score        = 0.0;   // record count / region capacity
	double weirdness_avg         = 0.0;
	double macro_usefulness_avg  = 0.0;
	double extrapolation_risk    = 0.0;   // fraction of records with confidence < 0.5
};

// ============================================================================
// ScoreOptions / Result
// ============================================================================

struct ScoreOptions {
	std::string db_path;
	int         batch     = 500;
	bool        verbose   = false;
	bool        recompute = false;   // recompute for all promoted records
};

struct ScoreResult {
	std::string db_path;
	int         processed    = 0;
	int         scored       = 0;
	int         failed       = 0;
	double      avg_composite = 0.0;
	double      max_weird    = 0.0;
	std::string max_weird_key;
	std::string weakest_region;
	bool        success      = false;
	std::string error_message;
};

// ============================================================================
// MetaScoreEngine
// ============================================================================

class MetaScoreEngine {
public:
	MetaScoreEngine();

	// Compute MetaScoreBlock fields for a record.
	// Requires at minimum identity, force_field blocks populated.
	void compute(UFXMaterialRecord& rec,
				 const CoverageMap& coverage) const;

	// Bias weight for choosing the next weak axis region.
	// Higher = more likely sampled next.
	double region_priority(const RegionKey& region,
						   const CoverageMap& coverage,
						   const std::map<RegionKey, RegionMeta>& meta_map) const noexcept;

	// Update coverage map meta fields after a record is promoted.
	void update_coverage(CoverageMap& coverage,
						 const UFXMaterialRecord& rec) const;

	// Build priority weight map for all known regions.
	std::map<RegionKey, double> build_priority_map(
		const CoverageMap& coverage,
		const std::map<RegionKey, RegionMeta>& meta_map) const;

private:
	double weirdness_(const UFXMaterialRecord& rec,
					  const CoverageMap& coverage) const noexcept;
	double novelty_  (const UFXMaterialRecord& rec,
					  const CoverageMap& coverage) const noexcept;
	double macro_usefulness_(const UFXMaterialRecord& rec) const noexcept;
	double extrapolation_risk_(const UFXMaterialRecord& rec) const noexcept;
	double local_consistency_(const UFXMaterialRecord& rec,
							  const CoverageMap& coverage) const noexcept;
};

// ============================================================================
// Run score command over the DB (used by CLI)
// ============================================================================

ScoreResult ufx_auto2_score(const ScoreOptions& opts);
void print_score_result(const ScoreResult& r);

} // namespace vsepr::ufx
