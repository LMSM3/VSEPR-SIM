/**
 * vsepr_export.cpp  --  Day 84 / WO-84E
 * ============================================================================
 * JSON-lite export for VSEPRReport.  See header for the field contract.
 * ============================================================================
 */

#include "atomistic/export/vsepr_export.hpp"

#include <cmath>
#include <sstream>

namespace atomistic {
namespace classify {

namespace {

// Emit a finite double or 0.0 (never NaN/Inf in chemistry outputs).
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

} // namespace

const char* vsepr_lone_pair_source(const VSEPRSite& site) {
	if (site.used_provider_lone_pair)          return "provider";
	if (site.used_element_lone_pair_inference) return "element";
	if (site.used_geometry_lone_pair_fallback) return "geometry";
	return "none";
}

std::size_t vsepr_central_site_index(const VSEPRReport& report) {
	std::size_t best = 0;
	std::size_t best_domains = 0;
	for (std::size_t i = 0; i < report.sites.size(); ++i) {
		if (report.sites[i].electron_domain_count > best_domains) {
			best_domains = report.sites[i].electron_domain_count;
			best = i;
		}
	}
	return best;
}

std::string vsepr_to_json(const VSEPRReport& report) {
	std::ostringstream os;
	os << "{\n";
	os << "  \"schema\": \"vsepr-lite/1\",\n";
	os << "  \"site_count\": " << report.sites.size() << ",\n";

	if (report.sites.empty()) {
		os << "  \"central_atom\": 0,\n";
		os << "  \"bonding_domains\": 0,\n";
		os << "  \"lone_pairs\": 0,\n";
		os << "  \"geometry\": \"unknown\",\n";
		os << "  \"provider_source\": \"none\",\n";
		os << "  \"fallback_used\": false,\n";
		os << "  \"confidence\": 0.0,\n";
		os << "  \"sites\": []\n";
		os << "}\n";
		return os.str();
	}

	const std::size_t ci = vsepr_central_site_index(report);
	const VSEPRSite& c = report.sites[ci];
	const char* src = vsepr_lone_pair_source(c);
	const bool fallback = c.used_element_lone_pair_inference ||
						  c.used_geometry_lone_pair_fallback;

	os << "  \"central_atom\": " << c.center_Z << ",\n";
	os << "  \"bonding_domains\": " << c.bonded_domain_count << ",\n";
	os << "  \"lone_pairs\": " << c.lone_pair_domain_count << ",\n";
	os << "  \"geometry\": \"" << json_escape(to_string(c.molecular_shape)) << "\",\n";
	os << "  \"provider_source\": \"" << src << "\",\n";
	os << "  \"fallback_used\": " << (fallback ? "true" : "false") << ",\n";
	os << "  \"confidence\": " << finite_or_zero(c.confidence) << ",\n";

	os << "  \"summary\": {\n";
	os << "    \"linear\": "      << report.linear_count      << ",\n";
	os << "    \"planar\": "      << report.planar_count      << ",\n";
	os << "    \"tetrahedral\": " << report.tetrahedral_count << ",\n";
	os << "    \"bent\": "        << report.bent_count        << ",\n";
	os << "    \"pyramidal\": "   << report.pyramidal_count   << ",\n";
	os << "    \"hypervalent\": " << report.hypervalent_count << "\n";
	os << "  },\n";

	os << "  \"provider_lone_pair_available\": "
	   << (report.provider_lone_pair_available ? "true" : "false") << ",\n";
	os << "  \"provider_lone_pair_sites\": " << report.provider_lone_pair_sites << ",\n";
	os << "  \"fallback_lone_pair_sites\": " << report.fallback_lone_pair_sites << ",\n";

	os << "  \"sites\": [\n";
	for (std::size_t i = 0; i < report.sites.size(); ++i) {
		const VSEPRSite& s = report.sites[i];
		os << "    {\n";
		os << "      \"index\": " << s.center_index << ",\n";
		os << "      \"Z\": " << s.center_Z << ",\n";
		os << "      \"ax_label\": \"" << json_escape(s.ax_label) << "\",\n";
		os << "      \"bonding_domains\": " << s.bonded_domain_count << ",\n";
		os << "      \"lone_pairs\": " << s.lone_pair_domain_count << ",\n";
		os << "      \"geometry\": \"" << json_escape(to_string(s.molecular_shape)) << "\",\n";
		os << "      \"electron_geometry\": \"" << json_escape(to_string(s.electron_geometry)) << "\",\n";
		os << "      \"lone_pair_source\": \"" << vsepr_lone_pair_source(s) << "\",\n";
		os << "      \"d8_square_planar\": " << (s.used_d8_square_planar ? "true" : "false") << ",\n";
		os << "      \"hypervalent\": " << (s.is_hypervalent ? "true" : "false") << ",\n";
		os << "      \"confidence\": " << finite_or_zero(s.confidence) << "\n";
		os << "    }" << (i + 1 < report.sites.size() ? "," : "") << "\n";
	}
	os << "  ]\n";
	os << "}\n";
	return os.str();
}

} // namespace classify
} // namespace atomistic
