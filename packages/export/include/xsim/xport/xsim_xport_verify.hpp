#pragma once
/**
 * xsim_xport_verify.hpp
 * =====================
 * VerifyResult   — outcome of a post-export verification pass.
 * XportVerifier  — structural checker for a completed export bundle.
 *
 * Verification confirms that the files listed in ExportResult::written_files
 * exist, are non-empty, and pass any format-level sanity checks.
 *
 * namespace xsim::xport
 */

#include <string>
#include <vector>
#include "xsim_xport_job.hpp"
#include "xsim_xport_result.hpp"

namespace xsim::xport {

struct VerifyResult {
	bool passed = false;
	std::vector<std::string> failures;
	std::vector<std::string> warnings;
};

class XportVerifier {
public:
	VerifyResult verify(const ExportJob& job, const ExportResult& result);
};

} // namespace xsim::xport
