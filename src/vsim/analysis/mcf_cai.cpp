/**
 * src/vsim/analysis/mcf_cai.cpp
 * ================================
 * WO-76 Step 3  |  MCF-CAI State Vector -- parser helpers implementation
 *
 * Implements:
 *   parse_mcf_layer()          -- "macro"/"chemical"/"fundamental" -> McfLayer
 *   parse_cai_basis()          -- "carrier"/"action"/"information" -> CaiBasis
 *   parse_mcf_cai_section()    -- "object.<layer>.<basis>" -> layer + basis
 *   McfCaiCell::cell_name()    -- human-readable cell label
 *
 * Added: WO-76 Step 3 (Day 76)
 */

#include "vsim/analysis/mcf_cai.hpp"

namespace vsim {
namespace analysis {

// ============================================================================
// parse_mcf_layer()
// ============================================================================

bool parse_mcf_layer(const std::string& s, McfLayer& out) noexcept {
	if (s == "macro")        { out = McfLayer::MACRO;    return true; }
	if (s == "chemical")     { out = McfLayer::CHEMICAL; return true; }
	if (s == "fundamental")  { out = McfLayer::FUND;     return true; }
	return false;
}

// ============================================================================
// parse_cai_basis()
// ============================================================================

bool parse_cai_basis(const std::string& s, CaiBasis& out) noexcept {
	if (s == "carrier")     { out = CaiBasis::CARRIER;     return true; }
	if (s == "action")      { out = CaiBasis::ACTION;      return true; }
	if (s == "information") { out = CaiBasis::INFORMATION; return true; }
	return false;
}

// ============================================================================
// parse_mcf_cai_section()
//
// Accepts "object.<layer>.<basis>" — the leading "object." prefix is part of
// the VSIM section name as it appears in the script.  E.g.:
//   "object.macro.carrier"       -> MACRO, CARRIER
//   "object.chemical.information"-> CHEMICAL, INFORMATION
//   "object.fundamental.action"  -> FUND, ACTION
// ============================================================================

bool parse_mcf_cai_section(const std::string& section,
							McfLayer& layer_out,
							CaiBasis& basis_out) noexcept {
	// Must start with "object."
	static const std::string prefix = "object.";
	if (section.compare(0, prefix.size(), prefix) != 0) return false;

	const std::string rest = section.substr(prefix.size()); // "<layer>.<basis>"
	const auto dot = rest.find('.');
	if (dot == std::string::npos) return false;

	const std::string layer_str = rest.substr(0, dot);
	const std::string basis_str = rest.substr(dot + 1);

	McfLayer  ml;
	CaiBasis  cb;
	if (!parse_mcf_layer(layer_str, ml)) return false;
	if (!parse_cai_basis(basis_str, cb)) return false;

	layer_out = ml;
	basis_out = cb;
	return true;
}

// ============================================================================
// McfCaiCell::cell_name()
// ============================================================================

std::string McfCaiCell::cell_name() const {
	const char* layers[] = { "macro", "chemical", "fundamental" };
	const char* bases[]  = { "carrier", "action", "information" };
	const int l = static_cast<int>(layer);
	const int b = static_cast<int>(basis);
	if (l < 0 || l >= MCF_LAYERS || b < 0 || b >= CAI_BASES)
		return "unknown.unknown";
	return std::string(layers[l]) + "." + bases[b];
}

} // namespace analysis
} // namespace vsim
