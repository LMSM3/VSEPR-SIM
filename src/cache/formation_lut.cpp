// WO-72V — formation route lookup table implementation
#include "vsim/cache/formation_lut.hpp"
#include <algorithm>

namespace vsim::cache {

void FormationLUT::add(const FormationRoute& route) {
	routes_[route.product].push_back(route);
}

std::vector<FormationRoute> FormationLUT::routes_for(const std::string& formula) const {
	auto it = routes_.find(formula);
	if (it == routes_.end()) return {};
	return it->second;
}

std::optional<FormationRoute> FormationLUT::best_route(const std::string& formula) const {
	auto it = routes_.find(formula);
	if (it == routes_.end() || it->second.empty()) return std::nullopt;
	auto best = std::max_element(it->second.begin(), it->second.end(),
		[](const FormationRoute& a, const FormationRoute& b) {
			return a.confidence < b.confidence;
		});
	return *best;
}

void FormationLUT::load_builtins() {
	add({"NaCl", {"Na", "Cl2"}, "solid-state", -411.2, 1073.0, 0.95, "NIST"});
	add({"NaCl", {"NaOH", "HCl"}, "solution",  -411.2,  298.0, 0.90, "standard"});
	add({"MgO",  {"Mg", "O2"},   "combustion", -601.6, 1073.0, 0.92, "NIST"});
	add({"Al2O3",{"Al", "O2"},   "combustion",-1675.7, 1273.0, 0.90, "NIST"});
	add({"Fe3O4",{"Fe", "O2"},   "oxidation",  -1118.4, 873.0, 0.85, "standard"});
	add({"TiO2", {"Ti", "O2"},   "oxidation",   -944.0,1073.0, 0.88, "NIST"});
	add({"SiO2", {"Si", "O2"},   "oxidation",   -910.7,1273.0, 0.91, "NIST"});
}

} // namespace vsim::cache
