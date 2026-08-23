// mcf_cai_parser.cpp  -  MCF-CAI Kernel Fork  -  .vsim object block parser
// Phase 3b | v1.0 | VSPER-SIM v5.0.0-main | Status: BLUE

#include "../../include/vsim/kernel_mcf/mcf_cai_parser.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace vsim::kernel_mcf {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace {

std::string trim(const std::string& s) {
	const auto b = s.find_first_not_of(" \t\r\n");
	if (b == std::string::npos) return "";
	const auto e = s.find_last_not_of(" \t\r\n");
	return s.substr(b, e - b + 1);
}

double to_double(const std::string& v) {
	try { return std::stod(v); } catch (...) { return 0.0; }
}
int to_int(const std::string& v) {
	try { return std::stoi(v); } catch (...) { return 0; }
}
bool to_bool(const std::string& v) {
	std::string l = v;
	for (auto& c : l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return l == "true" || l == "1" || l == "yes";
}
std::string strip_quotes(const std::string& v) {
	if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
		return v.substr(1, v.size() - 2);
	return v;
}
Vec3d parse_vec3(const std::string& v) {
	Vec3d r = kZeroVec3d;
	std::string s = v;
	for (auto& c : s) if (c == '[' || c == ']' || c == ',') c = ' ';
	std::istringstream ss(s);
	ss >> r[0] >> r[1] >> r[2];
	return r;
}

struct SectionKey {
	std::string obj_name;
	std::string layer;   // macro | chemical | fundamental
	std::string basis;   // carrier | action | information
	bool valid = false;
};

SectionKey parse_section(const std::string& line) {
	SectionKey sk;
	if (line.size() < 2 || line.front() != '[' || line.back() != ']') return sk;
	std::string inner = line.substr(1, line.size() - 2);
	std::vector<std::string> parts;
	std::istringstream ss(inner);
	std::string tok;
	while (std::getline(ss, tok, '.')) parts.push_back(trim(tok));
	if (parts.size() < 4) return sk;
	if (parts[0] != "object") return sk;
	static const std::unordered_map<std::string, bool> layers = {
		{"macro", true}, {"chemical", true}, {"fundamental", true}
	};
	static const std::unordered_map<std::string, bool> bases = {
		{"carrier", true}, {"action", true}, {"information", true}
	};
	if (!layers.count(parts[2]) || !bases.count(parts[3])) return sk;
	sk.obj_name = parts[1];
	sk.layer    = parts[2];
	sk.basis    = parts[3];
	sk.valid    = true;
	return sk;
}

} // anon namespace

// ---------------------------------------------------------------------------
// parse_stream
// ---------------------------------------------------------------------------
std::vector<McfCaiParseError>
McfCaiParser::parse_stream(std::istream& in, McfCaiWorld& world) {
	std::vector<McfCaiParseError> errors;
	std::unordered_map<std::string, McfCaiObject> staging;

	std::string line;
	int lineno = 0;
	SectionKey cur;

	auto get_or_make = [&](const std::string& name) -> McfCaiObject& {
		if (!staging.count(name)) {
			staging[name]      = McfCaiObject{};
			staging[name].name = name;
		}
		return staging[name];
	};

	while (std::getline(in, line)) {
		++lineno;
		const std::string t = trim(line);
		if (t.empty() || t[0] == '#') continue;

		if (t[0] == '[' && t.back() == ']') {
			cur = parse_section(t);
			continue;
		}
		if (!cur.valid) continue;

		const auto eq = t.find('=');
		if (eq == std::string::npos) continue;
		const std::string key = trim(t.substr(0, eq));
		const std::string val = trim(t.substr(eq + 1));
		if (key.empty()) continue;

		auto& obj = get_or_make(cur.obj_name);

		// ---- macro.carrier ------------------------------------------------
		if (cur.layer == "macro" && cur.basis == "carrier") {
			if      (key == "position")      obj.macro_c.position       = parse_vec3(val);
			else if (key == "phase")         obj.macro_c.phase          = strip_quotes(val);
			else if (key == "geometry")      obj.macro_c.geometry       = strip_quotes(val);
			else if (key == "volume_A3")     obj.macro_c.volume_A3      = to_double(val);
			else if (key == "grain_size_A")  obj.macro_c.grain_size_A   = to_double(val);
			else if (key == "grain_count")   obj.macro_c.grain_count    = to_int(val);
			else if (key == "phase_fraction")obj.macro_c.phase_fraction = to_double(val);
		}
		// ---- macro.action -------------------------------------------------
		else if (cur.layer == "macro" && cur.basis == "action") {
			if      (key == "stress_diag")   obj.macro_a.stress_diag   = parse_vec3(val);
			else if (key == "heat_flux")     obj.macro_a.heat_flux      = parse_vec3(val);
			else if (key == "flow_velocity") obj.macro_a.flow_velocity  = parse_vec3(val);
			else if (key == "deformation")   obj.macro_a.deformation    = to_double(val);
			else if (key == "fracture_prob") obj.macro_a.fracture_prob  = to_double(val);
			else if (key == "diffusion_rate")obj.macro_a.diffusion_rate = to_double(val);
			else if (key == "temperature")   obj.macro_a.temperature    = to_double(val);
		}
		// ---- macro.information --------------------------------------------
		else if (cur.layer == "macro" && cur.basis == "information") {
			if      (key == "formation_age")    obj.macro_i.formation_age    = to_double(val);
			else if (key == "defect_memory")    obj.macro_i.defect_memory    = to_double(val);
			else if (key == "coarse_grain_loss")obj.macro_i.coarse_grain_loss= to_double(val);
		}
		// ---- chemical.carrier ---------------------------------------------
		else if (cur.layer == "chemical" && cur.basis == "carrier") {
			if      (key == "Z")              obj.chem_c.Z             = to_int(val);
			else if (key == "mass_amu")       obj.chem_c.mass_amu      = to_double(val);
			else if (key == "charge_e")       obj.chem_c.charge_e      = to_double(val);
			else if (key == "oxidation_state")obj.chem_c.oxidation_state= to_int(val);
			else if (key == "coordination")   obj.chem_c.coordination  = to_int(val);
			else if (key == "symbol")         obj.chem_c.symbol        = strip_quotes(val);
			else if (key == "hybridization")  obj.chem_c.hybridization = strip_quotes(val);
			else if (key == "residue")        obj.chem_c.residue       = strip_quotes(val);
			else if (key == "residue_index")  obj.chem_c.residue_index = to_int(val);
		}
		// ---- chemical.action ----------------------------------------------
		else if (cur.layer == "chemical" && cur.basis == "action") {
			if      (key == "reaction_enabled") obj.chem_a.reaction_enabled = to_bool(val);
			else if (key == "bond_exchange")    obj.chem_a.bond_exchange    = to_bool(val);
			else if (key == "reaction_rate")    obj.chem_a.reaction_rate    = to_double(val);
			else if (key == "solvation_energy") obj.chem_a.solvation_energy = to_double(val);
			else if (key == "ionisation_energy")obj.chem_a.ionisation_energy= to_double(val);
		}
		// ---- chemical.information -----------------------------------------
		else if (cur.layer == "chemical" && cur.basis == "information") {
			if      (key == "formation_route")       obj.chem_i.formation_route       = strip_quotes(val);
			else if (key == "reaction_entropy_loss") obj.chem_i.reaction_entropy_loss = to_double(val);
			else if (key == "dist_chem")             obj.chem_i.dist_chem             = to_double(val);
		}
		// ---- fundamental.carrier ------------------------------------------
		else if (cur.layer == "fundamental" && cur.basis == "carrier") {
			if      (key == "charge")        obj.fund_c.net_charge_e = to_double(val);
			else if (key == "spin_proxy")    obj.fund_c.spin_proxy   = to_double(val);
			else if (key == "isotope_A")     obj.fund_c.isotope_A    = to_int(val);
			else if (key == "nuclear_state") obj.fund_c.nuclear_state= to_int(val);
			else if (key == "colour_state") {
				auto v3 = parse_vec3(val);
				obj.fund_c.caf_channel = { static_cast<float>(v3[0]),
										   static_cast<float>(v3[1]),
										   static_cast<float>(v3[2]) };
			}
		}
		// ---- fundamental.action -------------------------------------------
		else if (cur.layer == "fundamental" && cur.basis == "action") {
			if      (key == "em_coupling")       obj.fund_a.em_coupling      = to_bool(val);
			else if (key == "strong_proxy")      obj.fund_a.strong_proxy     = to_bool(val);
			else if (key == "decay_enabled")     obj.fund_a.decay_enabled    = to_bool(val);
			else if (key == "annihilation_flag") obj.fund_a.annihilation_flag= to_bool(val);
			else if (key == "field_strength")    obj.fund_a.field_strength   = to_double(val);
			else if (key == "field_direction")   obj.fund_a.field_direction  = parse_vec3(val);
		}
		// ---- fundamental.information --------------------------------------
		else if (cur.layer == "fundamental" && cur.basis == "information") {
			if      (key == "hidden_W")        obj.fund_i.hidden_W        = to_double(val);
			else if (key == "projection_loss") obj.fund_i.projection_loss = to_double(val);
			else if (key == "entropy_loss")    obj.fund_i.entropy_loss    = to_double(val);
			else if (key == "dist_fund")       obj.fund_i.dist_fund       = to_double(val);
		}
	}

	// Commit staged objects to world
	for (auto& [name, obj] : staging)
		world.add(std::move(obj));

	return errors;
}

// ---------------------------------------------------------------------------
// parse_file
// ---------------------------------------------------------------------------
std::vector<McfCaiParseError>
McfCaiParser::parse_file(const std::string& path, McfCaiWorld& world) {
	std::ifstream f(path);
	if (!f.is_open()) return {{0, "Cannot open: " + path}};
	return parse_stream(f, world);
}

// ---------------------------------------------------------------------------
// is_mcf_kernel_enabled
// ---------------------------------------------------------------------------
bool McfCaiParser::is_mcf_kernel_enabled_stream(std::istream& in) {
	std::string line;
	bool in_run = false;
	while (std::getline(in, line)) {
		const std::string t = trim(line);
		if (t == "[mcf_kernel]") return true;
		if (t == "[run]") { in_run = true; continue; }
		if (!t.empty() && t[0] == '[') in_run = false;
		if (in_run) {
			const auto eq = t.find('=');
			if (eq != std::string::npos) {
				const std::string key = trim(t.substr(0, eq));
				const std::string val = trim(t.substr(eq + 1));
				if (key == "use_mcf_kernel" && to_bool(val)) return true;
			}
		}
	}
	return false;
}
bool McfCaiParser::is_mcf_kernel_enabled(const std::string& path) {
	std::ifstream f(path);
	if (!f.is_open()) return false;
	return is_mcf_kernel_enabled_stream(f);
}

} // namespace vsim::kernel_mcf
