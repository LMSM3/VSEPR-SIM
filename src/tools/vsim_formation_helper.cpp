/**
 * src/tools/vsim_formation_helper.cpp
 * =====================================
 * vsim_formation_pathway_helper  -  Formation route analysis tool
 *
 * Usage:
 *   vsim_formation_helper --target CH4 --reactants C,H,H,H,H [--preset molecular]
 *                         [--out pathways_CH4.json] [--verbose]
 *
 * Exit codes:
 *   0  success (pathways_found may be true or false)
 *   1  invalid arguments
 *   2  stoichiometry error
 *   3  output file write error
 *
 * Reference: FinalChapter/VSIM_FORMATION_HELPER_V1.md
 * WO-75C  |  v5.0.0-main
 */

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// ============================================================================
// Stoichiometry checker
// ============================================================================

static std::map<std::string, int> parse_hill(const std::string& formula)
{
	std::map<std::string, int> counts;
	for (size_t i = 0; i < formula.size(); ) {
		if (!std::isupper(static_cast<unsigned char>(formula[i]))) { ++i; continue; }
		std::string elem;
		elem += formula[i++];
		while (i < formula.size() && std::islower(static_cast<unsigned char>(formula[i])))
			elem += formula[i++];
		int n = 0;
		while (i < formula.size() && std::isdigit(static_cast<unsigned char>(formula[i])))
			n = n * 10 + (formula[i++] - '0');
		counts[elem] += (n == 0 ? 1 : n);
	}
	return counts;
}

static std::map<std::string, int> sum_reactants(const std::vector<std::string>& reactants)
{
	std::map<std::string, int> counts;
	for (const auto& r : reactants) {
		for (auto& [el, n] : parse_hill(r))
			counts[el] += n;
	}
	return counts;
}

static bool stoichiometry_matches(const std::string& target,
								   const std::vector<std::string>& reactants)
{
	return parse_hill(target) == sum_reactants(reactants);
}

// ============================================================================
// Route table
// ============================================================================

struct RouteEntry {
	std::string target;
	std::string route_id;
	std::string description;
	double      score;
	std::vector<std::string> intermediates;
	std::string notes;
};

static std::vector<RouteEntry> build_route_table()
{
	return {
		// CH4
		{ "CH4", "direct_relaxation",
		  "Direct seeded relaxation from point-identity carriers.",
		  0.92, {},
		  "No intermediates required. FIRE relaxation from random seed." },
		{ "CH4", "stepwise_CH_CH2_CH3_CH4",
		  "Stepwise: C+H->CH, CH+H->CH2, CH2+H->CH3, CH3+H->CH4.",
		  0.71, {"CH", "CH2", "CH3"},
		  "Longer path. Only preferred if direct relaxation fails to converge." },
		// NH3
		{ "NH3", "direct_relaxation",
		  "Direct seeded relaxation from atomic carriers.",
		  0.90, {},
		  "Trigonal pyramidal geometry seed." },
		{ "NH3", "stepwise_NH_NH2_NH3",
		  "Stepwise: N+H->NH, NH+H->NH2, NH2+H->NH3.",
		  0.68, {"NH", "NH2"},
		  "Stepwise route via imidogen." },
		// H2O
		{ "H2O", "direct_relaxation",
		  "Direct seeded relaxation (bent geometry).",
		  0.91, {},
		  "V-shape seed angle 104.5 deg." },
		{ "H2O", "stepwise_OH_H2O",
		  "Stepwise: O+H->OH, OH+H->H2O.",
		  0.74, {"OH"},
		  "Via hydroxyl radical." },
		// CO2
		{ "CO2", "direct_relaxation",
		  "Direct relaxation to linear geometry.",
		  0.89, {},
		  "Linear arrangement; 180 deg seed." },
		// N2
		{ "N2",  "direct_relaxation",
		  "Direct triple-bond relaxation.",
		  0.95, {},
		  "Diatomic; trivial convergence." },
		// NaCl
		{ "NaCl", "ionic_seed_relaxation",
		  "Ionic seed with alternating Na+/Cl- on rocksalt lattice.",
		  0.93, {},
		  "B1 prototype seed. Ewald forces active." },
		// Si
		{ "Si",  "crystal_seed_relaxation",
		  "Diamond cubic seed relaxation.",
		  0.94, {},
		  "A4 prototype; requires PBC." },
	};
}

// ============================================================================
// Route ranker
// ============================================================================

struct RankedRoute {
	std::string route_id;
	std::string description;
	double      score;
	std::vector<std::string> intermediates;
	std::string notes;
};

static std::vector<RankedRoute> rank_routes(
	const std::string& target,
	const std::vector<RouteEntry>& table,
	const std::string& /*preset*/)
{
	std::vector<RankedRoute> result;
	for (const auto& row : table) {
		if (row.target == target)
			result.push_back({ row.route_id, row.description,
							   row.score, row.intermediates, row.notes });
	}
	// Sort descending by score
	std::sort(result.begin(), result.end(),
		[](const RankedRoute& a, const RankedRoute& b){ return a.score > b.score; });
	// Generic fallback if no known route
	if (result.empty()) {
		result.push_back({ "direct_relaxation",
			"Direct seeded relaxation (generic fallback).",
			0.55, {},
			"No specific route table entry for this target." });
	}
	return result;
}

// ============================================================================
// JSON output
// ============================================================================

static std::string json_string(const std::string& s)
{
	std::string out = "\"";
	for (char c : s) {
		if (c == '"') out += "\\\"";
		else if (c == '\\') out += "\\\\";
		else out += c;
	}
	return out + "\"";
}

static std::string write_json(
	const std::string& target,
	const std::vector<std::string>& reactants,
	const std::string& preset,
	bool stoich_ok,
	const std::vector<RankedRoute>& routes)
{
	std::ostringstream j;
	j << "{\n";
	j << "  \"target\": " << json_string(target) << ",\n";

	// reactants array
	j << "  \"reactants\": [";
	for (size_t i = 0; i < reactants.size(); ++i) {
		if (i) j << ", ";
		j << json_string(reactants[i]);
	}
	j << "],\n";

	j << "  \"preset\": " << json_string(preset) << ",\n";
	j << "  \"stoichiometry_valid\": " << (stoich_ok ? "true" : "false") << ",\n";
	j << "  \"pathways_found\": " << (!routes.empty() ? "true" : "false") << ",\n";

	// recommended route (top-ranked)
	if (!routes.empty()) {
		const auto& r = routes[0];
		j << "  \"recommended_route\": {\n";
		j << "    \"route_id\": " << json_string(r.route_id) << ",\n";
		j << "    \"description\": " << json_string(r.description) << ",\n";
		j << "    \"score\": " << r.score << ",\n";
		j << "    \"initialization\": \"seeded_random_near_target\",\n";
		j << "    \"intermediate_species\": [";
		for (size_t i = 0; i < r.intermediates.size(); ++i) {
			if (i) j << ", ";
			j << json_string(r.intermediates[i]);
		}
		j << "],\n";
		j << "    \"notes\": " << json_string(r.notes) << "\n";
		j << "  },\n";
	} else {
		j << "  \"recommended_route\": null,\n";
	}

	// candidate routes array
	j << "  \"candidate_routes\": [\n";
	for (size_t i = 0; i < routes.size(); ++i) {
		const auto& r = routes[i];
		if (i) j << ",\n";
		j << "    {\n";
		j << "      \"route_id\": " << json_string(r.route_id) << ",\n";
		j << "      \"description\": " << json_string(r.description) << ",\n";
		j << "      \"score\": " << r.score << ",\n";
		j << "      \"intermediate_species\": [";
		for (size_t k = 0; k < r.intermediates.size(); ++k) {
			if (k) j << ", ";
			j << json_string(r.intermediates[k]);
		}
		j << "],\n";
		j << "      \"notes\": " << json_string(r.notes) << "\n";
		j << "    }";
	}
	j << "\n  ],\n";

	j << "  \"warnings\": [],\n";
	j << "  \"helper_version\": \"V1\"\n";
	j << "}\n";
	return j.str();
}

// ============================================================================
// CLI entry point
// ============================================================================

int main(int argc, char* argv[])
{
	std::string target;
	std::string reactants_raw;
	std::string preset   = "molecular";
	std::string out_path;
	bool        verbose  = false;

	// Parse arguments
	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		if ((a == "--target" || a == "-t") && i + 1 < argc)
			target = argv[++i];
		else if ((a == "--reactants" || a == "-r") && i + 1 < argc)
			reactants_raw = argv[++i];
		else if ((a == "--preset") && i + 1 < argc)
			preset = argv[++i];
		else if ((a == "--out" || a == "-o") && i + 1 < argc)
			out_path = argv[++i];
		else if (a == "--verbose" || a == "-v")
			verbose = true;
		else if (a == "--help" || a == "-h") {
			std::cout <<
				"vsim_formation_pathway_helper\n"
				"Usage: vsim_formation_helper --target FORMULA --reactants A,B,C\n"
				"                             [--preset molecular|ionic|crystal]\n"
				"                             [--out FILE.json] [--verbose]\n";
			return 0;
		}
	}

	if (target.empty() || reactants_raw.empty()) {
		std::cerr << "[ERROR] --target and --reactants are required.\n";
		return 1;
	}

	// Parse reactants (comma-separated)
	std::vector<std::string> reactants;
	{
		std::istringstream ss(reactants_raw);
		std::string tok;
		while (std::getline(ss, tok, ',')) {
			// Trim whitespace
			tok.erase(0, tok.find_first_not_of(" \t"));
			tok.erase(tok.find_last_not_of(" \t") + 1);
			if (!tok.empty()) reactants.push_back(tok);
		}
	}

	// Stoichiometry check
	bool stoich_ok = stoichiometry_matches(target, reactants);
	if (!stoich_ok) {
		std::cerr << "[WARN] Stoichiometry mismatch: reactants do not sum to " << target << ".\n";
		// Exit 2 if strict mismatch — but we still produce output
		// (spec says exit 2 = stoichiometry error, no JSON)
		return 2;
	}

	// Rank routes
	auto table  = build_route_table();
	auto routes = rank_routes(target, table, preset);

	// Produce JSON
	std::string json = write_json(target, reactants, preset, stoich_ok, routes);

	if (!out_path.empty()) {
		std::ofstream f(out_path);
		if (!f) {
			std::cerr << "[ERROR] Cannot write to " << out_path << "\n";
			return 3;
		}
		f << json;
		std::cout << "[OK] Written: " << out_path << "\n";
	} else {
		std::cout << json;
	}

	if (verbose) {
		std::cout << "\n[INFO] target=" << target
				  << "  stoich_ok=" << (stoich_ok ? "true" : "false")
				  << "  routes_found=" << routes.size() << "\n";
	}

	return 0;
}
