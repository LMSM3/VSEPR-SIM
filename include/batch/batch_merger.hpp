#pragma once
/**
 * include/batch/batch_merger.hpp
 * ================================
 * WO-VSIM-62C — Axis Application & Template Loading
 *
 * BatchMerger provides two services:
 *
 *  1. load_template()    — parse a .vsim template file via VsimParser
 *  2. apply_axis_values()— mutate a VsimDocument by dot-path axis values
 *
 * Dot-path resolution follows the table in spec §2.4.
 * Unknown paths produce warnings (not hard errors — the hard check is in
 * BatchParser::validate()).
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/vsim/vsim_document.hpp"
#include <map>
#include <string>
#include <vector>

namespace vsim {
namespace batch {

class BatchMerger {
public:
	// Load and parse a .vsim template.  Errors → throws std::runtime_error.
	static VsimDocument load_template(const std::string& path,
									  std::vector<std::string>& errors);

	// Apply axis values (dot-path → string) to a VsimDocument.
	// Returns mutated copy.  Unrecognised paths pushed into warnings_out.
	static VsimDocument apply_axis_values(
		const VsimDocument&                         base,
		const std::map<std::string, std::string>&   axis_values,
		std::vector<std::string>&                   warnings_out);

private:
	// Set a single dot-path field; return false if path is unrecognised.
	static bool set_path(VsimDocument&      doc,
						 const std::string& path,
						 const std::string& value);
};

} // namespace batch
} // namespace vsim
