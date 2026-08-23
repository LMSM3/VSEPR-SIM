#pragma once
/**
 * xsim_xport_parser.hpp
 * =====================
 * ExportXParser — parses export.x package declaration files.
 *
 * export.x grammar (INI-style, line-oriented):
 *
 *   # comment
 *   [package]
 *     name    = "sio2_pipe_analysis_export"
 *     kind    = "xsim_export_bundle"
 *     version = "stable+0.0.1"
 *
 *   [run]
 *     run_dir = "runs/sio2_pipe_case_001"
 *
 *   [export]
 *     write_xyz = true
 *     output_dir = "out/sio2_pipe_analysis/final_bundle"
 *
 * Returns a fully populated ExportJob.
 *
 * No dependency on vsim_document.hpp or vsim_runtime.hpp.
 *
 * namespace xsim::xport
 */

#include <filesystem>
#include <string>
#include "xsim_xport_job.hpp"

namespace xsim::xport {

class ExportXParser {
public:
	ExportJob parse_file  (const std::filesystem::path& path);
	ExportJob parse_string(const std::string& text);

private:
	ExportJob parse(const std::string& text);
};

} // namespace xsim::xport
