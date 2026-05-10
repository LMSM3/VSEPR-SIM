#pragma once
/**
 * include/batch/batch_parser.hpp
 * ================================
 * WO-VSIM-62C — Batch Layer Parser
 *
 * Parses a .vsim study file (one that contains a [study] block) into a
 * BatchDocument.  Reuses the same tokenisation conventions as VsimParser:
 * # comments, key = value, [section], [[array-section]].
 *
 * validate() returns a list of error strings; empty = valid.
 * Validation checks are defined in spec §1.10.
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/batch/batch_document.hpp"
#include <string>
#include <vector>

namespace vsim {
namespace batch {

class BatchParser {
public:
	static BatchDocument parse_file  (const std::string& path);
	static BatchDocument parse_string(const std::string& src,
									  const std::string& source_path = "");

	static std::vector<std::string> validate(const BatchDocument& doc);

private:
	BatchDocument doc_;
	std::string   current_section_;
	bool          in_double_bracket_ = false;

	// Currently-being-built array entries (pushed on section change)
	BatchAxisEntry*          current_axis_     = nullptr;
	BatchCaseEntry*          current_case_     = nullptr;
	FormationLibraryEntry*   current_lib_      = nullptr;
	FormationStage*          current_stage_    = nullptr;

	void parse_content(const std::string& src);
	void handle_section(const std::string& sec);
	void handle_key_value(const std::string& key, const std::string& raw);

	// Finalise in-progress array entries
	void flush_axis();
	void flush_case();
	void flush_lib_stage();

	// Helpers matching vsim_parser style
	static std::string trim(const std::string& s);
	static std::string strip_comment(const std::string& s);
	static std::string unquote(const std::string& s);
	static bool        parse_bool(const std::string& s);
	static double      parse_double(const std::string& s);
	static int         parse_int(const std::string& s);
	static uint64_t    parse_u64(const std::string& s);
	static std::vector<std::string> parse_array(const std::string& s);
};

} // namespace batch
} // namespace vsim
