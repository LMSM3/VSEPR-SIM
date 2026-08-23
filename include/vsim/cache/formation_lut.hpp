#pragma once
// WO-72V — formation route lookup table
// Maps (reactant set) -> list of plausible formation routes with energy estimates.
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace vsim::cache {

struct FormationRoute {
	std::string product;           // target formula
	std::vector<std::string> reactants;
	std::string method;            // "solid-state" | "solution" | "CVD" | ...
	double      delta_H_kJ_mol = 0.0;
	double      temp_K         = 0.0; // synthesis temperature
	double      confidence     = 0.0; // 0–1
	std::string reference;
};

class FormationLUT {
public:
	void add(const FormationRoute& route);

	// All routes whose product matches formula.
	std::vector<FormationRoute> routes_for(const std::string& formula) const;

	// Best-confidence route for product formula; nullopt if none.
	std::optional<FormationRoute> best_route(const std::string& formula) const;

	std::size_t size() const { return routes_.size(); }
	void load_builtins();

private:
	// Key = product formula
	std::unordered_map<std::string, std::vector<FormationRoute>> routes_;
};

} // namespace vsim::cache
