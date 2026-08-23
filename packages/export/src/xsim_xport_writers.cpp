/**
 * src/xsim_xport_writers.cpp
 * ==========================
 * Implementations for all eight built-in export writers.
 *
 * Shared helper:  effective_out_dir(job)  — resolves the output directory.
 * Shared helper:  copy_artifact()         — copy file from run_dir or create placeholder.
 *
 * Output filename contract (from README):
 *   xyz              → trajectory.xyz
 *   analysis_json    → analysis.json
 *   metrics_tsv      → metrics.tsv
 *   events_json      → events.json
 *   symbolic_trace   → symbolic_trace.json
 *   report_md        → report.md
 *   verify_report    → verify_report.md
 *   verify_tsv       → verify.tsv
 *   manifest_json    → manifest.json     (last: scans output_dir)
 */

#include "xsim/xport/xsim_xport_writers.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace xsim::xport {

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

fs::path effective_out_dir(const ExportJob& job) {
	if (!job.output_dir.empty())        return job.output_dir;
	if (!job.config.output_dir.empty()) return job.config.output_dir;
	return fs::path(".");
}

// Create output directory, then either copy src_file from run_dir
// or create an empty placeholder when the source is absent.
ExportResult copy_artifact(const ExportJob& job,
							const char* src_name,
							const char* dst_name) {
	ExportResult result;
	const fs::path out_dir  = effective_out_dir(job);
	const fs::path dst      = out_dir / dst_name;

	std::error_code ec;
	fs::create_directories(out_dir, ec);
	if (ec) {
		result.errors.push_back(std::string(dst_name) + ": mkdir: " + ec.message());
		return result;
	}

	const fs::path src = job.run_dir / src_name;
	if (!job.run_dir.empty() && fs::exists(src, ec)) {
		fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
		if (ec) {
			result.errors.push_back(std::string(dst_name) + ": copy: " + ec.message());
			return result;
		}
	} else {
		std::ofstream f(dst, std::ios::out | std::ios::trunc);
		if (!f.is_open()) {
			result.errors.push_back(std::string(dst_name) + ": create: " + dst.string());
			return result;
		}
	}
	result.written_files.push_back(dst);
	return result;
}

// Write string content to a file in output_dir.
ExportResult write_text(const ExportJob& job,
						 const char* dst_name,
						 const std::string& content) {
	ExportResult result;
	const fs::path out_dir = effective_out_dir(job);
	const fs::path dst     = out_dir / dst_name;

	std::error_code ec;
	fs::create_directories(out_dir, ec);
	if (ec) {
		result.errors.push_back(std::string(dst_name) + ": mkdir: " + ec.message());
		return result;
	}

	std::ofstream f(dst, std::ios::out | std::ios::trunc);
	if (!f.is_open()) {
		result.errors.push_back(std::string(dst_name) + ": open: " + dst.string());
		return result;
	}
	f << content;
	result.written_files.push_back(dst);
	return result;
}

// Expected output filenames keyed to config flags.
struct ExpectedFile { std::string name; bool enabled; };

std::vector<ExpectedFile> expected_files(const ExportJob& job) {
	return {
		{ "trajectory.xyz",      job.config.write_xyz                 },
		{ "analysis.json",       job.config.write_analysis_json       },
		{ "metrics.tsv",         job.config.write_metrics_tsv         },
		{ "events.json",         job.config.write_events_json         },
		{ "symbolic_trace.json", job.config.write_symbolic_trace_json },
		{ "report.md",           job.config.write_report_md           },
	};
}

// Escape a string for use inside a JSON string literal.
std::string json_str(const std::string& s) {
	std::string out;
	out.reserve(s.size() + 2);
	out += '"';
	for (char c : s) {
		if (c == '"') out += "\\\"";
		else if (c == '\\') out += "\\\\";
		else out += c;
	}
	out += '"';
	return out;
}

} // anonymous namespace

// ============================================================================
// Copy writers
// ============================================================================

std::string AnalysisJsonWriter::format_key() const      { return "analysis_json"; }
std::string MetricsTsvWriter::format_key() const        { return "metrics_tsv"; }
std::string EventsJsonWriter::format_key() const        { return "events_json"; }
std::string SymbolicTraceJsonWriter::format_key() const { return "symbolic_trace_json"; }

ExportResult AnalysisJsonWriter::write(const ExportJob& job) {
	return copy_artifact(job, "analysis.json",       "analysis.json");
}
ExportResult MetricsTsvWriter::write(const ExportJob& job) {
	return copy_artifact(job, "metrics.tsv",         "metrics.tsv");
}
ExportResult EventsJsonWriter::write(const ExportJob& job) {
	return copy_artifact(job, "events.json",         "events.json");
}
ExportResult SymbolicTraceJsonWriter::write(const ExportJob& job) {
	return copy_artifact(job, "symbolic_trace.json", "symbolic_trace.json");
}

// ============================================================================
// ReportMdWriter — generate report.md
// ============================================================================

std::string ReportMdWriter::format_key() const { return "report_md"; }

ExportResult ReportMdWriter::write(const ExportJob& job) {
	const std::string run_id  = job.run_id.empty()       ? "(unknown)" : job.run_id;
	const std::string pkg     = job.package_name.empty() ? "(unknown)" : job.package_name;
	const std::string run_dir = job.run_dir.empty()      ? "(none)"    : job.run_dir.string();
	const std::string out_dir = effective_out_dir(job).string();

	std::ostringstream md;
	md << "# Simulation Report\n\n";
	md << "| Field          | Value |\n";
	md << "|----------------|-------|\n";
	md << "| **Run ID**     | " << run_id  << " |\n";
	md << "| **Package**    | " << pkg     << " |\n";
	md << "| **Run dir**    | `" << run_dir << "` |\n";
	md << "| **Output dir** | `" << out_dir << "` |\n";
	md << "\n---\n\n";
	md << "## Export Configuration\n\n";
	md << "| Output           | Enabled |\n";
	md << "|------------------|---------|\n";
	auto flag = [](bool v) { return v ? "yes" : "no"; };
	md << "| trajectory.xyz      | " << flag(job.config.write_xyz)                 << " |\n";
	md << "| analysis.json       | " << flag(job.config.write_analysis_json)       << " |\n";
	md << "| metrics.tsv         | " << flag(job.config.write_metrics_tsv)         << " |\n";
	md << "| events.json         | " << flag(job.config.write_events_json)         << " |\n";
	md << "| symbolic_trace.json | " << flag(job.config.write_symbolic_trace_json) << " |\n";
	md << "| report.md           | yes |\n";
	md << "| verify_report.md    | " << flag(job.config.write_verify_report)       << " |\n";
	md << "| verify.tsv          | " << flag(job.config.write_verify_tsv)          << " |\n";
	md << "| manifest.json       | " << flag(job.config.write_manifest_json)       << " |\n";
	md << "\n---\n\n";
	md << "*Generated by xsim::xport stable+0.0.1*\n";

	return write_text(job, "report.md", md.str());
}

// ============================================================================
// VerifyReportWriter — generate verify_report.md
// Checks that files enabled in config exist and are non-empty.
// Runs after all data writers and report_md.
// ============================================================================

std::string VerifyReportWriter::format_key() const { return "verify_report"; }

ExportResult VerifyReportWriter::write(const ExportJob& job) {
	const fs::path out_dir = effective_out_dir(job);
	const auto files = expected_files(job);

	struct Row { std::string name; std::string status; std::string note; };
	std::vector<Row> rows;
	bool all_pass = true;

	for (const auto& ef : files) {
		if (!ef.enabled) continue;
		const fs::path p = out_dir / ef.name;
		std::error_code ec;
		const bool exists    = fs::exists(p, ec);
		const auto sz        = exists ? fs::file_size(p, ec) : 0;
		const std::string st = !exists ? "MISSING" : (sz == 0 ? "EMPTY" : "OK");
		if (st != "OK") all_pass = false;
		rows.push_back({ ef.name, st, "" });
	}

	std::ostringstream md;
	md << "# Verification Report\n\n";
	md << "**Run ID:** " << (job.run_id.empty() ? "(unknown)" : job.run_id) << "\n\n";
	md << "---\n\n";
	md << "## File Checks\n\n";
	md << "| File | Status |\n";
	md << "|------|--------|\n";
	for (const auto& row : rows)
		md << "| " << row.name << " | " << row.status << " |\n";
	md << "\n---\n\n";
	md << "## Result\n\n";
	md << (all_pass ? "**PASS**" : "**FAIL**") << "\n";

	return write_text(job, "verify_report.md", md.str());
}

// ============================================================================
// VerifyTsvWriter — generate verify.tsv
// Same checks as VerifyReportWriter in TSV format.
// ============================================================================

std::string VerifyTsvWriter::format_key() const { return "verify_tsv"; }

ExportResult VerifyTsvWriter::write(const ExportJob& job) {
	const fs::path out_dir = effective_out_dir(job);
	const auto files = expected_files(job);

	std::ostringstream tsv;
	tsv << "file\tstatus\tsize_bytes\n";

	for (const auto& ef : files) {
		if (!ef.enabled) continue;
		const fs::path p = out_dir / ef.name;
		std::error_code ec;
		const bool exists = fs::exists(p, ec);
		const auto sz     = exists ? fs::file_size(p, ec) : static_cast<uintmax_t>(0);
		const std::string st = !exists ? "MISSING" : (sz == 0 ? "EMPTY" : "OK");
		tsv << ef.name << '\t' << st << '\t' << sz << '\n';
	}

	return write_text(job, "verify.tsv", tsv.str());
}

// ============================================================================
// ManifestJsonWriter — generate manifest.json
// Scans output_dir filesystem to inventory all artifacts.
// Must run LAST so all other files are present.
// ============================================================================

std::string ManifestJsonWriter::format_key() const { return "manifest_json"; }

ExportResult ManifestJsonWriter::write(const ExportJob& job) {
	const fs::path out_dir = effective_out_dir(job);
	const std::string manifest_name = "manifest.json";
	const fs::path manifest_path = out_dir / manifest_name;

	// Collect existing artifacts (everything except manifest itself, which isn't written yet).
	std::vector<std::string> artifacts;
	std::error_code ec;
	if (fs::exists(out_dir, ec)) {
		for (const auto& entry : fs::directory_iterator(out_dir, ec)) {
			if (entry.is_regular_file())
				artifacts.push_back(entry.path().filename().string());
		}
		std::sort(artifacts.begin(), artifacts.end());
	}
	// Append manifest itself last.
	artifacts.push_back(manifest_name);

	const std::string run_id  = job.run_id.empty()       ? "" : job.run_id;
	const std::string pkg     = job.package_name.empty() ? "" : job.package_name;
	const std::string run_dir = job.run_dir.string();

	std::ostringstream json;
	json << "{\n";
	json << "  \"xsim_version\": \"stable+0.0.1\",\n";
	json << "  \"run_id\": "       << json_str(run_id)  << ",\n";
	json << "  \"package_name\": " << json_str(pkg)     << ",\n";
	json << "  \"run_dir\": "      << json_str(run_dir) << ",\n";
	json << "  \"output_dir\": "   << json_str(out_dir.string()) << ",\n";
	json << "  \"artifacts\": [\n";
	for (size_t i = 0; i < artifacts.size(); ++i) {
		json << "    " << json_str(artifacts[i]);
		if (i + 1 < artifacts.size()) json << ",";
		json << "\n";
	}
	json << "  ]\n";
	json << "}\n";

	return write_text(job, manifest_name.c_str(), json.str());
}

} // namespace xsim::xport
