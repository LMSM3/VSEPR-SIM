/**
 * src/xsim_xport_verify.cpp
 * =========================
 * XportVerifier — post-export bundle verification.
 *
 * Phase 1 (skeleton): confirms that every file in
 * ExportResult::written_files exists and is non-empty.
 *
 * Real format-level checks (JSON parse, TSV column count, etc.)
 * are added in later phases.
 */

#include "xsim/xport/xsim_xport_verify.hpp"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace xsim::xport {

VerifyResult XportVerifier::verify(const ExportJob& /*job*/,
								   const ExportResult& result) {
	VerifyResult vr;
	vr.passed = true;

	for (const auto& path : result.written_files) {
		std::error_code ec;
		if (!fs::exists(path, ec)) {
			vr.failures.push_back("missing: " + path.string());
			vr.passed = false;
		} else if (fs::file_size(path, ec) == 0) {
			vr.warnings.push_back("empty file: " + path.string());
		}
	}

	if (!result.ok()) {
		vr.passed = false;
		for (const auto& e : result.errors)
			vr.failures.push_back("export error: " + e);
	}

	return vr;
}

} // namespace xsim::xport
