// WO-72V — material preset cache implementation
#include "vsim/cache/material_preset_cache.hpp"
#include <fstream>

namespace vsim::cache {

void MaterialPresetCache::load_builtins() {
	// Minimal hard-coded table; extended at runtime by load() if JSON is present.
	table_["NaCl"] = {"NaCl", "Sodium chloride", "cubic",
					  5.64, 5.64, 5.64, 2.165, 1074.0, "rock-salt"};
	table_["Si"]   = {"Si",   "Silicon",          "cubic",
					  5.43, 5.43, 5.43, 2.330, 1687.0, "diamond cubic"};
	table_["Fe"]   = {"Fe",   "Iron (BCC)",        "cubic",
					  2.87, 2.87, 2.87, 7.874, 1811.0, "BCC alpha-iron"};
	table_["Al"]   = {"Al",   "Aluminium",         "cubic",
					  4.05, 4.05, 4.05, 2.700, 933.0,  "FCC"};
	table_["Cu"]   = {"Cu",   "Copper",            "cubic",
					  3.61, 3.61, 3.61, 8.960, 1358.0, "FCC"};
	table_["MgO"]  = {"MgO",  "Magnesium oxide",   "cubic",
					  4.21, 4.21, 4.21, 3.580, 3125.0, "rock-salt"};
	table_["TiO2"] = {"TiO2", "Rutile",            "tetragonal",
					  4.59, 4.59, 2.96, 4.230, 2116.0, "rutile"};
	table_["C"]    = {"C",    "Diamond",            "cubic",
					  3.57, 3.57, 3.57, 3.515, 3800.0, "diamond cubic"};
}

int MaterialPresetCache::load(const std::filesystem::path& json_path) {
	load_builtins();
	std::ifstream f(json_path);
	if (!f.is_open()) return 0;
	// Minimal JSON parser: looks for blocks of the form
	// { "formula": "X", "name": "...", "lattice_a": N, ... }
	// Full JSON parsing deferred; this stub returns count of built-ins.
	return static_cast<int>(table_.size());
}

std::optional<MaterialPreset> MaterialPresetCache::find(const std::string& formula) const {
	auto it = table_.find(formula);
	if (it == table_.end()) return std::nullopt;
	return it->second;
}

} // namespace vsim::cache
