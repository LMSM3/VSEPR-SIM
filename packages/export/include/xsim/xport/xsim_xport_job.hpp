#pragma once
/**
 * xsim_xport_job.hpp
 * ==================
 * ExportJob — one concrete export operation.
 *
 * Populated by ExportXParser from an export.x file,
 * or constructed programmatically and handed to ExportPackage::run().
 *
 * The embedded ExportConfig defines what to produce.
 * run_dir is where completed simulation artifacts are read from.
 * output_dir is where the export bundle is written to.
 *
 * When output_dir is non-empty it overrides config.output_dir.
 *
 * namespace xsim::xport
 */

#include <filesystem>
#include <string>
#include "xsim_xport_config.hpp"

namespace xsim::xport {

struct ExportJob {
	std::filesystem::path run_dir;     // completed simulation output directory
	std::filesystem::path output_dir;  // destination for the export bundle
	ExportConfig          config;      // what to produce

	std::string run_id;        // identifier for the run (from [package] name)
	std::string package_name;  // originating package name
};

} // namespace xsim::xport
