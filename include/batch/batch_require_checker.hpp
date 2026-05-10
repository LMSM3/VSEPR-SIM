#pragma once
/**
 * include/batch/batch_require_checker.hpp
 * =========================================
 * WO-VSIM-62C — [batch.require] Enforcement
 *
 * Evaluated after each run completes.  Checks that declared files exist
 * in the run output directory, and that declared check names appear in
 * the run's verify_report.json.
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/vsim/vsim_document.hpp"
#include <string>
#include <vector>

namespace vsim {
namespace batch {

struct RequireCheckResult {
	bool        pass = true;
	std::string run_id;
	std::vector<std::string> missing_files;
	std::vector<std::string> missing_checks;
	std::string failure_mode; // "FAIL_OUTPUT_MISSING" | "FAIL_CHECK_MISSING" | ""
};

RequireCheckResult check_run_require(const BatchRequireSection& require,
									 const std::string&         run_output_dir,
									 const std::string&         run_id);

} // namespace batch
} // namespace vsim
