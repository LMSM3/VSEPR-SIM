#pragma once
/**
 * vsim_parser.hpp — .vsim file parser
 * ====================================
 *
 * Parses TOML-subset .vsim scripts into VsimDocument.
 *
 * Grammar:
 *   file        := (line '\n')*
 *   line        := comment | section_header | key_value | blank
 *   comment     := '#' <rest of line>
 *   section_hdr := '[' IDENTIFIER ']'
 *   key_value   := IDENTIFIER WS* '=' WS* value (WS* comment)?
 *   value       := bool_lit | int_lit | float_lit | string_lit | list_lit
 *   bool_lit    := 'true' | 'false'
 *   string_lit  := '"' <chars> '"'
 *   list_lit    := '[' value (',' value)* ']'
 *   IDENTIFIER  := [A-Za-z_][A-Za-z0-9_]*
 *
 * Known sections handled structurally:
 *   [project]             → ProjectSection
 *   [simulation]          → SimulationSection + MoleculeEntry sub-blocks
 *   [export]              → ExportSection
 *   [defaults.run]        → GoldenDefaultsSection::run
 *   [defaults.analysis]   → GoldenDefaultsSection::analysis
 *   [test.<name>]         → GoldenTestEntry (appended to golden_tests)
 *   [test.<name>.run]     → GoldenTestEntry::run
 *   [test.<name>.analysis]→ GoldenTestEntry::analysis
 *   [suite]               → SuiteSection
 *   [suite.limits]        → SuiteSection limits
 *   [suite.smoke]         → SuiteSection smoke subset
 *   [report]              → GoldenReportSection
 *
 * Unknown sections are captured in VsimDocument::raw_sections.
 *
 * WO-56C  |  v5.0.0-beta.7
 */

#include "vsim_document.hpp"
#include <string>
#include <stdexcept>

namespace vsim {

// ============================================================================
// ParseError — carries line number for diagnostics
// ============================================================================

struct ParseError : std::runtime_error {
	int line_number;
	ParseError(int line, const std::string& msg)
		: std::runtime_error("vsim parse error at line " + std::to_string(line) + ": " + msg)
		, line_number(line) {}
};

// ============================================================================
// Parser
// ============================================================================

class VsimParser {
public:
	/**
	 * Parse a .vsim file from disk.
	 * Throws ParseError on structural problems.
	 * Missing optional fields are filled with defaults.
	 */
	static VsimDocument parse_file(const std::string& path);

	/**
	 * Parse .vsim content from a string (useful for tests).
	 * source_path is stored in document.source_path (may be empty).
	 */
	static VsimDocument parse_string(const std::string& content,
									 const std::string& source_path = "<string>");

private:
	VsimParser() = default;

	void parse_content(const std::string& content);

	void handle_section(const std::string& section_name, int line_no);
	void handle_key_value(const std::string& key, const std::string& raw_value, int line_no);

	void apply_project_key(const std::string& key, const Value& val, int line_no);
	void apply_simulation_key(const std::string& key, const Value& val, int line_no);
	void apply_molecule_key(const std::string& key, const Value& val, int line_no);
	void apply_export_key(const std::string& key, const Value& val, int line_no);
	void apply_export_visual_key(const std::string& key, const Value& val, int line_no);
	void apply_visual_key(const std::string& key, const Value& val, int line_no);
	void apply_visual_external_key(const std::string& key, const Value& val, int line_no);
	void apply_variance_key(const std::string& key, const Value& val, int line_no);
	void apply_n_evolution_key(const std::string& key, const Value& val, int line_no);
	void apply_while_key(const std::string& key, const Value& val, int line_no);
	void apply_batch_key(const std::string& key, const Value& val, int line_no);

	// WO-57B: PBC block appliers
	void apply_cell_key(const std::string& key, const Value& val, int line_no);
	void apply_boundary_key(const std::string& key, const Value& val, int line_no);
	void apply_pbc_key(const std::string& key, const Value& val, int line_no);

	// Golden suite section appliers
	void apply_golden_run_config(GoldenTestRunConfig& cfg,
								 const std::string& key, const Value& val);
	void apply_golden_analysis(GoldenTestAnalysis& ana,
							   const std::string& key, const Value& val);
	void apply_defaults_run_key(const std::string& key, const Value& val);
	void apply_defaults_analysis_key(const std::string& key, const Value& val);
	void apply_test_key(const std::string& key, const Value& val);
	void apply_test_run_key(const std::string& key, const Value& val);
	void apply_test_analysis_key(const std::string& key, const Value& val);
	void apply_suite_key(const std::string& key, const Value& val);
	void apply_suite_limits_key(const std::string& key, const Value& val);
	void apply_suite_smoke_key(const std::string& key, const Value& val);
	void apply_report_key(const std::string& key, const Value& val);

	Value parse_value(const std::string& raw, int line_no);
	std::string strip_comment(const std::string& line);
	std::string trim(const std::string& s);

	VsimDocument  doc_;
	std::string   current_section_;
	bool          in_molecule_block_ = false;  // inside [[simulation.molecule]] sub-block

	// Golden suite parse state
	std::string   current_test_name_;          // active [test.<name>] key (empty = none)
	bool          in_test_run_block_      = false;
	bool          in_test_analysis_block_ = false;
	bool          in_suite_limits_        = false;
	bool          in_suite_smoke_         = false;
};

} // namespace vsim
