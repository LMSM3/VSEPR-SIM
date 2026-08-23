// WO-72W — route recommender implementation
#include "vsim/ml/route_recommender.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace vsim::ml {

void RouteRecommender::add(const MaterialCandidate& c) {
	candidates_.push_back(c);
}

void RouteRecommender::load_seed_set() {
	// Minimal built-in training / reference candidates covering bandgap targets.
	auto add_c = [&](const char* f, const char* name, const char* crys,
					 const char* prop, double val, const char* unit,
					 const char* doi, const char* src,
					 const char* route, const char* method, double T,
					 double conf, const char* vstatus) {
		MaterialCandidate c;
		c.formula = f; c.name = name; c.crystal_system = crys;
		c.target_property = prop; c.target_value = val; c.target_unit = unit;
		c.doi = doi; c.source_db = src;
		c.formation_route = route; c.process_method = method;
		c.synthesis_temp_K = T; c.confidence = conf; c.val_status = vstatus;
		candidates_.push_back(c);
	};

	// bandgap candidates (eV)
	add_c("Si",     "Silicon",         "cubic",      "bandgap", 1.12, "eV",
		  "10.1103/PhysRev.94.42",  "NIST",
		  "CZ growth from melt",   "Czochralski",  1687.0, 0.99, "validated");
	add_c("GaAs",   "Gallium arsenide","cubic",      "bandgap", 1.42, "eV",
		  "10.1063/1.90",           "Materials Project",
		  "LPE or MBE on GaAs sub","MBE",          1511.0, 0.97, "validated");
	add_c("GaN",    "Gallium nitride", "hexagonal",  "bandgap", 3.40, "eV",
		  "10.1143/JJAP.31.L139",   "ICSD",
		  "HVPE on sapphire",       "HVPE",         1173.0, 0.95, "validated");
	add_c("ZnO",    "Zinc oxide",      "hexagonal",  "bandgap", 3.37, "eV",
		  "10.1002/pssa.200675227",  "NIST",
		  "Hydrothermal synthesis", "hydrothermal",  473.0, 0.93, "validated");
	add_c("TiO2",   "Rutile",          "tetragonal", "bandgap", 3.05, "eV",
		  "10.1103/PhysRevB.13.5188","NIST",
		  "Sol-gel calcination",    "sol-gel",      1073.0, 0.90, "validated");
	add_c("CdS",    "Cadmium sulfide", "hexagonal",  "bandgap", 2.42, "eV",
		  "10.1063/1.1702707",       "ICSD",
		  "Chemical bath deposit",  "CBD",           373.0, 0.88, "validated");
	add_c("InP",    "Indium phosphide","cubic",       "bandgap", 1.35, "eV",
		  "10.1063/1.89407",         "Materials Project",
		  "LEC Bridgman growth",    "Bridgman",     1335.0, 0.94, "validated");
	add_c("AlN",    "Aluminium nitride","hexagonal",  "bandgap", 6.20, "eV",
		  "10.1063/1.4754274",       "ICSD",
		  "PVT sublimation",        "PVT",          2773.0, 0.89, "validated");

	// Compute features for all loaded candidates
	PropertyTrendFinder::compute_features(candidates_);
}

double RouteRecommender::score_candidate(const MaterialCandidate& c,
										  const std::string& prop,
										  double target,
										  const std::vector<TrendResult>& trends) const {
	if (c.target_property != prop) return 0.0;
	// Proximity score: Gaussian around target value, width = 20% of target
	double width  = std::max(std::abs(target) * 0.20, 0.1);
	double delta  = std::abs(c.target_value - target);
	double prox   = std::exp(-0.5 * (delta / width) * (delta / width));
	// Blend with stored confidence
	return 0.7 * prox + 0.3 * c.confidence;
}

std::vector<Recommendation> RouteRecommender::recommend(
	const std::string& target_property,
	double target_value,
	std::size_t max_results) const
{
	// Filter to matching property
	std::vector<MaterialCandidate> pool;
	for (auto& c : candidates_)
		if (c.target_property == target_property) pool.push_back(c);

	// Compute trends from pool
	auto trends = trend_finder_.find(pool);

	std::vector<Recommendation> recs;
	for (auto& c : pool) {
		Recommendation r;
		r.candidate = c;
		r.delta     = std::abs(c.target_value - target_value);
		r.score     = score_candidate(c, target_property, target_value, trends);
		// Build rationale
		std::ostringstream oss;
		oss << c.name << " (" << c.formula << ")  " << target_property
			<< "=" << c.target_value << " " << c.target_unit
			<< "  delta=" << r.delta
			<< "  via " << c.process_method
			<< "  [" << c.val_status << "]";
		r.rationale = oss.str();
		recs.push_back(r);
	}

	std::sort(recs.begin(), recs.end(), [](const Recommendation& a, const Recommendation& b) {
		return a.score > b.score;
	});
	if (recs.size() > max_results) recs.resize(max_results);
	return recs;
}

} // namespace vsim::ml
