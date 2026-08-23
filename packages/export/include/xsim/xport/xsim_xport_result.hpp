#pragma once
/**
 * xsim_xport_result.hpp
 * =====================
 * ExportResult — outcome of one export operation.
 *
 * No silent failure. Every I/O error is named.
 * Every file successfully written is listed.
 * ok() returns true only when the errors vector is empty.
 *
 * namespace xsim::xport
 */

#include <filesystem>
#include <string>
#include <vector>

namespace xsim::xport {

struct ExportResult {
	std::vector<std::filesystem::path> written_files;
	std::vector<std::string>           errors;

	[[nodiscard]] bool ok() const { return errors.empty(); }
};

} // namespace xsim::xport
