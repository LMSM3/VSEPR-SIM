#pragma once
/**
 * xsim_xport_config.hpp
 * =====================
 * ExportConfig — declares what an export run should produce.
 *
 * Populated from the [export] section of an export.x file,
 * or assigned directly when building an ExportJob programmatically.
 *
 * Field names mirror vsim::ExportSection for clean bridge translation.
 * No dependency on vsim_document.hpp or vsim_runtime.hpp.
 *
 * namespace xsim::xport
 */

#include <filesystem>

namespace xsim::xport {

struct ExportConfig {
	bool write_xyz                 = true;
	bool write_analysis_json       = true;
	bool write_metrics_tsv         = true;
	bool write_report_md           = true;
	bool write_events_json         = false;
	bool write_symbolic_trace_json = false;
	bool write_manifest_json       = true;
	bool write_verify_report       = false;
	bool write_verify_tsv          = false;

	std::filesystem::path output_dir;   // destination for this export run
};

} // namespace xsim::xport
