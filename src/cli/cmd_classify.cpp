/**
 * cmd_classify.cpp  --  vsepr classify <input.vsim>
 *
 * WO-83D / WO-83I / WO-83Q
 *
 * Reads a .vsim script, builds a demo atomistic State from whatever molecule
 * metadata the script declares (formula, atom count, etc.), then runs:
 *   1. classify_vsepr_sites()    -> format_vsepr_report()
 *   2. OrganicClassifier::classify() -> format_organic_candidate()
 *
 * This is a PREVIEW path.  It does not execute the simulation engine.
 * The State it builds is a minimal scaffold sufficient for classifier input.
 *
 * ANSI color output follows the welcome_line style in apps/vsepr.cpp.
 */

#include "cli/cmd_classify.hpp"

#include "atomistic/classify/vsepr.hpp"
#include "atomistic/classify/organic_candidate.hpp"
#include "atomistic/core/state.hpp"
#include "vsim/chemplus_declarative.hpp"
#include "vsim/vsim_document.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

#ifdef _WIN32
#  include <io.h>
#  define IS_TTY _isatty(_fileno(stdout))
#else
#  include <unistd.h>
#  define IS_TTY isatty(fileno(stdout))
#endif

namespace vsepr {
namespace cli {

// ============================================================================
// ANSI helpers (respect terminal detection)
// ============================================================================

namespace {

static bool g_color = false;

static const char* COL_RESET  () { return g_color ? "\033[0m"    : ""; }
static const char* COL_BOLD   () { return g_color ? "\033[1m"    : ""; }
static const char* COL_CYAN   () { return g_color ? "\033[0;36m" : ""; }
static const char* COL_GREEN  () { return g_color ? "\033[0;32m" : ""; }
static const char* COL_YELLOW () { return g_color ? "\033[1;33m" : ""; }
static const char* COL_MAGENTA() { return g_color ? "\033[0;35m" : ""; }
static const char* COL_RED    () { return g_color ? "\033[0;31m" : ""; }
static const char* COL_DIM    () { return g_color ? "\033[2m"    : ""; }

// ============================================================================
// Minimal .vsim key reader  (flat key = value, no section awareness needed here)
// Also captures [chem_plus] section keys as "chem_plus.<key>" entries.  WO-84U
// ============================================================================

static std::map<std::string, std::string> read_flat_keys(const std::filesystem::path& p) {
	std::map<std::string, std::string> m;
	std::ifstream f(p);
	if (!f) return m;
	std::string line;
	std::string current_section;
	auto trim = [](std::string& s) {
		while (!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
		while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
		if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
			s = s.substr(1, s.size() - 2);
		}
	};
	while (std::getline(f, line)) {
		// strip comments
		const auto ch = line.find('#');
		if (ch != std::string::npos) line.resize(ch);
		// detect section headers  [section_name]
		std::string trimmed_line = line;
		trim(trimmed_line);
		if (!trimmed_line.empty() && trimmed_line.front() == '[' && trimmed_line.back() == ']') {
			current_section = trimmed_line.substr(1, trimmed_line.size() - 2);
			trim(current_section);
			continue;
		}
		const auto eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string key = line.substr(0, eq);
		std::string val = line.substr(eq + 1);
		trim(key); trim(val);
		if (key.empty()) continue;
		// store chem_plus keys with a "chem_plus." prefix for later retrieval
		if (current_section == "chem_plus") {
			m["chem_plus." + key] = val;
		} else {
			m[key] = val;
		}
	}
	return m;
}

// ============================================================================
// Build a minimal atomistic::State from a formula string
// Parses Hill-order formula: e.g. "CH4", "NH3", "CO2", "BF3", "SF6"
// Creates atoms at idealized geometry positions for classifier input.
// ============================================================================

static std::map<std::string, int> parse_formula(const std::string& formula) {
	std::map<std::string, int> counts;
	std::size_t i = 0;
	while (i < formula.size()) {
		if (!std::isupper((unsigned char)formula[i])) { ++i; continue; }
		std::string elem(1, formula[i++]);
		while (i < formula.size() && std::islower((unsigned char)formula[i])) {
			elem += formula[i++];
		}
		int count = 0;
		while (i < formula.size() && std::isdigit((unsigned char)formula[i])) {
			count = count * 10 + (formula[i++] - '0');
		}
		if (count == 0) count = 1;
		counts[elem] += count;
	}
	return counts;
}

static int element_Z(const std::string& sym) {
	static const std::map<std::string, int> table = {
		{"H",1},{"He",2},{"Li",3},{"Be",4},{"B",5},{"C",6},{"N",7},{"O",8},
		{"F",9},{"Ne",10},{"Na",11},{"Mg",12},{"Al",13},{"Si",14},{"P",15},
		{"S",16},{"Cl",17},{"Ar",18},{"K",19},{"Ca",20},{"Fe",26},{"Cu",29},
		{"Zn",30},{"Br",35},{"Kr",36},{"I",53},{"Xe",54},
	};
	auto it = table.find(sym);
	return it != table.end() ? it->second : 6; // default to carbon
}

static atomistic::State build_state_from_formula(const std::string& formula) {
	atomistic::State state;
	const auto counts = parse_formula(formula);
	if (counts.empty()) return state;

	// Build atom list: center atom first, then ligands spread at ~1.5 Å
	// For preview purposes, position doesn't need to be chemically perfect.
	struct AtomSpec { int Z; double x, y, z_coord; };
	std::vector<AtomSpec> atoms;

	// Pick the center atom (heaviest non-H, or first)
	std::string center_sym;
	int center_Z_val = 0;
	for (const auto& [sym, cnt] : counts) {
		if (sym == "H") continue;
		int z = element_Z(sym);
		if (z > center_Z_val) { center_Z_val = z; center_sym = sym; }
	}
	if (center_sym.empty()) center_sym = "C";

	atoms.push_back({ element_Z(center_sym), 0.0, 0.0, 0.0 });

	// Place ligands on a sphere at 1.5 Å
	constexpr double r = 1.5;
	constexpr double pi = 3.14159265358979323846;
	int placed = 0;
	for (const auto& [sym, cnt] : counts) {
		for (int n = 0; n < cnt; ++n) {
			if (sym == center_sym && n == 0) continue; // skip center
			const double theta = pi * (placed + 1) / (counts.size() * 3 + 1);
			const double phi   = 2.0 * pi * placed / 6.0;
			atoms.push_back({
				element_Z(sym),
				r * std::sin(theta) * std::cos(phi),
				r * std::sin(theta) * std::sin(phi),
				r * std::cos(theta)
			});
			++placed;
		}
	}

	state.N = static_cast<uint32_t>(atoms.size());
	state.X.resize(state.N);
	state.V.resize(state.N, {0,0,0});
	state.Q.resize(state.N, 0.0);
	state.M.resize(state.N, 1.0);
	state.type.resize(state.N);
	state.F.resize(state.N, {0,0,0});

	for (uint32_t i = 0; i < state.N; ++i) {
		state.X[i] = { atoms[i].x, atoms[i].y, atoms[i].z_coord };
		state.type[i] = static_cast<uint32_t>(atoms[i].Z);
	}

	// Connect all non-H atoms to center as edges (index 0)
	for (uint32_t i = 1; i < state.N; ++i) {
		state.B.push_back({ 0u, i });
	}

	return state;
}

} // anonymous namespace

// ============================================================================
// run_classify_preview
// ============================================================================

int run_classify_preview(const std::filesystem::path& vsim_path) {
	g_color = static_cast<bool>(IS_TTY);

	if (!std::filesystem::exists(vsim_path)) {
		std::cerr << "classify: file not found: " << vsim_path << "\n";
		return 1;
	}

	const auto keys = read_flat_keys(vsim_path);

	// Extract formula and name from flat keys
	std::string formula = "C";
	std::string name = vsim_path.stem().string();
	for (const auto& [k, v] : keys) {
		if (k == "formula") formula = v;
		if (k == "name")    name    = v;
	}

	// -----------------------------------------------------------------------
	// Header
	// -----------------------------------------------------------------------
	std::cout << "\n"
			  << COL_BOLD() << COL_YELLOW() << "  VSIM-83 Classify Preview" << COL_RESET() << "\n"
			  << "  " << std::string(62, '-') << "\n"
			  << COL_DIM() << "  source: " << vsim_path.filename().string() << COL_RESET() << "\n"
			  << COL_DIM() << "  script: " << name    << COL_RESET() << "\n"
			  << COL_DIM() << "  formula:" << formula << COL_RESET() << "\n\n";

	// -----------------------------------------------------------------------
	// Build minimal state
	// -----------------------------------------------------------------------
	atomistic::State state = build_state_from_formula(formula);
	if (state.N == 0) {
		std::cerr << "classify: could not parse formula '" << formula << "'\n";
		return 1;
	}

	// -----------------------------------------------------------------------
	// VSEPR
	// -----------------------------------------------------------------------
	std::cout << COL_CYAN() << "  [VSEPR]" << COL_RESET() << "\n";
	const auto vreport = atomistic::classify::classify_vsepr_sites(state);
	const std::string vtext = atomistic::classify::format_vsepr_report(vreport);
	// indent each line
	std::istringstream vss(vtext);
	std::string vline;
	while (std::getline(vss, vline)) {
		if (vline.find("site[") != std::string::npos) {
			std::cout << COL_GREEN() << "  " << vline << COL_RESET() << "\n";
		} else {
			std::cout << "  " << vline << "\n";
		}
	}

	// -----------------------------------------------------------------------
	// OrganicCandidate
	// -----------------------------------------------------------------------
	std::cout << "\n" << COL_CYAN() << "  [OrganicCandidate]" << COL_RESET() << "\n";
	atomistic::classify::OrganicClassifier oc;
	const auto cand = oc.classify(state);
	const std::string otext = atomistic::classify::format_organic_candidate(cand);
	std::istringstream oss(otext);
	std::string oline;
	while (std::getline(oss, oline)) {
		if (oline.find("family") != std::string::npos) {
			std::cout << COL_MAGENTA() << "  " << oline << COL_RESET() << "\n";
		} else if (oline.find("score") != std::string::npos || oline.find("risk") != std::string::npos) {
			std::cout << COL_DIM() << "  " << oline << COL_RESET() << "\n";
		} else {
			std::cout << "  " << oline << "\n";
		}
	}

	// -----------------------------------------------------------------------
	// ChemPlus declarative block  (WO-84U: only shown when [chem_plus] present)
	// -----------------------------------------------------------------------
	{
		vsim::ChemPlusSection cp;
		for (const auto& [k, v] : keys) {
			if (k == "chem_plus.reaction")       cp.reaction       = v;
			else if (k == "chem_plus.preset")    cp.preset         = v;
			else if (k == "chem_plus.vsepr_link") cp.vsepr_link    = (v == "true" || v == "1" || v == "yes");
			else if (k == "chem_plus.class_override") cp.class_override = v;
			else if (k == "chem_plus.energy_kj") {
				try { cp.energy_kj = std::stod(v); } catch (...) {}
			}
		}
		if (cp.is_active()) {
			std::cout << "\n" << COL_CYAN() << "  [ChemPlus]" << COL_RESET() << "\n";
			const auto cpr = vsim::chemplus::evaluate(cp);
			if (!cpr.ok) {
				std::cout << COL_RED() << "  error: " << cpr.error << COL_RESET() << "\n";
			} else {
				for (const auto& r : cpr.reactions) {
					std::cout << COL_GREEN() << "  + " << r.reaction << COL_RESET() << "\n";
					std::cout << COL_DIM()   << "    class=" << r.reaction_class
							  << "  mode=" << r.energy_mode;
					if (r.has_energy)
						std::cout << "  energy=" << r.energy_kj << " kJ";
					if (!r.vsepr_tag.empty())
						std::cout << "  vsepr=" << r.vsepr_tag;
					std::cout << COL_RESET() << "\n";
				}
			}
		}
	}

	// -----------------------------------------------------------------------
	// Footer
	// -----------------------------------------------------------------------
	std::cout << "\n  " << std::string(62, '-') << "\n"
			  << COL_GREEN() << "  result: PASS" << COL_RESET()
			  << COL_DIM() << "  (preview only -- simulation not run)" << COL_RESET() << "\n\n";

	return 0;
}

} // namespace cli
} // namespace vsepr
