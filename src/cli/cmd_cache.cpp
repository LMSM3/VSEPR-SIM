// WO-72V — cache CLI command implementation
#include "cmd_cache.hpp"
#include "vsim/cache/cache.hpp"
#include <iostream>
#include <iomanip>

namespace vsepr::cli {

static void print_usage_cache() {
	std::cout <<
		"VSEPR-SIM cache commands:\n"
		"  vsepr cache list-presets [<formula>]   List material preset(s)\n"
		"  vsepr cache list-routes  <formula>     Formation routes for product\n"
		"  vsepr cache list-props   [<formula>]   Physical property table entry/entries\n"
		"  vsepr cache index-status               Trajectory index summary\n";
}

int cmd_cache(const std::vector<std::string>& args) {
	if (args.empty()) { print_usage_cache(); return 0; }

	const std::string sub = args[0];

	// ---- list-presets -------------------------------------------------------
	if (sub == "list-presets") {
		vsim::cache::MaterialPresetCache cache;
		cache.load_builtins();

		if (args.size() >= 2) {
			auto p = cache.find(args[1]);
			if (!p) { std::cerr << "No preset for: " << args[1] << "\n"; return 1; }
			std::cout << p->formula << "  " << p->name << "\n"
					  << "  crystal: " << p->crystal_system << "\n"
					  << "  a=" << p->lattice_a << " b=" << p->lattice_b
					  << " c=" << p->lattice_c << " Å\n"
					  << "  density=" << p->density_gcc << " g/cm³"
					  << "  Tm=" << p->melting_K << " K\n"
					  << "  notes: " << p->notes << "\n";
		} else {
			std::cout << std::left
					  << std::setw(8)  << "Formula"
					  << std::setw(24) << "Name"
					  << std::setw(14) << "Crystal"
					  << std::setw(10) << "Density"
					  << "Tm (K)\n"
					  << std::string(62, '-') << "\n";
			for (auto& [f, p] : cache.all()) {
				std::cout << std::setw(8)  << p.formula
						  << std::setw(24) << p.name
						  << std::setw(14) << p.crystal_system
						  << std::setw(10) << p.density_gcc
						  << p.melting_K << "\n";
			}
		}
		return 0;
	}

	// ---- list-routes --------------------------------------------------------
	if (sub == "list-routes") {
		if (args.size() < 2) { std::cerr << "Usage: vsepr cache list-routes <formula>\n"; return 1; }
		vsim::cache::FormationLUT lut;
		lut.load_builtins();
		auto routes = lut.routes_for(args[1]);
		if (routes.empty()) { std::cerr << "No routes for: " << args[1] << "\n"; return 1; }
		for (auto& r : routes) {
			std::cout << r.product << "  via " << r.method
					  << "  dH=" << r.delta_H_kJ_mol << " kJ/mol"
					  << "  T=" << r.temp_K << " K"
					  << "  conf=" << std::fixed << std::setprecision(2) << r.confidence
					  << "  [" << r.reference << "]\n";
		}
		return 0;
	}

	// ---- list-props ---------------------------------------------------------
	if (sub == "list-props") {
		vsim::cache::PropertyTable pt;
		pt.load_builtins();
		if (args.size() >= 2) {
			auto row = pt.find(args[1]);
			if (!row) { std::cerr << "No property data for: " << args[1] << "\n"; return 1; }
			std::cout << row->formula << "\n"
					  << "  density:       " << row->density_gcc       << " g/cm³\n"
					  << "  thermal cond:  " << row->thermal_cond_W_mK << " W/(m·K)\n"
					  << "  heat cap:      " << row->heat_cap_J_gK     << " J/(g·K)\n"
					  << "  elec cond:     " << row->electrical_cond_Sm<< " S/m\n"
					  << "  bandgap:       " << (row->bandgap_eV < 0 ? -1.0 : row->bandgap_eV) << " eV\n"
					  << "  Tm:            " << row->melting_K         << " K\n"
					  << "  Tb:            " << row->boiling_K         << " K\n";
		} else {
			std::cout << std::left
					  << std::setw(8) << "Formula"
					  << std::setw(10) << "rho"
					  << std::setw(10) << "k_th"
					  << std::setw(10) << "Tm(K)\n"
					  << std::string(38, '-') << "\n";
			// PropertyTable has no all() — iterate via known formulas
			for (auto& f : {"NaCl","Si","Fe","Al","Cu","MgO","TiO2","C","SiO2","Al2O3"}) {
				auto r = pt.find(f);
				if (!r) continue;
				std::cout << std::setw(8) << r->formula
						  << std::setw(10) << r->density_gcc
						  << std::setw(10) << r->thermal_cond_W_mK
						  << r->melting_K << "\n";
			}
		}
		return 0;
	}

	// ---- index-status -------------------------------------------------------
	if (sub == "index-status") {
		vsim::cache::TrajectoryIndex idx;
		// Attempt to load from default location
		namespace fs = std::filesystem;
		std::string localAppData;
		if (const char* p = std::getenv("LOCALAPPDATA")) localAppData = p;
		fs::path idxPath = localAppData.empty()
			? fs::path("trajectory_index.json")
			: fs::path(localAppData) / "VSEPR-SIM" / "cache" / "trajectory_index.json";
		bool loaded = idx.load(idxPath);
		std::cout << "Trajectory index: " << idxPath.string() << "\n"
				  << "  entries: " << idx.size() << "\n"
				  << "  status:  " << (loaded ? "loaded" : "not found") << "\n";
		return 0;
	}

	std::cerr << "Unknown cache sub-command: " << sub << "\n";
	print_usage_cache();
	return 1;
}

} // namespace vsepr::cli
