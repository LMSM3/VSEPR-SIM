/**
 * src/xsim_export.cpp
 * ===================
 * ExportPackage — top-level export orchestrator.
 *
 * The constructor registers all built-in writers.
 * load_package_file() parses export.x into template_job_.
 * run(ExportJob) dispatches to each writer enabled by job.config.
 */

#include "xsim/xport/xsim_xport.hpp"
#include "xsim/xport/xsim_xport_parser.hpp"
#include "xsim/xport/xsim_xport_writer.hpp"
#include "xsim/xport/xsim_xport_writers.hpp"

namespace xsim::xport {

// ============================================================================
// Construction — register built-in exporters
// ============================================================================

ExportPackage::ExportPackage() {
	// Data writers (order here is the canonical dispatch order)
	registry_.add(std::make_unique<XYZWriter>());
	registry_.add(std::make_unique<AnalysisJsonWriter>());
	registry_.add(std::make_unique<MetricsTsvWriter>());
	registry_.add(std::make_unique<EventsJsonWriter>());
	registry_.add(std::make_unique<SymbolicTraceJsonWriter>());
	// Summary writers (run after data files are present)
	registry_.add(std::make_unique<ReportMdWriter>());
	// Verification writers (run after report_md)
	registry_.add(std::make_unique<VerifyReportWriter>());
	registry_.add(std::make_unique<VerifyTsvWriter>());
	// Manifest runs last — scans output_dir to inventory everything
	registry_.add(std::make_unique<ManifestJsonWriter>());
}

// ============================================================================
// load_package_file
// ============================================================================

bool ExportPackage::load_package_file(const std::filesystem::path& path) {
	try {
		ExportXParser parser;
		template_job_ = parser.parse_file(path);
		return true;
	} catch (...) {
		return false;
	}
}

// ============================================================================
// run
// ============================================================================

ExportResult ExportPackage::run(const ExportJob& job) {
	ExportResult combined;

	auto merge = [&](ExportResult&& r) {
		for (auto& f : r.written_files) combined.written_files.push_back(std::move(f));
		for (auto& e : r.errors)        combined.errors.push_back(std::move(e));
	};

	// Dispatch to a named writer when the config flag is set.
	// Unregistered writers are silently skipped; I/O failures go in errors.
	auto dispatch = [&](bool enabled, const char* key) {
		if (!enabled) return;
		IWriter* w = registry_.find(key);
		if (w) merge(w->write(job));
	};

	// Dispatch order is intentional:
	//   1. Data files          — raw simulation artifacts
	//   2. report_md           — summary (references data)
	//   3. verify_report / tsv — checks data files exist
	//   4. manifest_json       — inventory of everything (must run last)
	dispatch(job.config.write_xyz,                 "xyz");
	dispatch(job.config.write_analysis_json,       "analysis_json");
	dispatch(job.config.write_metrics_tsv,         "metrics_tsv");
	dispatch(job.config.write_events_json,         "events_json");
	dispatch(job.config.write_symbolic_trace_json, "symbolic_trace_json");
	dispatch(job.config.write_report_md,           "report_md");
	dispatch(job.config.write_verify_report,       "verify_report");
	dispatch(job.config.write_verify_tsv,          "verify_tsv");
	dispatch(job.config.write_manifest_json,       "manifest_json");  // last

	return combined;
}

// ============================================================================
// Accessors
// ============================================================================

const ExportJob& ExportPackage::template_job() const {
	return template_job_;
}

// ============================================================================
// run_export() — convenience free function
// ============================================================================

ExportResult run_export(const ExportJob& job) {
	ExportPackage pkg;
	return pkg.run(job);
}

} // namespace xsim::xport
