#pragma once
// WO-72V — material preset cache
// Precomputed material presets keyed by formula string.
// Loaded once at startup from data/material_presets.json (if present);
// falls back to a hard-coded minimal table when the file is absent.
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <filesystem>

namespace vsim::cache {

struct MaterialPreset {
	std::string formula;        // e.g. "NaCl"
	std::string name;           // human name
	std::string crystal_system; // cubic / hexagonal / ...
	double      lattice_a  = 0.0;
	double      lattice_b  = 0.0;
	double      lattice_c  = 0.0;
	double      density_gcc = 0.0; // g/cm³
	double      melting_K   = 0.0;
	std::string notes;
};

class MaterialPresetCache {
public:
	// Load from JSON file; returns number of entries loaded (0 = file missing, uses built-ins).
	int  load(const std::filesystem::path& json_path);

	// Seed the built-in minimal table (always available).
	void load_builtins();

	std::optional<MaterialPreset> find(const std::string& formula) const;
	const std::unordered_map<std::string, MaterialPreset>& all() const { return table_; }
	std::size_t size() const { return table_.size(); }

private:
	std::unordered_map<std::string, MaterialPreset> table_;
};

} // namespace vsim::cache
