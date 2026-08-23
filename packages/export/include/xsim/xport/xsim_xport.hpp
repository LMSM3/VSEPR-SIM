#pragma once
/**
 * xsim_xport.hpp
 * ==============
 * Main public entry point for the xsim::xport package.
 * This is the only header most callers need.
 *
 *   #include <xsim/xport/xsim_xport.hpp>
 *
 * For the VSIM bridge (requires full project build):
 *   #include <xsim/xport/xsim_export_bridge.hpp>
 *
 * Execution flow:
 *   export.x  →  ExportXParser  →  ExportJob
 *                                       |
 *                              ExportPackage::run(job)
 *                                       |
 *                                  ExportResult
 *
 * namespace xsim::xport
 */

#include "xsim_xport_config.hpp"
#include "xsim_xport_job.hpp"
#include "xsim_xport_result.hpp"
#include "xsim_xport_parser.hpp"
#include "xsim_xport_writer.hpp"
#include "xsim_xport_bundle.hpp"

#include <filesystem>

namespace xsim::xport {

// ============================================================================
// ExportPackage — top-level export orchestrator
// ============================================================================

class ExportPackage {
public:
	ExportPackage();   // registers built-in writers (XYZWriter, ...)

	// Load package defaults from an export.x declaration file.
	// Returns false if the file cannot be read; template_job_ stays at defaults.
	bool load_package_file(const std::filesystem::path& path);

	// Run all enabled writers against the provided job.
	ExportResult run(const ExportJob& job);

	// Access the template job loaded from export.x.
	// Use this as a starting point when constructing a new ExportJob.
	[[nodiscard]] const ExportJob& template_job() const;

private:
	ExportJob      template_job_;
	WriterRegistry registry_;
};

// ============================================================================
// run_export() — convenience free function for single-call usage
//
//   ExportResult result = run_export(job);
//
// Equivalent to constructing ExportPackage and calling run(job) directly.
// No export.x file is loaded; all configuration is taken from the job.
// ============================================================================

[[nodiscard]] ExportResult run_export(const ExportJob& job);

} // namespace xsim::xport
