// WO-72W — property trend finder implementation
#include "vsim/ml/property_trend.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <regex>
#include <unordered_map>

namespace vsim::ml {

// Minimal elemental feature table (electronegativity, atomic mass, valence)
struct ElemFeatures { double chi; double mass; double valence; };
static const std::unordered_map<std::string, ElemFeatures>& elem_table() {
	static const std::unordered_map<std::string, ElemFeatures> t = {
		{"H",  {2.20,  1.008, 1}}, {"Li", {0.98,  6.941, 1}},
		{"C",  {2.55, 12.011, 4}}, {"N",  {3.04, 14.007, 5}},
		{"O",  {3.44, 15.999, 6}}, {"F",  {3.98, 18.998, 7}},
		{"Na", {0.93, 22.990, 1}}, {"Mg", {1.31, 24.305, 2}},
		{"Al", {1.61, 26.982, 3}}, {"Si", {1.90, 28.086, 4}},
		{"P",  {2.19, 30.974, 5}}, {"S",  {2.58, 32.065, 6}},
		{"Cl", {3.16, 35.453, 7}}, {"K",  {0.82, 39.098, 1}},
		{"Ca", {1.00, 40.078, 2}}, {"Ti", {1.54, 47.867, 4}},
		{"Fe", {1.83, 55.845, 3}}, {"Ni", {1.91, 58.693, 2}},
		{"Cu", {1.90, 63.546, 2}}, {"Zn", {1.65, 65.38,  2}},
		{"Ga", {1.81, 69.723, 3}}, {"Ge", {2.01, 72.640, 4}},
		{"As", {2.18, 74.922, 5}}, {"Se", {2.55, 78.960, 6}},
		{"Br", {2.96, 79.904, 7}}, {"Zr", {1.33, 91.224, 4}},
		{"Nb", {1.60, 92.906, 5}}, {"Mo", {2.16, 95.960, 6}},
		{"Ag", {1.93,107.868, 1}}, {"Sn", {1.96,118.710, 4}},
		{"Ba", {0.89,137.327, 2}}, {"La", {1.10,138.905, 3}},
	};
	return t;
}

static std::unordered_map<std::string, int> parse_formula(const std::string& f) {
	std::unordered_map<std::string, int> counts;
	std::regex tok("([A-Z][a-z]?)([0-9]*)");
	auto beg = std::sregex_iterator(f.begin(), f.end(), tok);
	for (auto it = beg; it != std::sregex_iterator(); ++it) {
		std::string sym = (*it)[1].str();
		int n = (*it)[2].str().empty() ? 1 : std::stoi((*it)[2].str());
		counts[sym] += n;
	}
	return counts;
}

void PropertyTrendFinder::compute_features(std::vector<MaterialCandidate>& candidates) {
	const auto& et = elem_table();
	for (auto& c : candidates) {
		auto counts = parse_formula(c.formula);
		double chi_sum = 0, mass_sum = 0, val_sum = 0;
		int total = 0;
		for (auto& [sym, n] : counts) {
			auto it = et.find(sym);
			if (it == et.end()) continue;
			chi_sum  += it->second.chi     * n;
			mass_sum += it->second.mass    * n;
			val_sum  += it->second.valence * n;
			total    += n;
		}
		if (total > 0) {
			c.electronegativity_mean = chi_sum  / total;
			c.atomic_mass_mean       = mass_sum / total;
			c.valence_mean           = val_sum  / total;
		}
		c.n_elements = static_cast<uint32_t>(counts.size());
	}
}

TrendResult PropertyTrendFinder::linear_fit(const std::vector<double>& x,
											 const std::vector<double>& y,
											 const std::string& feat,
											 const std::string& prop) const {
	TrendResult r;
	r.feature   = feat;
	r.property  = prop;
	r.n_samples = x.size();
	if (x.size() < 2) return r;

	double n  = static_cast<double>(x.size());
	double sx = std::accumulate(x.begin(), x.end(), 0.0);
	double sy = std::accumulate(y.begin(), y.end(), 0.0);
	double sxy = 0.0, sx2 = 0.0, sy2 = 0.0;
	for (std::size_t i = 0; i < x.size(); ++i) {
		sxy += x[i] * y[i];
		sx2 += x[i] * x[i];
		sy2 += y[i] * y[i];
	}
	double denom_r = std::sqrt((n*sx2 - sx*sx) * (n*sy2 - sy*sy));
	r.pearson_r   = (denom_r > 0) ? (n*sxy - sx*sy) / denom_r : 0.0;
	double denom_b = n*sx2 - sx*sx;
	r.slope       = (denom_b > 0) ? (n*sxy - sx*sy) / denom_b : 0.0;
	r.intercept   = (sy - r.slope * sx) / n;
	double sse = 0.0;
	for (std::size_t i = 0; i < x.size(); ++i) {
		double pred = r.slope * x[i] + r.intercept;
		sse += (y[i] - pred) * (y[i] - pred);
	}
	r.rmse = std::sqrt(sse / n);
	return r;
}

std::vector<TrendResult> PropertyTrendFinder::find(
	const std::vector<MaterialCandidate>& candidates) const
{
	if (candidates.empty()) return {};
	std::vector<double> y, x_chi, x_mass, x_val, x_nel;
	for (auto& c : candidates) {
		y.push_back(c.target_value);
		x_chi.push_back(c.electronegativity_mean);
		x_mass.push_back(c.atomic_mass_mean);
		x_val.push_back(c.valence_mean);
		x_nel.push_back(static_cast<double>(c.n_elements));
	}
	std::string prop = candidates[0].target_property;
	std::vector<TrendResult> results = {
		linear_fit(x_chi,  y, "electronegativity_mean", prop),
		linear_fit(x_mass, y, "atomic_mass_mean",       prop),
		linear_fit(x_val,  y, "valence_mean",           prop),
		linear_fit(x_nel,  y, "n_elements",             prop),
	};
	std::sort(results.begin(), results.end(), [](const TrendResult& a, const TrendResult& b) {
		return std::abs(a.pearson_r) > std::abs(b.pearson_r);
	});
	return results;
}

} // namespace vsim::ml
