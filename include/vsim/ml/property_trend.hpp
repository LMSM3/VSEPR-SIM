#pragma once
// WO-72W — property trend finder
// Scans a set of MaterialCandidates and identifies simple monotonic trends
// between elemental features and the target property.
#include "material_candidate.hpp"
#include <vector>
#include <string>

namespace vsim::ml {

struct TrendResult {
	std::string feature;           // e.g. "electronegativity_mean"
	std::string property;          // target property name
	double      pearson_r  = 0.0;  // Pearson correlation coefficient
	double      slope      = 0.0;  // linear regression slope
	double      intercept  = 0.0;
	double      rmse       = 0.0;  // root-mean-square error of the linear fit
	std::size_t n_samples  = 0;
};

class PropertyTrendFinder {
public:
	// Compute linear trends between each feature and the target_value field.
	// Returns one TrendResult per feature, sorted by |pearson_r| descending.
	std::vector<TrendResult> find(const std::vector<MaterialCandidate>& candidates) const;

	// Populate computed feature fields on candidates (modifies in place).
	static void compute_features(std::vector<MaterialCandidate>& candidates);

private:
	TrendResult linear_fit(const std::vector<double>& x,
						   const std::vector<double>& y,
						   const std::string& feature_name,
						   const std::string& prop_name) const;
};

} // namespace vsim::ml
