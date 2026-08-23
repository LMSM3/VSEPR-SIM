#pragma once
// WO-72W — process route recommender
// Given a target property and value, ranks MaterialCandidates by
// predicted suitability and returns an ordered recommendation list.
#include "material_candidate.hpp"
#include "property_trend.hpp"
#include <vector>
#include <string>

namespace vsim::ml {

struct Recommendation {
	MaterialCandidate candidate;
	double score        = 0.0;   // composite suitability score (0–1)
	double delta        = 0.0;   // |predicted - target|
	std::string rationale;       // human-readable reason string
};

class RouteRecommender {
public:
	// Load candidates (e.g. from the built-in seed set or a JSON file).
	void add(const MaterialCandidate& c);
	void load_seed_set();    // built-in minimal training set

	// Recommend candidates for a given target property + desired value.
	// max_results: cap the returned list.
	std::vector<Recommendation> recommend(const std::string& target_property,
										  double target_value,
										  std::size_t max_results = 10) const;

	std::size_t size() const { return candidates_.size(); }

private:
	std::vector<MaterialCandidate> candidates_;
	PropertyTrendFinder            trend_finder_;

	double score_candidate(const MaterialCandidate& c,
						   const std::string& prop,
						   double target,
						   const std::vector<TrendResult>& trends) const;
};

} // namespace vsim::ml
