#pragma once
/**
 * xsim_xport_manifest.hpp
 * ========================
 * ExportManifest — describes the contents of a completed export bundle.
 *
 * Populated by ExportPackage::run() after all writers have executed.
 * Written to output_dir/manifest.json by the manifest writer.
 *
 * namespace xsim::xport
 */

#include <filesystem>
#include <string>
#include <vector>

namespace xsim::xport {

struct ExportManifest {
	std::string           package_name;  // from [package] name
	std::string           kind;          // "xsim_export_bundle"
	std::string           version;       // from [package] version
	std::filesystem::path run_dir;       // source simulation directory
	std::filesystem::path output_dir;    // destination bundle directory

	std::vector<std::filesystem::path> artifacts;  // all files written to output_dir
};

} // namespace xsim::xport
