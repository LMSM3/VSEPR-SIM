// chemplus_declarative.hpp  -  WO-84T
//
// Chem+ declarative bridge.
//
// Evaluates a ChemPlusSection declared in a .vsim script and produces a
// structured ChemPlusResult that can be consumed by the observe sink,
// the reporting layer, or the VSEPR geometry bridge.
//
// Design:
//   - Header-only; zero external dependencies beyond vsim_document.hpp.
//   - Mirrors the classify / parse_energy / shell_anim vocabulary from
//     chem/chem_shell/controller.py without spawning a Python process.
//   - When vsepr_link = true, a topology tag is derived from the dominant
//     product species using the same AX-notation used by the observe sink.
//
// Usage:
//   #include "vsim/chemplus_declarative.hpp"
//   auto result = vsim::chemplus::evaluate(doc.chem_plus);
//   if (result.ok) { ... result.reaction_class ... result.vsepr_tag ... }
//
#pragma once

#include "vsim/vsim_document.hpp"

#include <string>
#include <vector>
#include <optional>
#include <cmath>

namespace vsim {
namespace chemplus {

// ---------------------------------------------------------------------------
// Per-reaction evaluation result
// ---------------------------------------------------------------------------
struct ReactionResult {
	std::string       reaction;         // original reaction string
	std::string       reaction_class;   // "combustion" | "decomposition" | ...
	double            energy_kj  = 0.0; // 0.0 = not present in string
	std::string       energy_mode;      // "exothermic" | "endothermic" | "unknown"
	std::string       shell_anim;       // animation hint (mirrors controller.py)
	std::string       web_anim;         // web animation hint
	std::string       vsepr_tag;        // e.g. "AX2E2" for water product (vsepr_link only)
	bool              has_energy = false;
};

// Top-level result for the whole ChemPlusSection
struct ChemPlusResult {
	bool                          ok = false;
	std::vector<ReactionResult>   reactions;  // one entry per evaluated reaction
	std::string                   error;      // non-empty on failure
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace detail {

inline std::string shell_anim_for(const std::string& cls) {
	if (cls == "combustion")    return "flash_arrow";
	if (cls == "decomposition") return "split_burst";
	if (cls == "synthesis")     return "merge_glow";
	if (cls == "acid-base")     return "neutralize_fade";
	return "sparkline";
}

inline std::string web_anim_for(const std::string& cls) {
	if (cls == "combustion")    return "heat_burst";
	if (cls == "decomposition") return "fragment_scatter";
	if (cls == "synthesis")     return "molecule_merge";
	if (cls == "acid-base")     return "ph_gradient";
	return "molecule_flow";
}

// Minimal VSEPR topology tag inference from a product formula token.
// Handles the common organics / inorganics seen in preset libraries.
inline std::string vsepr_tag_for_product(const std::string& product) {
	// Strip leading coefficient digits and whitespace
	std::size_t p = 0;
	while (p < product.size() && (std::isdigit(static_cast<unsigned char>(product[p])) ||
								   std::isspace(static_cast<unsigned char>(product[p])))) ++p;
	std::string sp = product.substr(p);

	// Common patterns
	if (sp == "H2O")                            return "AX2E2";  // bent
	if (sp == "CO2")                            return "AX2";    // linear
	if (sp == "NH3")                            return "AX3E";   // pyramidal
	if (sp == "CH4")                            return "AX4";    // tetrahedral
	if (sp == "HCl" || sp == "H2")             return "AX1";    // diatomic
	if (sp == "Cl2" || sp == "O2" || sp == "N2") return "AX1";
	if (sp == "SO3" || sp == "BF3")            return "AX3";    // trigonal planar
	if (sp == "H3PO4")                          return "AX4";
	if (sp == "H2SO4")                          return "AX4";
	if (sp.find("OH") != std::string::npos)     return "AX2E2";
	// Generic fallback based on first uppercase letter count as crude bond count
	int uc = 0;
	for (char c : sp) if (std::isupper(static_cast<unsigned char>(c))) ++uc;
	if (uc <= 1) return "AX1";
	if (uc == 2) return "AX2";
	if (uc == 3) return "AX3";
	return "AX4";
}

inline ReactionResult eval_one(const std::string& rxn_str, bool want_vsepr) {
	ReactionResult r;
	r.reaction      = rxn_str;

	// Classify
	const auto cls   = ChemPlusSection::classify(rxn_str);
	r.reaction_class = ChemPlusSection::class_to_string(cls);
	r.shell_anim     = shell_anim_for(r.reaction_class);
	r.web_anim       = web_anim_for(r.reaction_class);

	// Energy
	const double e   = ChemPlusSection::parse_energy(rxn_str);
	if (std::abs(e) > 1e-9) {
		r.energy_kj  = e;
		r.has_energy = true;
		r.energy_mode = (e > 0.0) ? "exothermic" : "endothermic";
	} else {
		r.energy_mode = "unknown";
	}

	// VSEPR link: infer topology from the first product after "->"
	if (want_vsepr) {
		const auto arrow = rxn_str.find("->");
		if (arrow != std::string::npos) {
			std::string rhs = rxn_str.substr(arrow + 2);
			// Strip trailing energy notation ("+ 891 kJ")
			const auto kj = rhs.find("kJ");
			if (kj != std::string::npos) rhs = rhs.substr(0, kj);
			// Take first '+'-separated token
			const auto plus = rhs.find('+');
			std::string first_product = (plus != std::string::npos) ? rhs.substr(0, plus) : rhs;
			// Trim whitespace
			while (!first_product.empty() && std::isspace(static_cast<unsigned char>(first_product.front())))
				first_product.erase(first_product.begin());
			while (!first_product.empty() && std::isspace(static_cast<unsigned char>(first_product.back())))
				first_product.pop_back();
			r.vsepr_tag = vsepr_tag_for_product(first_product);
		}
	}
	return r;
}

// Built-in preset libraries (mirrors controller.py PRESET_998 / PRESET_999)
inline std::vector<std::string> preset_998() {
	return {
		"2AgCl -> 2Ag + Cl2",
		"CaC2 + 2H2O -> C2H2 + Ca(OH)2",
		"Na2S2O3 + I2 -> Na2S4O6 + 2NaI",
		"3Mg + N2 -> Mg3N2",
		"2Fe + 3Cl2 -> 2FeCl3",
		"Zn + 2HCl -> ZnCl2 + H2",
		"2KClO3 -> 2KCl + 3O2",
		"CaCO3 -> CaO + CO2",
	};
}

inline std::vector<std::string> preset_999() {
	return {
		"CH4 + 2O2 -> CO2 + 2H2O + 891 kJ",
		"C2H6 + 3.5O2 -> 2CO2 + 3H2O + 1561 kJ",
		"C3H8 + 5O2 -> 3CO2 + 4H2O + 2219 kJ",
		"2H2 + O2 -> 2H2O + 572 kJ",
		"P2O5 + 3H2O -> 2H3PO4 + 177 kJ",
		"N2 + 3H2 -> 2NH3 + 92 kJ",
		"SO3 + H2O -> H2SO4 + 130 kJ",
		"C + O2 -> CO2 + 394 kJ",
	};
}

} // namespace detail

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Evaluate the ChemPlusSection and return a structured result.
inline ChemPlusResult evaluate(const ChemPlusSection& cfg) {
	ChemPlusResult out;

	if (!cfg.is_active()) {
		out.ok    = true;
		out.error = "chem_plus section is not active (no reaction or preset)";
		return out;
	}

	// Collect reaction strings
	std::vector<std::string> rxns;
	if (cfg.has_reaction())
		rxns.push_back(cfg.reaction);
	if (cfg.has_preset()) {
		if      (cfg.preset == "998") rxns = detail::preset_998();
		else if (cfg.preset == "999") rxns = detail::preset_999();
		else {
			out.error = "unknown preset tag: " + cfg.preset;
			return out;
		}
		// Also include explicit reaction if provided alongside preset
		if (cfg.has_reaction())
			rxns.insert(rxns.begin(), cfg.reaction);
	}

	for (const auto& rxn : rxns) {
		auto rr = detail::eval_one(rxn, cfg.vsepr_link);
		// Apply overrides
		if (!cfg.class_override.empty())
			rr.reaction_class = cfg.class_override;
		if (std::abs(cfg.energy_kj) > 1e-9) {
			rr.energy_kj  = cfg.energy_kj;
			rr.has_energy = true;
			rr.energy_mode = (cfg.energy_kj > 0.0) ? "exothermic" : "endothermic";
		}
		out.reactions.push_back(std::move(rr));
	}

	out.ok = true;
	return out;
}

// Convenience: format a single ReactionResult as a human-readable string.
inline std::string format_result(const ReactionResult& r) {
	std::string s;
	s  = "  reaction      : " + r.reaction + "\n";
	s += "  class          : " + r.reaction_class + "\n";
	if (r.has_energy)
		s += "  energy_kj      : " + std::to_string(r.energy_kj) + "  (" + r.energy_mode + ")\n";
	else
		s += "  energy_kj      : n/a\n";
	s += "  shell_anim     : " + r.shell_anim + "\n";
	s += "  web_anim       : " + r.web_anim   + "\n";
	if (!r.vsepr_tag.empty())
		s += "  vsepr_tag      : " + r.vsepr_tag + "\n";
	return s;
}

} // namespace chemplus
} // namespace vsim
