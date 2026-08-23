#pragma once
/**
 * xsim_export_bridge.hpp
 * ======================
 * Bridge between vsim::ExportSection / vsim::VsimDocument and
 * xsim::xport types.
 *
 * This header MUST NOT be included by any other xsim_xport_*.hpp header.
 * It is the only file in the xsim::xport package that may depend on
 * VSIM core types.
 *
 * Only valid in a full project build where include/vsim/ is on the path.
 *
 * Usage:
 *   #include <xsim/xport/xsim_export_bridge.hpp>
 *   auto config = xsim::xport::config_from_export_section(doc.exports);
 *   auto job    = xsim::xport::job_from_vsim_doc(doc, run_dir, output_dir);
 *
 * namespace xsim::xport
 */

#include <filesystem>
#include "xsim_xport_config.hpp"
#include "xsim_xport_job.hpp"

// VSIM core — only available in the full project build.
#include "vsim/vsim_document.hpp"

namespace xsim::xport {

// ============================================================================
// config_from_export_section
//
// Translates a vsim::ExportSection into an ExportConfig.
// Fields with no counterpart in ExportSection are defaulted to false.
// ============================================================================

inline ExportConfig config_from_export_section(const vsim::ExportSection& s) {
	ExportConfig c;
	c.write_xyz                 = s.write_xyz;
	c.write_analysis_json       = s.write_analysis_json;
	c.write_metrics_tsv         = s.write_metrics_tsv;
	c.write_report_md           = s.write_report_md;
	c.write_events_json         = s.write_events_json;
	c.write_symbolic_trace_json = s.write_symbolic_trace_json;
	c.write_manifest_json       = s.write_manifest_json;
	c.write_verify_report       = false;  // no ExportSection counterpart
	c.write_verify_tsv          = false;  // no ExportSection counterpart
	if (!s.output_dir.empty())
		c.output_dir = std::filesystem::path(s.output_dir);
	return c;
}

// ============================================================================
// job_from_vsim_doc
//
// Builds an ExportJob from a fully parsed VsimDocument plus the
// resolved run_dir and output_dir paths.
// ============================================================================

inline ExportJob job_from_vsim_doc(const vsim::VsimDocument& doc,
								   const std::filesystem::path& run_dir,
								   const std::filesystem::path& output_dir) {
	ExportJob job;
	job.run_dir      = run_dir;
	job.output_dir   = output_dir;
	job.package_name = doc.project.name;
	job.run_id       = doc.project.name;
	job.config       = config_from_export_section(doc.exports);
	if (!output_dir.empty())
		job.config.output_dir = output_dir;
	return job;
}

} // namespace xsim::xport
