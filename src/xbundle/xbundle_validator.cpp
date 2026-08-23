/**
 * src/xbundle/xbundle_validator.cpp
 * ====================================
 * WO-72A  -  .X Bundle Format  —  Validator implementation
 *
 * Validation rules  (see xbundle_validator.hpp for rule descriptions)
 *   V-01 … V-08
 *
 * WO-72A | V5.1.4
 */

#include "include/xbundle/xbundle_validator.hpp"

#include <set>
#include <string>
#include <vector>

namespace vsim {
namespace xbundle {

std::vector<std::string> XBundleValidator::validate(const XBundle& bundle) {
	std::vector<std::string> errors;

	// V-01: manifest populated
	if (!bundle.manifest.populated)
		errors.emplace_back("V-01: [manifest] block not found in bundle");

	// V-02: manifest name non-empty
	if (bundle.manifest.populated && bundle.manifest.name.empty())
		errors.emplace_back("V-02: manifest.name must not be empty");

	// V-03: at least one entry
	if (bundle.entries.empty())
		errors.emplace_back("V-03: bundle contains no [[member]] entries");

	// V-04, V-05, V-06, V-08: per-entry checks
	std::set<std::string> seen_names;
	for (std::size_t i = 0; i < bundle.entries.size(); ++i) {
		const auto& e   = bundle.entries[i];
		const std::string idx = "[member " + std::to_string(i) + "]";

		// V-04: non-empty name
		if (e.name.empty()) {
			errors.emplace_back("V-04: " + idx + " entry has empty name");
			continue;  // skip further checks for this entry
		}

		// V-05: unique names
		if (!seen_names.insert(e.name).second)
			errors.emplace_back("V-05: duplicate entry name \"" + e.name + "\"");

		// V-06: vsim entries must have content
		if (e.kind == XBundleEntryKind::vsim && e.content.empty())
			errors.emplace_back(
				"V-06: vsim entry \"" + e.name + "\" has empty content");

		// V-08: declared_size (if non-zero) must match actual byte count
		if (e.declared_size > 0 &&
			e.declared_size != static_cast<std::uint64_t>(e.content.size())) {
			errors.emplace_back(
				"V-08: entry \"" + e.name + "\" declared size " +
				std::to_string(e.declared_size) + " != actual " +
				std::to_string(e.content.size()));
		}
	}

	// V-07: entry_point (if set) must name an existing entry
	if (!bundle.manifest.entry_point.empty()) {
		bool found = false;
		for (const auto& e : bundle.entries)
			if (e.name == bundle.manifest.entry_point) { found = true; break; }
		if (!found)
			errors.emplace_back(
				"V-07: entry_point \"" + bundle.manifest.entry_point +
				"\" does not name any member in this bundle");
	}

	return errors;
}

} // namespace xbundle
} // namespace vsim
