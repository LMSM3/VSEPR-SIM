#pragma once
/**
 * include/vsim/intent/intent_bridge.hpp
 * ========================================
 * WO-VSIM-INTENT-BRIDGE-A  |  Phase 6  |  v5.1.x
 *
 * Intent Runtime Bridge — simplest bridge layer:
 *
 *   [material]     ->  generated particles / masses / charges
 *   [environment]  ->  boundary/PBC/temperature configuration
 *   [run]          ->  execution config (mode, steps, dt)
 *
 * Deferred to BRIDGE-B/C:
 *   [[raw.object]]         — explicit particle injection
 *   [[override.particle]]  — particle mutation
 *   [excite.*]             — excitation channels
 *
 * Architecture:
 *   IntentBridge reads VsimDocument fields, resolves the material
 *   via RegistryResolver, expands the basis string into IntentParticles,
 *   and fills IntentSystem with the resulting runtime configuration.
 *
 *   This is a pure data-transformation layer — no I/O, no physics.
 *   The caller (cmd_run_vsim.cpp or tests) owns the IntentSystem and
 *   decides what to do with it.
 *
 * Group 54 — Intent runtime bridge basic
 */

#include "vsim/vsim_document.hpp"
#include "vsim/vsim_registry.hpp"

#include <array>
#include <cctype>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace vsim {

// ============================================================================
// IntentParticle
// ============================================================================
//
// One particle emitted by the intent bridge from a [material] section.
// Positions are fractional coordinates from the basis (0.0–1.0 each axis).
// mass and charge are best-effort from the element or charge model.
//
struct IntentParticle {
	std::string symbol;                           // Element symbol: "Na", "Cl", "X"
	std::array<double, 3> frac_pos  = {0, 0, 0}; // Fractional basis position
	double mass   = 0.0;                          // amu (0 = unknown)
	double charge = 0.0;                          // e  (formal or neutral)
	int    basis_index = 0;                       // 0-based index in basis string
};

// ============================================================================
// IntentEnvironment
// ============================================================================
//
// Runtime environment configuration resolved from [environment] + [pbc].
//
struct IntentEnvironment {
	double temperature_K  = 300.0;
	double pressure_GPa   = 0.0;
	bool   periodic       = false;
	std::string medium;                  // "vacuum", "water", ...
	std::array<double, 3> field_V_A = {0, 0, 0};  // External E-field (V/Å)
	std::string boundary_x = "open";
	std::string boundary_y = "open";
	std::string boundary_z = "open";
};

// ============================================================================
// IntentRunConfig
// ============================================================================
//
// Execution configuration resolved from [run].
//
struct IntentRunConfig {
	std::string mode         = "relax";
	int         max_steps    = 500;
	double      dt_fs        = 1.0;
	double      temperature_K = 300.0;
	double      pressure_GPa = 0.0;
	bool        converge     = true;
	std::string output_level = "standard";
};

// ============================================================================
// IntentSystem
// ============================================================================
//
// Full resolved runtime intent — output of IntentBridge::apply().
//
struct IntentSystem {
	// Material origin
	std::string formula;
	std::string prototype;
	std::string space_group;
	std::string generator;
	bool        is_periodic = false;

	// Particle set (from material basis)
	std::vector<IntentParticle> particles;

	// Environment and run config
	IntentEnvironment  env;
	IntentRunConfig    run;

	// Diagnostic
	bool   material_resolved = false;    // True when RegistryResolver found a match
	bool   has_particles     = false;    // True when particles were generated
	std::string resolve_message;         // Human-readable resolution summary
};

// ============================================================================
// IntentBridge
// ============================================================================
//
// Transforms VsimDocument intent sections into a concrete IntentSystem.
//
class IntentBridge {
public:

	// -----------------------------------------------------------------------
	// apply  —  full apply: material + environment + run in one call.
	// -----------------------------------------------------------------------
	static IntentSystem apply(const VsimDocument& doc) {
		IntentSystem sys;
		apply_material(doc.material, sys);
		apply_environment(doc.environment, doc.boundary, sys);
		apply_run(doc.run, sys);
		return sys;
	}

	// -----------------------------------------------------------------------
	// apply_material  —  [material] → particles/masses/charges
	//
	// Resolution order:
	//   1. RegistryResolver maps prototype key → RegistryBundle (basis, space_group, …)
	//   2. Basis string is parsed into IntentParticles with fractional positions
	//   3. Mass: element lookup table (embedded below)
	//   4. Charge: formal charges for ionic prototypes; 0 for metallic/covalent
	// -----------------------------------------------------------------------
	static void apply_material(const MaterialSection& mat, IntentSystem& sys) {
		sys.formula   = mat.formula;
		sys.prototype = mat.prototype;

		if (!mat.has_formula() && !mat.has_prototype() && mat.structure.empty()) {
			sys.resolve_message = "no material defined";
			return;
		}

		// Resolve via registry
		RegistryBundle bundle = RegistryResolver::resolve(mat);
		sys.prototype    = bundle.prototype.empty() ? mat.prototype : bundle.prototype;
		sys.space_group  = bundle.space_group;
		sys.generator    = bundle.generator;
		sys.is_periodic  = bundle.is_periodic;
		sys.material_resolved = bundle.populated;

		// Parse basis → particles
		parse_basis(mat.formula, bundle, sys);

		// Build resolve message
		sys.resolve_message = "material=" + sys.formula
			+ " proto=" + sys.prototype
			+ " generator=" + sys.generator
			+ " particles=" + std::to_string(sys.particles.size());
		sys.has_particles = !sys.particles.empty();
	}

	// -----------------------------------------------------------------------
	// apply_environment  —  [environment] + [boundary] → IntentEnvironment
	// -----------------------------------------------------------------------
	static void apply_environment(const EnvironmentSection& env,
								  const BoundarySection&    bnd,
								  IntentSystem& sys)
	{
		sys.env.temperature_K  = (env.temperature > 0.0) ? env.temperature : 300.0;
		sys.env.pressure_GPa   = env.pressure;
		sys.env.periodic       = env.periodic;
		sys.env.medium         = env.medium;
		sys.env.field_V_A      = {env.field_x, env.field_y, env.field_z};
		sys.env.boundary_x     = bnd.x.empty() ? "open" : bnd.x;
		sys.env.boundary_y     = bnd.y.empty() ? "open" : bnd.y;
		sys.env.boundary_z     = bnd.z.empty() ? "open" : bnd.z;
	}

	// -----------------------------------------------------------------------
	// apply_run  —  [run] → IntentRunConfig
	// -----------------------------------------------------------------------
	static void apply_run(const RunSection& run, IntentSystem& sys) {
		sys.run.mode         = run.mode.empty() ? "relax" : run.mode;
		sys.run.max_steps    = (run.max_steps > 0) ? run.max_steps : 500;
		sys.run.dt_fs        = (run.dt_fs > 0.0)   ? run.dt_fs    : 1.0;
		sys.run.temperature_K = (run.temperature_K > 0.0) ? run.temperature_K : 300.0;
		sys.run.pressure_GPa = run.pressure_GPa;
		sys.run.converge     = run.converge;
		sys.run.output_level = run.output_level.empty() ? "standard" : run.output_level;
	}

private:

	// -----------------------------------------------------------------------
	// parse_basis  —  expand "Na:0,0,0; Cl:0.5,0.5,0.5" into IntentParticles.
	//
	// Format (from RegistryBundle::basis):
	//   "<Symbol>:<fx>,<fy>,<fz>; <Symbol>:<fx>,<fy>,<fz>; ..."
	//   Whitespace around separators is tolerated.
	//   Placeholder "X" is substituted with the first element of the formula.
	// -----------------------------------------------------------------------
	static void parse_basis(const std::string& formula,
							 const RegistryBundle& bundle,
							 IntentSystem& sys)
	{
		const std::string& basis = bundle.basis;
		if (basis.empty()) {
			// No basis — synthesize one particle from formula if available
			if (!formula.empty()) {
				IntentParticle p;
				p.symbol      = leading_element(formula);
				p.frac_pos    = {0, 0, 0};
				p.mass        = element_mass(p.symbol);
				p.charge      = 0.0;
				p.basis_index = 0;
				sys.particles.push_back(p);
			}
			return;
		}

		// Derive placeholder substitution from formula
		std::string placeholder = leading_element(formula);
		bool is_formal = (bundle.default_charge_model == "formal");

		// Split on ';'
		std::string tok;
		int idx = 0;
		std::istringstream ss(basis);
		while (std::getline(ss, tok, ';')) {
			tok = trim(tok);
			if (tok.empty()) continue;

			// Split on ':'
			auto colon = tok.find(':');
			if (colon == std::string::npos) continue;

			std::string sym = trim(tok.substr(0, colon));
			std::string coords_str = tok.substr(colon + 1);

			// Substitute placeholder
			if (sym == "X" && !placeholder.empty()) sym = placeholder;

			// Parse three comma-separated doubles
			double fx = 0, fy = 0, fz = 0;
			std::istringstream cs(coords_str);
			std::string c;
			int ci = 0;
			while (std::getline(cs, c, ',') && ci < 3) {
				try {
					double v = std::stod(trim(c));
					if (ci == 0) fx = v;
					else if (ci == 1) fy = v;
					else              fz = v;
				} catch (...) {}
				++ci;
			}

			IntentParticle p;
			p.symbol      = sym;
			p.frac_pos    = {fx, fy, fz};
			p.mass        = element_mass(sym);
			p.charge      = is_formal ? formal_charge(sym) : 0.0;
			p.basis_index = idx++;
			sys.particles.push_back(p);
		}
	}

	// -----------------------------------------------------------------------
	// leading_element  —  extract the leading element symbol from a formula.
	// E.g. "NaCl" → "Na",  "Fe2O3" → "Fe",  "Si" → "Si"
	// -----------------------------------------------------------------------
	static std::string leading_element(const std::string& formula) {
		if (formula.empty()) return "X";
		std::string sym;
		sym += formula[0];
		if (formula.size() > 1 && std::islower((unsigned char)formula[1]))
			sym += formula[1];
		return sym;
	}

	// -----------------------------------------------------------------------
	// element_mass  —  embedded atomic mass table (amu).
	// Covers common elements encountered in VSIM material scripts.
	// Unknown symbols return 1.0 (hydrogen-equivalent placeholder).
	// -----------------------------------------------------------------------
	static double element_mass(const std::string& sym) {
		static const std::pair<const char*, double> tbl[] = {
			{"H",   1.008},  {"He",  4.003},
			{"Li",  6.941},  {"Be",  9.012},  {"B",  10.811}, {"C",  12.011},
			{"N",  14.007},  {"O",  15.999},  {"F",  18.998}, {"Ne", 20.180},
			{"Na", 22.990},  {"Mg", 24.305},  {"Al", 26.982}, {"Si", 28.086},
			{"P",  30.974},  {"S",  32.065},  {"Cl", 35.453}, {"Ar", 39.948},
			{"K",  39.098},  {"Ca", 40.078},  {"Ti", 47.867}, {"V",  50.942},
			{"Cr", 51.996},  {"Mn", 54.938},  {"Fe", 55.845}, {"Co", 58.933},
			{"Ni", 58.693},  {"Cu", 63.546},  {"Zn", 65.38},  {"Ga", 69.723},
			{"Ge", 72.630},  {"As", 74.922},  {"Se", 78.971}, {"Br", 79.904},
			{"Kr", 83.798},  {"Rb", 85.468},  {"Sr", 87.62},  {"Y",  88.906},
			{"Zr", 91.224},  {"Nb", 92.906},  {"Mo", 95.96},  {"Tc", 98.0},
			{"Ru",101.07},   {"Rh",102.906},  {"Pd",106.42},  {"Ag",107.868},
			{"Cd",112.411},  {"In",114.818},  {"Sn",118.710}, {"Sb",121.760},
			{"Te",127.60},   {"I", 126.904},  {"Xe",131.293}, {"Cs",132.905},
			{"Ba",137.327},  {"La",138.905},  {"Ce",140.116}, {"Pr",140.908},
			{"Nd",144.242},  {"Sm",150.36},   {"Eu",151.964}, {"Gd",157.25},
			{"Tb",158.925},  {"Dy",162.500},  {"Ho",164.930}, {"Er",167.259},
			{"Tm",168.934},  {"Yb",173.054},  {"Lu",174.967}, {"Hf",178.49},
			{"Ta",180.948},  {"W", 183.84},   {"Re",186.207}, {"Os",190.23},
			{"Ir",192.217},  {"Pt",195.084},  {"Au",196.967}, {"Hg",200.592},
			{"Tl",204.383},  {"Pb",207.2},    {"Bi",208.980}, {"Th",232.038},
			{"U", 238.029},  {"Pu",244.0},
		};
		for (const auto& [s, m] : tbl)
			if (sym == s) return m;
		return 1.0;
	}

	// -----------------------------------------------------------------------
	// formal_charge  —  common formal charges for ionic elements.
	// -----------------------------------------------------------------------
	static double formal_charge(const std::string& sym) {
		static const std::pair<const char*, double> tbl[] = {
			{"Li", +1}, {"Na", +1}, {"K",  +1}, {"Rb", +1}, {"Cs", +1},
			{"Mg", +2}, {"Ca", +2}, {"Sr", +2}, {"Ba", +2},
			{"Al", +3}, {"Fe", +3}, {"Cr", +3}, {"La", +3}, {"Y",  +3},
			{"Fe", +2}, {"Ni", +2}, {"Cu", +2}, {"Zn", +2}, {"Pb", +2},
			{"Ti", +4}, {"Zr", +4}, {"Hf", +4}, {"Si", +4},
			{"F",  -1}, {"Cl", -1}, {"Br", -1}, {"I",  -1},
			{"O",  -2}, {"S",  -2}, {"Se", -2},
			{"N",  -3}, {"P",  -3},
		};
		for (const auto& [s, q] : tbl)
			if (sym == s) return q;
		return 0.0;
	}

	// -----------------------------------------------------------------------
	// trim  —  strip leading/trailing whitespace from a string.
	// -----------------------------------------------------------------------
	static std::string trim(const std::string& s) {
		const auto b = s.find_first_not_of(" \t\r\n");
		if (b == std::string::npos) return {};
		const auto e = s.find_last_not_of(" \t\r\n");
		return s.substr(b, e - b + 1);
	}
};

} // namespace vsim
