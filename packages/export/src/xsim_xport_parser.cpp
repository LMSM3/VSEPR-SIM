/**
 * src/xsim_xport_parser.cpp
 * =========================
 * ExportXParser — parses export.x package declaration files.
 *
 * Grammar:
 *   # comment
 *   [package]
 *     name    = "..."
 *     kind    = "..."
 *     version = "..."
 *   [run]
 *     run_dir = "..."
 *   [export]
 *     write_xyz  = true
 *     output_dir = "out/..."
 *
 * Rules:
 *   - Lines starting with '#' or empty lines are skipped.
 *   - Section headers: '[name]' on a non-indented line.
 *   - Key-value pairs: 'key = value' anywhere inside a section.
 *   - String values may be quoted; quotes are stripped.
 *   - Unknown keys and unknown sections are silently ignored.
 *   - Both [package] and [run] are optional; defaults apply.
 *   - Only [export] keys populate ExportConfig.
 *   - output_dir is written to both job.output_dir and job.config.output_dir.
 */

#include "xsim/xport/xsim_xport_parser.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace xsim::xport {

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

std::string trim(const std::string& s) {
	const auto b = s.find_first_not_of(" \t\r\n");
	if (b == std::string::npos) return {};
	const auto e = s.find_last_not_of(" \t\r\n");
	return s.substr(b, e - b + 1);
}

std::string strip_quotes(const std::string& s) {
	if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
		return s.substr(1, s.size() - 2);
	return s;
}

bool to_bool(const std::string& v) {
	return v == "true" || v == "1" || v == "yes" || v == "on";
}

// Split "key = value" — returns true when '=' is found.
bool split_kv(const std::string& line, std::string& key, std::string& val) {
	const auto eq = line.find('=');
	if (eq == std::string::npos) return false;
	key = trim(line.substr(0, eq));
	val = strip_quotes(trim(line.substr(eq + 1)));
	return !key.empty();
}

// Extract section name from "[name]" — returns empty string if not a header.
std::string section_name(const std::string& tr) {
	if (tr.size() >= 3 && tr.front() == '[' && tr.back() == ']')
		return trim(tr.substr(1, tr.size() - 2));
	return {};
}

} // anonymous namespace

// ============================================================================
// Section tracker
// ============================================================================

enum class Section { none, package_, run_, export_ };

// ============================================================================
// Core parse loop
// ============================================================================

ExportJob ExportXParser::parse(const std::string& text) {
	ExportJob job;
	Section   sec = Section::none;

	std::istringstream stream(text);
	std::string line;

	while (std::getline(stream, line)) {
		const std::string tr = trim(line);
		if (tr.empty() || tr.front() == '#') continue;

		// Section header?
		const std::string sname = section_name(tr);
		if (!sname.empty()) {
			if      (sname == "package") sec = Section::package_;
			else if (sname == "run")     sec = Section::run_;
			else if (sname == "export")  sec = Section::export_;
			else                         sec = Section::none;
			continue;
		}

		std::string key, val;
		if (!split_kv(tr, key, val)) continue;

		switch (sec) {
		case Section::package_:
			if      (key == "name")    { job.package_name = val; job.run_id = val; }
			else if (key == "kind")    { /* informational — not stored */ }
			else if (key == "version") { /* informational — not stored */ }
			break;

		case Section::run_:
			if (key == "run_dir")
				job.run_dir = std::filesystem::path(val);
			break;

		case Section::export_:
			if      (key == "write_xyz")                 job.config.write_xyz                 = to_bool(val);
			else if (key == "write_analysis_json")       job.config.write_analysis_json       = to_bool(val);
			else if (key == "write_metrics_tsv")         job.config.write_metrics_tsv         = to_bool(val);
			else if (key == "write_report_md")           job.config.write_report_md           = to_bool(val);
			else if (key == "write_events_json")         job.config.write_events_json         = to_bool(val);
			else if (key == "write_symbolic_trace_json") job.config.write_symbolic_trace_json = to_bool(val);
			else if (key == "write_manifest_json")       job.config.write_manifest_json       = to_bool(val);
			else if (key == "write_verify_report")       job.config.write_verify_report       = to_bool(val);
			else if (key == "write_verify_tsv")          job.config.write_verify_tsv          = to_bool(val);
			else if (key == "output_dir") {
				const std::filesystem::path p(val);
				job.config.output_dir = p;
				job.output_dir        = p;
			}
			break;

		default:
			break;
		}
	}

	// If job.output_dir was not set by [export] output_dir, derive from config.
	if (job.output_dir.empty() && !job.config.output_dir.empty())
		job.output_dir = job.config.output_dir;

	return job;
}

// ============================================================================
// Public API
// ============================================================================

ExportJob ExportXParser::parse_file(const std::filesystem::path& path) {
	std::ifstream f(path);
	if (!f.is_open())
		throw std::runtime_error("ExportXParser: cannot open: " + path.string());
	std::ostringstream ss;
	ss << f.rdbuf();
	return parse(ss.str());
}

ExportJob ExportXParser::parse_string(const std::string& text) {
	return parse(text);
}

} // namespace xsim::xport
