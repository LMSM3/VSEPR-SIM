/**
 * organic_candidate_export.cpp  --  Day 84 / WO-84E
 * ============================================================================
 * JSON-lite export for OrganicCandidate + provisional SceneHints.
 * See header for the field contract.
 * ============================================================================
 */

#include "atomistic/export/organic_candidate_export.hpp"

#include <cmath>
#include <cstdint>
#include <sstream>

namespace atomistic {
namespace classify {

namespace {

double finite_or_zero(double v) {
	return std::isfinite(v) ? v : 0.0;
}

std::string json_escape(const std::string& s) {
	std::string out;
	out.reserve(s.size() + 2);
	for (char c : s) {
		switch (c) {
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n";  break;
			case '\t': out += "\\t";  break;
			case '\r': out += "\\r";  break;
			default:    out += c;      break;
		}
	}
	return out;
}

void emit_string_array(std::ostringstream& os,
					   const std::vector<std::string>& items) {
	os << "[";
	for (std::size_t i = 0; i < items.size(); ++i) {
		os << "\"" << json_escape(items[i]) << "\"";
		if (i + 1 < items.size()) os << ", ";
	}
	os << "]";
}

// Provider provenance label for a candidate.
const char* provider_source_label(const OrganicCandidate& c) {
	if (c.used_provider_lone_pair) return "provider";
	if (c.used_fallback_lone_pair) return "fallback";
	return "none";
}

static bool is_hydrogen(uint32_t z) { return z == 1; }
static bool is_carbon(uint32_t z)   { return z == 6; }

} // namespace

SceneHints derive_scene_hints(const State& state, const OrganicCandidate& cand) {
	SceneHints hints;

	// Host-object heuristic: an Si + O rich inorganic skeleton with no carbon
	// suggests a silica/SiO2 host (pipe / conduit / reactor wall).
	int si = 0, o = 0, c = 0, metal = 0;
	for (uint32_t z : state.type) {
		if (z == 14)      ++si;
		else if (z == 8)  ++o;
		else if (z == 6)  ++c;
		else if (z >= 21 && z <= 30) ++metal; // first-row TM band
	}
	if (si > 0 && o >= si && c == 0) {
		hints.host_object = "SiO2";
		hints.surface_sites.push_back("silanol_candidate");
		hints.surface_sites.push_back("bridging_oxygen");
	} else if (metal > 0 && c == 0) {
		hints.host_object = "metal_surface";
		hints.surface_sites.push_back("metal_site");
	}

	// Adsorbate hints: carbon-bearing candidate near/on an inorganic host.
	if (c > 0) {
		if (cand.aromatic_ring_count > 0) {
			hints.adsorbates.push_back("aromatic_adsorbate");
			hints.residue_candidates.push_back("carbonaceous_fouling_candidate");
		}
		for (const std::string& fg : cand.functional_groups) {
			if (fg == "hydroxyl")      hints.adsorbates.push_back("polar_adsorbate");
			else if (fg == "amine")    hints.adsorbates.push_back("polar_adsorbate");
		}
		if (cand.polarity_score >= 0.5) {
			hints.adsorbates.push_back("polar_adsorbate");
		}
	}

	// Everything below is honest about what is NOT yet resolved.
	hints.unsupported_features.push_back("resolved_object_binding");
	hints.unsupported_features.push_back("surface_site_geometry");
	hints.unsupported_features.push_back("adsorbate_host_distance_classification");
	hints.unsupported_features.push_back("residue_layer_stratification");
	if (cand.aromatic_ring_count == 0 && cand.ring_count > 0) {
		hints.unsupported_features.push_back("non_aromatic_ring_process_meaning");
	}

	(void)is_hydrogen;
	(void)is_carbon;
	return hints;
}

namespace {

std::string organic_body(const OrganicCandidate& c,
						 const BondOrderTable& bond_orders) {
	std::ostringstream os;

	// Bond-order histogram (single/double/triple), deterministic.
	int single = 0, dbl = 0, triple = 0, aromatic = 0;
	for (const auto& [k, v] : bond_orders.orders) {
		(void)k;
		if (v >= 2.9)      ++triple;
		else if (v >= 1.9) ++dbl;
		else if (v >= 1.4) ++aromatic;   // 1.5 aromatic convention (if present)
		else               ++single;
	}

	os << "  \"formula\": \"" << json_escape(c.formula) << "\",\n";
	os << "  \"id_hash\": \"" << json_escape(c.id_hash) << "\",\n";
	os << "  \"primary_family\": \""
	   << json_escape(organic_family_name(c.primary_family)) << "\",\n";

	os << "  \"families\": [";
	for (std::size_t i = 0; i < c.families.size(); ++i) {
		os << "\"" << json_escape(organic_family_name(c.families[i])) << "\"";
		if (i + 1 < c.families.size()) os << ", ";
	}
	os << "],\n";

	os << "  \"functional_groups\": ";
	emit_string_array(os, c.functional_groups);
	os << ",\n";

	os << "  \"organic_score\": " << finite_or_zero(c.final_score) << ",\n";
	os << "  \"rings\": " << c.ring_count << ",\n";
	os << "  \"aromatic\": " << (c.aromatic_ring_count > 0 ? "true" : "false") << ",\n";
	os << "  \"aromatic_ring_count\": " << c.aromatic_ring_count << ",\n";

	os << "  \"bond_orders\": {\n";
	os << "    \"single\": " << single << ",\n";
	os << "    \"double\": " << dbl << ",\n";
	os << "    \"triple\": " << triple << ",\n";
	os << "    \"aromatic\": " << aromatic << "\n";
	os << "  },\n";

	os << "  \"rotatable_bonds\": " << c.rotatable_bond_count << ",\n";
	os << "  \"heteroatom_count\": " << c.heteroatom_count << ",\n";
	os << "  \"sp_count\": " << c.sp_count << ",\n";
	os << "  \"sp2_count\": " << c.sp2_count << ",\n";
	os << "  \"sp3_count\": " << c.sp3_count << ",\n";
	os << "  \"polarity_score\": " << finite_or_zero(c.polarity_score) << ",\n";
	os << "  \"strain_score\": " << finite_or_zero(c.strain_score) << ",\n";

	os << "  \"provider_source\": \"" << provider_source_label(c) << "\",\n";
	os << "  \"provider_set_available\": "
	   << (c.provider_set_available ? "true" : "false") << ",\n";
	os << "  \"fallback_used\": "
	   << (c.used_fallback_lone_pair ? "true" : "false") << "";
	return os.str();
}

std::string scene_hints_body(const SceneHints& h) {
	std::ostringstream os;
	os << "  \"scene_hints\": {\n";
	os << "    \"host_object\": \"" << json_escape(h.host_object) << "\",\n";
	os << "    \"adsorbates\": ";
	emit_string_array(os, h.adsorbates);
	os << ",\n";
	os << "    \"surface_sites\": ";
	emit_string_array(os, h.surface_sites);
	os << ",\n";
	os << "    \"residue_candidates\": ";
	emit_string_array(os, h.residue_candidates);
	os << ",\n";
	os << "    \"unsupported_features\": ";
	emit_string_array(os, h.unsupported_features);
	os << "\n";
	os << "  }";
	return os.str();
}

} // namespace

std::string organic_candidate_to_json(const OrganicCandidate& cand,
									  const BondOrderTable& bond_orders) {
	std::ostringstream os;
	os << "{\n";
	os << "  \"schema\": \"organic-candidate-lite/1\",\n";
	os << organic_body(cand, bond_orders) << "\n";
	os << "}\n";
	return os.str();
}

std::string organic_candidate_to_json(const OrganicCandidate& cand,
									  const BondOrderTable& bond_orders,
									  const SceneHints& hints) {
	std::ostringstream os;
	os << "{\n";
	os << "  \"schema\": \"organic-candidate-lite/1\",\n";
	os << organic_body(cand, bond_orders) << ",\n";
	os << scene_hints_body(hints) << "\n";
	os << "}\n";
	return os.str();
}

} // namespace classify
} // namespace atomistic
