/**
 * visual_settings.hpp  --  VSEPR-SIM visual/rendering preferences  (v5.1.4 / WO-72C)
 *
 * Persistent settings layer for viewer windows, trajectory replays, and
 * any future GUI that needs a shared set of visual defaults.
 *
 * Storage: %LOCALAPPDATA%\VSEPR-SIM\config\visual_settings.json
 *          Falls back to ./visual_settings.json when LOCALAPPDATA is absent.
 *
 * Usage:
 *   auto vs = vsim::VisualSettings::load();   // load or create defaults
 *   vs.theme = "light";
 *   vs.save();
 *
 * CLI surface:
 *   vsepr settings show
 *   vsepr settings set <key> <value>
 *   vsepr settings reset
 */

#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>

namespace vsim {

struct VisualSettings {
	// -----------------------------------------------------------------------
	// Fields  (all string to stay JSON-round-trip trivial)
	// -----------------------------------------------------------------------
	std::string theme             = "dark";      // dark | light
	std::string atom_radius_scale = "1.0";       // float multiplier
	std::string bond_radius       = "0.15";      // Angstrom
	std::string background_color  = "0x1a1a2e";  // hex RGB
	std::string label_font_size   = "12";        // pt
	std::string show_axes         = "true";
	std::string show_bonds        = "true";
	std::string show_unit_cell    = "true";
	std::string trajectory_fps    = "30";

	// -----------------------------------------------------------------------
	// Paths
	// -----------------------------------------------------------------------
	static std::filesystem::path default_path() {
		namespace fs = std::filesystem;
		const char* lad = std::getenv("LOCALAPPDATA");
		if (lad && *lad)
			return fs::path(lad) / "VSEPR-SIM" / "config" / "visual_settings.json";
		return fs::path("visual_settings.json");
	}

	// -----------------------------------------------------------------------
	// Load  (returns defaults if file absent or unreadable)
	// -----------------------------------------------------------------------
	static VisualSettings load(const std::filesystem::path& path = {}) {
		namespace fs = std::filesystem;
		VisualSettings vs;
		fs::path p = path.empty() ? default_path() : path;
		if (!fs::exists(p)) return vs;
		std::ifstream f(p);
		if (!f) return vs;
		// Minimal JSON key-value parser (no external dependency required)
		std::string line;
		while (std::getline(f, line)) {
			auto q1 = line.find('"');
			if (q1 == std::string::npos) continue;
			auto q2 = line.find('"', q1 + 1);
			if (q2 == std::string::npos) continue;
			std::string key = line.substr(q1 + 1, q2 - q1 - 1);
			auto col = line.find(':', q2 + 1);
			if (col == std::string::npos) continue;
			auto v1 = line.find('"', col + 1);
			if (v1 == std::string::npos) continue;
			auto v2 = line.find('"', v1 + 1);
			if (v2 == std::string::npos) continue;
			std::string val = line.substr(v1 + 1, v2 - v1 - 1);
			vs.apply(key, val);
		}
		return vs;
	}

	// -----------------------------------------------------------------------
	// Save
	// -----------------------------------------------------------------------
	void save(const std::filesystem::path& path = {}) const {
		namespace fs = std::filesystem;
		fs::path p = path.empty() ? default_path() : path;
		fs::create_directories(p.parent_path());
		std::ofstream f(p);
		f << "{\n"
		  << "  \"theme\": \""             << theme             << "\",\n"
		  << "  \"atom_radius_scale\": \"" << atom_radius_scale << "\",\n"
		  << "  \"bond_radius\": \""       << bond_radius       << "\",\n"
		  << "  \"background_color\": \""  << background_color  << "\",\n"
		  << "  \"label_font_size\": \""   << label_font_size   << "\",\n"
		  << "  \"show_axes\": \""         << show_axes         << "\",\n"
		  << "  \"show_bonds\": \""        << show_bonds        << "\",\n"
		  << "  \"show_unit_cell\": \""    << show_unit_cell    << "\",\n"
		  << "  \"trajectory_fps\": \""    << trajectory_fps    << "\"\n"
		  << "}\n";
	}

	// -----------------------------------------------------------------------
	// Accessors typed
	// -----------------------------------------------------------------------
	double atom_radius_scale_f()  const { return std::stod(atom_radius_scale); }
	double bond_radius_f()        const { return std::stod(bond_radius); }
	int    label_font_size_i()    const { return std::stoi(label_font_size); }
	int    trajectory_fps_i()     const { return std::stoi(trajectory_fps); }
	bool   show_axes_b()          const { return show_axes    == "true"; }
	bool   show_bonds_b()         const { return show_bonds   == "true"; }
	bool   show_unit_cell_b()     const { return show_unit_cell == "true"; }
	bool   is_dark()              const { return theme == "dark"; }

	// Apply a single key/value pair (public surface for 'vsepr settings set')
	void apply_key(const std::string& key, const std::string& val) { apply(key, val); }

private:
	void apply(const std::string& key, const std::string& val) {
		if (key == "theme")             theme             = val;
		else if (key == "atom_radius_scale") atom_radius_scale = val;
		else if (key == "bond_radius")  bond_radius       = val;
		else if (key == "background_color") background_color = val;
		else if (key == "label_font_size")  label_font_size  = val;
		else if (key == "show_axes")    show_axes         = val;
		else if (key == "show_bonds")   show_bonds        = val;
		else if (key == "show_unit_cell") show_unit_cell  = val;
		else if (key == "trajectory_fps") trajectory_fps  = val;
	}
};

} // namespace vsim
