/**
 * src/batch/batch_require_checker.cpp
 * ======================================
 * WO-VSIM-62C — [batch.require] Enforcement Implementation
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/batch/batch_require_checker.hpp"

#include <fstream>
#include <string>

namespace vsim {
namespace batch {

RequireCheckResult check_run_require(const BatchRequireSection& require,
									 const std::string&         run_output_dir,
									 const std::string&         run_id)
{
	RequireCheckResult result;
	result.run_id = run_id;

	// Check declared files exist
	for (const auto& fname : require.files) {
		std::string full_path = run_output_dir + "/" + fname;
		std::ifstream probe(full_path);
		if (!probe.good())
			result.missing_files.push_back(fname);
	}

	// Check declared check names appear in verify_report.json (if any)
	if (!require.checks.empty()) {
		std::string vr_path = run_output_dir + "/verify_report.json";
		std::ifstream vr(vr_path);
		if (!vr.good()) {
			// Can't read verify_report — all declared checks are missing
			for (const auto& c : require.checks)
				result.missing_checks.push_back(c);
		} else {
			std::string vr_content((std::istreambuf_iterator<char>(vr)),
									std::istreambuf_iterator<char>());
			for (const auto& check_name : require.checks) {
				// Presence check: look for the check name as a JSON key
				if (vr_content.find("\"" + check_name + "\"") == std::string::npos)
					result.missing_checks.push_back(check_name);
			}
		}
	}

	if (!result.missing_files.empty())
		result.failure_mode = "FAIL_OUTPUT_MISSING";
	else if (!result.missing_checks.empty())
		result.failure_mode = "FAIL_CHECK_MISSING";

	result.pass = result.missing_files.empty() && result.missing_checks.empty();
	return result;
}

} // namespace batch
} // namespace vsim
