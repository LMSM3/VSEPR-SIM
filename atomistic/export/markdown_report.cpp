/**
 * markdown_report.cpp  --  Day 84 / WO-84E
 * ============================================================================
 * Markdown report generation for VSEPR + OrganicCandidate + SceneHints.
 * See header for the section contract.
 * ============================================================================
 */

#include "atomistic/export/markdown_report.hpp"
#include "atomistic/export/vsepr_export.hpp"

#include <cmath>
#include <cstdint>
#include <sstream>

namespace atomistic {
namespace classify {

namespace {

double finite_or_zero(double v) {
	return std::isfinite(v) ? v : 0.0;
}

const char* provider_source_label(const OrganicCandidate& c) {
	if (c.used_provider_lone_pair) return "provider";
	if (c.used_fallback_lone_pair) return "fallback";
	return "none";
}

void emit_list(std::ostringstream& os, const std::vector<std::string>& items,
			   const char* empty_text) {
	if (items.empty()) {
		os << "_" << empty_text << "_\n";
		return;
	}
	for (const auto& s : items) {
		os << "- " << s << "\n";
	}
}

} // namespace

std::string build_markdown_report(const std::string& title,
								  const VSEPRReport& vsepr,
								  const OrganicCandidate& candidate,
								  const BondOrderTable& bond_orders,
								  const SceneHints& hints) {
	std::ostringstream os;

	os << "# Chemical Scene Report: " << title << "\n\n";
	os << "_Day 84 / WO-84E  --  VSEPR + OrganicCandidate + SceneHints_\n\n";

	// ---- [VSEPR] -----------------------------------------------------------
	os << "## [VSEPR]\n\n";
	if (vsepr.sites.empty()) {
		os << "_No VSEPR sites resolved._\n\n";
	} else {
		const std::size_t ci = vsepr_central_site_index(vsepr);
		const VSEPRSite& c = vsepr.sites[ci];
		const bool fallback = c.used_element_lone_pair_inference ||
							  c.used_geometry_lone_pair_fallback;
		os << "| field | value |\n";
		os << "|---|---|\n";
		os << "| central_atom | Z=" << c.center_Z << " |\n";
		os << "| bonding_domains | " << c.bonded_domain_count << " |\n";
		os << "| lone_pairs | " << c.lone_pair_domain_count << " |\n";
		os << "| geometry | " << to_string(c.molecular_shape) << " |\n";
		os << "| provider_source | " << vsepr_lone_pair_source(c) << " |\n";
		os << "| fallback_used | " << (fallback ? "true" : "false") << " |\n";
		os << "| confidence | " << finite_or_zero(c.confidence) << " |\n";
		if (c.used_d8_square_planar) {
			os << "| d8_square_planar | true |\n";
		}
		os << "\n";
	}

	// ---- [OrganicCandidate] ------------------------------------------------
	int single = 0, dbl = 0, triple = 0, aromatic = 0;
	for (const auto& [k, v] : bond_orders.orders) {
		(void)k;
		if (v >= 2.9)      ++triple;
		else if (v >= 1.9) ++dbl;
		else if (v >= 1.4) ++aromatic;
		else               ++single;
	}

	os << "## [OrganicCandidate]\n\n";
	os << "| field | value |\n";
	os << "|---|---|\n";
	os << "| formula | " << candidate.formula << " |\n";
	os << "| organic_score | " << finite_or_zero(candidate.final_score) << " |\n";
	os << "| rings | " << candidate.ring_count << " |\n";
	os << "| aromatic | " << (candidate.aromatic_ring_count > 0 ? "true" : "false")
	   << " (" << candidate.aromatic_ring_count << ") |\n";
	os << "| bond_orders | single=" << single << " double=" << dbl
	   << " triple=" << triple << " aromatic=" << aromatic << " |\n";
	os << "| rotatable_bonds | " << candidate.rotatable_bond_count << " |\n";
	os << "| provider_source | " << provider_source_label(candidate) << " |\n";
	os << "| fallback_used | " << (candidate.used_fallback_lone_pair ? "true" : "false")
	   << " |\n";
	os << "\n";

	os << "**functional_groups**\n\n";
	emit_list(os, candidate.functional_groups, "none detected");
	os << "\n";

	// ---- [SceneHints] ------------------------------------------------------
	os << "## [SceneHints]\n\n";
	os << "| field | value |\n";
	os << "|---|---|\n";
	os << "| host_object | "
	   << (hints.host_object.empty() ? "(unresolved)" : hints.host_object) << " |\n";
	os << "\n";

	os << "**adsorbates**\n\n";
	emit_list(os, hints.adsorbates, "none");
	os << "\n**surface_sites**\n\n";
	emit_list(os, hints.surface_sites, "none");
	os << "\n**residue_candidates**\n\n";
	emit_list(os, hints.residue_candidates, "none");
	os << "\n**unsupported_features**\n\n";
	emit_list(os, hints.unsupported_features, "none");
	os << "\n";

	// ---- Known limitations -------------------------------------------------
	os << "## Known limitations\n\n";
	os << "- SceneHints is provisional: resolved-object binding is not yet implemented.\n";
	os << "- Aromaticity is gated by Huckel 4n+2 over inferred Kekule bond orders; "
		  "unsupported ring conjugation is reported as non-aromatic rather than guessed.\n";
	os << "- Bond orders are inferred by valence-deficit accounting, not from a "
		  "quantum bond-order provider.\n";
	os << "- d8 square-planar disambiguation covers a fixed metal set "
		  "(Ni/Pd/Pt/Rh/Ir/Au/Co); other metals use ordinary AXE geometry.\n";

	return os.str();
}

} // namespace classify
} // namespace atomistic
